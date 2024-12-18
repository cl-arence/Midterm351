////////////////////////////////////////////////
// Generated by SmartState C++ Code Generator //
//                 DO NOT EDIT				  //
////////////////////////////////////////////////

#pragma warning(disable: 4786)
#pragma warning(disable: 4290)

//Additional Includes
#include "AtomicCOUT.h"
#include <stdlib.h>

/*  YMODEM Sender
Copyright (C) 2021 Craig Scratchley
wcs AT sfu.ca */


#include "ySenderSS.h"
#include "SenderY.h"

/*Messages
Define user specific messages in a file and
include that file in the additional includes section in 
the model.
-- FOLLOWING MESSAGES ARE USED --
TM
SER
KB_C
*/

//Additional Declarations
#define c wParam



namespace ySender_SS
{
using namespace std;
using namespace smartstate;

//State Mgr
//--------------------------------------------------------------------
ySenderSS::ySenderSS(SenderY* ctx, bool startMachine/*=true*/)
 : StateMgr("ySenderSS"),
   myCtx(ctx)
{
	myConcStateList.push_back(new Sender_TopLevel_ySenderSS("Sender_TopLevel_ySenderSS", 0, this));

	if(startMachine)
		start();
}

SenderY& ySenderSS::getCtx() const
{
	return *myCtx;
}

//Base State
//--------------------------------------------------------------------
ySenderBaseState::ySenderBaseState(const string& name, BaseState* parent, ySenderSS* mgr)
 : BaseState(name, parent, mgr)
{
}

//--------------------------------------------------------------------
Sender_TopLevel_ySenderSS::Sender_TopLevel_ySenderSS(const string& name, BaseState* parent, ySenderSS* mgr)
 : ySenderBaseState(name, parent, mgr)
{
	myHistory = false;
	mySubStates.push_back(new NON_CAN_Sender_TopLevel("NON_CAN_Sender_TopLevel", this, mgr));
	mySubStates.push_back(new CAN_Sender_TopLevel("CAN_Sender_TopLevel", this, mgr));
	setType(eSuper);
}

void Sender_TopLevel_ySenderSS::onEntry()
{
	/* -g option specified while compilation. */
	myMgr->debugLog("> Sender_TopLevel_ySenderSS <onEntry>");

	SenderY& ctx = getMgr()->getCtx();

	// Code from Model here
	    ctx.prepStatBlk(); ctx.errCnt=0; 
	    ctx.KbCan = false; ctx.tm(TM_VL); 
}

void Sender_TopLevel_ySenderSS::onExit()
{
	/* -g option specified while compilation. */
	myMgr->debugLog("< Sender_TopLevel_ySenderSS <onExit>");

}

void Sender_TopLevel_ySenderSS::onMessage(const Mesg& mesg)
{
	if(mesg.message == TM)
		onTMMessage(mesg);
	else if(mesg.message == SER)
		onSERMessage(mesg);
	else 
		super::onMessage(mesg);
}

void Sender_TopLevel_ySenderSS::onTMMessage(const Mesg& mesg)
{
	int wParam = mesg.wParam;
	int lParam = mesg.lParam;
	SenderY& ctx = getMgr()->getCtx();

		/* -g option specified while compilation. */
		myMgr->debugLog("Sender_TopLevel_ySenderSS TM <message trapped>");

	if(true)
	{
		/* -g option specified while compilation. */
		myMgr->debugLog("Sender_TopLevel_ySenderSS TM <executing exit>");

		const BaseState* root = getMgr()->executeExit("Sender_TopLevel_ySenderSS", "FinalState");
		/* -g option specified while compilation. */
		myMgr->debugLog("Sender_TopLevel_ySenderSS TM <executing effect>");


		//User specified effect begin
		ctx.cans();
		ctx.closeTransferredFile();
		if (ctx.KbCan)
		     ctx.result += "KbCancelled";
		else
		     ctx.result += "Timeout";
		//User specified effect end

		/* -g option specified while compilation. */
		myMgr->debugLog("Sender_TopLevel_ySenderSS TM <executing entry>");

		getMgr()->executeEntry(root, "FinalState");
		return;
	}

	super::onMessage(mesg);
}

void Sender_TopLevel_ySenderSS::onSERMessage(const Mesg& mesg)
{
	int wParam = mesg.wParam;
	int lParam = mesg.lParam;
	SenderY& ctx = getMgr()->getCtx();

		/* -g option specified while compilation. */
		myMgr->debugLog("Sender_TopLevel_ySenderSS SER <message trapped>");

	if(ctx.KbCan && (c==ACK || c==NAK || c=='C'))
	{
		/* -g option specified while compilation. */
		myMgr->debugLog("Sender_TopLevel_ySenderSS SER <executing exit>");

		const BaseState* root = getMgr()->executeExit("Sender_TopLevel_ySenderSS", "FinalState");
		/* -g option specified while compilation. */
		myMgr->debugLog("Sender_TopLevel_ySenderSS SER <executing effect>");


		//User specified effect begin
		ctx.cans();
		ctx.closeTransferredFile();
		ctx.result += "KbCancelled";
		//User specified effect end

		/* -g option specified while compilation. */
		myMgr->debugLog("Sender_TopLevel_ySenderSS SER <executing entry>");

		getMgr()->executeEntry(root, "FinalState");
		return;
	}

	super::onMessage(mesg);
}

//--------------------------------------------------------------------
NON_CAN_Sender_TopLevel::NON_CAN_Sender_TopLevel(const string& name, BaseState* parent, ySenderSS* mgr)
 : ySenderBaseState(name, parent, mgr)
{
	myHistory = true;
	mySubStates.push_back(new StatC_NON_CAN("StatC_NON_CAN", this, mgr));
	mySubStates.push_back(new ACKNAK_NON_CAN("ACKNAK_NON_CAN", this, mgr));
	mySubStates.push_back(new EOT1_NON_CAN("EOT1_NON_CAN", this, mgr));
	mySubStates.push_back(new ONE_NON_CAN("ONE_NON_CAN", this, mgr));
	mySubStates.push_back(new EOTEOT_NON_CAN("EOTEOT_NON_CAN", this, mgr));
	mySubStates.push_back(new ACKNAKSTAT_NON_CAN("ACKNAKSTAT_NON_CAN", this, mgr));
	setType(eSuper);
}

void NON_CAN_Sender_TopLevel::onEntry()
{
	/* -g option specified while compilation. */
	myMgr->debugLog("> NON_CAN_Sender_TopLevel <onEntry>");

}

void NON_CAN_Sender_TopLevel::onExit()
{
	/* -g option specified while compilation. */
	myMgr->debugLog("< NON_CAN_Sender_TopLevel <onExit>");

}

void NON_CAN_Sender_TopLevel::onMessage(const Mesg& mesg)
{
	if(mesg.message == SER)
		onSERMessage(mesg);
	else if(mesg.message == KB_C)
		onKB_CMessage(mesg);
	else 
		super::onMessage(mesg);
}

void NON_CAN_Sender_TopLevel::onSERMessage(const Mesg& mesg)
{
	int wParam = mesg.wParam;
	int lParam = mesg.lParam;
	SenderY& ctx = getMgr()->getCtx();

		/* -g option specified while compilation. */
		myMgr->debugLog("NON_CAN_Sender_TopLevel SER <message trapped>");

	if(c==NAK && (ctx.errCnt >= errB) )
	{
		/* -g option specified while compilation. */
		myMgr->debugLog("NON_CAN_Sender_TopLevel SER <executing exit>");

		const BaseState* root = getMgr()->executeExit("NON_CAN_Sender_TopLevel", "FinalState");
		/* -g option specified while compilation. */
		myMgr->debugLog("NON_CAN_Sender_TopLevel SER <executing effect>");


		//User specified effect begin
		ctx.cans();
		ctx.closeTransferredFile();
		ctx.result += "ExcessiveNAKs";
		//User specified effect end

		/* -g option specified while compilation. */
		myMgr->debugLog("NON_CAN_Sender_TopLevel SER <executing entry>");

		getMgr()->executeEntry(root, "FinalState");
		return;
	}
	else
	if(c == CAN)
	{
		/* -g option specified while compilation. */
		myMgr->debugLog("NON_CAN_Sender_TopLevel SER <executing exit>");

		const BaseState* root = getMgr()->executeExit("NON_CAN_Sender_TopLevel", "CAN_Sender_TopLevel");
		/* -g option specified while compilation. */
		myMgr->debugLog("NON_CAN_Sender_TopLevel SER <executing effect>");


		//User specified effect begin
		ctx.tmPush(
		   TM_CHAR);
		//User specified effect end

		/* -g option specified while compilation. */
		myMgr->debugLog("NON_CAN_Sender_TopLevel SER <executing entry>");

		getMgr()->executeEntry(root, "CAN_Sender_TopLevel");
		return;
	}

	super::onMessage(mesg);
}

void NON_CAN_Sender_TopLevel::onKB_CMessage(const Mesg& mesg)
{
	int wParam = mesg.wParam;
	int lParam = mesg.lParam;
	SenderY& ctx = getMgr()->getCtx();

		/* -g option specified while compilation. */
		myMgr->debugLog("NON_CAN_Sender_TopLevel KB_C <message trapped>");

	if(!ctx.KbCan)
	{
		/* -g option specified while compilation. */
		myMgr->debugLog("NON_CAN_Sender_TopLevel KB_C <executing effect>");


		//User specified effect begin
		ctx.KbCan = true;
		ctx.tmRed(TM_VL - 
		                 TM_2CHAR);
		
		//User specified effect end

		return;
	}

	super::onMessage(mesg);
}

//--------------------------------------------------------------------
ACKNAK_NON_CAN::ACKNAK_NON_CAN(const string& name, BaseState* parent, ySenderSS* mgr)
 : ySenderBaseState(name, parent, mgr)
{
	myHistory = false;
}

void ACKNAK_NON_CAN::onEntry()
{
	/* -g option specified while compilation. */
	myMgr->debugLog("> ACKNAK_NON_CAN <onEntry>");

}

void ACKNAK_NON_CAN::onExit()
{
	/* -g option specified while compilation. */
	myMgr->debugLog("< ACKNAK_NON_CAN <onExit>");

}

void ACKNAK_NON_CAN::onMessage(const Mesg& mesg)
{
	if(mesg.message == SER)
		onSERMessage(mesg);
	else 
		super::onMessage(mesg);
}

void ACKNAK_NON_CAN::onSERMessage(const Mesg& mesg)
{
	int wParam = mesg.wParam;
	int lParam = mesg.lParam;
	SenderY& ctx = getMgr()->getCtx();

		/* -g option specified while compilation. */
		myMgr->debugLog("ACKNAK_NON_CAN SER <message trapped>");

	if( (c==NAK || (c=='C' && ctx.firstBlk)) && (ctx.errCnt < errB) && !ctx.KbCan)
	{
		/* -g option specified while compilation. */
		myMgr->debugLog("ACKNAK_NON_CAN SER <executing effect>");


		//User specified effect begin
		ctx.resendBlk();
		ctx.errCnt++; ctx.tm(TM_VL); 
		//User specified effect end

		return;
	}
	else
	if((c==ACK) && !ctx.bytesRd && !ctx.KbCan)
	{
		/* -g option specified while compilation. */
		myMgr->debugLog("ACKNAK_NON_CAN SER <executing exit>");

		const BaseState* root = getMgr()->executeExit("ACKNAK_NON_CAN", "EOT1_NON_CAN");
		/* -g option specified while compilation. */
		myMgr->debugLog("ACKNAK_NON_CAN SER <executing effect>");


		//User specified effect begin
		ctx.sendByte(EOT);ctx.errCnt=0; 
		ctx.closeTransferredFile();
		ctx.tm(TM_VL); 
		
		//User specified effect end

		/* -g option specified while compilation. */
		myMgr->debugLog("ACKNAK_NON_CAN SER <executing entry>");

		getMgr()->executeEntry(root, "EOT1_NON_CAN");
		return;
	}
	else
	if((c==ACK) && ctx.bytesRd && !ctx.KbCan)
	{
		/* -g option specified while compilation. */
		myMgr->debugLog("ACKNAK_NON_CAN SER <executing effect>");


		//User specified effect begin
		ctx.sendBlkPrepNext(); 
		ctx.errCnt=0; ctx.tm(TM_VL); 
		ctx.firstBlk=false;
		//User specified effect end

		return;
	}

	super::onMessage(mesg);
}

//--------------------------------------------------------------------
EOT1_NON_CAN::EOT1_NON_CAN(const string& name, BaseState* parent, ySenderSS* mgr)
 : ySenderBaseState(name, parent, mgr)
{
	myHistory = false;
}

void EOT1_NON_CAN::onEntry()
{
	/* -g option specified while compilation. */
	myMgr->debugLog("> EOT1_NON_CAN <onEntry>");

}

void EOT1_NON_CAN::onExit()
{
	/* -g option specified while compilation. */
	myMgr->debugLog("< EOT1_NON_CAN <onExit>");

}

void EOT1_NON_CAN::onMessage(const Mesg& mesg)
{
	if(mesg.message == SER)
		onSERMessage(mesg);
	else 
		super::onMessage(mesg);
}

void EOT1_NON_CAN::onSERMessage(const Mesg& mesg)
{
	int wParam = mesg.wParam;
	int lParam = mesg.lParam;
	SenderY& ctx = getMgr()->getCtx();

		/* -g option specified while compilation. */
		myMgr->debugLog("EOT1_NON_CAN SER <message trapped>");

	if(c=='C' && ctx.firstBlk && ctx.errCnt < errB)
	{
		/* -g option specified while compilation. */
		myMgr->debugLog("EOT1_NON_CAN SER <executing effect>");


		//User specified effect begin
		ctx.sendByte(EOT);
		++ctx.errCnt;
		ctx.tm(TM_VL);
		//User specified effect end

		return;
	}
	else
	if(c == ACK && !ctx.KbCan)
	{
		/* -g option specified while compilation. */
		myMgr->debugLog("EOT1_NON_CAN SER <executing exit>");

		const BaseState* root = getMgr()->executeExit("EOT1_NON_CAN", "StatC_NON_CAN");
		/* -g option specified while compilation. */
		myMgr->debugLog("EOT1_NON_CAN SER <executing effect>");


		//User specified effect begin
		cout << "1st EOT ACK'd";
		ctx.prepStatBlk(); ctx.tm(TM_VL); 
		//User specified effect end

		/* -g option specified while compilation. */
		myMgr->debugLog("EOT1_NON_CAN SER <executing entry>");

		getMgr()->executeEntry(root, "StatC_NON_CAN");
		return;
	}
	else
	if(c==NAK && !ctx.KbCan)
	{
		/* -g option specified while compilation. */
		myMgr->debugLog("EOT1_NON_CAN SER <executing exit>");

		const BaseState* root = getMgr()->executeExit("EOT1_NON_CAN", "EOTEOT_NON_CAN");
		/* -g option specified while compilation. */
		myMgr->debugLog("EOT1_NON_CAN SER <executing effect>");


		//User specified effect begin
		ctx.sendByte(EOT);
		ctx.errCnt=0;ctx.tm(TM_VL); 
		ctx.firstBlk=false;
		//User specified effect end

		/* -g option specified while compilation. */
		myMgr->debugLog("EOT1_NON_CAN SER <executing entry>");

		getMgr()->executeEntry(root, "EOTEOT_NON_CAN");
		return;
	}

	super::onMessage(mesg);
}

//--------------------------------------------------------------------
ONE_NON_CAN::ONE_NON_CAN(const string& name, BaseState* parent, ySenderSS* mgr)
 : ySenderBaseState(name, parent, mgr)
{
	myHistory = false;
}

void ONE_NON_CAN::onEntry()
{
	/* -g option specified while compilation. */
	myMgr->debugLog("> ONE_NON_CAN <onEntry>");

}

void ONE_NON_CAN::onExit()
{
	/* -g option specified while compilation. */
	myMgr->debugLog("< ONE_NON_CAN <onExit>");

}

void ONE_NON_CAN::onMessage(const Mesg& mesg)
{
	if(mesg.message == SER)
		onSERMessage(mesg);
	else 
		super::onMessage(mesg);
}

void ONE_NON_CAN::onSERMessage(const Mesg& mesg)
{
	int wParam = mesg.wParam;
	int lParam = mesg.lParam;
	SenderY& ctx = getMgr()->getCtx();

		/* -g option specified while compilation. */
		myMgr->debugLog("ONE_NON_CAN SER <message trapped>");

	if(c == 'C' && !ctx.bytesRd && !ctx.KbCan)
	{
		/* -g option specified while compilation. */
		myMgr->debugLog("ONE_NON_CAN SER <executing exit>");

		const BaseState* root = getMgr()->executeExit("ONE_NON_CAN", "EOT1_NON_CAN");
		/* -g option specified while compilation. */
		myMgr->debugLog("ONE_NON_CAN SER <executing effect>");


		//User specified effect begin
		ctx.sendByte(EOT);
		ctx.tm(TM_VL); 
		ctx.errCnt=0; 
		ctx.closeTransferredFile();
		//User specified effect end

		/* -g option specified while compilation. */
		myMgr->debugLog("ONE_NON_CAN SER <executing entry>");

		getMgr()->executeEntry(root, "EOT1_NON_CAN");
		return;
	}
	else
	if(c=='C' && ctx.bytesRd && !ctx.KbCan)
	{
		/* -g option specified while compilation. */
		myMgr->debugLog("ONE_NON_CAN SER <executing exit>");

		const BaseState* root = getMgr()->executeExit("ONE_NON_CAN", "ACKNAK_NON_CAN");
		/* -g option specified while compilation. */
		myMgr->debugLog("ONE_NON_CAN SER <executing effect>");


		//User specified effect begin
		ctx.sendBlkPrepNext();
		ctx.tm(TM_VL); ctx.errCnt=0; 
		//User specified effect end

		/* -g option specified while compilation. */
		myMgr->debugLog("ONE_NON_CAN SER <executing entry>");

		getMgr()->executeEntry(root, "ACKNAK_NON_CAN");
		return;
	}
	else
	if(c==NAK && !ctx.KbCan)
	{
		/* -g option specified while compilation. */
		myMgr->debugLog("ONE_NON_CAN SER <executing exit>");

		const BaseState* root = getMgr()->executeExit("ONE_NON_CAN", "ACKNAKSTAT_NON_CAN");
		/* -g option specified while compilation. */
		myMgr->debugLog("ONE_NON_CAN SER <executing effect>");


		//User specified effect begin
		ctx.resendBlk(); ctx.errCnt++;
		ctx.tm(TM_VL); 
		//User specified effect end

		/* -g option specified while compilation. */
		myMgr->debugLog("ONE_NON_CAN SER <executing entry>");

		getMgr()->executeEntry(root, "ACKNAKSTAT_NON_CAN");
		return;
	}

	super::onMessage(mesg);
}

//--------------------------------------------------------------------
EOTEOT_NON_CAN::EOTEOT_NON_CAN(const string& name, BaseState* parent, ySenderSS* mgr)
 : ySenderBaseState(name, parent, mgr)
{
	myHistory = false;
}

void EOTEOT_NON_CAN::onEntry()
{
	/* -g option specified while compilation. */
	myMgr->debugLog("> EOTEOT_NON_CAN <onEntry>");

}

void EOTEOT_NON_CAN::onExit()
{
	/* -g option specified while compilation. */
	myMgr->debugLog("< EOTEOT_NON_CAN <onExit>");

}

void EOTEOT_NON_CAN::onMessage(const Mesg& mesg)
{
	if(mesg.message == SER)
		onSERMessage(mesg);
	else 
		super::onMessage(mesg);
}

void EOTEOT_NON_CAN::onSERMessage(const Mesg& mesg)
{
	int wParam = mesg.wParam;
	int lParam = mesg.lParam;
	SenderY& ctx = getMgr()->getCtx();

		/* -g option specified while compilation. */
		myMgr->debugLog("EOTEOT_NON_CAN SER <message trapped>");

	if(c==NAK && !ctx.KbCan && ctx.errCnt < errB )
	{
		/* -g option specified while compilation. */
		myMgr->debugLog("EOTEOT_NON_CAN SER <executing effect>");


		//User specified effect begin
		ctx.sendByte(EOT);
		ctx.errCnt++;
		ctx.tm(TM_VL); 
		
		//User specified effect end

		return;
	}
	else
	if(c==ACK && !ctx.KbCan)
	{
		/* -g option specified while compilation. */
		myMgr->debugLog("EOTEOT_NON_CAN SER <executing exit>");

		const BaseState* root = getMgr()->executeExit("EOTEOT_NON_CAN", "StatC_NON_CAN");
		/* -g option specified while compilation. */
		myMgr->debugLog("EOTEOT_NON_CAN SER <executing effect>");


		//User specified effect begin
		ctx.result += "Done, ";
		ctx.prepStatBlk(); ctx.tm(TM_VL); 
		//User specified effect end

		/* -g option specified while compilation. */
		myMgr->debugLog("EOTEOT_NON_CAN SER <executing entry>");

		getMgr()->executeEntry(root, "StatC_NON_CAN");
		return;
	}

	super::onMessage(mesg);
}

//--------------------------------------------------------------------
StatC_NON_CAN::StatC_NON_CAN(const string& name, BaseState* parent, ySenderSS* mgr)
 : ySenderBaseState(name, parent, mgr)
{
	myHistory = false;
}

void StatC_NON_CAN::onEntry()
{
	/* -g option specified while compilation. */
	myMgr->debugLog("> StatC_NON_CAN <onEntry>");

}

void StatC_NON_CAN::onExit()
{
	/* -g option specified while compilation. */
	myMgr->debugLog("< StatC_NON_CAN <onExit>");

}

void StatC_NON_CAN::onMessage(const Mesg& mesg)
{
	if(mesg.message == SER)
		onSERMessage(mesg);
	else if(mesg.message == KB_C)
		onKB_CMessage(mesg);
	else 
		super::onMessage(mesg);
}

void StatC_NON_CAN::onSERMessage(const Mesg& mesg)
{
	int wParam = mesg.wParam;
	int lParam = mesg.lParam;
	SenderY& ctx = getMgr()->getCtx();

		/* -g option specified while compilation. */
		myMgr->debugLog("StatC_NON_CAN SER <message trapped>");

	if(c=='C' && ctx.fileName && ctx.transferringFileD == -1)
	{
		/* -g option specified while compilation. */
		myMgr->debugLog("StatC_NON_CAN SER <executing exit>");

		const BaseState* root = getMgr()->executeExit("StatC_NON_CAN", "FinalState");
		/* -g option specified while compilation. */
		myMgr->debugLog("StatC_NON_CAN SER <executing effect>");


		//User specified effect begin
		ctx.cans();
		ctx.result += "OpenError";
		//User specified effect end

		/* -g option specified while compilation. */
		myMgr->debugLog("StatC_NON_CAN SER <executing entry>");

		getMgr()->executeEntry(root, "FinalState");
		return;
	}
	else
	if(c=='C' && ctx.transferringFileD != -1)
	{
		/* -g option specified while compilation. */
		myMgr->debugLog("StatC_NON_CAN SER <executing exit>");

		const BaseState* root = getMgr()->executeExit("StatC_NON_CAN", "ACKNAKSTAT_NON_CAN");
		/* -g option specified while compilation. */
		myMgr->debugLog("StatC_NON_CAN SER <executing effect>");


		//User specified effect begin
		ctx.sendBlkPrepNext(); ctx.errCnt=0; 
		ctx.tm(TM_VL); 
		
		//User specified effect end

		/* -g option specified while compilation. */
		myMgr->debugLog("StatC_NON_CAN SER <executing entry>");

		getMgr()->executeEntry(root, "ACKNAKSTAT_NON_CAN");
		return;
	}

	super::onMessage(mesg);
}

void StatC_NON_CAN::onKB_CMessage(const Mesg& mesg)
{
	int wParam = mesg.wParam;
	int lParam = mesg.lParam;
	SenderY& ctx = getMgr()->getCtx();

		/* -g option specified while compilation. */
		myMgr->debugLog("StatC_NON_CAN KB_C <message trapped>");

	if(true)
	{
		/* -g option specified while compilation. */
		myMgr->debugLog("StatC_NON_CAN KB_C <executing exit>");

		const BaseState* root = getMgr()->executeExit("StatC_NON_CAN", "FinalState");
		/* -g option specified while compilation. */
		myMgr->debugLog("StatC_NON_CAN KB_C <executing effect>");


		//User specified effect begin
		if (ctx.transferringFileD == -1) ctx.result="KbCancelledOpenErr"; 
		else {ctx.closeTransferredFile(); ctx.result="KbCancelledFromStatC";}
		/* need to differentiate beginning from another file WRT cans() */
		//User specified effect end

		/* -g option specified while compilation. */
		myMgr->debugLog("StatC_NON_CAN KB_C <executing entry>");

		getMgr()->executeEntry(root, "FinalState");
		return;
	}

	super::onMessage(mesg);
}

//--------------------------------------------------------------------
ACKNAKSTAT_NON_CAN::ACKNAKSTAT_NON_CAN(const string& name, BaseState* parent, ySenderSS* mgr)
 : ySenderBaseState(name, parent, mgr)
{
	myHistory = false;
}

void ACKNAKSTAT_NON_CAN::onEntry()
{
	/* -g option specified while compilation. */
	myMgr->debugLog("> ACKNAKSTAT_NON_CAN <onEntry>");

}

void ACKNAKSTAT_NON_CAN::onExit()
{
	/* -g option specified while compilation. */
	myMgr->debugLog("< ACKNAKSTAT_NON_CAN <onExit>");

}

void ACKNAKSTAT_NON_CAN::onMessage(const Mesg& mesg)
{
	if(mesg.message == SER)
		onSERMessage(mesg);
	else 
		super::onMessage(mesg);
}

void ACKNAKSTAT_NON_CAN::onSERMessage(const Mesg& mesg)
{
	int wParam = mesg.wParam;
	int lParam = mesg.lParam;
	SenderY& ctx = getMgr()->getCtx();

		/* -g option specified while compilation. */
		myMgr->debugLog("ACKNAKSTAT_NON_CAN SER <message trapped>");

	if(c==ACK && ctx.fileName && !ctx.KbCan)
	{
		/* -g option specified while compilation. */
		myMgr->debugLog("ACKNAKSTAT_NON_CAN SER <executing exit>");

		const BaseState* root = getMgr()->executeExit("ACKNAKSTAT_NON_CAN", "ONE_NON_CAN");
		/* -g option specified while compilation. */
		myMgr->debugLog("ACKNAKSTAT_NON_CAN SER <executing effect>");


		//User specified effect begin
		ctx.firstBlk= true; 
		ctx.tm(TM_VL); 
		
		//User specified effect end

		/* -g option specified while compilation. */
		myMgr->debugLog("ACKNAKSTAT_NON_CAN SER <executing entry>");

		getMgr()->executeEntry(root, "ONE_NON_CAN");
		return;
	}
	else
	if((c==NAK || c=='C') && !ctx.KbCan)
	{
		/* -g option specified while compilation. */
		myMgr->debugLog("ACKNAKSTAT_NON_CAN SER <executing effect>");


		//User specified effect begin
		ctx.resendBlk();
		++ ctx.errCnt;
		ctx.tm(TM_VL); 
		
		//User specified effect end

		return;
	}
	else
	if(c==ACK && !ctx.fileName)
	{
		/* -g option specified while compilation. */
		myMgr->debugLog("ACKNAKSTAT_NON_CAN SER <executing exit>");

		const BaseState* root = getMgr()->executeExit("ACKNAKSTAT_NON_CAN", "FinalState");
		/* -g option specified while compilation. */
		myMgr->debugLog("ACKNAKSTAT_NON_CAN SER <executing effect>");


		//User specified effect begin
		ctx.result+="EndOfSession";
		//User specified effect end

		/* -g option specified while compilation. */
		myMgr->debugLog("ACKNAKSTAT_NON_CAN SER <executing entry>");

		getMgr()->executeEntry(root, "FinalState");
		return;
	}

	super::onMessage(mesg);
}

//--------------------------------------------------------------------
CAN_Sender_TopLevel::CAN_Sender_TopLevel(const string& name, BaseState* parent, ySenderSS* mgr)
 : ySenderBaseState(name, parent, mgr)
{
	myHistory = false;
}

void CAN_Sender_TopLevel::onEntry()
{
	/* -g option specified while compilation. */
	myMgr->debugLog("> CAN_Sender_TopLevel <onEntry>");

}

void CAN_Sender_TopLevel::onExit()
{
	/* -g option specified while compilation. */
	myMgr->debugLog("< CAN_Sender_TopLevel <onExit>");

}

void CAN_Sender_TopLevel::onMessage(const Mesg& mesg)
{
	if(mesg.message == KB_C)
		onKB_CMessage(mesg);
	else if(mesg.message == SER)
		onSERMessage(mesg);
	else if(mesg.message == TM)
		onTMMessage(mesg);
	else 
		super::onMessage(mesg);
}

void CAN_Sender_TopLevel::onKB_CMessage(const Mesg& mesg)
{
	int wParam = mesg.wParam;
	int lParam = mesg.lParam;
	SenderY& ctx = getMgr()->getCtx();

		/* -g option specified while compilation. */
		myMgr->debugLog("CAN_Sender_TopLevel KB_C <message trapped>");

	if(true)
	{
		/* -g option specified while compilation. */
		myMgr->debugLog("CAN_Sender_TopLevel KB_C <executing effect>");


		//User specified effect begin
		ctx.KbCan=true;
		//User specified effect end

		return;
	}

	super::onMessage(mesg);
}

void CAN_Sender_TopLevel::onSERMessage(const Mesg& mesg)
{
	int wParam = mesg.wParam;
	int lParam = mesg.lParam;
	SenderY& ctx = getMgr()->getCtx();

		/* -g option specified while compilation. */
		myMgr->debugLog("CAN_Sender_TopLevel SER <message trapped>");

	if(c != CAN             && !ctx.KbCan)
	{
		/* -g option specified while compilation. */
		myMgr->debugLog("CAN_Sender_TopLevel SER <executing exit>");

		const BaseState* root = getMgr()->executeExit("CAN_Sender_TopLevel", "NON_CAN_Sender_TopLevel");
		/* -g option specified while compilation. */
		myMgr->debugLog("CAN_Sender_TopLevel SER <executing effect>");


		//User specified effect begin
		ctx.tmPop();
		//User specified effect end

		/* -g option specified while compilation. */
		myMgr->debugLog("CAN_Sender_TopLevel SER <executing entry>");

		getMgr()->executeEntry(root, "NON_CAN_Sender_TopLevel");
		return;
	}
	else
	if(c == CAN)
	{
		/* -g option specified while compilation. */
		myMgr->debugLog("CAN_Sender_TopLevel SER <executing exit>");

		const BaseState* root = getMgr()->executeExit("CAN_Sender_TopLevel", "FinalState");
		/* -g option specified while compilation. */
		myMgr->debugLog("CAN_Sender_TopLevel SER <executing effect>");


		//User specified effect begin
		ctx.closeTransferredFile();
		ctx.clearCan();
		ctx.result+="RcvCancelled";
		//User specified effect end

		/* -g option specified while compilation. */
		myMgr->debugLog("CAN_Sender_TopLevel SER <executing entry>");

		getMgr()->executeEntry(root, "FinalState");
		return;
	}

	super::onMessage(mesg);
}

void CAN_Sender_TopLevel::onTMMessage(const Mesg& mesg)
{
	int wParam = mesg.wParam;
	int lParam = mesg.lParam;
	SenderY& ctx = getMgr()->getCtx();

		/* -g option specified while compilation. */
		myMgr->debugLog("CAN_Sender_TopLevel TM <message trapped>");

	if(!ctx.KbCan)
	{
		/* -g option specified while compilation. */
		myMgr->debugLog("CAN_Sender_TopLevel TM <executing exit>");

		const BaseState* root = getMgr()->executeExit("CAN_Sender_TopLevel", "NON_CAN_Sender_TopLevel");
		/* -g option specified while compilation. */
		myMgr->debugLog("CAN_Sender_TopLevel TM <executing effect>");


		//User specified effect begin
		ctx.tmPop();
		//User specified effect end

		/* -g option specified while compilation. */
		myMgr->debugLog("CAN_Sender_TopLevel TM <executing entry>");

		getMgr()->executeEntry(root, "NON_CAN_Sender_TopLevel");
		return;
	}

	super::onMessage(mesg);
}


} /*end namespace*/

//___________________________________vv^^vv___________________________________
