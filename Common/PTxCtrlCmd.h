//===========================================================================
#pragma once
#ifndef _PTXCTRLCMD_20210331112723813_H_INCLUDED_
#define _PTXCTRLCMD_20210331112723813_H_INCLUDED_
//---------------------------------------------------------------------------

#include "../Common/SharedMem.h"
//===========================================================================
namespace PRY8EAlByw {
//---------------------------------------------------------------------------

#define	PTXCTRLCMDMAXDATA	1
#define	PTXCTRLCMDTIMEOUT	30000

enum PTXCTRLCMD : DWORD {
	PTXCTRLCMD_IDLE,
	PTXCTRLCMD_SUPPORTED,
	PTXCTRLCMD_ACTIVATEPT,
	PTXCTRLCMD_GETTUNERCOUNT,
};

class CPTxCtrlCmdOperator : public CSharedCmdOperator
{
protected:
	bool ServerMode;
	struct OP {
		DWORD id;
		union {
			PTXCTRLCMD cmd;
			BOOL res;
		};
		DWORD data[PTXCTRLCMDMAXDATA];
	};
	DWORD StaticId;
	HANDLE HXferMutex;
	bool XferLock(DWORD timeout=PTXCTRLCMDTIMEOUT) const ;
	bool XferUnlock() const ;
	BOOL Xfer(OP &cmd, OP &res, DWORD timeout=PTXCTRLCMDTIMEOUT);
public:
	CPTxCtrlCmdOperator(std::wstring name, bool server=false);
	virtual ~CPTxCtrlCmdOperator();
public:
	// for Client Operations
	BOOL CmdIdle(DWORD timeout=PTXCTRLCMDTIMEOUT);
	BOOL CmdSupported(DWORD &PtSupportedBits, DWORD timeout=PTXCTRLCMDTIMEOUT);
	BOOL CmdActivatePt(DWORD PtVer, DWORD timeout=PTXCTRLCMDTIMEOUT);
	BOOL CmdGetTunerCount(DWORD PtVer, DWORD &TunerCount, DWORD timeout=PTXCTRLCMDTIMEOUT);
protected:
	// for Server Operations
	virtual BOOL ResIdle()	{ return TRUE ; }
	virtual BOOL ResSupported(DWORD &PtSupportedBits)	{ return FALSE ; }
	virtual BOOL ResActivatePt(DWORD PtVer)	{ return FALSE ; }
	virtual BOOL ResGetTunerCount(DWORD PtVer, DWORD &TunerCount) { return FALSE ; }
public:
	// Service Reaction ( It will be called from the server after Listen() )
	BOOL ServiceReaction(DWORD timeout=PTXCTRLCMDTIMEOUT);
};

//---------------------------------------------------------------------------
} // End of namespace PRY8EAlByw
//===========================================================================
using namespace PRY8EAlByw ;
//===========================================================================
#endif // _PTXCTRLCMD_20210331112723813_H_INCLUDED_
