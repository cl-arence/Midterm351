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
static int daSktPr1[2];

// Descriptor array for the second socket pair
static int daSktPr2[2];

// Descriptor array for the third socket pair
static int daSktPr3[2];
