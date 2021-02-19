#include "StdAfx.h"
#include "PT1CtrlMain.h"

CPT1CtrlMain::CPT1CtrlMain(void)
{
	m_hStopEvent = _CreateEvent(TRUE, FALSE, NULL);
	m_bService = FALSE;
}

CPT1CtrlMain::~CPT1CtrlMain(void)
{
	StopMain();
	if( m_hStopEvent != NULL ){
		CloseHandle(m_hStopEvent);
	}
//	m_cPipeserver.StopServer();
}

void CPT1CtrlMain::StartMain(BOOL bService)
{
	BOOL bInit = TRUE;
	if( m_cPT1.LoadSDK() == FALSE ){
		OutputDebugString(L"PT SDKのロードに失敗");
		bInit = FALSE;
	}
	if( bInit ){
		m_cPT1.Init();
	}
	m_bService = bService;

	//Pipeサーバースタート
	CPipeServer cPipeserver;
	cPipeserver.StartServer(CMD_PT1_CTRL_EVENT_WAIT_CONNECT, CMD_PT1_CTRL_PIPE, OutsideCmdCallback, this);

	while(1){
		if( WaitForSingleObject(m_hStopEvent, 15*1000) != WAIT_TIMEOUT ){
			break;
		}else{
			//アプリ層死んだ時用のチェック
			if( m_cPT1.CloseChk() == FALSE && m_bService == FALSE){
				break;
			}
		}
		if( bInit == FALSE ){
			break;
		}
	}

	cPipeserver.StopServer();
	m_cPT1.UnInit();
}

void CPT1CtrlMain::StopMain()
{
	if( m_hStopEvent != NULL ){
		SetEvent(m_hStopEvent);
	}
}

int CALLBACK CPT1CtrlMain::OutsideCmdCallback(void* pParam, CMD_STREAM* pCmdParam, CMD_STREAM* pResParam)
{
	CPT1CtrlMain* pSys = (CPT1CtrlMain*)pParam;

	switch( pCmdParam->dwParam ){
		case CMD_CLOSE_EXE:
			pSys->CmdCloseExe(pCmdParam, pResParam);
			break;
		case CMD_OPEN_TUNER:
			pSys->CmdOpenTuner(pCmdParam, pResParam);
			break;
		case CMD_CLOSE_TUNER:
			pSys->CmdCloseTuner(pCmdParam, pResParam);
			break;
		case CMD_SET_CH:
			pSys->CmdSetCh(pCmdParam, pResParam);
			break;
		case CMD_GET_SIGNAL:
			pSys->CmdGetSignal(pCmdParam, pResParam);
			break;
		case CMD_OPEN_TUNER2:
			pSys->CmdOpenTuner2(pCmdParam, pResParam);
			break;
		default:
			pResParam->dwParam = CMD_NON_SUPPORT;
			break;
	}
	return 0;
}

//CMD_CLOSE_EXE PT1Ctrl.exeの終了
void CPT1CtrlMain::CmdCloseExe(CMD_STREAM* pCmdParam, CMD_STREAM* pResParam)
{
	pResParam->dwParam = CMD_SUCCESS;
	StopMain();
}

//CMD_OPEN_TUNER OpenTuner
void CPT1CtrlMain::CmdOpenTuner(CMD_STREAM* pCmdParam, CMD_STREAM* pResParam)
{
	BOOL bSate;
	CopyDefData((DWORD*)&bSate, pCmdParam->bData);
	int iID = m_cPT1.OpenTuner(bSate);
	if( iID != -1 ){
		pResParam->dwParam = CMD_SUCCESS;
	}else{
		pResParam->dwParam = CMD_ERR;
	}
	CreateDefStream(iID, pResParam);
}

//CMD_CLOSE_TUNER CloseTuner
void CPT1CtrlMain::CmdCloseTuner(CMD_STREAM* pCmdParam, CMD_STREAM* pResParam)
{
	int iID;
	CopyDefData((DWORD*)&iID, pCmdParam->bData);
	m_cPT1.CloseTuner(iID);
	pResParam->dwParam = CMD_SUCCESS;
	if (m_bService == FALSE) {
		HANDLE h = _CreateMutex(TRUE, PT1_GLOBAL_LOCK_MUTEX);
		if (m_cPT1.IsFindOpen() == FALSE) {
			// 今から終了するので問題が無くなるタイミングまで別プロセスの開始を抑制
			ResetEvent(g_hStartEnableEvent);
			StopMain();
		}
		ReleaseMutex(h);
		CloseHandle(h);
	}
}

//CMD_SET_CH SetChannel
// MARK : void CPT1CtrlMain::CmdSetCh(CMD_STREAM* pCmdParam, CMD_STREAM* pResParam)
void CPT1CtrlMain::CmdSetCh(CMD_STREAM* pCmdParam, CMD_STREAM* pResParam)
{
	int iID;
	DWORD dwCh;
	DWORD dwTSID;
	BOOL hasStream = FALSE;
	CopyDefData3((DWORD*)&iID, &dwCh, &dwTSID, pCmdParam->bData);
	if( m_cPT1.SetCh(iID,dwCh,dwTSID,hasStream) ){
		pResParam->dwParam = CMD_SUCCESS;
	}else{
		pResParam->dwParam = CMD_ERR;
	}
	if(!hasStream) {
		pResParam->dwParam |= CMD_BIT_NON_STREAM;
	}
}

//CMD_GET_SIGNAL GetSignalLevel
void CPT1CtrlMain::CmdGetSignal(CMD_STREAM* pCmdParam, CMD_STREAM* pResParam)
{
	int iID;
	DWORD dwCn100;
	CopyDefData((DWORD*)&iID, pCmdParam->bData);
	dwCn100 = m_cPT1.GetSignal(iID);

	pResParam->dwParam = CMD_SUCCESS;
	CreateDefStream(dwCn100, pResParam);
}

void CPT1CtrlMain::CmdOpenTuner2(CMD_STREAM* pCmdParam, CMD_STREAM* pResParam)
{
	BOOL bSate;
	int iTunerID;
	CopyDefData2((DWORD*)&bSate, (DWORD*)&iTunerID, pCmdParam->bData);
	int iID = m_cPT1.OpenTuner2(bSate, iTunerID);
	if( iID != -1 ){
		pResParam->dwParam = CMD_SUCCESS;
	}else{
		pResParam->dwParam = CMD_ERR;
	}
	CreateDefStream(iID, pResParam);
}

BOOL CPT1CtrlMain::IsFindOpen()
{
	return m_cPT1.IsFindOpen();
}
