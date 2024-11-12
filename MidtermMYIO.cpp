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

