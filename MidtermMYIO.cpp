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

// Template function to retrieve a value for a key in a map or return a default if the key is not found.
// Useful for safely accessing socket descriptor information in desInfoMap.
template <typename Key, typename Value, typename T>
Value get_or(std::map<Key, Value>& m, const Key& key, T&& default_value)
{
    // Find the key in the map. If found, "it" points to the key-value pair; otherwise, "it" is m.end().
    auto it{m.find(key)};
    
    // Check if key is absent (it == m.end()). If absent, return the provided default value.
    if (m.end() == it) {
        return default_value;
    }
    
    // If key is found, return the corresponding value.
    return it->second;
}
// socketInfoClass: Stores information and synchronization controls for each socket in a socket pair. Below is the declaration
class socketInfoClass;

typedef shared_ptr<socketInfoClass> socketInfoClassSp; // Defines an alias(ie a shorthand) for a shared pointer to socketInfoClass.

map<int, socketInfoClassSp> desInfoMap; // Defines a map that maps each socket descriptor (int key) to a shared_ptr of socketInfoClass, holding its state.

// A shared mutex to protect desInfoMap, ensuring that only one thread can modify it at a time.
// Allows multiple threads to hold shared (read) locks or one exclusive (write) lock for thread safety.
shared_mutex mapMutex;

// socketInfoClass holds state information and controls synchronization for a socket descriptor.
class socketInfoClass {
    unsigned totalWritten{0}; // Total bytes written to the socket (used for tracking data in myWrite).
    unsigned maxTotalCanRead{0}; // Max bytes that can be read from the socket (used in myReadcond).

    condition_variable cvDrain; // Condition variable for managing myTcdrain (waits until data is fully read).
    condition_variable cvRead; // Condition variable for notifying when data is available to be read. Used by myreadcond()

#ifdef CIRCBUF
    CircBuf<char> circBuffer; // Circular buffer for data storage if CIRCBUF is defined.
#endif

    mutex socketInfoMutex; // Mutex to ensure thread-safe access to this socket's state (like totalWritten).

public:
    int pair; // Descriptor of paired socket; set to -1 if this socket descriptor is closed, -2 if paired socket(descriptor) is closed.
//pair allows mywrite and mytcdrain to reference a paired socket, ensuring operations on one socket are synchronized with its pair

    // Constructor initializes the pair variable with the descriptor of the paired socket.Ensures each instance of Sockeinfoclass has a reference to its paired socket
    socketInfoClass(unsigned pairInit)
    : pair(pairInit) 
    {
#ifdef CIRCBUF
        circBuffer.reserve(1100); // Allocates 1100 bytes in the circular buffer if CIRCBUF is defined.
#endif
    }
};

// draining: Waits until all data written to the socket has been read (used by myTcdrain to ensure synch btw writer and reader).
int draining(shared_lock<shared_mutex> &desInfoLk)
{
    // Acquire a unique lock on socketInfoMutex for exclusive access to this socket's state variables totalWritten and maxTotalCanRead.
    //Since it accesses state vars and we need exclusive access to avoid conflicts with other threads 
    unique_lock socketLk(socketInfoMutex);
    
    // Release the shared lock on mapMutex, allowing other threads to access desInfoMap.
    desInfoLk.unlock();

    // Check if the paired socket is open (pair >= 0) and if there is unread data (totalWritten > maxTotalCanRead).
    // If both conditions are true, wait on cvDrain until notified by a reader.
    if (pair >= 0 && totalWritten > maxTotalCanRead)
        cvDrain.wait(socketLk); // Waits until data is drained. Linux pthreads handle spurious wakeups.

    // Return 0 to indicate successful completion of the drain operation.
    return 0;
}

// writing: Writes data to the socket and updates the total amount of data written(totalWritten).
//          Also notifies readers that new data is available.
// writes nbyte bytes from buf to the socket represented by descriptor buf
int writing(int des, const void* buf, size_t nbyte, shared_lock<shared_mutex> &desInfoLk)
{
    // Lock the socketInfoMutex for exclusive access to this socket's state.
    lock_guard socketLk(socketInfoMutex);
    
    // Release the shared lock on mapMutex, allowing other threads to access desInfoMap.
    desInfoLk.unlock();

    // Writing data to the socket: Conditionally compiled code(ifdef): writes to a circular buffer if CIRCBUF is defined,
    // otherwise, writes directly to the socket.
#ifdef CIRCBUF
    int written = circBuffer.write((const char*) buf, nbyte); // Write to circular buffer.
#else
    int written = write(des, buf, nbyte); // Write data to socket directly.
#endif

    // If data was successfully written (written > 0), update totalWritten and notify readers.
    if (written > 0) {
        totalWritten += written;     // Update totalWritten to reflect the new data sent.
        cvRead.notify_one();         // Notify one waiting reader that data is available.
    }

    // Return the number of bytes written to indicate success or failure to the caller.
    return written;
}


