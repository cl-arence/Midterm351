//============================================================================
// Name        : myIO.cpp
// Author(s)   : Craig Scratchley
//			   :
// Version     : November 2024 -- tcdrain for socketpairs.
// Copyright   : Copyright 2024, W. Craig Scratchley, SFU
// Description : An implementation of tcdrain-like behaviour for socketpairs.
//============================================================================

// use a circular buffer instead of read() and write() functions.
//#define CIRCBUF

#include <sys/socket.h>
#include <unistd.h>				// for posix i/o functions
#include <stdlib.h>
#include <termios.h>			// for tcdrain()
#include <fcntl.h>				// for open/creat
#include <errno.h>
#include <stdarg.h>
#include <mutex>				
#include <shared_mutex>
#include <condition_variable>	
#include <map>
#include <memory>
#include "AtomicCOUT.h"
#include "SocketReadcond.h"
#include "VNPE.h"
#ifdef CIRCBUF
    #include "RageUtil_CircularBuffer.h"
#endif

using namespace std;

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

//Unnamed namespace
namespace{

    class socketInfoClass;

    typedef shared_ptr<socketInfoClass> socketInfoClassSp;
    map<int, socketInfoClassSp> desInfoMap;

    //  A shared mutex used to protect desInfoMap so only a single thread can modify the map at a time.
    //  This also means that only one call to functions like mySocketpair() or myClose() can make progress at a time.
    //  This mutex is also used to prevent a paired socket from being closed at the beginning of a myWrite or myTcdrain function.
    //  Shared mutex is described in Section 3.3.2 of Williams 2e
    shared_mutex mapMutex;

    class socketInfoClass {
        unsigned totalWritten{0};
        unsigned maxTotalCanRead{0};
        condition_variable cvDrain;
        condition_variable cvRead;
    #ifdef CIRCBUF
        CircBuf<char> circBuffer;
//        bool connectionReset = false;
    #endif
        mutex socketInfoMutex;
    public:
        int pair;   // Cannot be private because myWrite and myTcdrain using it.
                    // -1 when descriptor closed, -2 when paired descriptor is closed

        socketInfoClass(unsigned pairInit)
        :pair(pairInit) {
    #ifdef CIRCBUF
           circBuffer.reserve(1100); // note constant of 1100
    #endif
        }

	/*
	 * Function:  if necessary, make the calling thread wait for a reading thread to drain the data
	 */
	int draining(shared_lock<shared_mutex> &desInfoLk)
	{ // operating on object for paired descriptor of original des
		unique_lock socketLk(socketInfoMutex);
		desInfoLk.unlock();

		//  once the reader decides the drainer should wakeup, it should wakeup
		if (pair >= 0 && totalWritten > maxTotalCanRead)
			cvDrain.wait(socketLk); // spurious wakeup?  Not a problem with Linux p-threads
		   // In the Solaris implementation of condition variables,
		   //    a spurious wakeup may occur without the condition being assigned
		   //    if the process is signaled; the wait system call aborts and
		   //    returns EINTR.[2] The Linux p-thread implementation of condition variables
		   //    guarantees that it will not do that.[3][4]
		   // https://en.wikipedia.org/wiki/Spurious_wakeup
		   //  cvDrain.wait(socketLk, [this]{return pair < 0 || totalWritten <= maxTotalCanRead;});

//		if (pair == -2) { // shouldn't normally happen
//			errno = EBADF; // check errno
//			return -1;
//		}
		return 0;
	}

	int writing(int des, const void* buf, size_t nbyte, shared_lock<shared_mutex> &desInfoLk)	{
		// operating on object for paired descriptor
		lock_guard socketLk(socketInfoMutex);
      desInfoLk.unlock();

#ifdef CIRCBUF
		int written = circBuffer.write((const char*) buf, nbyte);
#else
		int written = write(des, buf, nbyte);
#endif
        if (written > 0) {
            totalWritten += written;
            cvRead.notify_one();
        }
        return written;
	}

	int reading(int des, void * buf, int n, int min, int time, int timeout, shared_lock<shared_mutex> &desInfoLk)
	{ // it is assumed that des is for a socket in a socketpair created by mySocketpair
		int bytesRead;
		unique_lock socketLk(socketInfoMutex);
      desInfoLk.unlock();

		// would not have got this far if pair == -1
      if (-2 == pair)
         bytesRead = 0; // this avoids errno == 104 (Connection Reset by Peer)
      else if (!maxTotalCanRead && totalWritten >= (unsigned) min) {
         if (0 == min && 0 == totalWritten)
             bytesRead = 0;
         else {
#ifdef CIRCBUF
		        bytesRead = circBuffer.read((char *) buf, n);
#else
		        bytesRead = read(des, buf, n); // at least min will be waiting
#endif
		        if (bytesRead > 0) {
		           totalWritten -= bytesRead;
		           if (totalWritten <= maxTotalCanRead) {
		              int errnoHold{errno};
                    cvDrain.notify_all();
                    errno = errnoHold;
		           }
		        }
		    }
		}
		else {
			maxTotalCanRead += n;
         int errnoHold{errno};
         cvDrain.notify_all(); // totalWritten must be less than min
			if (0 != time || 0 != timeout) {
			   COUT << "Currently only supporting no timeouts or immediate timeout" << endl;
			   exit(EXIT_FAILURE);
			}

			cvRead.wait(socketLk, [this, min] {
			   return totalWritten >= (unsigned) min || pair < 0;});
         errno = errnoHold;
//			if (pair == -1) { // shouldn't normally happen
//			   errno = EBADF; // check errno value
//			   return -1;
//			}
#ifdef CIRCBUF
			bytesRead = circBuffer.read((char *) buf, n);
         totalWritten -= bytesRead;
#else
			// choice below could affect "Connection reset by peer" from read/wcsReadcond
			bytesRead = read(des, buf, n);
//			bytesRead = wcsReadcond(des, buf, n, min, time, timeout);

			if (-1 != bytesRead)
            totalWritten -= bytesRead;
			else
			   if (ECONNRESET == errno)
			      bytesRead = 0;
#endif // #ifdef CIRCBUF
            
			maxTotalCanRead -= n;
			if (0 < totalWritten || -2 == pair) {
            int errnoHold{errno}; // debug gui not updating errnoHold very well.
            cvRead.notify_one();
            errno = errnoHold;
         }
		}
		return bytesRead;
	} // .reading()

