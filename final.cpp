/* final.cpp -- Dec. 9 -- Copyright 2024 Craig Scratchley */
#define AF_LOCAL 1
#define SOCK_STREAM 1
#include <condition_variable>
#include <mutex>
#include <shared_mutex>
#include <map>
#include <cstring>         // for memcpy ?
#include <atomic>
#include <iostream>
#include "posixThread.hpp" // you are not examined on details of this class

using namespace std;
using namespace std::chrono;

/* Lock-free circular buffer.  This should be threadsafe if one thread is reading
 * and another is writing. */
template<class T>
class CircBuf {
   /* If read_pos == write_pos, the buffer is empty.
    *
    * There will always be at least one position empty, as a completely full
    * buffer (read_pos == write_pos) is indistinguishable from an empty buffer.
    *
    * Invariants: read_pos < size, write_pos < size. */
   static const unsigned size = 8 + 1; // capacity of 8 elements
    T buf[size];
   std::atomic<unsigned> read_pos = 0, write_pos = 0;
public:
   /* Write buffer_size elements from buffer into the circular buffer object,
    * and advance the write pointer.  Return the number of elements that were
    * able to be written.  If the data will not fit entirely,
    * as much data as possible will be fit in. */
   unsigned write( const T *buffer, unsigned buffer_size ) {
      T *p[2];
      unsigned sizes[2];

        p[0] = buf + write_pos.load( memory_order_relaxed );
        const unsigned rpos = read_pos.load( memory_order_acquire );
        /* Subtract 1 below, to account for the element that we never fill. */
        if( rpos <= write_pos.load( memory_order_relaxed ) ) {
            // The buffer looks like "eeeeDDDDeeee" or "eeeeeeeeeeee" (e = empty, D = data)
            p[1] = buf;
            if (rpos) {
                sizes[0] = size - write_pos.load( memory_order_relaxed );
                sizes[1] = rpos - 1;
            }
            else {
                sizes[0] = size - write_pos.load( memory_order_relaxed ) - 1;
                sizes[1] = rpos; // 0
            }
        } else {
            /* The buffer looks like "DDeeeeeeeeDD" (e = empty, D = data). */
            p[1] = nullptr; // could comment out
            sizes[0] = rpos - write_pos.load( memory_order_relaxed ) - 1;
            sizes[1] = 0;
        }

      buffer_size = min( buffer_size, sizes[0]+sizes[1]);//max_write_sz=sizes[0]+sizes[1]
      const int from_first = min( buffer_size, sizes[0] );
      memcpy( p[0], buffer, from_first*sizeof(T) );
      if( buffer_size > sizes[0] )
         memcpy(p[1], buffer+from_first, max(buffer_size-sizes[0], 0u)*sizeof(T) );
      write_pos.store( (write_pos.load( memory_order_relaxed ) + buffer_size) % size,
                  memory_order_release );
      return buffer_size;
   }

   /* Read buffer_size elements into buffer from the circular buffer object,
    * and advance the read pointer.  Return the number of elements that were
    * read.  If buffer_size elements cannot be read, as many elements as
    * possible will be read */
   unsigned read( T *buffer, unsigned buffer_size ) {
      T *p[2];
      unsigned sizes[2];

        p[0] = buf + read_pos.load( memory_order_relaxed );
        const unsigned wpos = write_pos.load( memory_order_acquire );
        if( read_pos.load( memory_order_relaxed ) <= wpos ) {
            // The buffer looks like "eeeeDDDDeeee" or "eeeeeeeeeeee" (e = empty, D = data)
            p[1] = nullptr; // could comment out
            sizes[0] = wpos - read_pos.load( memory_order_relaxed );
            sizes[1] = 0;
        } else {
            /* The buffer looks like "DDeeeeeeeeDD" (e = empty, D = data). */
            p[1] = buf;
            sizes[0] = size - read_pos.load( memory_order_relaxed );
            sizes[1] = wpos;
        }

      buffer_size = min( buffer_size, sizes[0]+sizes[1]);//max_read_sz=sizes[0]+sizes[1];
      const int from_first = min( buffer_size, sizes[0] );
      memcpy( buffer, p[0], from_first*sizeof(T) );
      if( buffer_size > sizes[0] )
         memcpy( buffer+from_first, p[1], max(buffer_size-sizes[0], 0u)*sizeof(T) );
      read_pos.store( (read_pos.load( memory_order_relaxed ) + buffer_size) % size,
                  memory_order_release );
      return buffer_size;
   } /* Original source Copyright (c) 2004 Glenn Maynard. */
};  /* See Craig for more details and code before improvements. */

