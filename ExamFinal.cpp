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

// Lock-free circular buffer template class for thread-safe single-producer, single-consumer scenarios. 
//Circular buffers efficiently implement queues where elements are written and read in a fixed-size buffer, especially for producer-consumer scenarios.
template<class T> // Declares CircBuf as a template class that allows the buffer to work with any data type (e.g., int, float, custom objects).
class CircBuf {
   /* The buffer uses `read_pos` and `write_pos` to track read and write operations.
    *
    * If `read_pos == write_pos`, the buffer is considered empty.
    * There is always one unused position in the buffer to distinguish between
    * "full" and "empty" states. This ensures no ambiguity when the buffer is completely full.
    *
    * Invariants:
    * - `read_pos < size`
    * - `write_pos < size`
    * The pointers read_pos and write_pos are always less than the buffer’s size to avoid out-of-bounds access.
    */

   static const unsigned size = 8 + 1; // Total buffer capacity is 8 elements + 1 unused slot.

   T buf[size]; // Array to store buffer elements of type `T`.
   std::atomic<unsigned> read_pos = 0, write_pos = 0; 
   // Atomic variables for thread-safe tracking of read and write positions.
   //std::atomic<unsigned> read_pos: Tracks(Points to) the position of the next element to be read.
   //std::atomic<unsigned> write_pos: Tracks the position of the next slot to write data.
   //Both read_pos and write_pos are initialized to 0
   //Atomicity: Ensures thread-safe updates to read_pos and write_pos without locks.

// Atomic variables `read_pos` and `write_pos` are used for lock-free synchronization.
// CircBuf class is lock-free, meaning it doesn’t use traditional mutexes or locks to synchronize access.
// Since one thread writes to the buffer while another reads, atomicity ensures both threads can safely operate without overwriting or missing updates
//
// These variables ensure thread-safe concurrent access by:
// 1. Guaranteeing atomic updates to avoid race conditions so when a thread increments read_pos or write_pos, no other thread can interrupt or see an intermediate state..
// 2. Supporting one producer (writer) and one consumer (reader) operating simultaneously.
//
// Memory orderings used:
// - `memory_order_relaxed`: Ensures efficient updates with minimal synchronization.
// - `memory_order_acquire`: Prevents reordering of reads after the load operation
// - `memory_order_release`: Prevents reordering of writes before the store operation.
//
// Key Concept:
// - The buffer is never completely full. At least one slot remains empty to distinguish between
//   "empty" (read_pos == write_pos) and "full" states.
//
// Example Operation:
// - Writer adds data at `write_pos`, then advances it atomically.
// - Reader consumes data from `read_pos`, then advances it atomically.

public:

/* 
 * Writes up to `buffer_size` elements from the provided `buffer` into the circular buffer.
 * Advances the `write_pos` pointer after writing.
 *
 * Key Details:
 * - Handles buffer wrap-around when the data crosses the end of the buffer.
 * - Ensures the buffer does not overwrite unread data by leaving at least one slot empty.
 * - Thread-safe using atomic operations for `read_pos` and `write_pos`.
 *
 * Returns:
 * - The number of elements successfully written to the buffer.
 */
unsigned write(const T *buffer, unsigned buffer_size) {
    T *p[2];               
   // This defines an array of two pointers (p[0] and p[1]). Each pointer represents a region of the buffer where data can be written.
   //First Segment (p[0]): From write_pos to the end of the buffer.
   //Second Segment (p[1]): From the start of the buffer to just before read_pos.
   
    unsigned sizes[2];     
   // Sizes of the writable segments.
   //Depending on the positions of write_pos and read_pos, the writable space is given by:
   //sizes[0] : the free space after write_pos to the end of the buffer.
   //sizes[1] : the free space from the start of the buffer to just before read_pos. (wrapped around block)
   //If the buffer looks like "DDeeeeeeeeDD", there is no wrap-around, and sizes[1] = 0.

   // Set start of first segment.
   //retrieves the current value of the atomic variable write_pos in a thread-safe manner.
   //memory_order_relaxed allows the operation to avoid unnecessary synchronization overhead. 
   //This is sufficient here because the value of write_pos is only used locally and doesn't depend on updates from other threads.
   //buf is a pointer to the beginning of the circular buffer array.
   //Adding write_pos to buf advances the pointer to the position in the buffer where the next write operation should occur.
    p[0] = buf + write_pos.load(memory_order_relaxed);

    // Load the read position to determine available space, ensuring visibility across threads.
   //memory_order_acquire ensures that any memory operations (writes to the buffer) performed by other threads before updating read_pos are visible to this thread after the load operation.
   //Prevents reordering of subsequent reads with this load operation, ensuring the thread sees a consistent state of memory.
    const unsigned rpos = read_pos.load(memory_order_acquire);

    // Determine writable space in the buffer.
    if (rpos <= write_pos.load(memory_order_relaxed)) {
        // Case 1: Buffer wraps around ("eeeeDDDDeeee"). (e = empty, D = data)
        //  Index:  0   1   2   3   4   5   6   7   8   9
       //Content:   p1  p1  D   D   D   D   p0  p0  p0  p0
          //              ^read_pos             ^write_pos

        p[1] = buf;  // Second segment starts at the beginning of the buffer.
        if (rpos) { //Checks if rpos != 0
            sizes[0] = size - write_pos.load(memory_order_relaxed); // Space until end of buffer.
            sizes[1] = rpos - 1;  // Space before read position. Reserve one slot.
        } else { //Checks if rpos == 0, means the read_pos is at the start of the buffer (index 0), and there is no second writable segment before read_pos.
            sizes[0] = size - write_pos.load(memory_order_relaxed) - 1; // Reserve one slot.
            sizes[1] = 0; // No second segment needed.
        }
    } else {
        // Case 2: Buffer does not wrap around ("DDeeeeeeeDD").
      //Index:      0   1   2   3   4   5   6   7   8   9
       //Content:   D   D   p0  p0  p0  p0  p0  p0  D   D
           //          ^write_pos                 ^read_pos

        p[1] = nullptr; // No second segment used.
        sizes[0] = rpos - write_pos.load(memory_order_relaxed) - 1; // Space before read position. Reserve one slot.
        sizes[1] = 0;
    }

    // Ensure we only write as much as the buffer can fit.
   //buffer_size: The number of elements the user wants to write.
   // By taking the minimum of buffer_size and sizes[0] + sizes[1], the function ensures:
   //If there is enough space in the buffer, all buffer_size elements will be written.
   //If the buffer is nearly full, only as much as possible (sizes[0] + sizes[1]) will be written.
    buffer_size = min(buffer_size, sizes[0] + sizes[1]);

    // Write data into the first segment.
   //from_first: The number of elements to write to the first segment and is min of the two
   //memcpy Copies from_first elements from the provided buffer into the first segment (p[0])
    const int from_first = min(buffer_size, sizes[0]);
    memcpy(p[0], buffer, from_first * sizeof(T));

    // If needed, write remaining data into the second segment.
   //the if condition checks whether the data to be written (buffer_size) is larger than the space available in the first segment (sizes[0]).
   /*
   --p[1]: Pointer to the second writable segment, typically at the start of the buffer (index 0).
   --buffer + from_first implements the below actions:
   Skips over the portion of the input buffer already written to p[0].
   from_first represents the number of elements written to p[0] (min(buffer_size, sizes[0])).
   --(buffer_size - sizes[0]) implements the below actions:
   Represents the number of elements remaining to be written after filling the first segment.
   --sizeof(T): Multiplies the number of remaining elements by the size of each element to calculate the total number of bytes to copy. 
   */
    if (buffer_size > sizes[0]) {
        memcpy(p[1], buffer + from_first, (buffer_size - sizes[0]) * sizeof(T));
    }

    // Update the write pointer (write_pos) in the circular buffer after successfully writing data, wrapping around if necessary.
   // % size ensures that write_pos wraps around to the beginning of the buffer when it reaches the end.
   //The circular buffer is implemented as a fixed-size array (buf[size]), so this modulo operation prevents out-of-bounds access.
    write_pos.store((write_pos.load(memory_order_relaxed) + buffer_size) % size, memory_order_release);

    // Return the number of elements written.
    return buffer_size;
}

/* 
 * Reads up to `buffer_size` elements from the circular buffer into the provided `buffer`.
 * Advances the read pointer (`read_pos`) to reflect the consumed data.
 * 
 * Key Features:
 * 1. Determines the available data based on `write_pos` and `read_pos`.
 * 2. Handles both contiguous and wrap-around scenarios:
 *    - Contiguous Data: Data lies in a single segment (`p[0]`).
 *    - Wrap-Around: Data spans two segments (`p[0]` and `p[1]`).
 * 3. Ensures thread-safe access with atomic operations.
 * 
 * Returns:
 * - The number of elements successfully read from the buffer.
 */
unsigned read(T *buffer, unsigned buffer_size) {
    T *p[2];         // Pointers to data segments where data will be read from.
    unsigned sizes[2]; // Sizes of the data segments corresponding to `p[0]` and `p[1]`.

    // Initialize the first segment, starting at the current `read_pos`.
    p[0] = buf + read_pos.load(memory_order_relaxed);

    // Load the current `write_pos` to calculate available data for reading.
    const unsigned wpos = write_pos.load(memory_order_acquire);

    // Determine the layout of data in the buffer.
    if (read_pos.load(memory_order_relaxed) <= wpos) {
        // Case 1: No Wrap-Around
        // Data is contiguous from `read_pos` to `wpos`.
        // Index:    0   1   2   3   4   5   6   7   8   9
        //Content:   e   e   D   D   D   D   e   e   e   e
      //                   ^read_pos             ^write_pos
        p[1] = nullptr;             // No second segment.
        sizes[0] = wpos - read_pos.load(memory_order_relaxed); // Size of the first segment.
        sizes[1] = 0;               // No data in the second segment.
    } else {
        // Case 2: Wrap-Around
        // Data wraps around the end of the buffer.
        // Index:   0   1   2   3   4   5   6   7   8   9
       //Content:   D   D   e   e   e   e   e   e   D   D
        //            ^write_pos                 ^read_pos
        p[1] = buf;                 // Second segment starts at the beginning of the buffer.
        sizes[0] = size - read_pos.load(memory_order_relaxed); // Data from `read_pos` to end of buffer.
        sizes[1] = wpos;            // Data from start of buffer to `wpos`.
    }

    // Limit the amount of data to read to the total available data.
    buffer_size = min(buffer_size, sizes[0] + sizes[1]);

    // Step 1: Read from the first segment.
    const int from_first = min(buffer_size, sizes[0]); // Number of elements to read from `p[0]`.
    memcpy(buffer, p[0], from_first * sizeof(T));       // Copy data to the provided buffer.

    // Step 2: Read from the second segment, if needed.
    if (buffer_size > sizes[0]) {
        memcpy(buffer + from_first, p[1], (buffer_size - sizes[0]) * sizeof(T)); // Copy remaining data.
    }

    // Update the `read_pos` pointer after reading.
    // Wraps around using `% size` to stay within the circular buffer bounds.
    read_pos.store((read_pos.load(memory_order_relaxed) + buffer_size) % size, memory_order_release);

    // Return the actual number of elements read.
    return buffer_size;
}
};

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

