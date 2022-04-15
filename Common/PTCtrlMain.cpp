#include "StdAfx.h"
#include <memory>
#include "PTCtrlMain.h"

CPTCtrlMain::CPTCtrlMain(
	wstring strGlobalLockMutex,
	wstring strPipeEvent,
	wstring strPipeName, BOOL bResetStartEnableOnClose )
{
	m_bResetStartEnableOnClose = bResetStartEnableOnClose ;
	m_pManager = NULL;
	m_strGlobalLockMutex = strGlobalLockMutex;
	m_strPipeEvent = strPipeEvent;
	m_strPipeName = strPipeName;
	m_hStopEvent = _CreateEvent(TRUE, FALSE,NULL);
	m_bService = FALSE;
}

CPTCtrlMain::~CPTCtrlMain(void)
{
	StopMain();
	if( m_hStopEvent != NULL ){
		CloseHandle(m_hStopEvent);
	}
//	m_cPipeserver.StopServer();
}

BOOL CPTCtrlMain::Init(BOOL bService, IPTManager *pManager)
{
	if(!pManager) return FALSE;

	UnInit();

	m_pManager = pManager ;

	BOOL bInit = TRUE;
	if( m_pManager->LoadSDK() == FALSE ){
		OutputDebugString(L"PTx SDKのロードに失敗");
		bInit = FALSE;
	}

	DBGOUT("PTx SDKのロード%s\n",bInit?"成功":"失敗") ;

	if( bInit ){
		m_pManager->Init();
	}
	m_bService = bService;

	return bInit ;
}

void CPTCtrlMain::UnInit()
{
	if(m_pManager) {
		m_pManager->UnInit();
		m_pManager=NULL;
	}
}

CPipeServer *CPTCtrlMain::MakePipeServer()
{
	CPipeServer *pcPipeServer = new CPipeServer;
	pcPipeServer->StartServer(
		m_strPipeEvent.c_str()/*CMD_PT_CTRL_EVENT_WAIT_CONNECT*/,
		m_strPipeName.c_str()/*CMD_PT_CTRL_PIPE*/, OutsideCmdCallback, this);
	return pcPipeServer ;
}

void CPTCtrlMain::StartMain(BOOL bService, IPTManager *pManager)
{
	if(!Init(bService, pManager)) {
		m_pManager=NULL;
		return ;
	}

	//Pipeサーバースタート
	shared_ptr<CPipeServer> pipeServer(MakePipeServer());

	while(1){
		if( HRWaitForSingleObject(m_hStopEvent, 15*1000) != WAIT_TIMEOUT ){
			break;
		}else{
			//アプリ層死んだ時用のチェック
			if( m_pManager->CloseChk() == FALSE && m_bService == FALSE){
				break;
			}
		}
	}

	pipeServer->StopServer();
	UnInit();
}

void CPTCtrlMain::StopMain()
{
	if( m_hStopEvent != NULL ){
		SetEvent(m_hStopEvent);
	}
}

int CALLBACK CPTCtrlMain::OutsideCmdCallback(void* pParam, CMD_STREAM* pCmdParam, CMD_STREAM* pResParam, BOOL* /*pbResDataAbandon*/)
{
	CPTCtrlMain* pSys = (CPTCtrlMain*)pParam;

	switch( pCmdParam->dwParam ){
		case CMD_CLOSE_EXE:
			pSys->CmdCloseExe(pCmdParam, pResParam);
			break;
		case CMD_GET_TOTAL_TUNER_COUNT:
			pSys->CmdGetTotalTunerCount(pCmdParam, pResParam);
			break;
		case CMD_GET_ACTIVE_TUNER_COUNT:
			pSys->CmdGetActiveTunerCount(pCmdParam, pResParam);
			break;
		case CMD_SET_LNB_POWER:
			pSys->CmdSetLnbPower(pCmdParam, pResParam);
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
		case CMD_GET_STREAMING_METHOD:
			pSys->CmdGetStreamingMethod(pCmdParam, pResParam);
			break;
		case CMD_SET_FREQ:
			pSys->CmdSetFreq(pCmdParam, pResParam);
			break;
		case CMD_GET_IDLIST_S:
			pSys->CmdGetIdListS(pCmdParam, pResParam);
			break;
		case CMD_SET_ID_S:
			pSys->CmdSetIdS(pCmdParam, pResParam);
			break;
		case CMD_GET_ID_S:
			pSys->CmdGetIdS(pCmdParam, pResParam);
			break;
		default:
			pResParam->dwParam = CMD_NON_SUPPORT;
			break;
	}
	return 0;
}

