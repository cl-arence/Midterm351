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