namespace{ //Unnamed (anonymous) namespace

// socketInfoClass: Stores information and synchronization controls for each socket in a socket pair. Below is the declaration
class socketInfoClass;

typedef shared_ptr<socketInfoClass> socketInfoClassSp; // Defines an alias(ie a shorthand) for a shared pointer to socketInfoClass.

map<int, socketInfoClassSp> desInfoMap; // Defines a map that maps each socket descriptor (int key) to a shared_ptr of socketInfoClass, holding its state.

// A shared mutex to protect desInfoMap, ensuring that only one thread can modify it at a time.
// Allows multiple threads to hold shared (read) locks or one exclusive (write) lock for thread safety.
    //   Protects desInfoMap so only a single thread can modify the map
    //  at a time.  This also means that only one call to functions like mySocketpair() or
    //  myClose() can make progress at a time.  This shared mutex is also used to prevent a
    //  paired socket from being closed at the beginning of a myWrite or myTcdrain function.
shared_mutex mapMutex;

// socketInfoClass holds state information and controls synchronization for a socket descriptor.
class socketInfoClass {
    int totalWritten{0}; // Total bytes written to the socket (used for tracking data in myWrite). 
   //In reading it tracks the total amount of data currently available in the buffer (data written but not yet read).
    int maxTotalCanRead{0}; // Max bytes that can be read from the socket

