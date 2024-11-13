/* Ensc351Part3-test.cpp -- October 9 -- Copyright 2024 Craig Scratchley */

/* This program can be used to test your changes to myIO.cpp
 *
 * Put this project in the same workspace as your Ensc351 library project,
 * and build it.
 *
 * With 3 created threads for a total of 4 threads, the output that I get with my solution
 * is in the file "output-fromSolution".
 *
 */

#include <signal.h>
#include <sys/socket.h>
#include <stdlib.h>				// for exit()
#include <sched.h>
#include <format>
#include "posixThread.hpp"
#include "VNPE.h"
#include "myIO.h"

//#define COUT cout
#include "AtomicCOUT.h"

using namespace std;
using namespace pthreadSupport;

// Macro: REPORT0
// Logs the start and completion of a statement (S) in a thread for debugging.
//Used for logging the start and end of a statement (denoted by S) in a thread.
//This provides useful debug information about which thread is executing which statement and when it completes.

//The three lines are as follows: 
//Logs the thread name (threadName) and the string representation of the statement (#S) before it begins execution.
//Executes the actual statement S.
//Logs the thread name and the string representation of the statement after it finishes execution.
#define REPORT0(S) COUT << threadName << ": " << #S << "; statement will now be started\n";  \
    S; \
    COUT << threadName << ": " << #S << "; statement has now finished\n";

// Macro: REPORT1
// Logs the start, execution, and result of a function call (FC).
// If the function fails (RV == -1), logs the error code (errno) and associated error message.
#define REPORT1(FC) {COUT << threadName << ": " << #FC << " will now be called\n";  \
    int RV = FC; \
    COUT << threadName << ": " << #FC << " result was " << RV << \
       (-1 == RV ? format( " errno {}: {}\n", errno, strerror(errno)) : "\n"); \
}

// Macro: REPORT2
// Similar to REPORT1 but adds logging for an additional state variable (Ba) if the function succeeds.
// If RV > 0, logs the value of Ba. If RV == -1, logs the error code and message.
#define REPORT2(FC) {COUT << threadName << ": " << #FC << " will now be called\n";  \
    int RV = FC; \
    COUT << threadName << ": " << #FC << " result was " << RV << \
       (-1 == RV ? format( " errno {}: {}\n", errno, strerror(errno)) : \
             (0 < RV ? format(" Ba: {}\n", Ba) : "\n") \
       ); \
}

// Socket Pair Definitions
// Global arrays to store descriptors for connected socket pairs used in testing.

// Descriptor array for the first socket pair
//Limited to this file by static
//socket pair definitions and mySocketpair work together
//with mySocketpair creating the sockets and managing their state, and the global arrays storing their descriptors for use in the tests.
static int daSktPr1[2];

// Descriptor array for the second socket pair
static int daSktPr2[2];

// Descriptor array for the third socket pair
static int daSktPr3[2];


//main :: sets up the testing environment, handles CPU affinity and thread priority, 
//and launches the main testing thread (threadT32Func) to perform the primary tests.
int main() {
    // below line is to ignore SIGPIPE signals to prevent the program from terminating
    // when attempting to write to a closed socket.
    /*PE_SIG_ERR*/(signal(SIGPIPE, SIG_IGN)); 

    // Configure CPU affinity to limit execution to CPU 0 to 
    //ensure consistent thread behavior by running all threads on the same CPU,
    cpu_set_t cpu_set;                // Define a CPU set.
    const int myCpu{0};               // Specify CPU 0.
    CPU_ZERO(&cpu_set);               // Clear the CPU set.
    CPU_SET(myCpu, &cpu_set);         // Add CPU 0 to the set.

    // Set the name of the primary thread to Pri for debugging/logging.
    const char* threadName{"Pri"};
    PE_0(pthread_setname_np(pthread_self(), threadName));

    // Apply the CPU affinity to the current thread and its children to ensure they all run at CPU 0. 
    PE(sched_setaffinity(0, sizeof(cpu_set), &cpu_set));

    //try block for exception handling 
    try {
        // Configure the primary thread's priority and scheduling policy.
        int primaryPriority{90};     // Desired priority for the primary thread.
        sched_param sch;             // Structure to store scheduling parameters.
        int policy{-1};              // Variable to store the scheduling policy.

        // Retrieve the current scheduling policy and priority.
        getSchedParam(&policy, &sch); 

        // Warn the user if the debugger is not running at a high priority sice it shoul be higher than main at around 99
        if (98 > sch.__sched_priority) {
            cout << "**** If you are debugging, debugger is not running at a high priority. ****\n"
                 << "**** This could cause problems with debugging. Consider debugging\n"
                 << "**** with the proper debug launch configuration ****" << std::endl;
        }

        // Log the current scheduling policy and priority.
        COUT << "Primary Thread was executing at policy " << policy
             << " and priority " << sch.sched_priority << endl;

        // Set the new priority and scheduling policy to SCHED_FIFO (real-time).
        sch.__sched_priority = primaryPriority;
        setSchedParam(SCHED_FIFO, sch); 

        // Verify and log the updated scheduling policy and priority.
        getSchedParam(&policy, &sch);
        COUT << "Primary Thread now executing at policy (should be 1) " << policy
             << " and priority (should be " << primaryPriority << ") " << sch.sched_priority << endl;

        // Launch the main testing thread (T32) with priority 70 and join it after execution.
        //posixThread T32(SCHED_FIFO, 70, threadT32Func) creates and starts a new thread (T32)
        //The thread runs the function threadT32Func with the SCHED_FIFO policy and a priority of 70.
        //T32.join() makes Pri wait for the thread T32 to finish execution before continuing hence, 
        //the main thread blocks at this point, ensuring that T32 completes its work before the main program proceeds.
        //Only T32 is launched as it also calls the other threads on its own
        REPORT0(posixThread T32(SCHED_FIFO, 70, threadT32Func)); 
        REPORT0(T32.join()); 

        // Then after it logs that the primary thread is finishing execution.
        COUT << "Primary Thread finishing" << endl;
        return 0; // Exit successfully.
    }

        // Then finally catch block to handle system errors, log the error details, and return the error code.
    catch (std::system_error& error) {
        cout << "Error: " << error.code() << " - " << error.what() << endl;
        return error.code().value();
    }
    catch (...) {
        // Re-throw any other exceptions for debugging purposes.
        throw;
    }
}



