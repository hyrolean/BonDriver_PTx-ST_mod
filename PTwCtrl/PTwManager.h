#pragma once

#include "inc/EarthPtIf.h"
#include "../Common/Util.h"
#include "../Common/PTManager.h"

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

#include "DataIO.h"

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

	BOOL SetFreq(int iID, unsigned long ulCh);
	BOOL GetIdListS(int iID, PTTSIDLIST* pPtTSIDList);
	BOOL GetIdS(int iID, DWORD *pdwTSID);
	BOOL SetIdS(int iID, DWORD dwTSID);

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
		_DEV_STATUS(BOOL bMemStreaming=FALSE) : cDataIO(bMemStreaming) {
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
#include "PtDrvWrap.h"
#include "../Common/PTOutsideCtrlCmdDef.h"
#include "../Common/StringUtil.h"
#include "../Common/PipeServer.h"
#include "../Common/SharedMem.h"
#include "PTxWDMCmdSrv.h"

#define AuxiliaryMaxWait	5000
#define AuxiliaryMaxAlive	30000

class CPTxWDMCtrlAuxiliary
{
protected:
	CPTxWDMCmdServiceOperator	Op;
	CSharedTransportStreamer	*St;
	CPipeServer					*Ps;
	BYTE *PsBuff;
	DWORD CmdWait, Timeout;
	HANDLE Thread;
	BOOL ThTerm;

private: // MemStreaming
	BOOL TxWriteDone;
	static BOOL __stdcall TxDirectWriteProc(LPVOID dst, DWORD &sz, PVOID arg) {
		auto this_ = static_cast<CPTxWDMCtrlAuxiliary*>(arg) ;
		return (this_->TxWriteDone = this_->Op.GetStreamData(dst, sz)) ;
	}

	int MemStreamingThreadProcMain() {
		const DWORD szp = Op.StreamerPacketSize() ;
		bool retry=false;
		while(!ThTerm) {
			if(!retry) {
				if(Op.CurStreamSize()<szp) {HRSleep(10);continue;}
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

	static unsigned int __stdcall MemStreamingThreadProc (PVOID pv) {
		auto this_ = static_cast<CPTxWDMCtrlAuxiliary*>(pv) ;
		unsigned int result = this_->MemStreamingThreadProcMain() ;
		_endthreadex(result) ;
		return result;
	}

private: // PipeStreaming
	void PipeCmdSendData(CMD_STREAM* pCmdParam, CMD_STREAM* pResParam,
			BOOL* pbResDataAbandon) {
		const DWORD szp = Op.StreamerPacketSize() ;
		BOOL bSend = FALSE; int cnt=0;
		if(Op.CurStreamSize()>=szp) {
			DWORD sz=szp;
			if(Op.GetStreamData(PsBuff, sz) && sz>0) {
				pResParam->dwSize = sz;
				pResParam->bData = PsBuff;
				*pbResDataAbandon = TRUE;
				if(!(cnt++&1023)) Op.KeepAlive();
				bSend = TRUE ;
			}
		}
		pResParam->dwParam = bSend? CMD_SUCCESS: CMD_ERR_BUSY;
	}

	static int CALLBACK PipeCmdCallback(void* pParam, CMD_STREAM* pCmdParam,
			CMD_STREAM* pResParam, BOOL* pbResDataAbandon) {
		auto pSys = static_cast<CPTxWDMCtrlAuxiliary*>(pParam);
		switch( pCmdParam->dwParam ){
			case CMD_SEND_DATA:
				pSys->PipeCmdSendData(pCmdParam, pResParam, pbResDataAbandon);
				break;
			default:
				pResParam->dwParam = CMD_NON_SUPPORT;
				break;
		}
		return 0;
	}

protected:
	void StartStreaming() {
		if(!Op.PipeStreaming()&&Thread==INVALID_HANDLE_VALUE) { // MemStreaming
			Thread = (HANDLE)_beginthreadex(NULL, 0, MemStreamingThreadProc, this,
				CREATE_SUSPENDED, NULL) ;
			if(Thread != INVALID_HANDLE_VALUE) {
				SAFE_DELETE(St);
				St = new CSharedTransportStreamer(
					Op.Name()+SHAREDMEM_TRANSPORT_STREAM_SUFFIX, FALSE,
					Op.StreamerPacketSize(), Op.CtrlPackets() );
				DBGOUT("AUX MemStreamer memName: %s\n", wcs2mbcs(St->Name()).c_str());
				DBGOUT("-- Start MemStreaming --\n");
				::SetThreadPriority( Thread, Op.StreamerThreadPriority() );
				ThTerm=FALSE;
				::ResumeThread(Thread) ;
			}else {
				DBGOUT("*** MemStreaming thread creation failed. ***\n");
			}
		}else if(Ps==NULL) { // PipeStreaming
			int iPtVer=PT_VER, iID=-1;
			swscanf_s(Op.Name().c_str(), SHAREDMEM_TRANSPORT_FORMAT, &iPtVer, &iID);
			Ps = new CPipeServer();
			if(Ps) {
				wstring strPipe = L"";
				wstring strEvent = L"";
				Format(strPipe, L"%s%d", CMD_PT_DATA_PIPE, iID );
				Format(strEvent, L"%s%d", CMD_PT_DATA_EVENT_WAIT_CONNECT, iID );
				SAFE_DELETE_ARRAY(PsBuff);
				PsBuff = new BYTE[Op.StreamerPacketSize()] ;
				if(Ps->StartServer(strEvent.c_str(), strPipe.c_str(), PipeCmdCallback,
						this, Op.StreamerThreadPriority()))
					DBGOUT("-- Start PipeStreaming --\n");
				else
					DBGOUT("*** PipeStreaming failed to StartServer. ***\n");
			}
		}
	}

	void StopStreaming() {
		if(!Op.PipeStreaming()&&Thread!=INVALID_HANDLE_VALUE) { // MemStreaming
			ThTerm=TRUE;
			if(::HRWaitForSingleObject(Thread,Timeout*2) != WAIT_OBJECT_0) {
				::TerminateThread(Thread, 0);
			}
			CloseHandle(Thread);
			Thread = INVALID_HANDLE_VALUE ;
			DBGOUT("-- Stop MemStreaming --\n");
		}else if(Ps!=NULL) { // PipeStreaming
			Ps->StopServer();
			DBGOUT("-- Stop PipeStreaming --\n");
		}
		SAFE_DELETE(St);
		SAFE_DELETE(Ps);
		SAFE_DELETE_ARRAY(PsBuff) ;
	}

public:
	CPTxWDMCtrlAuxiliary(wstring name, DWORD cmdwait=INFINITE, DWORD timeout=INFINITE)
	 :	Op( name ), St( NULL ), Ps(NULL), Thread(INVALID_HANDLE_VALUE), ThTerm(TRUE)
	{ CmdWait = cmdwait ; Timeout = timeout ; PsBuff = NULL ; }
	~CPTxWDMCtrlAuxiliary() { StopStreaming(); }

	int MainLoop() {
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

	//PTSTREAMING GetStreamingMethod() {return PTSTREAMING_SHAREDMEM;}

	DWORD GetTotalTunerCount() { return DWORD(m_EnumDev.size()<<1) ; }
	DWORD GetActiveTunerCount(BOOL bSate);
	BOOL SetLnbPower(int iID, BOOL bEnabled);

	int OpenTuner(BOOL bSate);
	int OpenTuner2(BOOL bSate, int iTunerID);
	BOOL CloseTuner(int iID);

	BOOL SetCh(int iID, unsigned long ulCh, DWORD dwTSID, BOOL &hasStream);
	DWORD GetSignal(int iID);

	BOOL SetFreq(int iID, unsigned long ulCh);
	BOOL GetIdListS(int iID, PTTSIDLIST* pPtTSIDList);
	BOOL GetIdS(int iID, DWORD *pdwTSID);
	BOOL SetIdS(int iID, DWORD dwTSID);

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