// derived from https://stackoverflow.com/a/40159821
// tweaked by Craig Scratchley
// other variations might be useful.
// get a Value for a key in m or return a default_value if the key is not present
template <typename Key, typename Value, typename T>
Value get_or(std::map<Key, Value>& m, const Key& key, T&& default_value)
{
    auto it{m.find(key)};
    if (m.end() == it) {
        return default_value;
    }
    return it->second;
}

namespace{ //Unnamed (anonymous) namespace

    class socketInfoClass;
    typedef shared_ptr<socketInfoClass> socketInfoClassSp;
    map<int, socketInfoClassSp> desInfoMap;

    //   A shared mutex used to protect desInfoMap so only a single thread can modify the map
    //  at a time.  This also means that only one call to functions like mySocketpair() or
    //  myClose() can make progress at a time.  This shared mutex is also used to prevent a
    //  paired socket from being closed at the beginning of a myWrite or myTcdrain function.
    //  Shared mutex is described in Section 3.3.2 of Williams 2e
    shared_mutex mapMutex;

    class socketInfoClass {
        int totalWritten{0};
        int maxTotalCanRead{0};
        condition_variable cvDrain;
        condition_variable cvRead;
        CircBuf<char> circBuffer;
        mutex socketInfoMutex;
    public:
        int pair;   // Cannot be private because myWrite and myTcdrain using it.
                    // -1 when descriptor closed, -2 when paired descriptor is closed
   socketInfoClass(unsigned pairInit)
   :pair(pairInit) { }

    // If necessary, make the calling thread wait for a reading thread to drain the data
   int draining(shared_lock<shared_mutex> &desInfoLk) { // operating on object for pair
      unique_lock socketLk(socketInfoMutex);
      desInfoLk.unlock();

      //  once the reader decides the drainer should wakeup, it should wakeup
      if (0 <= pair && totalWritten > maxTotalCanRead)
         cvDrain.wait(socketLk); // spurious wakeup?  Not a problem with Linux p-threads
         // In the Solaris implementation of condition variables,
         //    a spurious wakeup may occur without the condition being assigned
         //    if the process is signaled; the wait system call aborts and
         //    returns EINTR.[2] The Linux p-thread implementation of condition variables
         //    guarantees that it will not do that.[3][4]
         // https://en.wikipedia.org/wiki/Spurious_wakeup
         //  cvDrain.wait(socketLk, [this]{return pair < 0 || totalWritten <= maxTotalCanRead;});
      return 0;
   }

   int writing(int des, const void* buf, size_t nbyte, shared_lock<shared_mutex> &desInfoLk) {
      // operating on object for paired descriptor of original des
      lock_guard socketLk(socketInfoMutex);
      desInfoLk.unlock();
      int written = circBuffer.write((const char*) buf, nbyte);
      if (written > 0) {
         totalWritten += written;
         cvRead.notify_one();
      }
      return written;
   }

