//===========================================================================

#ifndef _PTXWDMCMDSRV_20210304223243864_H_INCLUDED_
#define _PTXWDMCMDSRV_20210304223243864_H_INCLUDED_
//---------------------------------------------------------------------------

#include "PTxWDMCmd.h"
#include "PtDrvWrap.h"
//===========================================================================
namespace PRY8EAlByw {
//---------------------------------------------------------------------------

  // CPTxWDMCmdServiceOperator (PTxWDM Command Operator for Server)

class CPTxWDMCmdServiceOperator : public CPTxWDMCmdOperator
{
protected:
	CRITICAL_SECTION Critical_;
	class critical_lock {
		CRITICAL_SECTION *obj_;
	public:
		critical_lock(CRITICAL_SECTION *obj) : obj_(obj)
		{ EnterCriticalSection(obj_); }
		~critical_lock()
		{ LeaveCriticalSection(obj_); }
	};
	DWORD LastAlive_;
	BOOL Sate_;
	CPtDrvWrapper *Tuner_;
	BOOL Terminated_;
	BOOL StreamingEnabled_;
	SERVER_SETTINGS Settings_;
public:
	CPTxWDMCmdServiceOperator(std::wstring name);
	virtual ~CPTxWDMCmdServiceOperator();
protected:
	// for Server Operations (override)
	BOOL ResTerminate();
	BOOL ResOpenTuner(BOOL Sate, DWORD TunerID);
	BOOL ResCloseTuner();
	BOOL ResGetTunerCount(DWORD &Count);
	BOOL ResSetTunerSleep(BOOL Sleep);
	BOOL ResSetStreamEnable(BOOL Enable);
	BOOL ResIsStreamEnabled(BOOL &Enable);
	BOOL ResSetChannel(BOOL &Tuned, DWORD Freq, DWORD TSID, DWORD Stream);
	BOOL ResSetFreq(DWORD Freq);
	BOOL ResCurFreq(DWORD &Freq);
	BOOL ResGetIdListS(TSIDLIST &TSIDList);
	BOOL ResGetIdS(DWORD &TSID);
	BOOL ResSetIdS(DWORD TSID);
	BOOL ResSetLnbPower(BOOL Power);
	BOOL ResGetCnAgc(DWORD &Cn100, DWORD &CurAgc, DWORD &MaxAgc);
	BOOL ResPurgeStream();
	BOOL ResSetupServer(const SERVER_SETTINGS *Options);
public:
	CPtDrvWrapper *Tuner() { return Tuner_; }
	VOID KeepAlive() { LastAlive_ = GetTickCount(); }
	DWORD LastAlive() { return LastAlive_; }
	BOOL Terminated() { return Terminated_; }
	BOOL StreamingEnabled() { return StreamingEnabled_; }
	int StreamerThreadPriority() { return Settings_.StreamerThreadPriority; }
	DWORD StreamerPacketSize() { return Settings_.StreamerPacketSize; }
	DWORD CtrlPackets() { return Settings_.CtrlPackets; }
	BOOL PipeStreaming() { return Settings_.PipeStreaming; }
	DWORD CurStreamSize();
	BOOL GetStreamData(LPVOID data, DWORD &size);
};

//---------------------------------------------------------------------------
} // End of namespace PRY8EAlByw
//===========================================================================
using namespace PRY8EAlByw ;
//===========================================================================
#endif // _PTXWDMCMDSRV_20210304223243864_H_INCLUDED_
