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
    unsigned maxTotalCanRead{0}; // Max bytes that can be read from the socket

    condition_variable cvDrain; // Condition variable for managing myTcdrain (waits until data is fully read).
    condition_variable cvRead; // Condition variable for notifying when data is available to be read. Used by myreadcond()

#ifdef CIRCBUF
    CircBuf<char> circBuffer; // Circular buffer for data storage if CIRCBUF is defined.
#endif

    mutex socketInfoMutex; // Mutex to ensure thread-safe access to this socket's state (like totalWritten).

public:
    int pair; // Descriptor of paired socket; set to -1 if this socket descriptor is closed, -2 if paired socket(descriptor) is closed.
//pair allows mywrite and mytcdrain to reference a paired socket, ensuring operations on one socket are synchronized with its pair
//If pair == -2, it means the paired socket has been closed.
//If pair >= 0, it’s a valid socket descriptor representing the other end of the socket pair.

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
        // If no minimum data is required and no data is available, return 0 bytes read.
        if (0 == min && 0 == totalWritten)
            bytesRead = 0; 
        else {
#ifdef CIRCBUF
            bytesRead = circBuffer.read((char *) buf, n); // Read from circular buffer if CIRCBUF is defined.
#else
            bytesRead = read(des, buf, n); // Directly read data from the socket if circbuf is not defined
#endif
            //if data is read, update totalwritten and notify cvDrain if conditions are met
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
        
        //The cvRead.wait(socketLk, condition) function suspends the thread and releases socketLk, 
        //allowing other threads to access socketInfoMutex.
        //Condition: The thread resumes once totalWritten >= min 
        //(enough data is available to read) or pair < 0 (the paired socket is closed).
        cvRead.wait(socketLk, [this, min] {
            return totalWritten >= (unsigned) min || pair < 0;
        });
        errno = errnoHold;

        //reading after wait
#ifdef CIRCBUF
        bytesRead = circBuffer.read((char *) buf, n); // Read data from circular buffer.
        totalWritten -= bytesRead;
#else
        bytesRead = read(des, buf, n); // Directly read data from the socket.
        
        // then handle connection reset by peer error.
        //ECCONNRESET happens when one end of a network socket is closed while the other end is still trying to communicate
        if (-1 != bytesRead)
            totalWritten -= bytesRead;
        else if (ECONNRESET == errno)
            bytesRead = 0;
#endif
        //notify waiting writers after read
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


// closing: Safely closes a socket rep by des and synchronizes with the paired socket to avoid race conditions.
//          Notifies any threads waiting on reads or writes to let them know the socket is closed.

int closing(int des)
{
    // mapMutex is already locked when calling this function, so no other myClose (or mySocketpair) can proceed.

    // Check if the paired socket has already been closed.
    if (pair != -2) { // Only proceed if the paired socket is still open. Since no need to close if its already closed 

        // Get the socketInfoClass instance of the paired socket from desInfoMap.
        //Retrieves the shared_ptr for the paired socket’s socketInfoClass from desInfoMap
        socketInfoClassSp des_pair{desInfoMap[pair]};

        // Lock both this socket’s mutex and the paired socket’s mutex to avoid race conditions.
        //Uses scoped_lock to lock both socketInfoMutex (for the current socket) and des_pair->socketInfoMutex (for the paired socket).
        scoped_lock guard(socketInfoMutex, des_pair->socketInfoMutex);

        pair = -1;            // Mark this socket as the first of the pair to close.
        des_pair->pair = -2;  // Mark the paired socket as the second to close.

        //Checks if totalWritten > maxTotalCanRead, meaning there’s data that hasn’t yet been read from this socket.
        if (totalWritten > maxTotalCanRead) {
            // Notify any threads waiting on myTcdrain that the socket is closing,
            // since this will discard any unread buffered data.
            cvDrain.notify_all();
        }

        //Checks if des_pair->maxTotalCanRead > 0, which would mean there’s a thread potentially waiting on a read from the paired socket.
        if (des_pair->maxTotalCanRead > 0) {
            // Notify any thread waiting on reading from the paired socket.
            // This signals that no more data will be written since this socket is closed and this is because: 
            //The reader thread that’s waiting on cvRead will wake up and check the socket state (such as pair < 0).
            //By examining pair, the reader can determine if the socket has been closed.
            des_pair->cvRead.notify_one();
        }
    }

    // Use the system's close function to close the socket descriptor.
    return close(des);
} // .closing()


// myReadcond: Attempts to read data from a socket with specific conditions,
//             such as requiring a minimum number of bytes before returning.
//            Uses mapMutex and desInfoMap to locate socket info and calls the reading method if socket info is available 
int myReadcond(int des, void * buf, int n, int min, int time, int timeout) {
//des: The socket descriptor to read from.
//buf: A pointer to the buffer where the data will be stored.
//n: The number of bytes to read.
//min: The minimum number of bytes required to complete the read operation.
//time and timeout: Timing parameters for the read operation.
    
    // Acquire a shared lock on mapMutex to safely access desInfoMap
    //It is not unlocked as it will release the lock when it goes out of scope 
    shared_lock desInfoLk(mapMutex);

    // Calls get_or to retrieve the shared_ptr to the socketInfoClass associated with the socket descriptor des from desInfoMap.
    // If des is not found in desInfoMap, returns nullptr.
    auto desInfoP{get_or(desInfoMap, des, nullptr)}; // make a local shared pointer

    // If socket information is found in desInfoMap (desInfoP is valid),
    // call the reading method to perform a conditional read and pass the parameters as seen in the brackets.
    if (desInfoP)
        return desInfoP->reading(des, buf, n, min, time, timeout, desInfoLk);

    // If desInfoP is nullptr (socket not found in desInfoMap),
    // fall back to wcsReadcond to handle conditional reading.
    return wcsReadcond(des, buf, n, min, time, timeout);

}

// myRead(wrapper around standard read): Attempts to read data from a socket, ensuring a minimum of 1 byte(minimal conditional read) is read if the socket
//         is managed by mySocketpair. Uses desInfoMap to manage socket state if available.
//uses desInfoMap to track socket state if mySocketpair created the socket
//attempts to read nbyte bytes from a socket represented by des into buf
ssize_t myRead(int des, void* buf, size_t nbyte) {
   
    // Acquire a shared lock on mapMutex to safely access desInfoMap for thread-safe reading.
    shared_lock desInfoLk(mapMutex);

    // Retrieve the shared pointer to the socket's state information from desInfoMap.
    // If the socket descriptor is not in desInfoMap, returns nullptr.
    auto desInfoP{get_or(desInfoMap, des, nullptr)};

    //If socket information is found in desInfoMap (desInfoP is valid), call reading on the socket's state.
    // Passes a minimum read condition of 1 byte to simulate typical socket behavior.
    //myRead sets min to 1, ensuring at least 1 byte is read without specifying additional conditions.
    //myReadcond allows a customizable min, letting the caller specify a different minimum byte requirement.
    //myRead uses 0 for both time and timeout, meaning it won’t wait for additional time beyond data availability.
    //myReadcond accepts custom values for time and timeout, allowing the caller to control how long the function will wait if data isn’t immediately available.
    if (desInfoP)
        return desInfoP->reading(des, buf, nbyte, 1, 0, 0, desInfoLk);

    // If desInfoP is nullptr (socket not found in desInfoMap), fall back to standard system read.
    return read(des, buf, nbyte);
}

// myWrite: Attempts to write data to a socket, checking if it is part of a socket pair
//using desInfoMap to check the socket’s state and synchronize with the paired socket if it exists.
//It defaults to a standard write if the socket is not managed by mySocketpair
//or if the paired socket is closed.
//attempts to write nbyte bytes from buf to the socket descriptor des.
ssize_t myWrite(int des, const void* buf, size_t nbyte) {
    {
        // Acquire a shared lock on mapMutex to safely access desInfoMap.
        shared_lock desInfoLk(mapMutex);

        // Call get-or to retrieve the shared pointer to the socket's state from desInfoMap.
        // If the socket descriptor is not in desInfoMap, desInfoP will be nullptr.
        auto desInfoP{get_or(desInfoMap, des, nullptr)};
        
        // If socket information is found in desInfoMap (desInfoP is valid),
        if (desInfoP) {
            // Retrieve the paired socket descriptor's value from the socket's state information.
            auto pair{desInfoP->pair};

            // If the paired socket is still open (pair is not -2), attempt a synchronized write
            // using the writing function of the paired socket.
            if (-2 != pair)
                return desInfoMap[pair]->writing(des, buf, nbyte, desInfoLk);
        }
    }
    
    // If the socket descriptor is not found in desInfoMap or the paired socket is closed,
    // fall back to the standard system write function for direct writing.
    return write(des, buf, nbyte);
}

// myTcdrain: Ensures that all written data has been fully transmitted from the buffer associated with
//            the socket descriptor `des`. If `des` is part of a socket pair, it calls draining function 
//            on the paired socket to synch the drain operation
// It falls back to standard system tcdrain function if the socket isnt in a pair. 
int myTcdrain(int des) {
    {
        // Acquire a shared lock on mapMutex to safely access desInfoMap.
        shared_lock desInfoLk(mapMutex);

        // Call get-or to retrieve the shared pointer to the socket's state information from desInfoMap.
        // If the socket descriptor is not in desInfoMap, desInfoP will be nullptr.
        auto desInfoP{get_or(desInfoMap, des, nullptr)};

        // If socket information is found in desInfoMap (desInfoP is valid),
        if (desInfoP) {
            // Retrieve the paired socket descriptor from the socket's state information.
            auto pair{desInfoP->pair};

            // If the paired descriptor is closed (pair is -2), no draining is needed, so return 0.
            if (-2 == pair)
                return 0;
            else {
                // If the paired socket is still open, create a shared pointer to its state information
                //ie. retrieve the state information of a socket, which is stored in a shared_ptr<socketInfoClass> in desInfoMap
                // and call draining to synchronize the drain operation on this paired socket.
                auto desPairInfoSp{desInfoMap[pair]};
                return desPairInfoSp->draining(desInfoLk);
            }
        }
    }
    
    // If the socket descriptor is not found in desInfoMap or not part of a pair, fall back to standard tcdrain.
    return tcdrain(des);
}

// mySocketpair: Creates a pair of connected sockets and stores their state information
//               in desInfoMap for custom management and synchronization.
//               This function is a wrapper around the standard socketpair call.
int mySocketpair(int domain, int type, int protocol, int des[2]) {
    
    // Call the standard system socketpair function to create a pair of connected sockets.
    // If socketpair succeeds, it returns 0 and fills des[0] and des[1] with the descriptors of the new sockets.
    //This connection enables read/write operations to flow between des[0] and des[1]
    //If it fails, it returns -1.
    int returnVal{socketpair(domain, type, protocol, des)};
    
    // If socketpair was successful (returnVal is not -1), proceed to store the socket states.
    if (-1 != returnVal) {
        
        // Lock mapMutex to ensure exclusive access to desInfoMap while modifying it.
        lock_guard desInfoLk(mapMutex);
        
       
        //Then create a shared_ptr<socketInfoClass> for des[0], initialized with des[1] as its paired socket descriptor since:
        //make_shared creates a shared_ptr to a new socketInfoClass object and;
        //socketInfoClass(des[1]) constructs the new socketInfoClass instance with des[1] as the pair, establishing that des[0] is paired with des[1].
        //LHS stores this shared_ptr in desInfoMap under the key des[0], allowing access to the state of socket des[0] through desInfoMap
        desInfoMap[des[0]] = make_shared<socketInfoClass>(des[1]);
        
        // Now do the same and store state information for socket des[1], with des[0] as its paired socket descriptor.
        desInfoMap[des[1]] = make_shared<socketInfoClass>(des[0]);
    }
    
    // Return the result of socketpair (0 if successful, -1 if there was an error).
    return returnVal;
}


// myClose: Closes a socket descriptor `des`. If `des` is managed by mySocketpair (exists in desInfoMap),
//          it removes its state information from desInfoMap and synchronizes closure with any paired socket.
//          If `des` is not in desInfoMap, it falls back to the standard close function.
int myClose(int des) {
    {
        // Acquire an exclusive lock on mapMutex to ensure safe access to desInfoMap while modifying it.
        lock_guard desInfoLk(mapMutex);

        // Attempt to find the socket descriptor `des` in desInfoMap using find method.
        // If des is in the map, iter will be an iterator pointing to its entry.
        auto iter{desInfoMap.find(des)};
        
        //Checks if iter is not equal to desInfoMap.end(), which would mean that des was found in desInfoMap
        if (iter != desInfoMap.end()) {
            // Retrieve the shared_ptr<socketInfoClass> associated with `des` in the map, stored in mySp
            auto mySp{iter->second};
            
            // Remove `des` from desInfoMap, effectively deleting its state information entry.
            desInfoMap.erase(iter);
            
            // If the shared pointer is valid(mySp), call the `closing` function in socketInfoClass
            // to handle synchronized closing with any paired socket esuring any paired soket is notified.
            if (mySp)
                return mySp->closing(des);
        }
    }
    
    // If `des` is not in desInfoMap, fall back to the standard system close function to close `des`.
    return close(des);
}

// myOpen: Wrapper around the standard open function, allowing optional handling of the mode argument.
//         This function enables flexibility by conditionally using mode when required by flags.
//pathname: The path to the file to open.
//flags: Flags that specify how the file should be opened (e.g., read-only, read-write).
//...: An ellipsis (...) indicates that this function can accept additional, optional arguments, typically the file mode.
int myOpen(const char *pathname, int flags, ...) //, mode_t mode)
{
    //Initializes a mode_t variable named mode to 0 so it has a default value.
    //mode will eventually hold the permissions with which to open the file if needed.
    mode_t mode{0}; 
    
    // In theory, we should check here whether `mode` is required based on `flags`.
    
    va_list arg; // Declares a va_list variable named arg, which will manage the optional arguments passed after flags. va_list is a type in C/C++ for handling variable arguments in functions with ....
    va_start(arg, flags); //Calls va_start, a macro that initializes arg for use with the additional arguments. va_start(arg, flags): Initializes arg to retrieve arguments after flags in the function call.
    
   //Calls va_arg to retrieve the next argument in arg as a mode_t value, storing it in mode.
   //va_arg(arg, mode_t): Retrieves the next argument in arg, expecting it to be of type mode_t.
    mode = va_arg(arg, mode_t); 
    
    //Calls va_end to clean up arg, completing the use of the variable arguments.
    //va_end(arg): This is required to avoid potential memory leaks or undefined behavior by signaling that arg is no longer needed.
    va_end(arg); 

    // Call the standard open function with pathname, flags, and mode.
    // final operation that opens the file. 
    // By calling open, myOpen behaves just like the system open function but with additional flexibility in handling optional arguments.
    return open(pathname, flags, mode);
}

// myCreat: Wrapper around the standard creat function. This function opens a file for writing,
//          truncating it to zero length if it exists, or creating it with the specified mode if it doesn’t.
//          It mirrors the behavior of creat without adding additional functionality.
int myCreat(const char *pathname, mode_t mode)
{
    // Call the standard creat function:
    // - If the file exists: Truncates it to zero length and opens it for writing.
    // - If the file doesn’t exist: Creates it with the specified mode.
    return creat(pathname, mode);
}