    condition_variable cvDrain; // Condition variable for managing myTcdrain (waits until data is fully read).
    condition_variable cvRead; // Condition variable for notifying when data is available to be read. Used by myreadcond()

    CircBuf<char> circBuffer; // Circular buffer for data storage.

    mutex socketInfoMutex; // Mutex to ensure thread-safe access to this socket's state (like totalWritten).

public:
    int pair; // Descriptor of paired socket; set to -1 if this socket descriptor is closed, -2 if paired socket(descriptor) is closed.
//pair allows mywrite and mytcdrain to reference a paired socket, ensuring operations on one socket are synchronized with its pair
//If pair == -2, it means the paired socket has been closed.
//If pair >= 0, it’s a valid socket descriptor representing the other end of the socket pair.

    // Constructor initializes the pair variable with the descriptor of the paired socket.Ensures each instance of Sockeinfoclass has a reference to its paired socket
    socketInfoClass(unsigned pairInit)
    : pair(pairInit)  { }

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
    if (0 <= pair && totalWritten > maxTotalCanRead)
        cvDrain.wait(socketLk); // Waits until data is drained. Linux pthreads handle spurious wakeups.
         // spurious wakeup?  Not a problem with Linux p-threads
         // In the Solaris implementation of condition variables,
         //    a spurious wakeup may occur without the condition being assigned
         //    if the process is signaled; the wait system call aborts and
         //    returns EINTR.[2] The Linux p-thread implementation of condition variables
         //    guarantees that it will not do that.[3][4]
         // https://en.wikipedia.org/wiki/Spurious_wakeup
         //  cvDrain.wait(socketLk, [this]{return pair < 0 || totalWritten <= maxTotalCanRead;});

