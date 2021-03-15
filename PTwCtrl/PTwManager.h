#pragma once

#include "inc/EarthPtIf.h"
#include "../Common/Util.h"
#include "../Common/PTManager.h"
#include "DataIO.h"

namespace EARTH {
	namespace OS {
		class Library {
			static	status NewBusFunction_(PT::Bus **lplpBus) {
				*lplpBus = new PT::Bus ;
				if(*lplpBus==nullptr)
					return PT::STATUS_OUT_OF_MEMORY_ERROR;
				return PT::STATUS_OK;
			}
		public:
			PT::Bus::NewBusFunction Function() const {
				return &NewBusFunction_;
			}
		};
	}
}

using namespace EARTH;

#ifndef PTW_GALAPAGOS // NOT Galapagosization ( EARTH defacto standard )

class CPTwManager : public IPTManager
{
public:
	CPTwManager(void);

	BOOL LoadSDK();
	BOOL Init();
	void UnInit();

	DWORD GetTotalTunerCount() { return DWORD(m_EnumDev.size()<<1) ; }
	DWORD GetActiveTunerCount(BOOL bSate);
	BOOL SetLnbPower(int iID, BOOL bEnabled);

	int OpenTuner(BOOL bSate);
	int OpenTuner2(BOOL bSate, int iTunerID);
	BOOL CloseTuner(int iID);

	BOOL SetCh(int iID, unsigned long ulCh, DWORD dwTSID, BOOL &hasStream);
	DWORD GetSignal(int iID);

	BOOL IsFindOpen();

	BOOL CloseChk();

protected:
	OS::Library* m_cLibrary;
	PT::Bus* m_cBus;

	typedef struct _DEV_STATUS{
		PT::Bus::DeviceInfo stDevInfo;
		PT::Device* pcDevice;
		BOOL bOpen;
		BOOL bUseT0;
		BOOL bUseT1;
		BOOL bUseS0;
		BOOL bUseS1;
		BOOL bLnbS0;
		BOOL bLnbS1;
		CDataIO cDataIO;
		_DEV_STATUS(void){
			bOpen = FALSE;
			pcDevice = NULL;
			bUseT0 = FALSE;
			bUseT1 = FALSE;
			bUseS0 = FALSE;
			bUseS1 = FALSE;
			bLnbS0 = FALSE;
			bLnbS1 = FALSE;
		}
	}DEV_STATUS;
	vector<DEV_STATUS*> m_EnumDev;

protected:
	void FreeSDK();
};

#else // begin of PTW_GALAPAGOS

#include <process.h>
#include "PTxWDMCmdSrv.h"

#define AuxiliaryMaxWait	5000
#define AuxiliaryMaxAlive	30000

class CPTxWDMCtrlAuxiliary
{
protected:
	CPTxWDMCmdServiceOperator	Op;
	CSharedTransportStreamer	*St;
	DWORD CmdWait, Timeout;
	HANDLE Thread;
	BOOL ThTerm;

private:
	BOOL TxWriteDone;
	static BOOL __stdcall TxDirectWriteProc(LPVOID dst, DWORD &sz, PVOID arg) {
		auto this_ = static_cast<CPTxWDMCtrlAuxiliary*>(arg) ;
		return (this_->TxWriteDone = this_->Op.GetStreamData(dst, sz)) ;
	}

	int StreamingThreadProcMain() {
		const DWORD szp = Op.StreamerPacketSize() ;
		bool retry=false;
		while(!ThTerm) {
			if(!retry) {
				if(Op.CurStreamSize()<szp) {Sleep(10);continue;}
			}
			TxWriteDone = FALSE;
			BOOL r = retry ?
				St->TxDirect(NULL, &TxWriteDone, CmdWait):
				St->TxDirect(TxDirectWriteProc, this, CmdWait);
			retry = !r && TxWriteDone ;
		}
		DBGOUT("-- Streaming Done --\n");
		return 0;
	}

	static unsigned int __stdcall StreamingThreadProc (PVOID pv) {
		auto this_ = static_cast<CPTxWDMCtrlAuxiliary*>(pv) ;
		unsigned int result = this_->StreamingThreadProcMain() ;
		_endthreadex(result) ;
		return result;
	}

protected:
	void StartStreaming() {
		if(Thread != INVALID_HANDLE_VALUE) return /*already activated*/;
		Thread = (HANDLE)_beginthreadex(NULL, 0, StreamingThreadProc, this,
			CREATE_SUSPENDED, NULL) ;
		if(Thread != INVALID_HANDLE_VALUE) {
			if(St) delete St;
			St = new CSharedTransportStreamer(
				Op.Name()+SHAREDMEM_TRANSPORT_STREAM_SUFFIX, FALSE,
				Op.StreamerPacketSize(), Op.CtrlPackets() );
			DBGOUT("AUX Streamer memName: %s\n", wcs2mbcs(St->Name()).c_str());
			DBGOUT("-- Start Streaming --\n");
			::SetThreadPriority( Thread, Op.StreamerThreadPriority() );
			ThTerm=FALSE;
			::ResumeThread(Thread) ;
		}else {
			DBGOUT("*** Streaming thread creation failed. ***\n");
		}
	}

