// PTxCtrl.cpp : アプリケーションのエントリ ポイントを定義します。
//

#include "stdafx.h"
#include "../Common/PTManager.h"
#include "../Common/PTCtrlMain.h"
#include "../Common/ServiceUtil.h"
#include "PTxCtrl.h"

// サービス実行中にクライアントが居なくなったらSDKを閉じてメモリを開放するかどうか
BOOL g_bXCompactService = FALSE ;

// クライアントが居なくなったあとにサービスを閉じるまでの最大待機時間(msec)
DWORD g_dwXServiceDeactWaitMSec = 5000 ;

CPTCtrlMain g_cMain3(PT3_GLOBAL_LOCK_MUTEX, CMD_PT3_CTRL_EVENT_WAIT_CONNECT, CMD_PT3_CTRL_PIPE);
CPTCtrlMain g_cMain1(PT1_GLOBAL_LOCK_MUTEX, CMD_PT1_CTRL_EVENT_WAIT_CONNECT, CMD_PT1_CTRL_PIPE);

HANDLE g_hMutex;
SERVICE_STATUS_HANDLE g_hStatusHandle;

#define PT_CTRL_MUTEX L"PT0_CTRL_EXE_MUTEX"
#define SERVICE_NAME L"PTxCtrl Service"

extern "C" IPTManager* CreatePT1Manager(void);
extern "C" IPTManager* CreatePT3Manager(void);

int APIENTRY _tWinMain(HINSTANCE hInstance,
					 HINSTANCE hPrevInstance,
					 LPTSTR    lpCmdLine,
					 int       nCmdShow)
{
	WCHAR strExePath[512] = L"";
	GetModuleFileName(hInstance, strExePath, 512);

	wstring strIni;
	{
		WCHAR szPath[_MAX_PATH];	// パス
		WCHAR szDrive[_MAX_DRIVE];
		WCHAR szDir[_MAX_DIR];
		WCHAR szFname[_MAX_FNAME];
		WCHAR szExt[_MAX_EXT];
		_tsplitpath_s( strExePath, szDrive, _MAX_DRIVE, szDir, _MAX_DIR, szFname, _MAX_FNAME, szExt, _MAX_EXT );
		_tmakepath_s(  szPath, _MAX_PATH, szDrive, szDir, NULL, NULL );
		strIni = szPath;
	}

	strIni += L"\\BonDriver_PTx-ST.ini";
	g_bXCompactService = GetPrivateProfileInt(L"SET", L"xCompactService", g_bXCompactService, strIni.c_str());
	g_dwXServiceDeactWaitMSec = GetPrivateProfileInt(L"SET", L"xServiceDeactWaitMSec", g_dwXServiceDeactWaitMSec, strIni.c_str());
	SetHRTimerMode(GetPrivateProfileInt(L"SET", L"UseHRTimer", 0, strIni.c_str()));

	if( _tcslen(lpCmdLine) > 0 ){
		if( lpCmdLine[0] == '-' || lpCmdLine[0] == '/' ){
			if( _tcsicmp( _T("install"), lpCmdLine+1 ) == 0 ){
				WCHAR strExePath[512] = L"";
				GetModuleFileName(NULL, strExePath, 512);
				InstallService(strExePath, SERVICE_NAME,SERVICE_NAME);
				return 0;
			}else if( _tcsicmp( _T("remove"), lpCmdLine+1 ) == 0 ){
				RemoveService(SERVICE_NAME);
				return 0;
			}
		}
	}

	if( IsInstallService(SERVICE_NAME) == FALSE ){
		//普通にexeとして起動を行う
		#if 0 // ??
		HANDLE h = ::OpenMutexW(SYNCHRONIZE, FALSE, PT0_GLOBAL_LOCK_MUTEX);
		if (h != NULL) {
			BOOL bErr = FALSE;
			if (::HRWaitForSingleObject(h, 100) == WAIT_TIMEOUT) {
				bErr = TRUE;
			}
			::ReleaseMutex(h);
			::CloseHandle(h);
			if (bErr) {
				return -1;
			}
		}
		#endif

		g_hStartEnableEvent = _CreateEvent(TRUE, FALSE, PT0_STARTENABLE_EVENT);
		if (g_hStartEnableEvent == NULL) {
			return -2;
		}
		#if 0
		// 別プロセスが終了処理中の場合は終了を待つ(最大1.5秒)
		if (::HRWaitForSingleObject(g_hStartEnableEvent, g_bXCompactService?1000:1500) == WAIT_TIMEOUT) {
			::CloseHandle(g_hStartEnableEvent);
			return -3;
		}
		#endif

		g_hMutex = _CreateMutex(TRUE, PT_CTRL_MUTEX);
		if (g_hMutex == NULL) {
			::CloseHandle(g_hStartEnableEvent);
			return -4;
		}
		if (::HRWaitForSingleObject(g_hMutex, 0) == WAIT_TIMEOUT) {
			// 別プロセスが実行中だった
			::CloseHandle(g_hMutex);
			::SetEvent(g_hStartEnableEvent); // 再始動要請
			::CloseHandle(g_hStartEnableEvent);
			return -5;
		}

		//起動
		StartMain(FALSE);

		::ReleaseMutex(g_hMutex);
		::CloseHandle(g_hMutex);

		::SetEvent(g_hStartEnableEvent);
		::CloseHandle(g_hStartEnableEvent);
	}else{
		//サービスとしてインストール済み
		if( IsStopService(SERVICE_NAME) == FALSE ){
			g_hMutex = _CreateMutex(TRUE, PT_CTRL_MUTEX);
			int err = GetLastError();
			if( g_hMutex != NULL && err != ERROR_ALREADY_EXISTS ) {
				//起動
				SERVICE_TABLE_ENTRY dispatchTable[] = {
					{ SERVICE_NAME, (LPSERVICE_MAIN_FUNCTION)service_main},
					{ NULL, NULL}
				};
				if( StartServiceCtrlDispatcher(dispatchTable) == FALSE ){
					OutputDebugString(_T("StartServiceCtrlDispatcher failed"));
				}
			}
		}else{
			//Stop状態なので起動する
			StartServiceCtrl(SERVICE_NAME);
		}
	}
	return 0;
}