    // Return 0 to indicate successful completion of the drain operation.
    return 0;
}

/ writing: Writes data to the socket and updates the total amount of data written(totalWritten).
//          Also notifies readers that new data is available.
// writes nbyte bytes from buf to the socket represented by descriptor buf
int writing(int des, const void* buf, size_t nbyte, shared_lock<shared_mutex> &desInfoLk)
{
    // Lock the socketInfoMutex for exclusive access to this socket's state.
    lock_guard socketLk(socketInfoMutex);
    
    // Release the shared lock on mapMutex, allowing other threads to access desInfoMap.
    desInfoLk.unlock();

    // Writing data to the socket: writes to a circular buffer 
    int written = circBuffer.write((const char*) buf, nbyte); // Write to circular buffer.

    // If data was successfully written (written > 0), update totalWritten and notify readers.
    if (written > 0) {
        totalWritten += written;     // Update totalWritten to reflect the new data sent.
        cvRead.notify_one();         // Notify one waiting reader that data is available.
    }

    // Return the number of bytes written to indicate success or failure to the caller.
    return written;
}

// reading: Reads data from a socket and manages synchronization, timeouts, and thread safety with the writer.
// handles cases like connection state, timeouts and synch with writing and draining 
//          Waits for sufficient data (`totalWritten >= min`) or socket closure (`pair < 0`) before proceeding.

//reads n bytes from the socket identified by des into buf
//min: Minimum number of bytes required before returning.
//time and timeout: Conditions for blocking behavior, but only immediate timeout (0) is supported.
//desInfoLk: A reference to a shared lock on mapMutex, which ensures safe access to desInfoMap
int reading(int des, void *buf, int n, int min, int time, int timeout, shared_lock<shared_mutex> &desInfoLk) {
    // Acquire a unique lock on socketInfoMutex for exclusive access to socket state.
    unique_lock socketLk(socketInfoMutex);
    
    // Release the shared lock on mapMutex, allowing other threads to access desInfoMap.
    desInfoLk.unlock();

    // Check if there is sufficient data to read or if the paired socket is open.
   //maxTotalCanRead: Tracks the maximum bytes the reader can read in a single operation. If non-zero, reading proceeds.
    if (maxTotalCanRead || (totalWritten < min && -2 != pair)) {
        // Allow up to `n` more bytes to be read in this operation. Signals to writers how much data it can read so they can add more if needed
        maxTotalCanRead += n;

        // Notify all writers waiting on `cvDrain` that the reader is ready for more data.
        cvDrain.notify_all();

        // Handle different timeout configurations.
        if (0 == time && 0 == timeout) {
            // No timeout: Wait until sufficient data is available(totalWritten >= min) or the paired socket is closed(0 > pair).
            cvRead.wait(socketLk, [this, min] {
                return totalWritten >= min || 0 > pair;
            });
        } else if (0 != time && 0 != timeout) {
            // Wait with a timeout: Wait for the specified timeout duration.
           //timeout: Specifies the total time to wait for data.
           //time: Interval between checks for data availability.
          //The reader waits for notifications but exits early if the timeout expires
            if (cv_status::timeout != cvRead.wait_for(socketLk, duration<int, deci>{timeout})) {
                // Periodically check if the required data is available or the socket is closed.
                while (totalWritten < min && 0 <= pair) {
                    if (cv_status::timeout == cvRead.wait_for(socketLk, duration<int, deci>{time})) {
                        break; // Exit loop if timeout occurs.
                    }
                }
            }
        } else {
            // Unsupported timeout configuration: Exit with an error.
            cout << "Currently not supporting this configuration of time and timeout." << endl;
            exit(EXIT_FAILURE);
        }

        // Adjust `maxTotalCanRead` back to its original value after the read operation.
        maxTotalCanRead -= n;
    }

    // Perform the read operation.
    int bytesRead = circBuffer.read((char *)buf, n); // Read data into the buffer.
    totalWritten -= bytesRead; // Decrease `totalWritten` by the number of bytes read.

    // Notify writers if enough data has been drained.
    if (totalWritten <= maxTotalCanRead) {
        int errnoHold{errno}; // Preserve errno during notification.
        cvDrain.notify_all(); // Notify all waiting writers.
        errno = errnoHold;
    }

    // Notify readers if more data is available or the paired socket is closed.
    if (0 < totalWritten || -2 == pair) {
        int errnoHold{errno}; // Preserve errno during notification.
        cvRead.notify_one(); // Notify one waiting reader.
        errno = errnoHold;
    }

    // Return the number of bytes read.
    return bytesRead;
}

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
     return 0;
   } // .closing()
  }; // socketInfoClass
} // unnamed namespace

