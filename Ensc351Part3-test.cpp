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

#define REPORT0(S) COUT << threadName << ": " << #S << "; statement will now be started\n";  \
    S; \
    COUT << threadName << ": " << #S << "; statement has now finished\n";

#define REPORT1(FC) {COUT << threadName << ": " << #FC << " will now be called\n";  \
    int RV = FC; \
    COUT << threadName << ": " << #FC << " result was " << RV << \
       (-1 == RV ? format( " errno {}: {}\n", errno, strerror(errno)) : "\n"); \
}

#define REPORT2(FC) {COUT << threadName << ": " << #FC << " will now be called\n";  \
    int RV = FC; \
    COUT << threadName << ": " << #FC << " result was " << RV << \
       (-1 == RV ? format( " errno {}: {}\n", errno, strerror(errno)) : \
             (0 < RV ? format(" Ba: {}\n", Ba) : "\n") \
       ); \
}

static int daSktPr1[2];    // Descriptor Array for Socket Pair
static int daSktPr2[2];    // Descriptor Array for 2nd Socket Pair
static int daSktPr3[2];    // Descriptor Array for another Socket Pair

void threadT41Func(void) // starts at priority 50
{
    char    Ba[20];
    const char* threadName{"T41"};
    PE_0(pthread_setname_np(pthread_self(), threadName));
    //
    // Blank lines below indicate that the statement above the blank line
    // will finish after one or more other threads in the process have made progress.
    REPORT2(myReadcond(daSktPr1[1], Ba, 20, 12, 0, 0));   // will block until myWrite of 10 characters

    REPORT2(myReadcond(daSktPr1[1], Ba, 20, 0, 0, 0));
    REPORT1(myWrite(daSktPr1[1], "Will not be read", 17));
    REPORT2(myReadcond(daSktPr1[1], Ba, 20, 12, 0, 0));   // will block until myClose(daSktPr1[0])

    REPORT2(myReadcond(daSktPr1[1], Ba, 20, 1, 0, 0));   // will return -1 with error 104, Connection reset by peer
    REPORT1(myWrite(daSktPr3[1], "Will not be read", 17));
    REPORT2(myReadcond(daSktPr3[1], Ba, 20, 1, 0, 0));   // will block until myClose(daSktPr3[0])

    REPORT2(myReadcond(daSktPr3[1], Ba, 20, 1, 0, 0));   // will return 0
    REPORT1(myWrite(daSktPr1[1], "Added", 6));
    REPORT2(myRead(daSktPr2[1], Ba, 20));

    REPORT2(myRead(daSktPr2[1], Ba, 20));

    REPORT1(myClose(daSktPr2[1]));
    REPORT1(myClose(daSktPr3[1]));
    REPORT1(myClose(daSktPr1[1]));
    REPORT1(myClose(daSktPr1[1]));
    REPORT2(myRead(daSktPr1[1], Ba, 20));
    REPORT2(myReadcond(daSktPr1[1], Ba, 20, 0, 0, 0));
    REPORT1(myWrite(daSktPr1[1], Ba, 20));
    REPORT1(myTcdrain(daSktPr1[1]));
}

void threadT42Func(void) // starts at priority 60
{
    const char* threadName{"T42"};
    PE_0(pthread_setname_np(pthread_self(), threadName));
    //
    REPORT1(PE_NOT(myWrite(daSktPr1[1], "ijkl", 5), 5));
    REPORT0(posixThread threadT41(50, threadT41Func));
    REPORT1(myTcdrain(daSktPr1[1])); // will block until myClose(daSktPr1[0]) in T32

    REPORT1(myTcdrain(daSktPr1[1]));
    REPORT0(threadT41.join());

} // output happens at this time from the above REPORT0

void threadT32Func(void) // priority 70 -> priority 40 -> priority 80 -> priority 40
{
    const char* threadName{"T32"};
    PE_0(pthread_setname_np(pthread_self(), threadName));
    //
    REPORT1(mySocketpair(AF_LOCAL, SOCK_STREAM, 0, daSktPr1));
    REPORT1(mySocketpair(AF_LOCAL, SOCK_STREAM, 0, daSktPr2));
    REPORT1(mySocketpair(AF_LOCAL, SOCK_STREAM, 0, daSktPr3));
    //
    REPORT1(PE_NOT(myWrite(daSktPr1[0], "abcd", 4), 4));
    REPORT0(posixThread threadT42(60, threadT42Func));
    REPORT1(myTcdrain(daSktPr1[0])); // will block until 1st myReadcond(..., 12, ...);

    REPORT1(setSchedPrio(40));
    REPORT1(PE_NOT(myWrite(daSktPr1[0], "123456789", 10), 10)); // don't forget nul termination character

    REPORT1(setSchedPrio(80));
    REPORT1(PE_NOT(myWrite(daSktPr1[0], "xyz", 4), 4));
    REPORT1(PE(myTcdrain(daSktPr1[0])));
    REPORT1(myClose(daSktPr1[0]));
    REPORT1(setSchedPrio(40));

    REPORT1(myClose(daSktPr3[0]));

    REPORT1(PE_NOT(myWrite(daSktPr2[0], "mno", 4), 4));

    REPORT1(myClose(daSktPr2[0]));

    REPORT0(threadT42.join());
}

//#include "signal.h"
int main() {
   // we should check for errors on the next line
   /*PE_SIG_ERR*/(signal(SIGPIPE, SIG_IGN)); // ignore SIGPIPE signals

   cpu_set_t cpu_set;
   const int myCpu{0};
   CPU_ZERO(&cpu_set); // this is important and was missing.
   CPU_SET(myCpu, &cpu_set);

   const char* threadName{"Pri"};
   PE_0(pthread_setname_np(pthread_self(), threadName));
   PE(sched_setaffinity(0, sizeof(cpu_set), &cpu_set)); // set processor affinity for current and child threads

	try {
      int primaryPriority{90};
	   sched_param sch;
	   int policy{-1};

	   getSchedParam(&policy, &sch);
		if (98 > sch.__sched_priority)
		   cout << "**** If you are debugging, debugger is not running at a high priority. ****\n" <<
		                    " **** This could cause problems with debugging.  Consider debugging\n" <<
		                    " **** with the proper debug launch configuration ****" << std::endl;
		COUT << "Primary Thread was executing at policy " << policy << " and priority " <<  sch.sched_priority << endl;
		sch.__sched_priority = primaryPriority;
		setSchedParam(SCHED_FIFO, sch); //SCHED_FIFO == 1, SCHED_RR == 2
		getSchedParam(&policy, &sch);
		COUT << "Primary Thread now executing at policy (should be 1) " << policy << " and priority (should be " << primaryPriority << ") " <<  sch.sched_priority << endl;
		//sleep(1);
//	   /*PE_SIG_ERR*/(signal(SIGPIPE, SIG_IGN)); // ignore SIGPIPE signals

		REPORT0(posixThread T32(SCHED_FIFO, 70, threadT32Func));
		REPORT0(T32.join());

      COUT << "Primary Thread finishing" << endl;
		return 0;
	}
	catch (std::system_error& error) {
		cout << "Error: " << error.code() << " - " << error.what() << endl;
		return error.code().value();
	}
	catch (...) { throw; }
}
