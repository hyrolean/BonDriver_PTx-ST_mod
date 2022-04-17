//===========================================================================
#include "stdafx.h"

#include "PTxCtrlCmd.h"
#include "../Common/Util.h"
#include "../Common/HRTimer.h"
//---------------------------------------------------------------------------

using namespace std;

//===========================================================================
namespace PRY8EAlByw {
//---------------------------------------------------------------------------
//===========================================================================
// CPTxCtrlCmdOperator
//---------------------------------------------------------------------------
CPTxCtrlCmdOperator::CPTxCtrlCmdOperator(wstring name, bool server)
  : CSharedCmdOperator(name, server, sizeof OP)
{
	ServerMode = server;
	StaticId = 0 ;

	wstring xfer_mutex_name = Name() + L"_PTxCtrlCmdOperator_XferMutex";
	HXferMutex = _CreateMutex(FALSE, xfer_mutex_name.c_str());
}
//---------------------------------------------------------------------------
CPTxCtrlCmdOperator::~CPTxCtrlCmdOperator()
{
	if(HXferMutex)   CloseHandle(HXferMutex);
}
//---------------------------------------------------------------------------
bool CPTxCtrlCmdOperator::XferLock(DWORD timeout) const
{
	if(!HXferMutex) return false ;
	return HRWaitForSingleObject(HXferMutex, timeout) == WAIT_OBJECT_0 ;
}
//---------------------------------------------------------------------------
bool CPTxCtrlCmdOperator::XferUnlock() const
{
	if(!HXferMutex) return false ;
	return ReleaseMutex(HXferMutex) ? true : false ;
}
//---------------------------------------------------------------------------
BOOL CPTxCtrlCmdOperator::Xfer(OP &cmd, OP &res, DWORD timeout)
{
	BOOL r=FALSE;
	if(XferLock(timeout)) {
		do {
			if(ServerMode) break;
			cmd.id = StaticId ;
			if(!Send(&cmd,timeout)) break;
			res.id = StaticId-1;
			res.res = FALSE;
			if(!Listen(timeout)) break;
			if(!Recv(&res,timeout)) break;
			if(res.id!=StaticId) break;
			StaticId++;
			r=TRUE;
		}while(0);
		if(!XferUnlock()) r=FALSE;
	}
	return r;
}
//---------------------------------------------------------------------------
BOOL CPTxCtrlCmdOperator::CmdIdle(DWORD timeout)
{
	OP op;
	op.cmd = PTXCTRLCMD_IDLE ;
	if(!Xfer(op,op,timeout)) return FALSE;
	return op.res ;
}
//---------------------------------------------------------------------------
BOOL CPTxCtrlCmdOperator::CmdSupported(DWORD &PtSupportedBits, DWORD timeout)
{
	OP op;
	op.cmd = PTXCTRLCMD_SUPPORTED ;
	if(!Xfer(op,op,timeout)) return FALSE;
	if(op.res) PtSupportedBits = op.data[0];
	return op.res ;
}
//---------------------------------------------------------------------------
BOOL CPTxCtrlCmdOperator::CmdActivatePt(DWORD PtVer, DWORD timeout)
{
	OP op;
	op.cmd = PTXCTRLCMD_ACTIVATEPT ;
	op.data[0] = PtVer ;
	if(!Xfer(op,op,timeout)) return FALSE;
	return op.res ;
}
//---------------------------------------------------------------------------
BOOL CPTxCtrlCmdOperator::ServiceReaction(DWORD timeout)
{
	if(!ServerMode) return FALSE;
	OP cmd, res;
	if(!Recv(&cmd,timeout)) return FALSE ;
	BOOL result = TRUE ;
	DBGOUT("service reaction: cmd=%d\n",cmd.cmd);
	switch(cmd.cmd) {
	case PTXCTRLCMD_IDLE:
		res.res = ResIdle();
		break;
	case PTXCTRLCMD_SUPPORTED:
		res.res = ResSupported(res.data[0]);
		break;
	case PTXCTRLCMD_ACTIVATEPT:
		res.res = ResActivatePt(cmd.data[0]);
		break;
	default:
		result = res.res = FALSE ;
	}
	res.id = cmd.id ;
	if(!Send(&res,timeout)) result = FALSE ;
	return result ;
}
//---------------------------------------------------------------------------
} // End of namespace PRY8EAlByw
//===========================================================================
