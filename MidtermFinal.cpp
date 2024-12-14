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
        // Case 1: Buffer does not wrap around ("eeeeDDDDeeee"). (e = empty, D = data)
        p[1] = buf;  // Second segment starts at the beginning of the buffer.
        if (rpos) { //Checks if rpos != 0
            sizes[0] = size - write_pos.load(memory_order_relaxed); // Space until end of buffer.
            sizes[1] = rpos - 1;  // Space before read position. Reserve one slot.
        } else { //Checks if rpos == 0, means the read_pos is at the start of the buffer (index 0), and there is no second writable segment before read_pos.
            sizes[0] = size - write_pos.load(memory_order_relaxed) - 1; // Reserve one slot.
            sizes[1] = 0; // No second segment needed.
        }
    } else {
        // Case 2: Buffer wraps around ("DDeeeeeeeDD").
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
   //checks whether the data to be written (buffer_size) is larger than the space available in the first segment (sizes[0]).
   /*
   p[1]: Pointer to the second writable segment, typically at the start of the buffer (index 0).
   buffer + from_first:
   Skips over the portion of the input buffer already written to p[0].
   from_first represents the number of elements written to p[0] (min(buffer_size, sizes[0])).
   (buffer_size - sizes[0]):
   Represents the number of elements remaining to be written after filling the first segment.
   sizeof(T): Multiplies the number of remaining elements by the size of each element to calculate the total number of bytes to copy. 
   */
    if (buffer_size > sizes[0]) {
        memcpy(p[1], buffer + from_first, (buffer_size - sizes[0]) * sizeof(T));
    }

    // Update the write position, wrapping around if necessary.
    write_pos.store((write_pos.load(memory_order_relaxed) + buffer_size) % size, memory_order_release);

    // Return the number of elements written.
    return buffer_size;
}