// threadT32Func: Testing function executed by thread T32 (priority changes: 70 → 40 → 80 → 40).
// This function tests socket creation, writing, draining, closing, and synchronization at different priorities.
void threadT32Func(void) {
    const char* threadName{"T32"};
    PE_0(pthread_setname_np(pthread_self(), threadName)); // Set the thread name to "T32" for identification in logs.

    // Create three socket pairs for testing purposes.
    //Sets up the environment for subsequent tests by establishing connected sockets.
    REPORT1(mySocketpair(AF_LOCAL, SOCK_STREAM, 0, daSktPr1)); // Create socket pair 1.
    REPORT1(mySocketpair(AF_LOCAL, SOCK_STREAM, 0, daSktPr2)); // Create socket pair 2.
    REPORT1(mySocketpair(AF_LOCAL, SOCK_STREAM, 0, daSktPr3)); // Create socket pair 3.

    // Write "abcd" (4 bytes) to the first socket in the first pair.
    // Ensure the number of bytes written matches the expected value (4).
    //This is a test for myWrite
    REPORT1(PE_NOT(myWrite(daSktPr1[0], "abcd", 4), 4));

    // Launch thread T42 with priority 60, running the function threadT42Func.
    //Introduces concurrent behavior, allowing testing of synchronization mechanisms like myTcdrain and myReadcond.
    REPORT0(posixThread threadT42(60, threadT42Func));

     //Drains the socket, blocking until all data written to daSktPr1[0] is read by myReadcond in another thread.
     //Verifies that myTcdrain waits correctly for data to be processed by the receiver
     //Logs the start of the drain operation but doesn't log the result because it blocks, 
     //waiting for T41 to read data or the paired socket to close.

    //**********Then since its blocked, jump to T42***********////////////
    //This is because myTcdrain blocks until specific conditions are met (all data is read, or the socket is closed). 
    //Other higher-priority threads (like T42) execute in the meantime.
    //myTcdrain(daSktPr1[0]) results in 0 after T41 reads enough data or closes the paired socket (daSktPr1[1])
    REPORT1(myTcdrain(daSktPr1[0]));

    // Now coming back from T41 Lower the priority of this thread to 40.
    REPORT1(setSchedPrio(40));

    // Write "123456789" (10 bytes, including null terminator) to daSktPr1[0].
    // Ensure all bytes are written successfully.
    //Note daSktPr1[0] is used for writing, and daSktPr1[1] is used for reading.
    //***This will unblock T41 which has higher priority so we go to it******
    REPORT1(PE_NOT(myWrite(daSktPr1[0], "123456789", 10), 10));

    // Raise the priority of this thread to 80.
    REPORT1(setSchedPrio(80));

    // Write "xyz" (4 bytes) to daSktPr1[0] and ensure the write is successful.
    REPORT1(PE_NOT(myWrite(daSktPr1[0], "xyz", 4), 4));

    // Drain the socket on daSktPr1[0].
    // Ensures all written data has been processed by the paired socket.
    REPORT1(PE(myTcdrain(daSktPr1[0])));

    // Close the first socket in the first pair to test socket closure behavior.
    REPORT1(myClose(daSktPr1[0]));

    // Lower the priority of this thread back to 40.
    REPORT1(setSchedPrio(40));

    // Close the first socket in the third pair.
    REPORT1(myClose(daSktPr3[0]));

    // Write "mno" (4 bytes) to daSktPr2[0] and ensure the write is successful.
    REPORT1(PE_NOT(myWrite(daSktPr2[0], "mno", 4), 4));

    // Close the first socket in the second pair to test socket closure behavior.
    REPORT1(myClose(daSktPr2[0]));

    // Wait for thread T42 to finish execution before exiting this function.
    REPORT0(threadT42.join());
}