// myReadcond: Attempts to read data from a socket with specific conditions,
//             such as requiring a minimum number of bytes before returning.
//            Uses mapMutex and desInfoMap to locate socket info and calls the reading method if socket info is available 
int myReadcond(int des, void * buf, int n, int min, int time, int timeout) {
//des: The socket descriptor to read from.
//buf: A pointer to the buffer where the data will be stored.
//n: Represents the size of the user’s buffer (buf) and the upper limit of how much data the function can read.
//Even if more than n bytes are available in the socket buffer, myReadcond will not read more than n bytes.
//min: The minimum number of bytes required to complete the read operation.
//time and timeout: Timing parameters for the read operation.
    
    // Acquire a shared lock on mapMutex to safely access desInfoMap
    //It is not unlocked as it will release the lock when it goes out of scope 
    shared_lock desInfoLk(mapMutex);

    // Calls get_or to retrieve the shared_ptr to the socketInfoClass associated with the socket descriptor des from desInfoMap.
    // If des is not found in desInfoMap, returns nullptr.
    auto desInfoP{get_or(desInfoMap, des, nullptr)}; // make a local shared pointer

    if (!desInfoP) {
      // If desInfoP is nullptr (socket not found in desInfoMap),
      // not an open "socket" [created with mySocketpair()]
      errno = EBADF; return -1;
   }
   // If socket information is found in desInfoMap (desInfoP is valid),
    // call the reading method to perform a conditional read and pass the parameters as seen in the brackets.
 	return desInfoP->reading(des, buf, n, min, time, timeout, desInfoLk);
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
                  else {
                // If the paired socket is closed,
                // Set the global error number (errno) to EPIPE, which stands for "Broken Pipe".
                //This error occurs when the writer attempts to write to a socket or pipe where the paired endpoint has been closed.
                //-1 indicates the failure of the myWrite operation.
                     errno = EPIPE; return -1;
                  }
               }
            //If the socket is not found in desInfoMap (i.e., desInfoP == nullptr).
            //This means the socket descriptor (des) is either invalid or not managed by mySocketpair.
            //Sets errno to EBADF, which stands for "Bad File Descriptor".
               errno = EBADF; return -1;
    }