//CMD_CLOSE_EXE PT3Ctrl.exeの終了
void CPTCtrlMain::CmdCloseExe(CMD_STREAM* pCmdParam, CMD_STREAM* pResParam)
{
	pResParam->dwParam = CMD_SUCCESS;
	StopMain();
}

//CMD_GET_TOTAL_TUNER_COUNT GetTotalTunerCount
void CPTCtrlMain::CmdGetTotalTunerCount(CMD_STREAM* pCmdParam, CMD_STREAM* pResParam)
{
	DWORD dwNumTuner;
	dwNumTuner = m_pManager->GetTotalTunerCount();

	pResParam->dwParam = CMD_SUCCESS;
	CreateDefStream(dwNumTuner, pResParam);
}

//CMD_GET_ACTIVE_TUNER_COUNT GetActiveTunerCount
void CPTCtrlMain::CmdGetActiveTunerCount(CMD_STREAM* pCmdParam, CMD_STREAM* pResParam)
{
	BOOL bSate;
	CopyDefData((DWORD*)&bSate, pCmdParam->bData);

	DWORD dwNumTuner;
	dwNumTuner = m_pManager->GetActiveTunerCount(bSate);

	pResParam->dwParam = CMD_SUCCESS;
	CreateDefStream(dwNumTuner, pResParam);
}

//CMD_SET_LNB_POWER SetLnbPower
void CPTCtrlMain::CmdSetLnbPower(CMD_STREAM* pCmdParam, CMD_STREAM* pResParam)
{
	int iID;
	BOOL bEnabled;
	CopyDefData2((DWORD*)&iID, (DWORD*)&bEnabled, pCmdParam->bData);

	BOOL r = m_pManager->SetLnbPower(iID, bEnabled);

	if( r ){
		pResParam->dwParam = CMD_SUCCESS;
	}else{
		pResParam->dwParam = CMD_ERR;
	}
}

//CMD_OPEN_TUNER OpenTuner
void CPTCtrlMain::CmdOpenTuner(CMD_STREAM* pCmdParam, CMD_STREAM* pResParam)
{
	BOOL bSate;
	CopyDefData((DWORD*)&bSate, pCmdParam->bData);
	int iID = m_pManager->OpenTuner(bSate);
	if( iID != -1 ){
		if(HRWaitForSingleObject(m_hStopEvent,0)==WAIT_OBJECT_0)
			ResetEvent(m_hStopEvent); // 終了処理の取消
		pResParam->dwParam = CMD_SUCCESS;
	}else{
		pResParam->dwParam = CMD_ERR;
	}
	CreateDefStream(iID, pResParam);
}

//CMD_CLOSE_TUNER CloseTuner
void CPTCtrlMain::CmdCloseTuner(CMD_STREAM* pCmdParam, CMD_STREAM* pResParam)
{
	int iID;
	CopyDefData((DWORD*)&iID, pCmdParam->bData);
	m_pManager->CloseTuner(iID);
	pResParam->dwParam = CMD_SUCCESS;
	if (m_bService == FALSE) {
		HANDLE h = _CreateMutex(TRUE, m_strGlobalLockMutex.c_str());
		if (m_pManager->IsFindOpen() == FALSE) {
			// 今から終了するので問題が無くなるタイミングまで別プロセスの開始を抑制
			if(m_bResetStartEnableOnClose)
				ResetEvent(g_hStartEnableEvent);
			StopMain();
		}
		ReleaseMutex(h);
		CloseHandle(h);
	}
}

//CMD_SET_CH SetChannel
void CPTCtrlMain::CmdSetCh(CMD_STREAM* pCmdParam, CMD_STREAM* pResParam)
{
	int iID;
	DWORD dwCh;
	DWORD dwTSID;
	BOOL hasStream=FALSE;
	CopyDefData3((DWORD*)&iID, &dwCh, &dwTSID, pCmdParam->bData);
	if( m_pManager->SetCh(iID,dwCh,dwTSID,hasStream) ){
		pResParam->dwParam = CMD_SUCCESS;
	}else{
		pResParam->dwParam = CMD_ERR;
	}
	if(!hasStream) {
		pResParam->dwParam |= CMD_BIT_NON_STREAM;
	}
}

