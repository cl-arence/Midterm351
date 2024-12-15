////////////////////////////////////////////////
// Generated by SmartState C++ Code Generator //
//                 DO NOT EDIT				  //
////////////////////////////////////////////////

#pragma warning(disable: 4786)
#pragma warning(disable: 4290)

//Additional Includes
#include "AtomicCOUT.h"
//#include <iostream>
#include <stdlib.h>


#include "yReceiverSS.h"
#include "ReceiverY.h"

/*Messages
Define user specific messages in a file and
include that file in the additional includes section in 
the model.
-- FOLLOWING MESSAGES ARE USED --
KB_C
SER
TM
CONT
*/

//Additional Declarations
#define c wParam



namespace yReceiver_SS
{
using namespace std;
using namespace smartstate;

//State Mgr
//--------------------------------------------------------------------
yReceiverSS::yReceiverSS(ReceiverY* ctx, bool startMachine/*=true*/)
 : StateMgr("yReceiverSS"),
   myCtx(ctx)
{
	myConcStateList.push_back(new Receiver_TopLevel_yReceiverSS("Receiver_TopLevel_yReceiverSS", 0, this));

	if(startMachine)
		start();
}

ReceiverY& yReceiverSS::getCtx() const
{
	return *myCtx;
}

//Base State
//--------------------------------------------------------------------
yReceiverBaseState::yReceiverBaseState(const string& name, BaseState* parent, yReceiverSS* mgr)
 : BaseState(name, parent, mgr)
{
}

//--------------------------------------------------------------------
Receiver_TopLevel_yReceiverSS::Receiver_TopLevel_yReceiverSS(const string& name, BaseState* parent, yReceiverSS* mgr)
 : yReceiverBaseState(name, parent, mgr)
{
	myHistory = true;
	mySubStates.push_back(new NON_CAN_Receiver_TopLevel("NON_CAN_Receiver_TopLevel", this, mgr));
	mySubStates.push_back(new CAN_Receiver_TopLevel("CAN_Receiver_TopLevel", this, mgr));
	setType(eSuper);
}

void Receiver_TopLevel_yReceiverSS::onEntry()
{
	/* -g option specified while compilation. */
	myMgr->debugLog("> Receiver_TopLevel_yReceiverSS <onEntry>");

	ReceiverY& ctx = getMgr()->getCtx();

	// Code from Model here
	    ctx.sendByte(ctx.NCGbyte); 
	    ctx.closeProb = -1;
	    ctx.errCnt = 0; 
	    ctx.tm(TM_SOH);
	    ctx.KbCan = false;
}

void Receiver_TopLevel_yReceiverSS::onExit()
{
	/* -g option specified while compilation. */
	myMgr->debugLog("< Receiver_TopLevel_yReceiverSS <onExit>");

}

void Receiver_TopLevel_yReceiverSS::onMessage(const Mesg& mesg)
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

void Receiver_TopLevel_yReceiverSS::onKB_CMessage(const Mesg& mesg)
{
	int wParam = mesg.wParam;
	int lParam = mesg.lParam;
	ReceiverY& ctx = getMgr()->getCtx();

		/* -g option specified while compilation. */
		myMgr->debugLog("Receiver_TopLevel_yReceiverSS KB_C <message trapped>");

	if(true)
	{
		/* -g option specified while compilation. */
		myMgr->debugLog("Receiver_TopLevel_yReceiverSS KB_C <executing effect>");


		//User specified effect begin
		ctx.KbCan = true;
		//User specified effect end

		return;
	}

	super::onMessage(mesg);
}

void Receiver_TopLevel_yReceiverSS::onSERMessage(const Mesg& mesg)
{
	int wParam = mesg.wParam;
	int lParam = mesg.lParam;
	ReceiverY& ctx = getMgr()->getCtx();

		/* -g option specified while compilation. */
		myMgr->debugLog("Receiver_TopLevel_yReceiverSS SER <message trapped>");

	if(true)
	{
		/* -g option specified while compilation. */
		myMgr->debugLog("Receiver_TopLevel_yReceiverSS SER <executing exit>");

		const BaseState* root = getMgr()->executeExit("Receiver_TopLevel_yReceiverSS", "FinalState");
		/* -g option specified while compilation. */
		myMgr->debugLog("Receiver_TopLevel_yReceiverSS SER <executing effect>");


		//User specified effect begin
		ctx.purge();  ctx.cans();
		if (ctx.KbCan)
		    ctx.result += "KbCancelled (delayed)";
		else
		    ctx.result += "ExcessiveErrors";
		//User specified effect end

		/* -g option specified while compilation. */
		myMgr->debugLog("Receiver_TopLevel_yReceiverSS SER <executing entry>");

		getMgr()->executeEntry(root, "FinalState");
		return;
	}

	super::onMessage(mesg);
}

void Receiver_TopLevel_yReceiverSS::onTMMessage(const Mesg& mesg)
{
	int wParam = mesg.wParam;
	int lParam = mesg.lParam;
	ReceiverY& ctx = getMgr()->getCtx();

		/* -g option specified while compilation. */
		myMgr->debugLog("Receiver_TopLevel_yReceiverSS TM <message trapped>");

	if(true)
	{
		/* -g option specified while compilation. */
		myMgr->debugLog("Receiver_TopLevel_yReceiverSS TM <executing exit>");

		const BaseState* root = getMgr()->executeExit("Receiver_TopLevel_yReceiverSS", "FinalState");
		/* -g option specified while compilation. */
		myMgr->debugLog("Receiver_TopLevel_yReceiverSS TM <executing effect>");


		//User specified effect begin
		ctx.cans();
		if (ctx.KbCan)
		     ctx.result += "KbCancelled";
		else
		     ctx.result += "ExcessiveErrors";
		//User specified effect end

		/* -g option specified while compilation. */
		myMgr->debugLog("Receiver_TopLevel_yReceiverSS TM <executing entry>");

		getMgr()->executeEntry(root, "FinalState");
		return;
	}

	super::onMessage(mesg);
}

//--------------------------------------------------------------------
NON_CAN_Receiver_TopLevel::NON_CAN_Receiver_TopLevel(const string& name, BaseState* parent, yReceiverSS* mgr)
 : yReceiverBaseState(name, parent, mgr)
{
	myHistory = true;
	mySubStates.push_back(new FirstByteStat_NON_CAN("FirstByteStat_NON_CAN", this, mgr));
	mySubStates.push_back(new DataCancelable_NON_CAN("DataCancelable_NON_CAN", this, mgr));
	mySubStates.push_back(new CondTransientData_NON_CAN("CondTransientData_NON_CAN", this, mgr));
	mySubStates.push_back(new CondTransientCheck_NON_CAN("CondTransientCheck_NON_CAN", this, mgr));
	mySubStates.push_back(new CondTransientOpen_NON_CAN("CondTransientOpen_NON_CAN", this, mgr));
	mySubStates.push_back(new CondTransientEOT_NON_CAN("CondTransientEOT_NON_CAN", this, mgr));
	mySubStates.push_back(new CondlTransientStat_NON_CAN("CondlTransientStat_NON_CAN", this, mgr));
	mySubStates.push_back(new AreWeDone_NON_CAN("AreWeDone_NON_CAN", this, mgr));
	setType(eSuper);
}

void NON_CAN_Receiver_TopLevel::onEntry()
{
	/* -g option specified while compilation. */
	myMgr->debugLog("> NON_CAN_Receiver_TopLevel <onEntry>");

}

void NON_CAN_Receiver_TopLevel::onExit()
{
	/* -g option specified while compilation. */
	myMgr->debugLog("< NON_CAN_Receiver_TopLevel <onExit>");

}

void NON_CAN_Receiver_TopLevel::onMessage(const Mesg& mesg)
{
	if(mesg.message == SER)
		onSERMessage(mesg);
	else if(mesg.message == TM)
		onTMMessage(mesg);
	else 
		super::onMessage(mesg);
}

void NON_CAN_Receiver_TopLevel::onSERMessage(const Mesg& mesg)
{
	int wParam = mesg.wParam;
	int lParam = mesg.lParam;
	ReceiverY& ctx = getMgr()->getCtx();

		/* -g option specified while compilation. */
		myMgr->debugLog("NON_CAN_Receiver_TopLevel SER <message trapped>");

	if(!ctx.KbCan  && c!=CAN)
	{
		/* -g option specified while compilation. */
		myMgr->debugLog("NON_CAN_Receiver_TopLevel SER <executing effect>");


		//User specified effect begin
		ctx.purge();
		//User specified effect end

		return;
	}
	else
	if(c==CAN)
	{
		/* -g option specified while compilation. */
		myMgr->debugLog("NON_CAN_Receiver_TopLevel SER <executing exit>");

		const BaseState* root = getMgr()->executeExit("NON_CAN_Receiver_TopLevel", "CAN_Receiver_TopLevel");
		/* -g option specified while compilation. */
		myMgr->debugLog("NON_CAN_Receiver_TopLevel SER <executing effect>");


		//User specified effect begin
		ctx.tmPush(TM_2CHAR);
		//User specified effect end

		/* -g option specified while compilation. */
		myMgr->debugLog("NON_CAN_Receiver_TopLevel SER <executing entry>");

		getMgr()->executeEntry(root, "CAN_Receiver_TopLevel");
		return;
	}

	super::onMessage(mesg);
}

void NON_CAN_Receiver_TopLevel::onTMMessage(const Mesg& mesg)
{
	int wParam = mesg.wParam;
	int lParam = mesg.lParam;
	ReceiverY& ctx = getMgr()->getCtx();

		/* -g option specified while compilation. */
		myMgr->debugLog("NON_CAN_Receiver_TopLevel TM <message trapped>");

	if(ctx.errCnt < errB && !ctx.KbCan)
	{
		/* -g option specified while compilation. */
		myMgr->debugLog("NON_CAN_Receiver_TopLevel TM <executing effect>");


		//User specified effect begin
		if (ctx.transferringFileD == -1)
		    ctx.sendByte(ctx.NCGbyte);
		else
		    ctx.sendByte(NAK);
		++ ctx.errCnt;
		ctx.tm(TM_SOH);
		//User specified effect end

		return;
	}

	super::onMessage(mesg);
}

//--------------------------------------------------------------------
DataCancelable_NON_CAN::DataCancelable_NON_CAN(const string& name, BaseState* parent, yReceiverSS* mgr)
 : yReceiverBaseState(name, parent, mgr)
{
	myHistory = false;
	mySubStates.push_back(new FirstByteData_DataCancelable("FirstByteData_DataCancelable", this, mgr));
	mySubStates.push_back(new EOT_DataCancelable("EOT_DataCancelable", this, mgr));
	setType(eSuper);
}

void DataCancelable_NON_CAN::onEntry()
{
	/* -g option specified while compilation. */
	myMgr->debugLog("> DataCancelable_NON_CAN <onEntry>");

}

void DataCancelable_NON_CAN::onExit()
{
	/* -g option specified while compilation. */
	myMgr->debugLog("< DataCancelable_NON_CAN <onExit>");

}

void DataCancelable_NON_CAN::onMessage(const Mesg& mesg)
{
	if(mesg.message == SER)
		onSERMessage(mesg);
	else 
		super::onMessage(mesg);
}

void DataCancelable_NON_CAN::onSERMessage(const Mesg& mesg)
{
	int wParam = mesg.wParam;
	int lParam = mesg.lParam;
	ReceiverY& ctx = getMgr()->getCtx();

		/* -g option specified while compilation. */
		myMgr->debugLog("DataCancelable_NON_CAN SER <message trapped>");

	if(!ctx.KbCan && c==SOH)
	{
		/* -g option specified while compilation. */
		myMgr->debugLog("DataCancelable_NON_CAN SER <executing exit>");

		const BaseState* root = getMgr()->executeExit("DataCancelable_NON_CAN", "CondTransientData_NON_CAN");
		/* -g option specified while compilation. */
		myMgr->debugLog("DataCancelable_NON_CAN SER <executing effect>");


		//User specified effect begin
		ctx.getRestBlk();
		if (ctx.goodBlk1st) {
		     ctx.errCnt = 0;
		     ctx.anotherFile=0;
		}
		else ++ctx.errCnt;
		//User specified effect end

		/* -g option specified while compilation. */
		myMgr->debugLog("DataCancelable_NON_CAN SER <executing entry>");

		getMgr()->executeEntry(root, "CondTransientData_NON_CAN");
		return;
	}

	super::onMessage(mesg);
}

//--------------------------------------------------------------------
FirstByteData_DataCancelable::FirstByteData_DataCancelable(const string& name, BaseState* parent, yReceiverSS* mgr)
 : yReceiverBaseState(name, parent, mgr)
{
	myHistory = false;
}

void FirstByteData_DataCancelable::onEntry()
{
	/* -g option specified while compilation. */
	myMgr->debugLog("> FirstByteData_DataCancelable <onEntry>");

}

void FirstByteData_DataCancelable::onExit()
{
	/* -g option specified while compilation. */
	myMgr->debugLog("< FirstByteData_DataCancelable <onExit>");

}

void FirstByteData_DataCancelable::onMessage(const Mesg& mesg)
{
	if(mesg.message == SER)
		onSERMessage(mesg);
	else 
		super::onMessage(mesg);
}

void FirstByteData_DataCancelable::onSERMessage(const Mesg& mesg)
{
	int wParam = mesg.wParam;
	int lParam = mesg.lParam;
	ReceiverY& ctx = getMgr()->getCtx();

		/* -g option specified while compilation. */
		myMgr->debugLog("FirstByteData_DataCancelable SER <message trapped>");

	if(!ctx.KbCan &&       c == EOT)
	{
		/* -g option specified while compilation. */
		myMgr->debugLog("FirstByteData_DataCancelable SER <executing exit>");

		const BaseState* root = getMgr()->executeExit("FirstByteData_DataCancelable", "EOT_DataCancelable");
		/* -g option specified while compilation. */
		myMgr->debugLog("FirstByteData_DataCancelable SER <executing effect>");


		//User specified effect begin
		ctx.purge();  ctx.sendByte(NAK);
		++ctx.errCnt;
		ctx.tm(TM_SOH);
		//User specified effect end

		/* -g option specified while compilation. */
		myMgr->debugLog("FirstByteData_DataCancelable SER <executing entry>");

		getMgr()->executeEntry(root, "EOT_DataCancelable");
		return;
	}

	super::onMessage(mesg);
}

//--------------------------------------------------------------------
EOT_DataCancelable::EOT_DataCancelable(const string& name, BaseState* parent, yReceiverSS* mgr)
 : yReceiverBaseState(name, parent, mgr)
{
	myHistory = false;
}

void EOT_DataCancelable::onEntry()
{
	/* -g option specified while compilation. */
	myMgr->debugLog("> EOT_DataCancelable <onEntry>");

}

void EOT_DataCancelable::onExit()
{
	/* -g option specified while compilation. */
	myMgr->debugLog("< EOT_DataCancelable <onExit>");

}

void EOT_DataCancelable::onMessage(const Mesg& mesg)
{
	if(mesg.message == SER)
		onSERMessage(mesg);
	else 
		super::onMessage(mesg);
}

void EOT_DataCancelable::onSERMessage(const Mesg& mesg)
{
	int wParam = mesg.wParam;
	int lParam = mesg.lParam;
	ReceiverY& ctx = getMgr()->getCtx();

		/* -g option specified while compilation. */
		myMgr->debugLog("EOT_DataCancelable SER <message trapped>");

	if(c==EOT)
	{
		/* -g option specified while compilation. */
		myMgr->debugLog("EOT_DataCancelable SER <executing exit>");

		const BaseState* root = getMgr()->executeExit("EOT_DataCancelable", "CondTransientEOT_NON_CAN");
		/* -g option specified while compilation. */
		myMgr->debugLog("EOT_DataCancelable SER <executing effect>");


		//User specified effect begin
		ctx.closeTransferredFile();
		//User specified effect end

		/* -g option specified while compilation. */
		myMgr->debugLog("EOT_DataCancelable SER <executing entry>");

		getMgr()->executeEntry(root, "CondTransientEOT_NON_CAN");
		return;
	}

	super::onMessage(mesg);
}

//--------------------------------------------------------------------
CondTransientData_NON_CAN::CondTransientData_NON_CAN(const string& name, BaseState* parent, yReceiverSS* mgr)
 : yReceiverBaseState(name, parent, mgr)
{
	myHistory = false;
}

void CondTransientData_NON_CAN::onEntry()
{
	/* -g option specified while compilation. */
	myMgr->debugLog("> CondTransientData_NON_CAN <onEntry>");

	ReceiverY& ctx = getMgr()->getCtx();

	// Code from Model here
	     ctx.tm(0);
}

void CondTransientData_NON_CAN::onExit()
{
	/* -g option specified while compilation. */
	myMgr->debugLog("< CondTransientData_NON_CAN <onExit>");

}

void CondTransientData_NON_CAN::onMessage(const Mesg& mesg)
{
	if(mesg.message == KB_C)
		onKB_CMessage(mesg);
	else if(mesg.message == TM)
		onTMMessage(mesg);
	else if(mesg.message == SER)
		onSERMessage(mesg);
	else 
		super::onMessage(mesg);
}

void CondTransientData_NON_CAN::onKB_CMessage(const Mesg& mesg)
{
	int wParam = mesg.wParam;
	int lParam = mesg.lParam;
	ReceiverY& ctx = getMgr()->getCtx();

		/* -g option specified while compilation. */
		myMgr->debugLog("CondTransientData_NON_CAN KB_C <message trapped>");

	if(true)
	{
		/* -g option specified while compilation. */
		myMgr->debugLog("CondTransientData_NON_CAN KB_C <executing exit>");

		const BaseState* root = getMgr()->executeExit("CondTransientData_NON_CAN", "FinalState");
		/* -g option specified while compilation. */
		myMgr->debugLog("CondTransientData_NON_CAN KB_C <executing effect>");


		//User specified effect begin
		ctx.cans();
		ctx.result += "kbCancelled (immediate)";
		//User specified effect end

		/* -g option specified while compilation. */
		myMgr->debugLog("CondTransientData_NON_CAN KB_C <executing entry>");

		getMgr()->executeEntry(root, "FinalState");
		return;
	}

	super::onMessage(mesg);
}

void CondTransientData_NON_CAN::onTMMessage(const Mesg& mesg)
{
	int wParam = mesg.wParam;
	int lParam = mesg.lParam;
	ReceiverY& ctx = getMgr()->getCtx();

		/* -g option specified while compilation. */
		myMgr->debugLog("CondTransientData_NON_CAN TM <message trapped>");

	if(!ctx.syncLoss && (ctx.errCnt < errB))
	{
		/* -g option specified while compilation. */
		myMgr->debugLog("CondTransientData_NON_CAN TM <executing exit>");

		const BaseState* root = getMgr()->executeExit("CondTransientData_NON_CAN", "DataCancelable_NON_CAN");
		/* -g option specified while compilation. */
		myMgr->debugLog("CondTransientData_NON_CAN TM <executing effect>");


		//User specified effect begin
		if (ctx.goodBlk) { 
		     ctx.sendByte(ACK);
		     if (ctx.anotherFile) ctx.sendByte('C');
		}
		else  ctx.sendByte(NAK);
		if (ctx.goodBlk1st) 
		     ctx.writeChunk();
		ctx.tm(TM_SOH);
		
		//User specified effect end

		/* -g option specified while compilation. */
		myMgr->debugLog("CondTransientData_NON_CAN TM <executing entry>");

		getMgr()->executeEntry(root, "DataCancelable_NON_CAN");
		return;
	}
	else
	if(ctx.syncLoss || ctx.errCnt >= errB)
	{
		/* -g option specified while compilation. */
		myMgr->debugLog("CondTransientData_NON_CAN TM <executing exit>");

		const BaseState* root = getMgr()->executeExit("CondTransientData_NON_CAN", "FinalState");
		/* -g option specified while compilation. */
		myMgr->debugLog("CondTransientData_NON_CAN TM <executing effect>");


		//User specified effect begin
		ctx.cans();
		ctx.closeTransferredFile();
		if (ctx.syncLoss)
		     ctx.result +="LossOfSyncronization";
		else
		     ctx.result += "ExcessiveErrors";
		//User specified effect end

		/* -g option specified while compilation. */
		myMgr->debugLog("CondTransientData_NON_CAN TM <executing entry>");

		getMgr()->executeEntry(root, "FinalState");
		return;
	}

	super::onMessage(mesg);
}

void CondTransientData_NON_CAN::onSERMessage(const Mesg& mesg)
{
	int wParam = mesg.wParam;
	int lParam = mesg.lParam;
	ReceiverY& ctx = getMgr()->getCtx();

		/* -g option specified while compilation. */
		myMgr->debugLog("CondTransientData_NON_CAN SER <message trapped>");

	if(true)
	{
		/* -g option specified while compilation. */
		myMgr->debugLog("CondTransientData_NON_CAN SER <executing effect>");


		//User specified effect begin
		ctx.purge();
		//User specified effect end

		return;
	}

	super::onMessage(mesg);
}

//--------------------------------------------------------------------
FirstByteStat_NON_CAN::FirstByteStat_NON_CAN(const string& name, BaseState* parent, yReceiverSS* mgr)
 : yReceiverBaseState(name, parent, mgr)
{
	myHistory = false;
}

void FirstByteStat_NON_CAN::onEntry()
{
	/* -g option specified while compilation. */
	myMgr->debugLog("> FirstByteStat_NON_CAN <onEntry>");

}

void FirstByteStat_NON_CAN::onExit()
{
	/* -g option specified while compilation. */
	myMgr->debugLog("< FirstByteStat_NON_CAN <onExit>");

}

void FirstByteStat_NON_CAN::onMessage(const Mesg& mesg)
{
	if(mesg.message == SER)
		onSERMessage(mesg);
	else 
		super::onMessage(mesg);
}

void FirstByteStat_NON_CAN::onSERMessage(const Mesg& mesg)
{
	int wParam = mesg.wParam;
	int lParam = mesg.lParam;
	ReceiverY& ctx = getMgr()->getCtx();

		/* -g option specified while compilation. */
		myMgr->debugLog("FirstByteStat_NON_CAN SER <message trapped>");

	if(c==EOT && !ctx.closeProb && ctx.errCnt >= errB)
	{
		/* -g option specified while compilation. */
		myMgr->debugLog("FirstByteStat_NON_CAN SER <executing exit>");

		const BaseState* root = getMgr()->executeExit("FirstByteStat_NON_CAN", "FinalState");
		/* -g option specified while compilation. */
		myMgr->debugLog("FirstByteStat_NON_CAN SER <executing effect>");


		//User specified effect begin
		ctx.cans();
		ctx.result += "ExcessiveEOTs";
		//User specified effect end

		/* -g option specified while compilation. */
		myMgr->debugLog("FirstByteStat_NON_CAN SER <executing entry>");

		getMgr()->executeEntry(root, "FinalState");
		return;
	}
	else
	if(c==EOT && !ctx.closeProb && ctx.errCnt < errB)
	{
		/* -g option specified while compilation. */
		myMgr->debugLog("FirstByteStat_NON_CAN SER <executing effect>");


		//User specified effect begin
		ctx.sendByte(ACK);
		ctx.sendByte(ctx.NCGbyte);
		++ ctx.errCnt;  ctx.tm(TM_SOH);
		//User specified effect end

		return;
	}
	else
	if(!ctx.KbCan && c==SOH)
	{
		/* -g option specified while compilation. */
		myMgr->debugLog("FirstByteStat_NON_CAN SER <executing exit>");

		const BaseState* root = getMgr()->executeExit("FirstByteStat_NON_CAN", "CondlTransientStat_NON_CAN");
		/* -g option specified while compilation. */
		myMgr->debugLog("FirstByteStat_NON_CAN SER <executing effect>");


		//User specified effect begin
		ctx.getRestBlk();
		if (!ctx.closeProb) {
		    ctx.errCnt = 0;
		    ctx.closeProb = -1;
		}
		//User specified effect end

		/* -g option specified while compilation. */
		myMgr->debugLog("FirstByteStat_NON_CAN SER <executing entry>");

		getMgr()->executeEntry(root, "CondlTransientStat_NON_CAN");
		return;
	}

	super::onMessage(mesg);
}

//--------------------------------------------------------------------
CondTransientCheck_NON_CAN::CondTransientCheck_NON_CAN(const string& name, BaseState* parent, yReceiverSS* mgr)
 : yReceiverBaseState(name, parent, mgr)
{
	myHistory = false;
}

void CondTransientCheck_NON_CAN::onEntry()
{
	/* -g option specified while compilation. */
	myMgr->debugLog("> CondTransientCheck_NON_CAN <onEntry>");

	ReceiverY& ctx = getMgr()->getCtx();

	// Code from Model here
	     POST("*",CONT);
}

void CondTransientCheck_NON_CAN::onExit()
{
	/* -g option specified while compilation. */
	myMgr->debugLog("< CondTransientCheck_NON_CAN <onExit>");

}

void CondTransientCheck_NON_CAN::onMessage(const Mesg& mesg)
{
	if(mesg.message == CONT)
		onCONTMessage(mesg);
	else 
		super::onMessage(mesg);
}

void CondTransientCheck_NON_CAN::onCONTMessage(const Mesg& mesg)
{
	int wParam = mesg.wParam;
	int lParam = mesg.lParam;
	ReceiverY& ctx = getMgr()->getCtx();

		/* -g option specified while compilation. */
		myMgr->debugLog("CondTransientCheck_NON_CAN CONT <message trapped>");

	if(!ctx.anotherFile)
	{
		/* -g option specified while compilation. */
		myMgr->debugLog("CondTransientCheck_NON_CAN CONT <executing exit>");

		const BaseState* root = getMgr()->executeExit("CondTransientCheck_NON_CAN", "AreWeDone_NON_CAN");
		/* -g option specified while compilation. */
		myMgr->debugLog("CondTransientCheck_NON_CAN CONT <executing effect>");


		//User specified effect begin
		ctx.sendByte(ACK);
		ctx.tm(TM_SOH); 
		//User specified effect end

		/* -g option specified while compilation. */
		myMgr->debugLog("CondTransientCheck_NON_CAN CONT <executing entry>");

		getMgr()->executeEntry(root, "AreWeDone_NON_CAN");
		return;
	}
	else
	if(ctx.anotherFile)
	{
		/* -g option specified while compilation. */
		myMgr->debugLog("CondTransientCheck_NON_CAN CONT <executing exit>");

		const BaseState* root = getMgr()->executeExit("CondTransientCheck_NON_CAN", "CondTransientOpen_NON_CAN");
		/* -g option specified while compilation. */
		myMgr->debugLog("CondTransientCheck_NON_CAN CONT <executing effect>");


		//User specified effect begin
		ctx.openFileForTransfer();
		
		
		
		//User specified effect end

		/* -g option specified while compilation. */
		myMgr->debugLog("CondTransientCheck_NON_CAN CONT <executing entry>");

		getMgr()->executeEntry(root, "CondTransientOpen_NON_CAN");
		return;
	}

	super::onMessage(mesg);
}

//--------------------------------------------------------------------
CondTransientOpen_NON_CAN::CondTransientOpen_NON_CAN(const string& name, BaseState* parent, yReceiverSS* mgr)
 : yReceiverBaseState(name, parent, mgr)
{
	myHistory = false;
}

void CondTransientOpen_NON_CAN::onEntry()
{
	/* -g option specified while compilation. */
	myMgr->debugLog("> CondTransientOpen_NON_CAN <onEntry>");

	ReceiverY& ctx = getMgr()->getCtx();

	// Code from Model here
	     POST("*",CONT);
}

void CondTransientOpen_NON_CAN::onExit()
{
	/* -g option specified while compilation. */
	myMgr->debugLog("< CondTransientOpen_NON_CAN <onExit>");

}

void CondTransientOpen_NON_CAN::onMessage(const Mesg& mesg)
{
	if(mesg.message == CONT)
		onCONTMessage(mesg);
	else 
		super::onMessage(mesg);
}

void CondTransientOpen_NON_CAN::onCONTMessage(const Mesg& mesg)
{
	int wParam = mesg.wParam;
	int lParam = mesg.lParam;
	ReceiverY& ctx = getMgr()->getCtx();

		/* -g option specified while compilation. */
		myMgr->debugLog("CondTransientOpen_NON_CAN CONT <message trapped>");

	if(ctx.transferringFileD != -1)
	{
		/* -g option specified while compilation. */
		myMgr->debugLog("CondTransientOpen_NON_CAN CONT <executing exit>");

		const BaseState* root = getMgr()->executeExit("CondTransientOpen_NON_CAN", "FirstByteData_DataCancelable");
		/* -g option specified while compilation. */
		myMgr->debugLog("CondTransientOpen_NON_CAN CONT <executing effect>");


		//User specified effect begin
		ctx.sendByte(ACK);
		ctx.sendByte(
		      ctx.NCGbyte);
		ctx.tm(TM_SOH);
		
		//User specified effect end

		/* -g option specified while compilation. */
		myMgr->debugLog("CondTransientOpen_NON_CAN CONT <executing entry>");

		getMgr()->executeEntry(root, "FirstByteData_DataCancelable");
		return;
	}
	else
	if(ctx.transferringFileD == -1)
	{
		/* -g option specified while compilation. */
		myMgr->debugLog("CondTransientOpen_NON_CAN CONT <executing exit>");

		const BaseState* root = getMgr()->executeExit("CondTransientOpen_NON_CAN", "FinalState");
		/* -g option specified while compilation. */
		myMgr->debugLog("CondTransientOpen_NON_CAN CONT <executing effect>");


		//User specified effect begin
		ctx.cans();
		ctx.result += "CreatError";
		//User specified effect end

		/* -g option specified while compilation. */
		myMgr->debugLog("CondTransientOpen_NON_CAN CONT <executing entry>");

		getMgr()->executeEntry(root, "FinalState");
		return;
	}

	super::onMessage(mesg);
}

//--------------------------------------------------------------------
CondTransientEOT_NON_CAN::CondTransientEOT_NON_CAN(const string& name, BaseState* parent, yReceiverSS* mgr)
 : yReceiverBaseState(name, parent, mgr)
{
	myHistory = false;
}

void CondTransientEOT_NON_CAN::onEntry()
{
	/* -g option specified while compilation. */
	myMgr->debugLog("> CondTransientEOT_NON_CAN <onEntry>");

	ReceiverY& ctx = getMgr()->getCtx();

	// Code from Model here
	     POST("*",CONT);
}

void CondTransientEOT_NON_CAN::onExit()
{
	/* -g option specified while compilation. */
	myMgr->debugLog("< CondTransientEOT_NON_CAN <onExit>");

}

void CondTransientEOT_NON_CAN::onMessage(const Mesg& mesg)
{
	if(mesg.message == CONT)
		onCONTMessage(mesg);
	else 
		super::onMessage(mesg);
}

void CondTransientEOT_NON_CAN::onCONTMessage(const Mesg& mesg)
{
	int wParam = mesg.wParam;
	int lParam = mesg.lParam;
	ReceiverY& ctx = getMgr()->getCtx();

		/* -g option specified while compilation. */
		myMgr->debugLog("CondTransientEOT_NON_CAN CONT <message trapped>");

	if(!ctx.closeProb)
	{
		/* -g option specified while compilation. */
		myMgr->debugLog("CondTransientEOT_NON_CAN CONT <executing exit>");

		const BaseState* root = getMgr()->executeExit("CondTransientEOT_NON_CAN", "FirstByteStat_NON_CAN");
		/* -g option specified while compilation. */
		myMgr->debugLog("CondTransientEOT_NON_CAN CONT <executing effect>");


		//User specified effect begin
		ctx.sendByte(ACK);
		ctx.sendByte(ctx.NCGbyte);
		ctx.result += "Done, ";
		ctx.errCnt = 0;
		ctx.tm(TM_SOH);
		//User specified effect end

		/* -g option specified while compilation. */
		myMgr->debugLog("CondTransientEOT_NON_CAN CONT <executing entry>");

		getMgr()->executeEntry(root, "FirstByteStat_NON_CAN");
		return;
	}
	else
	if(ctx.closeProb)
	{
		/* -g option specified while compilation. */
		myMgr->debugLog("CondTransientEOT_NON_CAN CONT <executing exit>");

		const BaseState* root = getMgr()->executeExit("CondTransientEOT_NON_CAN", "FinalState");
		/* -g option specified while compilation. */
		myMgr->debugLog("CondTransientEOT_NON_CAN CONT <executing effect>");


		//User specified effect begin
		ctx.cans(); 
		ctx.result += "CloseError";
		//User specified effect end

		/* -g option specified while compilation. */
		myMgr->debugLog("CondTransientEOT_NON_CAN CONT <executing entry>");

		getMgr()->executeEntry(root, "FinalState");
		return;
	}

	super::onMessage(mesg);
}

//--------------------------------------------------------------------
CondlTransientStat_NON_CAN::CondlTransientStat_NON_CAN(const string& name, BaseState* parent, yReceiverSS* mgr)
 : yReceiverBaseState(name, parent, mgr)
{
	myHistory = false;
}

void CondlTransientStat_NON_CAN::onEntry()
{
	/* -g option specified while compilation. */
	myMgr->debugLog("> CondlTransientStat_NON_CAN <onEntry>");

	ReceiverY& ctx = getMgr()->getCtx();

	// Code from Model here
	     POST("*",CONT);
}

void CondlTransientStat_NON_CAN::onExit()
{
	/* -g option specified while compilation. */
	myMgr->debugLog("< CondlTransientStat_NON_CAN <onExit>");

}

void CondlTransientStat_NON_CAN::onMessage(const Mesg& mesg)
{
	if(mesg.message == CONT)
		onCONTMessage(mesg);
	else 
		super::onMessage(mesg);
}

void CondlTransientStat_NON_CAN::onCONTMessage(const Mesg& mesg)
{
	int wParam = mesg.wParam;
	int lParam = mesg.lParam;
	ReceiverY& ctx = getMgr()->getCtx();

		/* -g option specified while compilation. */
		myMgr->debugLog("CondlTransientStat_NON_CAN CONT <message trapped>");

	if(!ctx.syncLoss && (ctx.errCnt < errB)  && !ctx.goodBlk)
	{
		/* -g option specified while compilation. */
		myMgr->debugLog("CondlTransientStat_NON_CAN CONT <executing exit>");

		const BaseState* root = getMgr()->executeExit("CondlTransientStat_NON_CAN", "FirstByteStat_NON_CAN");
		/* -g option specified while compilation. */
		myMgr->debugLog("CondlTransientStat_NON_CAN CONT <executing effect>");


		//User specified effect begin
		ctx.sendByte(NAK);
		++ ctx.errCnt;
		ctx.tm(TM_SOH);
		
		//User specified effect end

		/* -g option specified while compilation. */
		myMgr->debugLog("CondlTransientStat_NON_CAN CONT <executing entry>");

		getMgr()->executeEntry(root, "FirstByteStat_NON_CAN");
		return;
	}
	else
	if(!ctx.syncLoss && (ctx.errCnt < errB) && ctx.goodBlk)
	{
		/* -g option specified while compilation. */
		myMgr->debugLog("CondlTransientStat_NON_CAN CONT <executing exit>");

		const BaseState* root = getMgr()->executeExit("CondlTransientStat_NON_CAN", "CondTransientCheck_NON_CAN");
		/* -g option specified while compilation. */
		myMgr->debugLog("CondlTransientStat_NON_CAN CONT <executing effect>");


		//User specified effect begin
		ctx.checkForAnotherFile();
		//User specified effect end

		/* -g option specified while compilation. */
		myMgr->debugLog("CondlTransientStat_NON_CAN CONT <executing entry>");

		getMgr()->executeEntry(root, "CondTransientCheck_NON_CAN");
		return;
	}
	else
	if(ctx.syncLoss || ctx.errCnt >= errB)
	{
		/* -g option specified while compilation. */
		myMgr->debugLog("CondlTransientStat_NON_CAN CONT <executing exit>");

		const BaseState* root = getMgr()->executeExit("CondlTransientStat_NON_CAN", "FinalState");
		/* -g option specified while compilation. */
		myMgr->debugLog("CondlTransientStat_NON_CAN CONT <executing effect>");


		//User specified effect begin
		ctx.cans();
		if (ctx.syncLoss)
		     ctx.result += "LossOfSync at Stat Blk";
		else
		     ctx.result += "ExcessiveErrors at Stat";
		//User specified effect end

		/* -g option specified while compilation. */
		myMgr->debugLog("CondlTransientStat_NON_CAN CONT <executing entry>");

		getMgr()->executeEntry(root, "FinalState");
		return;
	}

	super::onMessage(mesg);
}

//--------------------------------------------------------------------
AreWeDone_NON_CAN::AreWeDone_NON_CAN(const string& name, BaseState* parent, yReceiverSS* mgr)
 : yReceiverBaseState(name, parent, mgr)
{
	myHistory = false;
}

void AreWeDone_NON_CAN::onEntry()
{
	/* -g option specified while compilation. */
	myMgr->debugLog("> AreWeDone_NON_CAN <onEntry>");

}

void AreWeDone_NON_CAN::onExit()
{
	/* -g option specified while compilation. */
	myMgr->debugLog("< AreWeDone_NON_CAN <onExit>");

}

void AreWeDone_NON_CAN::onMessage(const Mesg& mesg)
{
	if(mesg.message == TM)
		onTMMessage(mesg);
	else if(mesg.message == SER)
		onSERMessage(mesg);
	else 
		super::onMessage(mesg);
}

void AreWeDone_NON_CAN::onTMMessage(const Mesg& mesg)
{
	int wParam = mesg.wParam;
	int lParam = mesg.lParam;
	ReceiverY& ctx = getMgr()->getCtx();

		/* -g option specified while compilation. */
		myMgr->debugLog("AreWeDone_NON_CAN TM <message trapped>");

	if(true)
	{
		/* -g option specified while compilation. */
		myMgr->debugLog("AreWeDone_NON_CAN TM <executing exit>");

		const BaseState* root = getMgr()->executeExit("AreWeDone_NON_CAN", "FinalState");
		/* -g option specified while compilation. */
		myMgr->debugLog("AreWeDone_NON_CAN TM <executing effect>");


		//User specified effect begin
		ctx.result += "EndOfSession";
		
		//User specified effect end

		/* -g option specified while compilation. */
		myMgr->debugLog("AreWeDone_NON_CAN TM <executing entry>");

		getMgr()->executeEntry(root, "FinalState");
		return;
	}

	super::onMessage(mesg);
}

void AreWeDone_NON_CAN::onSERMessage(const Mesg& mesg)
{
	int wParam = mesg.wParam;
	int lParam = mesg.lParam;
	ReceiverY& ctx = getMgr()->getCtx();

		/* -g option specified while compilation. */
		myMgr->debugLog("AreWeDone_NON_CAN SER <message trapped>");

	if(!ctx.KbCan && c==SOH)
	{
		/* -g option specified while compilation. */
		myMgr->debugLog("AreWeDone_NON_CAN SER <executing exit>");

		const BaseState* root = getMgr()->executeExit("AreWeDone_NON_CAN", "CondlTransientStat_NON_CAN");
		/* -g option specified while compilation. */
		myMgr->debugLog("AreWeDone_NON_CAN SER <executing effect>");


		//User specified effect begin
		ctx.getRestBlk();
		++ ctx.errCnt;
		//User specified effect end

		/* -g option specified while compilation. */
		myMgr->debugLog("AreWeDone_NON_CAN SER <executing entry>");

		getMgr()->executeEntry(root, "CondlTransientStat_NON_CAN");
		return;
	}

	super::onMessage(mesg);
}

//--------------------------------------------------------------------
CAN_Receiver_TopLevel::CAN_Receiver_TopLevel(const string& name, BaseState* parent, yReceiverSS* mgr)
 : yReceiverBaseState(name, parent, mgr)
{
	myHistory = false;
}

void CAN_Receiver_TopLevel::onEntry()
{
	/* -g option specified while compilation. */
	myMgr->debugLog("> CAN_Receiver_TopLevel <onEntry>");

}

void CAN_Receiver_TopLevel::onExit()
{
	/* -g option specified while compilation. */
	myMgr->debugLog("< CAN_Receiver_TopLevel <onExit>");

}

void CAN_Receiver_TopLevel::onMessage(const Mesg& mesg)
{
	if(mesg.message == SER)
		onSERMessage(mesg);
	else if(mesg.message == TM)
		onTMMessage(mesg);
	else 
		super::onMessage(mesg);
}

void CAN_Receiver_TopLevel::onSERMessage(const Mesg& mesg)
{
	int wParam = mesg.wParam;
	int lParam = mesg.lParam;
	ReceiverY& ctx = getMgr()->getCtx();

		/* -g option specified while compilation. */
		myMgr->debugLog("CAN_Receiver_TopLevel SER <message trapped>");

	if(c != CAN && !ctx.KbCan)
	{
		/* -g option specified while compilation. */
		myMgr->debugLog("CAN_Receiver_TopLevel SER <executing exit>");

		const BaseState* root = getMgr()->executeExit("CAN_Receiver_TopLevel", "NON_CAN_Receiver_TopLevel");
		/* -g option specified while compilation. */
		myMgr->debugLog("CAN_Receiver_TopLevel SER <executing effect>");


		//User specified effect begin
		ctx.purge();
		ctx.tmPop();
		//User specified effect end

		/* -g option specified while compilation. */
		myMgr->debugLog("CAN_Receiver_TopLevel SER <executing entry>");

		getMgr()->executeEntry(root, "NON_CAN_Receiver_TopLevel");
		return;
	}
	else
	if(c == CAN)
	{
		/* -g option specified while compilation. */
		myMgr->debugLog("CAN_Receiver_TopLevel SER <executing exit>");

		const BaseState* root = getMgr()->executeExit("CAN_Receiver_TopLevel", "FinalState");
		/* -g option specified while compilation. */
		myMgr->debugLog("CAN_Receiver_TopLevel SER <executing effect>");


		//User specified effect begin
		ctx.closeTransferredFile();
		ctx.clearCan();
		ctx.result += "SndCancelled";
		
		//User specified effect end

		/* -g option specified while compilation. */
		myMgr->debugLog("CAN_Receiver_TopLevel SER <executing entry>");

		getMgr()->executeEntry(root, "FinalState");
		return;
	}

	super::onMessage(mesg);
}

void CAN_Receiver_TopLevel::onTMMessage(const Mesg& mesg)
{
	int wParam = mesg.wParam;
	int lParam = mesg.lParam;
	ReceiverY& ctx = getMgr()->getCtx();

		/* -g option specified while compilation. */
		myMgr->debugLog("CAN_Receiver_TopLevel TM <message trapped>");

	if(!ctx.KbCan)
	{
		/* -g option specified while compilation. */
		myMgr->debugLog("CAN_Receiver_TopLevel TM <executing exit>");

		const BaseState* root = getMgr()->executeExit("CAN_Receiver_TopLevel", "NON_CAN_Receiver_TopLevel");
		/* -g option specified while compilation. */
		myMgr->debugLog("CAN_Receiver_TopLevel TM <executing effect>");


		//User specified effect begin
		ctx.tmPop();
		//User specified effect end

		/* -g option specified while compilation. */
		myMgr->debugLog("CAN_Receiver_TopLevel TM <executing entry>");

		getMgr()->executeEntry(root, "NON_CAN_Receiver_TopLevel");
		return;
	}

	super::onMessage(mesg);
}


} /*end namespace*/

//___________________________________vv^^vv___________________________________