// myTcdrain: Ensures that all written data has been fully transmitted from the buffer associated with
//            the socket descriptor `des`. If `des` is part of a socket pair, it calls draining function 
//            on the paired socket to synch the drain operation 
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
      //If the socket is not found in desInfoMap (i.e., desInfoP == nullptr).
            //This means the socket descriptor (des) is either invalid or not managed by mySocketpair.
            //Sets errno to EBADF, which stands for "Bad File Descriptor".
   errno = EBADF; return -1;
}

/* mysocketpair:
 * Creates a custom socket pair and initializes their state for bidirectional communication.
 * This function mimics the behavior of the standard `socketpair` system call and is a wrapper around it.
 * 
 * Parameters:
 * - des[2]: An array where the two socket descriptors (endpoints) will be stored.
 * 
 * Key Operations:
 * 1. Assigns fixed descriptors (`des[0] = 3`, `des[1] = 4`) for the socket pair.
 * 2. Associates each descriptor with its pair using `desInfoMap`, a global map for socket management.
 * 3. Initializes the state of each socket (using `socketInfoClass`), which includes:
 *    - Tracking the paired socket.
 *    - Synchronization primitives like mutexes and condition variables for thread-safe communication.
 * 4. Ensures thread safety when accessing `desInfoMap` using a global lock (`mapMutex`).
 * 
 * Returns:
 * - 0 on success, indicating the socket pair was successfully created.
 */
int mySocketpair(int domain, int type, int protocol, int des[2]) {
    // Lock the global map `desInfoMap` to ensure thread-safe access.
    lock_guard desInfoLk(mapMutex);

    // Assign fixed values to the two descriptors of the socket pair to be placeholders for testing.
    des[0] = 3; // Endpoint 1 of the socket pair.
    des[1] = 4; // Endpoint 2 of the socket pair.
   
        //Then create a shared_ptr<socketInfoClass> for des[0], initialized with des[1] as its paired socket descriptor since:
        //make_shared creates a shared_ptr to a new socketInfoClass object and;
        //socketInfoClass(des[1]) constructs the new socketInfoClass instance with des[1] as the pair, establishing that des[0] is paired with des[1].
        //LHS stores this shared_ptr in desInfoMap under the key des[0], allowing access to the state of socket des[0] through desInfoMap
    desInfoMap[des[0]] = make_shared<socketInfoClass>(des[1]);

    //  Now do the same and store state information for socket des[1], with des[0] as its paired socket descriptor.
    desInfoMap[des[1]] = make_shared<socketInfoClass>(des[0]);

    // Return success.
    return 0;
}

/* myClose:
 * Closes a custom-managed socket and removes its state from `desInfoMap`.
 * Mimics the behavior of the standard `close` system call.
 *
 * Parameters:
 * - des: The socket descriptor to close.
 *
 * Key Steps:
 * 1. Ensures thread-safe access to `desInfoMap` using a lock.
 * 2. Checks if the descriptor exists in `desInfoMap`:
 *    - If found: Removes the descriptor and calls the `closing` method.
 *    - If not found: Returns `-1` and sets `errno` to `EBADF`.
 * 
 * Returns:
 * - 0 on successful closure.
 * - -1 if the descriptor is invalid (sets `errno = EBADF`).
 */
int myClose(int des) {
    // Lock the global map `desInfoMap` for thread-safe access.
    lock_guard desInfoLk(mapMutex);

    // Search for the descriptor in `desInfoMap`.
    auto iter{desInfoMap.find(des)};

    // If the descriptor exists in the map:
    if (iter != desInfoMap.end()) {
        // Remove the descriptor from the map.
        desInfoMap.erase(iter);

        // Call the `closing` method for cleanup and return its result.
        return iter->second->closing(des);
    }

    // If the descriptor does not exist, set `errno` and return failure.
    errno = EBADF; // Bad file descriptor.
    return -1;
}

/*
 * Main function to set up real-time scheduling, create a socket pair,
 * and test inter-thread communication using custom sockets.
 */
