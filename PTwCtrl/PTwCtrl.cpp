// PTwCtrl.cpp : アプリケーションのエントリ ポイントを定義します。
//

#include "stdafx.h"
#include "PTwCtrl.h"
#include "PTwManager.h"
#include "../Common/PTCtrlMain.h"
#include "../Common/ServiceUtil.h"

CPTCtrlMain g_cMain(PT2_GLOBAL_LOCK_MUTEX);
HANDLE g_hMutex;
SERVICE_STATUS_HANDLE g_hStatusHandle;

#define PT1_CTRL_MUTEX L"PT2_CTRL_EXE_MUTEX"
#define SERVICE_NAME L"PT2Ctrl Service"

#ifdef PTW_GALAPAGOS // begin of PTW_GALAPAGOS

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
				St->TxDirect(NULL, &TxWriteDone, Timeout):
				St->TxDirect(TxDirectWriteProc, this, Timeout);
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

#endif // end of PTW_GALAPAGOS


int APIENTRY _tWinMain(HINSTANCE hInstance,
                     HINSTANCE hPrevInstance,
                     LPTSTR    lpCmdLine,
                     int       nCmdShow)
{
	if (_tcslen(lpCmdLine) > 0) {
		if (lpCmdLine[0] == '-' || lpCmdLine[0] == '/') {
			if (_tcsicmp(_T("install"), lpCmdLine + 1) == 0) {
				WCHAR strExePath[512] = L"";
				GetModuleFileName(NULL, strExePath, 512);
				InstallService(strExePath, SERVICE_NAME, SERVICE_NAME);
				return 0;
			}
			else if (_tcsicmp(_T("remove"), lpCmdLine + 1) == 0) {
				RemoveService(SERVICE_NAME);
				return 0;
			}
		}
#ifdef PTW_GALAPAGOS // begin of PTW_GALAPAGOS
		else {
			return CPTxWDMCtrlAuxiliary(lpCmdLine, AuxiliaryMaxWait, AuxiliaryMaxAlive).MainLoop() ;
		}
#endif // end of PTW_GALAPAGOS
	}

	if (IsInstallService(SERVICE_NAME) == FALSE) {
		//普通にexeとして起動を行う
		HANDLE h = ::OpenMutexW(SYNCHRONIZE, FALSE, PT2_GLOBAL_LOCK_MUTEX);
		if (h != NULL) {
			BOOL bErr = FALSE;
			if (::WaitForSingleObject(h, 100) == WAIT_TIMEOUT) {
				bErr = TRUE;
			}
			::ReleaseMutex(h);
			::CloseHandle(h);
			if (bErr) {
				return -1;
			}
		}

		g_hStartEnableEvent = _CreateEvent(TRUE, TRUE, PT2_STARTENABLE_EVENT);
		if (g_hStartEnableEvent == NULL) {
			return -2;
		}
		// 別プロセスが終了処理中の場合は終了を待つ(最大1秒)
		if (::WaitForSingleObject(g_hStartEnableEvent, 1000) == WAIT_TIMEOUT) {
			::CloseHandle(g_hStartEnableEvent);
			return -3;
		}

		g_hMutex = _CreateMutex(TRUE, PT1_CTRL_MUTEX);
		if (g_hMutex == NULL) {
			::CloseHandle(g_hStartEnableEvent);
			return -4;
		}
		if (::WaitForSingleObject(g_hMutex, 100) == WAIT_TIMEOUT) {
			// 別プロセスが実行中だった
			::CloseHandle(g_hMutex);
			::CloseHandle(g_hStartEnableEvent);
			return -5;
		}

		//起動
		StartMain(FALSE);

		::ReleaseMutex(g_hMutex);
		::CloseHandle(g_hMutex);

		::SetEvent(g_hStartEnableEvent);
		::CloseHandle(g_hStartEnableEvent);
	}
	else {
		//サービスとしてインストール済み
		if (IsStopService(SERVICE_NAME) == FALSE) {
			g_hMutex = _CreateMutex(TRUE, PT1_CTRL_MUTEX);
			int err = GetLastError();
			if (g_hMutex != NULL && err != ERROR_ALREADY_EXISTS) {
				//起動
				SERVICE_TABLE_ENTRY dispatchTable[] = {
					{ SERVICE_NAME, (LPSERVICE_MAIN_FUNCTION)service_main },
					{ NULL, NULL }
				};
				if (StartServiceCtrlDispatcher(dispatchTable) == FALSE) {
					OutputDebugString(_T("StartServiceCtrlDispatcher failed"));
				}
			}
		}
		else {
			//Stop状態なので起動する
			StartServiceCtrl(SERVICE_NAME);
		}
	}
	return 0;
}

void WINAPI service_main(DWORD dwArgc, LPTSTR *lpszArgv)
{
	g_hStatusHandle = RegisterServiceCtrlHandlerEx( SERVICE_NAME, (LPHANDLER_FUNCTION_EX)service_ctrl, NULL);

	if (g_hStatusHandle == NULL){
		goto cleanup;
	}

	SendStatusScm(SERVICE_START_PENDING, 0, 1);

	SendStatusScm(SERVICE_RUNNING, 0, 0);
	StartMain(TRUE);

cleanup:
	SendStatusScm(SERVICE_STOPPED, 0, 0);

   return;
}

DWORD WINAPI service_ctrl(DWORD dwControl, DWORD dwEventType, LPVOID lpEventData, LPVOID lpContext)
{
	switch (dwControl){
		case SERVICE_CONTROL_STOP:
		case SERVICE_CONTROL_SHUTDOWN:
			SendStatusScm(SERVICE_STOP_PENDING, 0, 1);
			StopMain();
			return NO_ERROR;
			break;
		case SERVICE_CONTROL_POWEREVENT:
			OutputDebugString(_T("SERVICE_CONTROL_POWEREVENT"));
			if ( dwEventType == PBT_APMQUERYSUSPEND ){
				OutputDebugString(_T("PBT_APMQUERYSUSPEND"));
				if( g_cMain.IsFindOpen() ){
					OutputDebugString(_T("BROADCAST_QUERY_DENY"));
					return BROADCAST_QUERY_DENY;
					}
			}else if( dwEventType == PBT_APMRESUMESUSPEND ){
				OutputDebugString(_T("PBT_APMRESUMESUSPEND"));
			}
			break;
		default:
			break;
	}
	SendStatusScm(NO_ERROR, 0, 0);
	return NO_ERROR;
}

BOOL SendStatusScm(int iState, int iExitcode, int iProgress)
{
	SERVICE_STATUS ss;

	ss.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
	ss.dwCurrentState = iState;
	ss.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN | SERVICE_ACCEPT_POWEREVENT;
	ss.dwWin32ExitCode = iExitcode;
	ss.dwServiceSpecificExitCode = 0;
	ss.dwCheckPoint = iProgress;
	ss.dwWaitHint = 10000;

	return SetServiceStatus(g_hStatusHandle, &ss);
}

void StartMain(BOOL bService)
{
	CPTwManager ptw_manager;
	g_cMain.StartMain(bService, &ptw_manager);
}

void StopMain()
{
	g_cMain.StopMain();
}
