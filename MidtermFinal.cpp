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