   int reading(int des, void * buf, int n, int min, int time, int timeout, shared_lock<shared_mutex> &desInfoLk)
   {
      unique_lock socketLk(socketInfoMutex);
      desInfoLk.unlock();

      // would not have got this far if pair == -1
      if (maxTotalCanRead || (totalWritten < min && -2 != pair)) {
         maxTotalCanRead += n;
         cvDrain.notify_all(); // maxTotalCanRead must be greater than totalWritten
         if (0 == time && 0 == timeout)
            cvRead.wait(socketLk, [this, min] {
               return totalWritten >= min || 0 > pair;});
         else if (0 != time && 0 != timeout) {
            // if (!totalWritten) // do we need to wait for timeout duration if something already written?
            if (cv_status::timeout != cvRead.wait_for(socketLk, duration<int, deci>{timeout}))
               // after notification, myTcdrain() might have been called, and block
               while (totalWritten < min && 0 <= pair) // do we assume maxTotalCanRead == n
                  if (cv_status::timeout == cvRead.wait_for(socketLk, duration<int, deci>{time}))
                     break;
         }
         else {
            cout << "Currently not supporting this configuration of time and timeout." << endl;
            exit(EXIT_FAILURE);
         }
         maxTotalCanRead -= n;
      }
      int bytesRead;
      bytesRead = circBuffer.read((char *) buf, n);
      totalWritten -= bytesRead;

      if (totalWritten <= maxTotalCanRead) {
         int errnoHold{errno};
         cvDrain.notify_all();
         errno = errnoHold;
      }
      if (0 < totalWritten || -2 == pair) {
         int errnoHold{errno}; // debug gui not updating errnoHold very well.
         cvRead.notify_one();
         errno = errnoHold;
      }
      return bytesRead;
   } // .reading()

   int closing(int des)
   {
      // mapMutex already locked at this point, so no other myClose (or mySocketpair)
      if(pair != -2) { // pair has not already been closed
         socketInfoClassSp des_pair{desInfoMap[pair]};
         // See Williams 2e, 2nd half of section 3.2.4
         scoped_lock guard(socketInfoMutex, des_pair->socketInfoMutex); // safely lock both mutexes
         pair = -1; // this is first socket in the pair to be closed
         des_pair->pair = -2; // paired socket will be the second of the two to close.
         if (totalWritten > maxTotalCanRead) {
             // by closing the socket we are throwing away any buffered data.
             // notification will be sent immediately below to any myTcdrain waiters on paired descriptor.
             cvDrain.notify_all();
         }

         if (des_pair->maxTotalCanRead > 0) {
            // no more data will be written from des
            // notify a thread waiting on reading on paired descriptor
            des_pair->cvRead.notify_one();
         }
      }
      return 0;
   } // .closing()
   }; // socketInfoClass
} // unnamed namespace

//see https://www.qnx.com/developers/docs/7.1/#com.qnx.doc.neutrino.lib_ref/topic/r/readcond.html
int myReadcond(int des, void * buf, int n, int min, int time, int timeout) {
   shared_lock desInfoLk(mapMutex);
   auto desInfoP{get_or(desInfoMap, des, nullptr)}; // make a local shared pointer
   if (!desInfoP) {
      // not an open "socket" [created with mySocketpair()]
      errno = EBADF; return -1;
   }
 	return desInfoP->reading(des, buf, n, min, time, timeout, desInfoLk);
}

ssize_t myWrite(int des, const void* buf, size_t nbyte) {
   shared_lock desInfoLk(mapMutex);
   auto desInfoP{get_or(desInfoMap, des, nullptr)};
   if (desInfoP) {
      auto pair{desInfoP->pair};
      if (-2 != pair)
         return desInfoMap[pair]->writing(des, buf, nbyte, desInfoLk);
      else {
         errno = EPIPE; return -1;
      }
   }
   errno = EBADF; return -1;
}

int myTcdrain(int des) {
   shared_lock desInfoLk(mapMutex);
   auto desInfoP{get_or(desInfoMap, des, nullptr)};
   if (desInfoP) {
      auto pair{desInfoP->pair};
      if (-2 == pair)
         return 0; // paired descriptor is closed.
      else {
         auto desPairInfoSp{desInfoMap[pair]}; // make a local shared pointer
         return desPairInfoSp->draining(desInfoLk);
      }
   }
   errno = EBADF; return -1;
}

int mySocketpair(int domain, int type, int protocol, int des[2]) {
   lock_guard desInfoLk(mapMutex);
   des[0] = 3;
   des[1] = 4;
   desInfoMap[des[0]] = make_shared<socketInfoClass>(des[1]);
   desInfoMap[des[1]] = make_shared<socketInfoClass>(des[0]);
   return 0;
}