void WINAPI service_main(DWORD dwArgc, LPTSTR *lpszArgv)
{
	g_hStatusHandle = RegisterServiceCtrlHandlerEx( SERVICE_NAME, (LPHANDLER_FUNCTION_EX)service_ctrl, NULL);

    do {

		if (g_hStatusHandle == NULL){
			break;
		}

		SendStatusScm(SERVICE_START_PENDING, 0, 1);

		SendStatusScm(SERVICE_RUNNING, 0, 0);
		StartMain(TRUE);

	} while(0);

	SendStatusScm(SERVICE_STOPPED, 0, 0);
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
				if( g_cMain1.IsFindOpen() || g_cMain3.IsFindOpen() ){
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
	CPTxCtrlCmdServiceOperator(CMD_PTX_CTRL_OP,bService).Main();
}

void StopMain()
{
	CPTxCtrlCmdServiceOperator::Stop();
}


  // CPTxCtrlCmdServiceOperator

CPTxCtrlCmdServiceOperator::CPTxCtrlCmdServiceOperator(wstring name, BOOL bService)
 : CPTxCtrlCmdOperator(name,true)
{
	PtService = bService ;
	PtPipeServer1 = PtPipeServer3 = NULL ;
	PtSupported = PtActivated = 0 ;
	Pt1Tuners = Pt3Tuners = 0 ;

	Pt1Manager = CreatePT1Manager();
	Pt3Manager = CreatePT3Manager();

	if(g_cMain1.Init(PtService, Pt1Manager)) {
		PtPipeServer1 = g_cMain1.MakePipeServer() ;
		PtSupported |= 1 ;
		Pt1Tuners = Pt1Manager->GetTotalTunerCount();
		DBGOUT("PTxCtrl: PT1 is Supported and was Activated.\n");
	}else {
		DBGOUT("PTxCtrl: PT1 is NOT Supported.\n");
	}

	if(g_cMain3.Init(PtService, Pt3Manager)) {
		PtPipeServer3 = g_cMain3.MakePipeServer() ;
		PtSupported |= 1<<2 ;
		Pt3Tuners = Pt3Manager->GetTotalTunerCount();
		DBGOUT("PTxCtrl: PT3 is Supported and was Activated.\n");
	}else {
		DBGOUT("PTxCtrl: PT3 is NOT Supported.\n");
	}

	LastActivated = GetTickCount() ;
	PtActivated = PtSupported ;
}

CPTxCtrlCmdServiceOperator::~CPTxCtrlCmdServiceOperator()
{
	SAFE_DELETE(PtPipeServer1) ;
	SAFE_DELETE(PtPipeServer3) ;
	SAFE_DELETE(Pt1Manager) ;
	SAFE_DELETE(Pt3Manager) ;
}

BOOL CPTxCtrlCmdServiceOperator::ResIdle()
{
	LastActivated = dur() ;
	return TRUE;
}


BOOL CPTxCtrlCmdServiceOperator::ResSupported(DWORD &PtBits)
{
	PtBits = PtSupported ;
	return TRUE;
}

BOOL CPTxCtrlCmdServiceOperator::ResActivatePt(DWORD PtVer)
{
	if( ! (PtSupported&(1<<(PtVer-1))) ) {
		DBGOUT("PTxCtrl: PT%d is Not Supported.\n",PtVer);
		return FALSE;
	}

	BOOL Result = FALSE ;

	if(!Pt1Manager) Pt1Manager = CreatePT1Manager();
	if(!Pt3Manager) Pt3Manager = CreatePT3Manager();

	if( PtActivated &(1<<(PtVer-1)) ) {
		DBGOUT("PTxCtrl: PT%d is Already Activated.\n",PtVer);
		Result = TRUE;
	}else if(PtVer==3) {
		if(g_cMain3.Init(PtService, Pt3Manager)) {
			if(!PtPipeServer3) PtPipeServer3 = g_cMain3.MakePipeServer() ;
			PtActivated |= 1<<2 ;
			Pt3Tuners = Pt3Manager->GetTotalTunerCount();
			DBGOUT("PTxCtrl: PT3 was Re-Activated.\n");
			Result = TRUE ;
		}
	}else if(PtVer==1) {
		if(g_cMain1.Init(PtService, Pt1Manager)) {
			if(!PtPipeServer1) PtPipeServer1 = g_cMain1.MakePipeServer() ;
			PtActivated |= 1 ;
			Pt1Tuners = Pt1Manager->GetTotalTunerCount();
			DBGOUT("PTxCtrl: PT1 was Re-Activated.\n");
			Result = TRUE ;
		}
	}

	if(Result) {
		LastActivated = dur() ;
	}

	return Result;
}

BOOL CPTxCtrlCmdServiceOperator::ResGetTunerCount(DWORD PtVer, DWORD &TunerCount)
{
	if( (PtSupported&(1<<(PtVer-1))) ) {
		switch(PtVer) {
		case 1: TunerCount = Pt1Tuners; return TRUE;
		case 3: TunerCount = Pt3Tuners; return TRUE;
		}
	}
	DBGOUT("PTxCtrl: PT%d is Not Supported.\n",PtVer);
	return FALSE;
}

void CPTxCtrlCmdServiceOperator::Main()
{
	//------ BEGIN OF THE SERVICE LOOP ------

	DBGOUT("PTxCtrl: The service is started.\n");

	DWORD LastDeactivated = dur();

	DWORD PrevPtDeactivated = 0 ;

	while(!PtTerminated) {

		DWORD dwServiceWait=g_dwXServiceDeactWaitMSec;
		DWORD dwDurLastAct = dur(LastActivated) ;

		if(dwDurLastAct<g_dwXServiceDeactWaitMSec) {

			// 新規クライアントアクティブ化問合わせからg_dwXServiceDeactWaitMSecﾐﾘ秒間は破棄処理禁止
			dwServiceWait=g_dwXServiceDeactWaitMSec-dwDurLastAct;

		}else if(PtActivated) {

			// 破棄処理
			DWORD PtDeactivated = 0 ;
			if(HRWaitForSingleObject(g_cMain3.GetStopEvent(),0)==WAIT_OBJECT_0)
				PtDeactivated |= 1<<2 ;
			if(HRWaitForSingleObject(g_cMain1.GetStopEvent(),0)==WAIT_OBJECT_0)
				PtDeactivated |= 1 ;

			if(PrevPtDeactivated!=PtDeactivated) {
				PrevPtDeactivated = PtDeactivated;
				LastDeactivated = dur();
			}

			// すべてのクライアントが居なくなった状態
			if((PtDeactivated&PtActivated)==PtActivated) {

				auto launchMutexCheck = []() ->bool {
					HANDLE h = ::OpenMutex(SYNCHRONIZE, FALSE, LAUNCH_PTX_CTRL_MUTEX);
					if (h != NULL) {
						::ReleaseMutex(h);
						::CloseHandle(h);
						return true ;
					}
					return false ;
				};

				// 一定時間破棄を抑制する
				if(dur(LastDeactivated)>=g_dwXServiceDeactWaitMSec&&!launchMutexCheck()) {

					if(PtActivated&(1<<2)) { // PT3
						mutex_locker_t locker(PT3_GLOBAL_LOCK_MUTEX, true);
						if(Pt3Manager->IsFindOpen() == FALSE) {
							DWORD last = g_cMain3.LastDeactivated(), cur = dur();
							if(dur(last,cur) < dur(LastDeactivated,cur)) {
								LastDeactivated = last ;
							}
							if(dur(LastDeactivated)>=g_dwXServiceDeactWaitMSec) {
								if(g_bXCompactService||!PtService) {
									if(g_bXCompactService) SAFE_DELETE(PtPipeServer3);
									g_cMain3.UnInit();
									if(g_bXCompactService) SAFE_DELETE(Pt3Manager) ;
									PtActivated &= ~(1<<2) ;
									DBGOUT("PTxCtrl: PT3 was De-Activated.\n");
								}else {
									Pt3Manager->FreeDevice();
								}
								ResetEvent(g_cMain3.GetStopEvent());
							}
						}
					}

					if(PtActivated&1) { // PT1/PT2
						mutex_locker_t locker(PT1_GLOBAL_LOCK_MUTEX, true);
						if(Pt1Manager->IsFindOpen() == FALSE) {
							DWORD last = g_cMain1.LastDeactivated(), cur = dur();
							if(dur(last,cur) < dur(LastDeactivated,cur)) {
								LastDeactivated = last ;
							}
							if(dur(LastDeactivated)>=g_dwXServiceDeactWaitMSec) {
								if(g_bXCompactService||!PtService) {
									if(g_bXCompactService) SAFE_DELETE(PtPipeServer1);
									g_cMain1.UnInit();
									if(g_bXCompactService) SAFE_DELETE(Pt1Manager) ;
									PtActivated &= ~1 ;
									DBGOUT("PTxCtrl: PT1 was De-Activated.\n");
								}else {
									Pt1Manager->FreeDevice();
								}
								ResetEvent(g_cMain1.GetStopEvent());
							}
						}
					}

					if(!PtActivated) SetEvent(g_hStartEnableEvent);

				}else {
					if(HRWaitForSingleObject(g_hStartEnableEvent, 0) == WAIT_OBJECT_0) {
						LastDeactivated = dur();
						ResetEvent(g_hStartEnableEvent);
					}
				}

			}

		}

		if(!PtActivated && !PtService) {
			// すべてのクライアントが居なくなった状態
			DWORD dwDurLastDeact = dur(LastDeactivated) ;
			// T->Sなどの切替の際のPTxCtrl.exeの再起動を一定時間抑制する処理
			if(dwDurLastDeact<g_dwXServiceDeactWaitMSec) {
				// g_dwXServiceDeactWaitMSecミリ秒だけ新規クライアントに接続のチャンスを与える
				dwServiceWait=g_dwXServiceDeactWaitMSec-dwDurLastDeact;
			}else {
				// g_dwXServiceDeactWaitMSecミリ秒経過しても新規クライアントが現れなかったら終了する
				PtTerminated=TRUE; continue;
			}
		}

		if(WaitForCmd(dwServiceWait)==WAIT_OBJECT_0) {

			if(!ServiceReaction()) {
				DBGOUT("PTxCtrl: The service reaction was failed.\n");
			}

		}else {

			//アプリ層死んだ時用のチェック

			if(PtActivated&(1<<2)) { // PT3
				mutex_locker_t locker(PT3_GLOBAL_LOCK_MUTEX);
				if(locker.lock(10)) {
					if( Pt3Manager->CloseChk() == FALSE){
						if(!PtService) SetEvent(g_cMain3.GetStopEvent()) ;
					}
				}
			}

			if(PtActivated&1) { // PT1/PT2
				mutex_locker_t locker(PT1_GLOBAL_LOCK_MUTEX);
				if(locker.lock(10)) {
					if( Pt1Manager->CloseChk() == FALSE){
						if(!PtService) SetEvent(g_cMain1.GetStopEvent()) ;
					}
				}
			}

		}

	}

	DBGOUT("PTxCtrl: The service was finished.\n");

	//------ END OF THE SERVICE LOOP ------

	SAFE_DELETE(PtPipeServer3);
	SAFE_DELETE(PtPipeServer1);
	if(PtActivated&1)
		g_cMain1.UnInit();
	if(PtActivated&(1<<2))
		g_cMain3.UnInit();

	PtActivated = 0 ;
}

BOOL CPTxCtrlCmdServiceOperator::PtTerminated=FALSE;
void CPTxCtrlCmdServiceOperator::Stop()
{
	g_cMain3.StopMain();
	g_cMain1.StopMain();
	PtTerminated = TRUE ;
}

