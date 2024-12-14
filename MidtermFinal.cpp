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
    T *p[2];               // Pointers to segments where data will be written.
    unsigned sizes[2];      // Sizes of the writable segments.

    // Start writing at the current write position.
    p[0] = buf + write_pos.load(memory_order_relaxed);

    // Load the read position to determine available space, ensuring visibility across threads.
    const unsigned rpos = read_pos.load(memory_order_acquire);

    // Determine writable space in the buffer.
    if (rpos <= write_pos.load(memory_order_relaxed)) {
        // Case 1: Buffer does not wrap around ("eeeeDDDDeeee").
        p[1] = buf;  // Second segment starts at the beginning of the buffer.
        if (rpos) {
            sizes[0] = size - write_pos.load(memory_order_relaxed); // Space until end of buffer.
            sizes[1] = rpos - 1;  // Space before read position.
        } else {
            sizes[0] = size - write_pos.load(memory_order_relaxed) - 1; // Reserve one slot.
            sizes[1] = 0; // No second segment needed.
        }
    } else {
        // Case 2: Buffer wraps around ("DDeeeeeeeDD").
        p[1] = nullptr; // No second segment used.
        sizes[0] = rpos - write_pos.load(memory_order_relaxed) - 1; // Space before read position.
        sizes[1] = 0;
    }

    // Ensure we only write as much as the buffer can fit.
    buffer_size = min(buffer_size, sizes[0] + sizes[1]);

    // Write data into the first segment.
    const int from_first = min(buffer_size, sizes[0]);
    memcpy(p[0], buffer, from_first * sizeof(T));

    // If needed, write remaining data into the second segment.
    if (buffer_size > sizes[0]) {
        memcpy(p[1], buffer + from_first, (buffer_size - sizes[0]) * sizeof(T));
    }

    // Update the write position, wrapping around if necessary.
    write_pos.store((write_pos.load(memory_order_relaxed) + buffer_size) % size, memory_order_release);

    // Return the number of elements written.
    return buffer_size;
}