int myClose(int des) {
   lock_guard desInfoLk(mapMutex);
   auto iter{desInfoMap.find(des)};
   if (iter != desInfoMap.end()) { // if in the map
      desInfoMap.erase(iter);
      return iter->second->closing(des);
   }
   errno = EBADF;
   return -1;
}

static const int BSize = 20;

static char B[BSize]; // initially zeroed (filled with NUL characters)
static char B2[BSize]; // initially zeroed (filled with NUL characters)
static int daSktPr[2]; // Descriptor Array for Socket Pair

void coutThreadFunc(void) {
   int RetVal;
   CircBuf<char> buffer;
   buffer.write("123456789", 10); // don't forget NUL termination character
   RetVal = buffer.read(B, BSize);
   cout << "Output Line 1 - RetVal: " << RetVal << "  B: " << B << endl;
   cout << "The next line will timeout in 5 or so seconds (50 deciseconds)" << endl;
   RetVal = myReadcond(daSktPr[1], B, BSize, 10, 50, 50);

   cout << "Output Line 2 - RetVal: " << RetVal << "  B: " << B << endl;
   myWrite(daSktPr[1], "wxyz", 5); // don't forget NUL termination char

   myWrite(daSktPr[0], "ab", 3); // don't forget NUL termination character
   RetVal = myReadcond(daSktPr[1], B, BSize, 5, 0, 0);

   cout << "Output Line 3 - RetVal: " << RetVal;
   if (RetVal == -1)
      cout << " Error:  " << strerror(errno);
   else if (RetVal > 0)
      cout << "  B: " << B;
   cout << endl;
   RetVal = myReadcond(daSktPr[1], B, BSize, 5, 0, 0);
   cout << "Output Line 4 - RetVal: " << RetVal;
   if (RetVal == -1)
      cout << " Error:  " << strerror(errno);
   else if (RetVal > 0)
      cout << "  B: " << B;
   cout << endl;
   RetVal = myReadcond(daSktPr[1], B, BSize, 5, 0, 0);
   cout << "Output Line 4b- RetVal: " << RetVal;
   if (RetVal == -1)
      cout << " Error:  " << strerror(errno);
   else if (RetVal > 0)
      cout << "  B: " << B;
   cout << endl;
   RetVal = myWrite(daSktPr[1], "123456789", 10); // don't forget NUL termination char
   cout << "Output Line 5-1 - RetVal: " << RetVal;
   if (RetVal == -1)
      cout << " Error:  " << strerror(errno) << endl;
   else cout << endl;
   RetVal = myWrite(daSktPr[0], "123456789", 10); // don't forget NUL termination char
   cout << "Output Line 5-0 - RetVal: " << RetVal;
   if (RetVal == -1)
      cout << " Error:  " << strerror(errno) << endl;
   else cout << endl;
}

int main() { // debug launch configuration ensures gdbserver runs at FIFO priority 98 ...
   cpu_set_t cpu_set;
   CPU_ZERO(&cpu_set);
   CPU_SET(0, &cpu_set);
   const char* threadName = "Pri";
   pthread_setname_np(pthread_self(), threadName);
   sched_setaffinity(0, sizeof(cpu_set), &cpu_set); // set processor affinity

   try {    // ... a realtime policy.
      sched_param sch;
      sch.__sched_priority = 60;
      pthreadSupport::setSchedParam(SCHED_FIFO, sch); //SCHED_FIFO == 1, SCHED_RR == 2
      mySocketpair(AF_LOCAL, SOCK_STREAM, 0, daSktPr);
      myWrite(daSktPr[0], "abc", 3);
      pthreadSupport::posixThread coutThread(SCHED_FIFO, 50, coutThreadFunc); // lower
      myTcdrain(daSktPr[0]);

      myWrite(daSktPr[0], "123", 4); // don't forget NUL termination character
      myReadcond(daSktPr[0], B2, 4, 4, 0, 0);

      pthreadSupport::setSchedPrio(40);

      myClose(daSktPr[0]);

      coutThread.join();
      return 0;
   }
   catch (system_error& error) {
      cout << "Error: " << error.code() << " - " << error.what() << '\n';
   }
   catch (...) { throw; }
}