// reading: Reads data from a socket and manages synchronization with the writer.
// handles cases like connection state, timeouts and synch with writing and draining 
//          Waits for sufficient data if necessary and updates totalWritten to track data flow.

//reads n bytes from the socket identified by des into buf
//min: Minimum number of bytes required before returning.
//time and timeout: Conditions for blocking behavior, but only immediate timeout (0) is supported.
//desInfoLk: A reference to a shared lock on mapMutex, which ensures safe access to desInfoMap
int reading(int des, void * buf, int n, int min, int time, int timeout, shared_lock<shared_mutex> &desInfoLk)
{
    int bytesRead; // Holds the number of bytes read.
    
    // Acquire a unique lock on socketInfoMutex for exclusive access to socket state.
    unique_lock socketLk(socketInfoMutex);
    
    // Release the shared lock on mapMutex, allowing other threads to access desInfoMap.
    desInfoLk.unlock();

    // If the paired socket is closed, return 0 bytes read to avoid connection reset errors.
    if (-2 == pair)
        bytesRead = 0;

    //reading data if available (totalWritten >= min) ie totalWritten has met minimum read requirement to ensure data availability before reading 
    //also ensures synchronization between writer and reader using cvDrain  
    // Check if sufficient data is available to read without waiting.
    else if (!maxTotalCanRead && totalWritten >= (unsigned) min) {
        if (0 == min && 0 == totalWritten)
            bytesRead = 0; // If no minimum data is required and no data is available, return 0 bytes read.
        else {
#ifdef CIRCBUF
            bytesRead = circBuffer.read((char *) buf, n); // Read from circular buffer if CIRCBUF is defined.
#else
            bytesRead = read(des, buf, n); // Directly read data from the socket if circbuf is not defined
#endif
            //if data is read, update totalwritten and notify cvDrain if conditios are met
            if (bytesRead > 0) {
                totalWritten -= bytesRead; // Update totalWritten after reading.
                // Notify all waiting writers if enough data has been drained.
                if (totalWritten <= maxTotalCanRead) {
                    int errnoHold{errno};
                    cvDrain.notify_all();
                    errno = errnoHold;
                }
            }
        }
    }
        
    // If not enough data is available (totalWritten < min) , adjust maxTotalCanRead and wait for more data.
    else {
        maxTotalCanRead += n; // Allow n more bytes to be read in the next read operation.
        int errnoHold{errno}; // Hold errno to restore after wait.
        cvDrain.notify_all(); // Notify writers that reader is ready to read more data.

        // Verifies time and timeout are zero; otherwise, exits, as only immediate timeout is supported.
        if (0 != time || 0 != timeout) {
            COUT << "Currently only supporting no timeouts or immediate timeout" << endl;
            exit(EXIT_FAILURE);
        }

        // Wait until sufficient data is available or paired socket is closed.
        cvRead.wait(socketLk, [this, min] {
            return totalWritten >= (unsigned) min || pair < 0;
        });
        errno = errnoHold;

#ifdef CIRCBUF
        bytesRead = circBuffer.read((char *) buf, n); // Read data from circular buffer.
        totalWritten -= bytesRead;
#else
        bytesRead = read(des, buf, n); // Directly read data from the socket.
        // Handle connection reset by peer error.
        if (-1 != bytesRead)
            totalWritten -= bytesRead;
        else if (ECONNRESET == errno)
            bytesRead = 0;
#endif

        // Adjust maxTotalCanRead back and notify if more data is still available.
        maxTotalCanRead -= n;
        if (0 < totalWritten || -2 == pair) {
            int errnoHold{errno}; 
            cvRead.notify_one(); // Notify a waiting reader.
            errno = errnoHold;
        }
    }
    return bytesRead; // Return the number of bytes read.
} // .reading()