int main() {
   // Configure CPU affinity to limit execution to CPU 0 to 
    //ensure consistent thread behavior by running all threads on the same CPU,
   //Restricts the execution of the thread to a specific CPU core (core 0).
   //Improves predictability by avoiding thread migrations across cores.
   //Useful in real-time systems where timing and processor locality are critical.
    cpu_set_t cpu_set;                // Define a CPU set.
    CPU_ZERO(&cpu_set);               // Clear the CPU set.
    CPU_SET(myCpu, &cpu_set);         // Specify CPU 0 and add CPU 0 to the set.
   
    //Set the name of the primary thread to Pri for debugging/logging.
    const char* threadName = "Pri";
    pthread_setname_np(pthread_self(), threadName);

   // Apply the CPU affinity to the current thread and its children to ensure they all run at CPU 0. 
    PE(sched_setaffinity(0, sizeof(cpu_set), &cpu_set));
       
    //try block for exception handling   
    try {
        // Set primary thread's real-time scheduling policy and priority (SCHED_FIFO, priority 60).
        sched_param sch; // Structure to store scheduling parameters.
        sch.__sched_priority = 60; 
        pthreadSupport::setSchedParam(SCHED_FIFO, sch);

        // Create a socket pair for inter-thread communication.
        mySocketpair(AF_LOCAL, SOCK_STREAM, 0, daSktPr);

        // Write initial data to the socket.
        myWrite(daSktPr[0], "abc", 3);

        // Create a new thread (`coutThreadFunc`) with lower priority (50).
        pthreadSupport::posixThread coutThread(SCHED_FIFO, 50, coutThreadFunc);

        // call mytcdrain to wait for data transmission to complete on `daSktPr[0]`.
       //Checks the buffer associated with des to ensure no pending data remains to be transmitted.
       //Blocks the calling thread until: All data has been written to the paired socket or underlying system buffer.
       //and the paired socket has acknowledged or synchronized the drain.
        myTcdrain(daSktPr[0]);

        // Write and read data from the socket.
        myWrite(daSktPr[0], "123", 4); //Since size is 4, we have a NULL character
       
       //call myReadCond to read a max of 4 bytes if a min of 4 bytes are avialable in the buffer
        myReadcond(daSktPr[0], B2, 4, 4, 0, 0);

        // Lower the main thread’s priority to 40.
        pthreadSupport::setSchedPrio(40);

        // Close the socket, signaling the end of communication.
        myClose(daSktPr[0]);

        // Wait for `coutThreadFunc` to complete.
        //oin() makes Pri wait for the thread running coutThreadFunc to finish execution before continuing hence, 
        //the main thread blocks at this point, ensuring the other one completes its work before the main program proceeds.
        coutThread.join();
        return 0;
    }
       
      // Then finally catch block to handle system errors, log the error details, and return the error code.
    catch (system_error& error) {
        // Handle system-level exceptions.
        cout << "Error: " << error.code() << " - " << error.what() << '\n';
    }
       // Re-throw any other exceptions for debugging purposes.
    catch (...) { throw; }
}

// Define buffer size for reading and writing operations.
static const int BSize = 20; 

// Buffers to store data for read operations, initialized to zero (NUL characters).
static char B[BSize];  // Primary buffer for data read operations.
static char B2[BSize]; // Secondary buffer for additional testing.

// Socket pair descriptors for inter-thread communication.
static int daSktPr[2];

