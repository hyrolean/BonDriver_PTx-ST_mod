#include "stdafx.h"
#include "PTSendCtrlCmdUtil.h"

HANDLE g_hSendCtrlExternalStopConnectObject = NULL;

static DWORD WaitConnect(LPCWSTR lpwszName, DWORD dwConnectTimeOut)
{
	HANDLE hEvent = _CreateEvent(FALSE, FALSE, lpwszName);
	if( hEvent == NULL ){
		return CMD_ERR;
	}
	HANDLE hObjs[2];
	hObjs[0]=hEvent;
	hObjs[1]=g_hSendCtrlExternalStopConnectObject;
	DWORD nObjs = hObjs[1]!=NULL? 2: 1;
	DWORD dwRet = CMD_SUCCESS;
	if(HRWaitForMultipleObjects(nObjs, hObjs, FALSE, dwConnectTimeOut) != WAIT_OBJECT_0){
		dwRet = CMD_ERR_TIMEOUT;
	}
	CloseHandle(hEvent);

	return dwRet;
}

static DWORD SendDefCmd(LPCWSTR lpwszEventName, LPCWSTR lpwszPipeName, DWORD dwConnectTimeOut, CMD_STREAM* pstSend, CMD_STREAM* pstRes, PTBUFFER_OBJECT *pPtBufObj=NULL)
{
	DWORD dwRet = CMD_SUCCESS;
	if( pstSend == NULL || pstRes == NULL ){
		return CMD_ERR_INVALID_ARG;
	}

	dwRet = WaitConnect(lpwszEventName, dwConnectTimeOut);
	if( dwRet != CMD_SUCCESS ){
		return dwRet;
	}
	HANDLE hPipe = _CreateFile(lpwszPipeName, GENERIC_READ|GENERIC_WRITE, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if( hPipe == INVALID_HANDLE_VALUE ){
		return CMD_ERR_CONNECT;
	}
	DWORD dwWrite = 0;
	DWORD dwRead = 0;

	if( WriteFile(hPipe, pstSend, sizeof(DWORD)*2, &dwWrite, NULL ) == FALSE ){
		CloseHandle(hPipe);
		return CMD_ERR;
	}
	if( pstSend->dwSize > 0 ){
		if( pstSend->bData == NULL ){
			CloseHandle(hPipe);
			return CMD_ERR_INVALID_ARG;
		}
		DWORD dwSendNum = 0;
		while(dwSendNum < pstSend->dwSize ){
			DWORD dwSendSize;
			if( pstSend->dwSize-dwSendNum < SEND_BUFF_SIZE ){
				dwSendSize = pstSend->dwSize-dwSendNum;
			}else{
				dwSendSize = SEND_BUFF_SIZE;
			}
			if( WriteFile(hPipe, pstSend->bData+dwSendNum, dwSendSize, &dwWrite, NULL ) == FALSE ){
				CloseHandle(hPipe);
				return CMD_ERR;
			}
			dwSendNum+=dwWrite;
		}
	}
	if( ReadFile(hPipe, pstRes, sizeof(DWORD)*2, &dwRead, NULL ) == FALSE ){
		return CMD_ERR;
	}
	if( pstRes->dwSize > 0 ){
		BYTE *pBuf ;
		if(pPtBufObj!=NULL) {
			if(!pPtBufObj->resize(pstRes->dwSize))
				return CMD_ERR;
			pBuf = pPtBufObj->data() ;
		}else {
			pBuf = pstRes->bData = new BYTE[pstRes->dwSize];
		}
		DWORD dwReadNum = 0;
		while(dwReadNum < pstRes->dwSize ){
			DWORD dwReadSize;
			if( pstRes->dwSize-dwReadNum < RES_BUFF_SIZE ){
				dwReadSize = pstRes->dwSize-dwReadNum;
			}else{
				dwReadSize = RES_BUFF_SIZE;
			}
			if( ReadFile(hPipe, pBuf+dwReadNum, dwReadSize, &dwRead, NULL ) == FALSE ){
				CloseHandle(hPipe);
				return CMD_ERR;
			}
			dwReadNum+=dwRead;
		}
	}
	CloseHandle(hPipe);

	return pstRes->dwParam;
}


// CPTSendCtrlCmdBase

CPTSendCtrlCmdBase::CPTSendCtrlCmdBase(int iPT, DWORD Timeout__)
  : m_iPT(iPT),Timeout_(Timeout__)
{}

DWORD CPTSendCtrlCmdBase::KeepAlive()
{
	CMD_STREAM stSend;
	CMD_STREAM stRes;

	stSend.dwParam = CMD_KEEP_ALIVE;

	DWORD dwRet = SendCmd(stSend, stRes);

	return dwRet;
}

DWORD CPTSendCtrlCmdBase::CloseExe()
{
	CMD_STREAM stSend;
	CMD_STREAM stRes;

	stSend.dwParam = CMD_CLOSE_EXE;

	DWORD dwRet = SendCmd(stSend, stRes);

	return dwRet;
}

DWORD CPTSendCtrlCmdBase::GetTotalTunerCount(DWORD* pdwNumTuner)
{
	CMD_STREAM stSend;
	CMD_STREAM stRes;

	stSend.dwParam = CMD_GET_TOTAL_TUNER_COUNT;

	DWORD dwRet = SendCmd(stSend, stRes);
	if( dwRet == CMD_SUCCESS ){
		CopyDefData(pdwNumTuner, stRes.bData);
	}

	return dwRet;
}

DWORD CPTSendCtrlCmdBase::GetActiveTunerCount(BOOL bSate, DWORD* pdwNumTuner)
{
	CMD_STREAM stSend;
	CMD_STREAM stRes;

	stSend.dwParam = CMD_GET_ACTIVE_TUNER_COUNT;
	CreateDefStream(bSate, &stSend);

	DWORD dwRet = SendCmd(stSend, stRes);
	if( dwRet == CMD_SUCCESS ){
		CopyDefData(pdwNumTuner, stRes.bData);
	}

	return dwRet;
}

DWORD CPTSendCtrlCmdBase::SetLnbPower(int iID, BOOL bEnabled)
{
	CMD_STREAM stSend;
	CMD_STREAM stRes;

	stSend.dwParam = CMD_SET_LNB_POWER;
	CreateDefStream2(iID, bEnabled, &stSend);

	DWORD dwRet = SendCmd(stSend, stRes);

	return dwRet;
}

DWORD CPTSendCtrlCmdBase::OpenTuner(BOOL bSate, int* piID)
{
	CMD_STREAM stSend;
	CMD_STREAM stRes;

	stSend.dwParam = CMD_OPEN_TUNER;
	CreateDefStream(bSate, &stSend);

	DWORD dwRet = SendCmd(stSend, stRes);
	if( dwRet == CMD_SUCCESS ){
		CopyDefData((DWORD*)piID, stRes.bData);
	}

	return dwRet;
}

DWORD CPTSendCtrlCmdBase::OpenTuner2(BOOL bSate, int iTunerID, int* piID)
{
	CMD_STREAM stSend;
	CMD_STREAM stRes;

	stSend.dwParam = CMD_OPEN_TUNER2;
	CreateDefStream2(bSate, iTunerID, &stSend);

	DWORD dwRet = SendCmd(stSend, stRes);
	if( dwRet == CMD_SUCCESS ){
		CopyDefData((DWORD*)piID, stRes.bData);
	}

	return dwRet;
}

DWORD CPTSendCtrlCmdBase::CloseTuner(int iID)
{
	CMD_STREAM stSend;
	CMD_STREAM stRes;

	stSend.dwParam = CMD_CLOSE_TUNER;
	CreateDefStream(iID, &stSend);

	DWORD dwRet = SendCmd(stSend, stRes);

	return dwRet;
}

DWORD CPTSendCtrlCmdBase::SetCh(int iID, DWORD dwCh, DWORD dwTSID)
{
	CMD_STREAM stSend;
	CMD_STREAM stRes;

	stSend.dwParam = CMD_SET_CH;
	CreateDefStream3(iID, dwCh, dwTSID, &stSend);

	DWORD dwRet = SendCmd(stSend, stRes);

	return dwRet;
}

DWORD CPTSendCtrlCmdBase::GetSignal(int iID, DWORD* pdwCn100)
{
	CMD_STREAM stSend;
	CMD_STREAM stRes;

	stSend.dwParam = CMD_GET_SIGNAL;
	CreateDefStream(iID, &stSend);

	DWORD dwRet = SendCmd(stSend, stRes);
	if( dwRet == CMD_SUCCESS ){
		CopyDefData(pdwCn100, stRes.bData);
	}

	return dwRet;
}

DWORD CPTSendCtrlCmdBase::GetStreamingMethod(PTSTREAMING *pPTStreaming)
{
	CMD_STREAM stSend;
	CMD_STREAM stRes;

	stSend.dwParam = CMD_GET_STREAMING_METHOD;

	DWORD dwRet = SendCmd(stSend, stRes);
	if( dwRet == CMD_SUCCESS ){
		DWORD dwPTStreaming;
		CopyDefData(&dwPTStreaming, stRes.bData);
		*pPTStreaming=static_cast<PTSTREAMING>(dwPTStreaming);
	}

	return dwRet;
}

DWORD CPTSendCtrlCmdBase::SetFreq(int iID, DWORD dwCh)
{
	CMD_STREAM stSend;
	CMD_STREAM stRes;

	stSend.dwParam = CMD_SET_FREQ;
	CreateDefStream2(iID, dwCh, &stSend);

	DWORD dwRet = SendCmd(stSend, stRes);

	return dwRet;
}

DWORD CPTSendCtrlCmdBase::GetIdListS(int iID, PTTSIDLIST *pPtTSIDList)
{
	CMD_STREAM stSend;
	CMD_STREAM stRes;

	stSend.dwParam = CMD_GET_IDLIST_S;
	CreateDefStream(iID, &stSend);

	DWORD dwRet = SendCmd(stSend, stRes);
	if( dwRet == CMD_SUCCESS ){
		CopyDefDataN(&(pPtTSIDList->dwId[0]), 8, stRes.bData);
	}

	return dwRet;
}

DWORD CPTSendCtrlCmdBase::GetIdS(int iID, DWORD *pdwTSID)
{
	CMD_STREAM stSend;
	CMD_STREAM stRes;

	stSend.dwParam = CMD_GET_ID_S;
	CreateDefStream(iID, &stSend);

	DWORD dwRet = SendCmd(stSend, stRes);
	if( dwRet == CMD_SUCCESS ){
		CopyDefData(pdwTSID, stRes.bData);
	}

	return dwRet;
}

DWORD CPTSendCtrlCmdBase::SetIdS(int iID, DWORD dwTSID)
{
	CMD_STREAM stSend;
	CMD_STREAM stRes;

	stSend.dwParam = CMD_SET_ID_S;
	CreateDefStream2(iID, dwTSID, &stSend);

	DWORD dwRet = SendCmd(stSend, stRes);

	return dwRet;
}


// CPTSendCtrlCmdPipe

CPTSendCtrlCmdPipe::CPTSendCtrlCmdPipe(int iPT, DWORD Timeout__)
	: CPTSendCtrlCmdBase(iPT, Timeout__)
{
	Format(m_strCmdEvent, CMD_PT_CTRL_EVENT_WAIT_CONNECT_FORMAT, m_iPT);
	Format(m_strCmdPipe,  CMD_PT_CTRL_PIPE_FORMAT, m_iPT);
}

DWORD CPTSendCtrlCmdPipe::SendCmd(CMD_STREAM &stSend, CMD_STREAM &stRes)
{
	return SendDefCmd(m_strCmdEvent.c_str(), m_strCmdPipe.c_str(),
		Timeout_, &stSend, &stRes);
}

DWORD CPTSendCtrlCmdPipe::SendData(int iID, BYTE** pbData, DWORD* pdwSize )
{
	CMD_STREAM stSend;
	CMD_STREAM stRes;

	stSend.dwParam = CMD_SEND_DATA;

	wstring strDataEvent, strDataPipe;
	Format(strDataEvent, CMD_PT_DATA_EVENT_WAIT_CONNECT_FORMAT ,m_iPT ,iID);
	Format(strDataPipe, CMD_PT_DATA_PIPE_FORMAT ,m_iPT ,iID);

	DWORD dwRet = SendDefCmd(strDataEvent.c_str(), strDataPipe.c_str(), Timeout_, &stSend, &stRes);
	if( dwRet == CMD_SUCCESS ){
		*pbData = stRes.bData;
		*pdwSize = stRes.dwSize;
		stRes.bData = NULL;		// ポインタで返すのでstResのデストラクタでdeleteされないように
	}

	return dwRet;
}

DWORD CPTSendCtrlCmdPipe::SendBufferObject(int iID, PTBUFFER_OBJECT *pPtBuffObj )
{
	CMD_STREAM stSend;
	CMD_STREAM stRes;

	stSend.dwParam = CMD_SEND_DATA;

	wstring strDataEvent, strDataPipe;
	Format(strDataEvent, CMD_PT_DATA_EVENT_WAIT_CONNECT_FORMAT ,m_iPT ,iID);
	Format(strDataPipe, CMD_PT_DATA_PIPE_FORMAT ,m_iPT ,iID);

	DWORD dwRet = SendDefCmd(strDataEvent.c_str(), strDataPipe.c_str(), Timeout_, &stSend, &stRes, pPtBuffObj);
	if(dwRet==CMD_SUCCESS) {
		if(!stRes.dwSize) dwRet=CMD_ERR_BUSY ;
	}

	return dwRet;
}