// threadT42Func: Testing function executed by thread T42 (starts at priority 60).
// This function tests socket writing, draining, and synchronization with T32 and T41.
void threadT42Func(void) {
    // Set the thread name to "T42" for easier identification in logs and debugging.
    const char* threadName{"T42"};
    PE_0(pthread_setname_np(pthread_self(), threadName));

    // Write "ijkl" (5 bytes) to the second socket in the first pair (daSktPr1[1]).
    // PE_NOT verifies that the number of bytes written matches the expected value (5).
    REPORT1(PE_NOT(myWrite(daSktPr1[1], "ijkl", 5), 5));

    // Launch thread T41 with priority 50 to test conditional reading and socket closure.
    REPORT0(posixThread threadT41(50, threadT41Func));

    //**********Then since it is blocked after below line, jump to T41***********
    //This is because myTcdrain blocks until specific conditions are met (all data is read, or the socket is closed). 
    //Other higher-priority threads (like T41) execute in the meantime.
    // Drain the socket on daSktPr1[1].
    // myTcdrain(daSktPr1[1]) results in 0 after T32 closes daSktPr1[0] (myClose(daSktPr1[0]) is called in thread T32).
    REPORT1(myTcdrain(daSktPr1[1]));

    // Drain again to ensure all remaining data has been processed by the paired socket.
    //Verifies that myTcdrain behaves consistently when called multiple times.
    REPORT1(myTcdrain(daSktPr1[1]));

    // Wait for thread T41 to complete its execution before finishing this function.
    //ensures all operations initiated by T41 (e.g., conditional reads or socket closures) are completed before T42 exits.
    REPORT0(threadT41.join());
} // Logs output at this point from the above REPORT0 calls.


// threadT41Func: Testing function executed by thread T41 (starts at priority 50).
// This function tests conditional reads, writes, and socket closure behavior in a multi-threaded environment.

void threadT41Func(void) {
    // Buffer for storing data read from sockets.
    char Ba[20];                                 
    const char* threadName{"T41"};
    PE_0(pthread_setname_np(pthread_self(), threadName)); // Set thread name to "T41" for logs and debugging.

    // Attempts to read up to 20 bytes from daSktPr1[1] but blocks until at least 12 bytes are available.
    //Tests myReadcond's ability to wait for sufficient data before proceeding.
    //*****So goes back to T32********
    //Then after coming back after the write we get output line 25
    REPORT2(myReadcond(daSktPr1[1], Ba, 20, 12, 0, 0));

    // Reads any available data from daSktPr1[1] without waiting (minimum required bytes = 0).
    //Verifies that myReadcond correctly handles non-blocking reads when data is already available.
    REPORT2(myReadcond(daSktPr1[1], Ba, 20, 0, 0, 0));

    // Writes 17 bytes ("Will not be read") to daSktPr1[1].
    REPORT1(myWrite(daSktPr1[1], "Will not be read", 17));

    //Attempts to read from daSktPr1[1] but blocks until the paired socket (daSktPr1[0]) is closed.
    REPORT2(myReadcond(daSktPr1[1], Ba, 20, 12, 0, 0));

   //Attempts to read from a socket but fails with error 104 (Connection reset by peer) because the paired socket has been closed.
    REPORT2(myReadcond(daSktPr1[1], Ba, 20, 1, 0, 0));

    // Write 17 bytes ("Will not be read") to daSktPr3[1].
    REPORT1(myWrite(daSktPr3[1], "Will not be read", 17));

    // Blocking conditional read: Waits until the paired socket (daSktPr3[0]) is closed by T32.
    REPORT2(myReadcond(daSktPr3[1], Ba, 20, 1, 0, 0));

    // Non-blocking read on a closed socket: Returns 0 because no data is available.
    REPORT2(myReadcond(daSktPr3[1], Ba, 20, 1, 0, 0));

    // Write 6 bytes ("Added") to daSktPr1[1].
    REPORT1(myWrite(daSktPr1[1], "Added", 6));

    // Perform two reads from daSktPr2[1].
    REPORT2(myRead(daSktPr2[1], Ba, 20)); 
    REPORT2(myRead(daSktPr2[1], Ba, 20));

    // Close the second socket in the second pair (daSktPr2[1]).
    REPORT1(myClose(daSktPr2[1]));

    // Close the second socket in the third pair (daSktPr3[1]).
    REPORT1(myClose(daSktPr3[1]));

    // Close the second socket in the first pair (daSktPr1[1]).
    REPORT1(myClose(daSktPr1[1]));

    // Attempt to close daSktPr1[1] again (redundant close).
    REPORT1(myClose(daSktPr1[1]));

    // Attempt to read from a closed socket (daSktPr1[1]).
    REPORT2(myRead(daSktPr1[1], Ba, 20));

    // Attempt a conditional read from a closed socket (daSktPr1[1]).
    REPORT2(myReadcond(daSktPr1[1], Ba, 20, 0, 0, 0));

    // Attempt to write to a closed socket (daSktPr1[1]).
    REPORT1(myWrite(daSktPr1[1], Ba, 20));

    // Attempt to drain a closed socket (daSktPr1[1]).
    REPORT1(myTcdrain(daSktPr1[1]));
}
