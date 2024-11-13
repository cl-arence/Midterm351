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