	/*
	 * Function:  Closing des. Should be done only after all other operations on des have returned.
	 */
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
//         if (maxTotalCanRead > 0) {
//             // there shouldn't be any threads waiting in myRead() or myReadcond() on des, but just in case.
//             cvRead.notify_all();
//         }

			if (des_pair->maxTotalCanRead > 0) {
				// no more data will be written from des
				// notify a thread waiting on reading on paired descriptor
				des_pair->cvRead.notify_one();
			}
//			if (des_pair->totalWritten > des_pair->maxTotalCanRead) {
//				// there shouldn't be any threads waiting in myTcdrain on des, but just in case.
//				des_pair->cvDrain.notify_all();
//			}
		}
		return close (des);
	} // .closing()
	}; // socketInfoClass
} // unnamed namespace

/*
 * Function:	Calling the reading member function to read
 * Return:		An integer with number of bytes read, or -1 for an error.
 * see https://www.qnx.com/developers/docs/7.1/#com.qnx.doc.neutrino.lib_ref/topic/r/readcond.html
 *
 */
int myReadcond(int des, void * buf, int n, int min, int time, int timeout) {
   shared_lock desInfoLk(mapMutex);
   auto desInfoP{get_or(desInfoMap, des, nullptr)}; // make a local shared pointer
   if (desInfoP)
	    return desInfoP->reading(des, buf, n, min, time, timeout, desInfoLk);
    return wcsReadcond(des, buf, n, min, time, timeout);
}

/*
 * Function:	Reading directly from a file or from a socketpair descriptor)
 * Return:		the number of bytes read , or -1 for an error
 */
ssize_t myRead(int des, void* buf, size_t nbyte) {
   shared_lock desInfoLk(mapMutex);
   auto desInfoP{get_or(desInfoMap, des, nullptr)}; // make a local shared pointer
	if (desInfoP)
	    // myRead (for sockets) usually reads a minimum of 1 byte
	    return desInfoP->reading(des, buf, nbyte, 1, 0, 0, desInfoLk);
	return read(des, buf, nbyte); // des is closed or not from a socketpair
}

/*
 * Return:		the number of bytes written, or -1 for an error
 */
ssize_t myWrite(int des, const void* buf, size_t nbyte) {
    {
        shared_lock desInfoLk(mapMutex);
        auto desInfoP{get_or(desInfoMap, des, nullptr)};
        if (desInfoP) {
           auto pair{desInfoP->pair};
           if (-2 != pair)
              return desInfoMap[pair]->writing(des, buf, nbyte, desInfoLk);
        }
    }
    return write(des, buf, nbyte); // des is not from a pair of sockets or socket or pair closed
}

/*
 * Function:  make the calling thread wait for a reading thread to drain the data
 */
int myTcdrain(int des) {
    {
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
    }
    return tcdrain(des); // des is not from a pair of sockets or socket closed
}

/*
 * Function:   Create pair of sockets and put them in desInfoMap
 * Return:     return an integer that indicate if it is successful (0) or not (-1)
 */
int mySocketpair(int domain, int type, int protocol, int des[2]) {
   int returnVal{socketpair(domain, type, protocol, des)};
   if(-1 != returnVal) {
      lock_guard desInfoLk(mapMutex);
      desInfoMap[des[0]] = make_shared<socketInfoClass>(des[1]);
      desInfoMap[des[1]] = make_shared<socketInfoClass>(des[0]);
   }
   return returnVal;
}

/*
 * Function:   close des
 *       myClose() should not be called until all other calls using the descriptor have finished.
 */
int myClose(int des) {
   {
        lock_guard desInfoLk(mapMutex);
        auto iter{desInfoMap.find(des)};
        if (iter != desInfoMap.end()) { // if in the map
            auto mySp{iter->second};
            desInfoMap.erase(iter);
            if (mySp) // if shared pointer exists (it should)
                return mySp->closing(des);
        }
   }
   return close(des);
}

/*
 * Function:	Open a file and get its file descriptor.
 * Return:		return value of open
 */
int myOpen(const char *pathname, int flags, ...) //, mode_t mode)
{
   mode_t mode{0};
   // in theory we should check here whether mode is needed.
   va_list arg;
   va_start (arg, flags);
   mode = va_arg (arg, mode_t);
   va_end (arg);
   return open(pathname, flags, mode);
}

/*
 * Function:	Create a new file and get its file descriptor.
 * Return:		return value of creat
 */
int myCreat(const char *pathname, mode_t mode)
{
   return creat(pathname, mode);
}