//CMD_GET_SIGNAL GetSignalLevel
void CPTCtrlMain::CmdGetSignal(CMD_STREAM* pCmdParam, CMD_STREAM* pResParam)
{
	int iID;
	DWORD dwCn100;
	CopyDefData((DWORD*)&iID, pCmdParam->bData);
	dwCn100 = m_pManager->GetSignal(iID);

	pResParam->dwParam = CMD_SUCCESS;
	CreateDefStream(dwCn100, pResParam);
}

//CMD_OPEN_TUNER2 OpenTuner2
void CPTCtrlMain::CmdOpenTuner2(CMD_STREAM* pCmdParam, CMD_STREAM* pResParam)
{
	BOOL bSate;
	int iTunerID;
	CopyDefData2((DWORD*)&bSate, (DWORD*)&iTunerID, pCmdParam->bData);
	int iID = m_pManager->OpenTuner2(bSate, iTunerID);
	if( iID != -1 ){
		if(HRWaitForSingleObject(m_hStopEvent,0)==WAIT_OBJECT_0)
			ResetEvent(m_hStopEvent); // 終了処理の取消
		pResParam->dwParam = CMD_SUCCESS;
	}else{
		pResParam->dwParam = CMD_ERR;
	}
	CreateDefStream(iID, pResParam);
}

//CMD_GET_STREAMING_METHOD GetStreamingMethod
void CPTCtrlMain::CmdGetStreamingMethod(CMD_STREAM* pCmdParam, CMD_STREAM* pResParam)
{
	DWORD method = m_pManager->GetStreamingMethod();

	pResParam->dwParam = CMD_SUCCESS;
	CreateDefStream(method, pResParam);
}

//CMD_SET_FREQ SetFreq
void CPTCtrlMain::CmdSetFreq(CMD_STREAM* pCmdParam, CMD_STREAM* pResParam)
{
	int iID;
	DWORD dwCh;
	CopyDefData2((DWORD*)&iID, (DWORD*)&dwCh, pCmdParam->bData);
	BOOL bRes = m_pManager->SetFreq(iID, dwCh);
	if( bRes ){
		pResParam->dwParam = CMD_SUCCESS;
	}else{
		pResParam->dwParam = CMD_ERR;
	}
}

//CMD_GET_IDLIST_S GetIdListS
void CPTCtrlMain::CmdGetIdListS(CMD_STREAM* pCmdParam, CMD_STREAM* pResParam)
{
	int iID;
	CopyDefData((DWORD*)&iID, pCmdParam->bData);
	PTTSIDLIST list = {0} ;
	BOOL bRes = m_pManager->GetIdListS(iID, &list);
	if( bRes ){
		pResParam->dwParam = CMD_SUCCESS;
	}else{
		pResParam->dwParam = CMD_ERR;
	}
	CreateDefStreamN(&list.dwId[0], 8, pResParam);
}

//CMD_GET_ID_S GetIdS
void CPTCtrlMain::CmdGetIdS(CMD_STREAM* pCmdParam, CMD_STREAM* pResParam)
{
	int iID;
	CopyDefData((DWORD*)&iID, pCmdParam->bData);
	DWORD dwId=0;
	BOOL bRes = m_pManager->GetIdS(iID, &dwId);
	if( bRes ){
		pResParam->dwParam = CMD_SUCCESS;
	}else{
		pResParam->dwParam = CMD_ERR;
	}
	CreateDefStream(dwId, pResParam);
}

//CMD_SET_ID_S SetIdS
void CPTCtrlMain::CmdSetIdS(CMD_STREAM* pCmdParam, CMD_STREAM* pResParam)
{
	int iID;
	DWORD dwId;
	CopyDefData2((DWORD*)&iID, (DWORD*)&dwId, pCmdParam->bData);
	BOOL bRes = m_pManager->SetIdS(iID, dwId);
	if( bRes ){
		pResParam->dwParam = CMD_SUCCESS;
	}else{
		pResParam->dwParam = CMD_ERR;
	}
}

BOOL CPTCtrlMain::IsFindOpen()
{
	if(m_pManager==NULL) return FALSE ;
	return m_pManager->IsFindOpen();
}