	void StopStreaming() {
		if(Thread == INVALID_HANDLE_VALUE) return /*already inactivated*/;
		ThTerm=TRUE;
		if(::WaitForSingleObject(Thread,Timeout*2) != WAIT_OBJECT_0) {
			::TerminateThread(Thread, 0);
		}
		Thread = INVALID_HANDLE_VALUE ;
		if(St) { delete St; St=NULL; }
		DBGOUT("-- Stop Streaming --\n");
	}

public:
	CPTxWDMCtrlAuxiliary(wstring name, DWORD cmdwait=INFINITE, DWORD timeout=INFINITE)
	 :	Op( name ), St( NULL ), Thread(INVALID_HANDLE_VALUE), ThTerm(TRUE)
	{ CmdWait = cmdwait ; Timeout = timeout ; }
	~CPTxWDMCtrlAuxiliary() { StopStreaming(); }

	int MainLoop() {
		auto dur =[](DWORD s=0, DWORD e=GetTickCount()) -> DWORD {
			// duration ( s -> e )
			return s <= e ? e - s : 0xFFFFFFFFUL - s + 1 + e;
		};
		BOOL enable_streaming = FALSE ;
		while(!Op.Terminated()) {
			DWORD wait_res = Op.WaitForCmd(CmdWait);
			if(wait_res==WAIT_OBJECT_0) {
				if(!Op.ServiceReaction(Timeout)) {
					DBGOUT("service reaction failed.\n");
					return 1;
				}
				BOOL streaming_enabled = Op.StreamingEnabled();
				if(enable_streaming != streaming_enabled) {
					enable_streaming = streaming_enabled;
					if(enable_streaming)	StartStreaming();
					else					StopStreaming();
				}
			}else if(wait_res==WAIT_TIMEOUT) {
				if(dur(Op.LastAlive())>=Timeout) {
					if(HANDLE mutex = OpenMutex(MUTEX_ALL_ACCESS,FALSE,Op.Name().c_str())) {
						Op.KeepAlive();
						CloseHandle(mutex);
					}else {
						DBGOUT("service timeout.\n");
						return -1;
					}
				}
			}
		}
		DBGOUT("service terminated.\n");
		return 0;
	}

};

class CPTwManager : public IPTManager
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

public:
	CPTwManager(void);
	virtual ~CPTwManager(void);

	BOOL LoadSDK();
	BOOL Init();
	void UnInit();

	PTSTREAMING GetStreamingMethod() {return PTSTREAMING_SHAREDMEM;}

	DWORD GetTotalTunerCount() { return DWORD(m_EnumDev.size()<<1) ; }
	DWORD GetActiveTunerCount(BOOL bSate);
	BOOL SetLnbPower(int iID, BOOL bEnabled);

	int OpenTuner(BOOL bSate);
	int OpenTuner2(BOOL bSate, int iTunerID);
	BOOL CloseTuner(int iID);

	BOOL SetCh(int iID, unsigned long ulCh, DWORD dwTSID, BOOL &hasStream);
	DWORD GetSignal(int iID);

	BOOL IsFindOpen();

	BOOL CloseChk();

protected:

	BOOL m_bAuxiliaryRedirectAll ;

	struct DEV_STATUS {
		CPTxWDMCmdOperator *cpOperatorT[2];
		HANDLE hProcessT[2];
		CPTxWDMCmdOperator *cpOperatorS[2];
		HANDLE hProcessS[2];
		bool Opened() {
			return cpOperatorT[0] || cpOperatorT[1] ||
				cpOperatorS[0] || cpOperatorS[1] ;
		}
		BOOL bLnbS0;
		BOOL bLnbS1;
		CDataIO cDataIO;
		DEV_STATUS(void){
			for(auto &v: cpOperatorT) v=nullptr;
			for(auto &v: cpOperatorS) v=nullptr;
			for(auto &h: hProcessT) h=NULL;
			for(auto &h: hProcessS) h=NULL;
			bLnbS0 = FALSE;
			bLnbS1 = FALSE;
		}
	};
	vector<DEV_STATUS*> m_EnumDev;
	CPTxWDMCmdOperator *m_cpPrimaryOperator;

	wstring m_strExePath;

protected:
	void FreeSDK();

	HANDLE m_hPrimaryAuxiliaryThread;
	bool StartPrimaryAuxiliary();
	bool StopPrimaryAuxiliary();
	static unsigned int __stdcall PrimaryAuxiliaryProc (PVOID pv) ;
};

#endif // end of PTW_GALAPAGOS