// Function to test reading and writing using circular buffers and custom socket operations.
void coutThreadFunc(void) {
    int RetVal; // Variable to store return values from read and write operations.

    // Initialize a circular buffer for testing.
    CircBuf<char> buffer;

    // Write data to the circular buffer.
    buffer.write("123456789", 10); // Write 10 bytes (including a NUL character for termination).

    // Read data from the circular buffer into the primary buffer `B`.
    RetVal = buffer.read(B, BSize);
    // Output the number of bytes read and the contents of `B`.
    cout << "Output Line 1 - RetVal: " << RetVal << "  B: " << B << endl;

    // Attempt to read from the socket with a minimum byte requirement and timeout.
    cout << "The next line will timeout in 5 or so seconds (50 deciseconds)" << endl;
    RetVal = myReadcond(daSktPr[1], B, BSize, 10, 50, 50); // Requires 10 bytes, waits up to 5 seconds.

    // Output the results of the read attempt.
    cout << "Output Line 2 - RetVal: " << RetVal << "  B: " << B << endl;

    // Write data to the socket `daSktPr[1]` for subsequent reading.
    myWrite(daSktPr[1], "wxyz", 5); // Write 5 bytes ("wxyz").

    // Write more data to the socket `daSktPr[0]` for testing cross-socket communication.
    myWrite(daSktPr[0], "ab", 3); // Write 3 bytes ("ab").

    // Read 5 bytes from `daSktPr[1]` into `B`, blocking if insufficient data.
    RetVal = myReadcond(daSktPr[1], B, BSize, 5, 0, 0); // Requires exactly 5 bytes, no timeout.

    // Output the results of the read operation, handling potential errors.
    cout << "Output Line 3 - RetVal: " << RetVal;
    if (RetVal == -1)
        cout << " Error:  " << strerror(errno); // Output error if `myReadcond` fails.
    else if (RetVal > 0)
        cout << "  B: " << B; // Output data if the read succeeds.
    cout << endl;

    // Perform another read operation with the same parameters.
    RetVal = myReadcond(daSktPr[1], B, BSize, 5, 0, 0);
    cout << "Output Line 4 - RetVal: " << RetVal;
    if (RetVal == -1)
        cout << " Error:  " << strerror(errno); // Output error if the read fails.
    else if (RetVal > 0)
        cout << "  B: " << B; // Output data if the read succeeds.
    cout << endl;

    // Attempt a third read operation, repeating the logic above.
    RetVal = myReadcond(daSktPr[1], B, BSize, 5, 0, 0);
    cout << "Output Line 4b- RetVal: " << RetVal;
    if (RetVal == -1)
        cout << " Error:  " << strerror(errno); // Output error if the read fails.
    else if (RetVal > 0)
        cout << "  B: " << B; // Output data if the read succeeds.
    cout << endl;

    // Write 10 bytes ("123456789") to `daSktPr[1]` and output the results.
    RetVal = myWrite(daSktPr[1], "123456789", 10); // Write 10 bytes.
    cout << "Output Line 5-1 - RetVal: " << RetVal;
    if (RetVal == -1)
        cout << " Error:  " << strerror(errno) << endl; // Output error if the write fails.
    else
        cout << endl;

    // Write 10 bytes ("123456789") to `daSktPr[0]` and output the results.
    RetVal = myWrite(daSktPr[0], "123456789", 10); // Write 10 bytes.
    cout << "Output Line 5-0 - RetVal: " << RetVal;
    if (RetVal == -1)
        cout << " Error:  " << strerror(errno) << endl; // Output error if the write fails.
    else
        cout << endl;
}

void coutThreadFunc(void) {
   int RetVal;
   CircBuf<char> buffer;
   buffer.write("1234", 5); // don't forget NUL termination character NULL != " "
   buffer.write("bodo", 4);
   RetVal = buffer.read(B, BSize);
   cout << "Output Line 1 - RetVal: " << RetVal << "  B: " << B << endl;
   cout << "The next line will timeout in 5 or so seconds (50 deciseconds)" << endl;
   RetVal = myReadcond(daSktPr[1], B, BSize, 10, 50, 50);

   cout << "Output Line 2 - RetVal: " << RetVal << "  B: " << B << endl;
   myWrite(daSktPr[1], "wxyz", 5); // don't forget NUL termination char

   myWrite(daSktPr[0], "babi", 5); // don't forget NUL termination character
   RetVal = myReadcond(daSktPr[1], B, 2, 2, 0, 0);

   cout << "Output Line 3 - RetVal: " << RetVal;
   if (RetVal == -1)
      cout << " Error:  " << strerror(errno);
   else if (RetVal > 0)
      cout << "  B: " << B;
   cout << endl;
   RetVal = myReadcond(daSktPr[1], B, 2, 1, 0, 0);
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
      myWrite(daSktPr[0], "abcde", 5);
      pthreadSupport::posixThread coutThread(SCHED_FIFO, 50, coutThreadFunc); // lower
      myTcdrain(daSktPr[0]);

      myWrite(daSktPr[0], "1234", 5); // don't forget NUL termination character
      myReadcond(daSktPr[0], B2, 4, 4, 0, 0);
      cout << "Output Line B2: " << B2 <<endl;

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

Output:
Output Line 1 - RetVal: 8  B: 1234
The next line will timeout in 5 or so seconds (50 deciseconds)
Output Line 2 - RetVal: 8  B: abcde123
Output Line B2: wxyz
Output Line 3 - RetVal: 2  B: bacde123
Output Line 4 - RetVal: 2  B: bicde123
Output Line 4b- RetVal: 1  B: 
Output Line 5-1 - RetVal: -1 Error:  Broken pipe
Output Line 5-0 - RetVal: -1 Error:  Bad file descriptor

