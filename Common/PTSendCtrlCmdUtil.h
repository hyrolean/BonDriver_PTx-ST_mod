#ifndef __PT1_SEND_CTRL_CMD_UTIL_H__
#define __PT1_SEND_CTRL_CMD_UTIL_H__

#include "PTOutsideCtrlCmdDef.h"
#include "PTCreateCtrlCmdUtil.h"
#include "StringUtil.h"
#include "PTManager.h"

#define CONNECT_TIMEOUT 30*1000

class CPTSendCtrlCmd
{
public:
	/*
	CPTSendCtrlCmdUtil(
		int iPT, wstring strCmdEvent=CMD_PT_CTRL_EVENT_WAIT_CONNECT,
		wstring strCmdPipe=CMD_PT_CTRL_PIPE)
		: m_iPT(iPT), m_strCmdEvent(strCmdEvent), m_strCmdPipe(strCmdPipe) {}
                                                                           */
	CPTSendCtrlCmd(int iPT);

protected:
	int m_iPT;
	wstring m_strCmdEvent, m_strCmdPipe;

public:
	int GetPTKind() {return m_iPT;}
	DWORD CloseExe(DWORD dwConnectTimeOut = CONNECT_TIMEOUT);
	DWORD GetTotalTunerCount(DWORD* pdwNumTuner, DWORD dwConnectTimeOut = CONNECT_TIMEOUT);
	DWORD GetActiveTunerCount(BOOL bSate, DWORD* pdwNumTuner, DWORD dwConnectTimeOut = CONNECT_TIMEOUT);
	DWORD SetLnbPower(int iID, BOOL bEnabled, DWORD dwConnectTimeOut = CONNECT_TIMEOUT);
	DWORD OpenTuner(BOOL bSate, int* piID, DWORD dwConnectTimeOut = CONNECT_TIMEOUT);
	DWORD OpenTuner2(BOOL bSate, int iTunerID, int* piID, DWORD dwConnectTimeOut = CONNECT_TIMEOUT);
	DWORD CloseTuner(int iID, DWORD dwConnectTimeOut = CONNECT_TIMEOUT);
	DWORD SetCh(int iID, DWORD dwCh, DWORD dwTSID, DWORD dwConnectTimeOut = CONNECT_TIMEOUT);
	DWORD GetSignal(int iID, DWORD* pdwCn100, DWORD dwConnectTimeOut = CONNECT_TIMEOUT);
	DWORD SendData(int iID, BYTE** pbData, DWORD* pdwSize, DWORD dwConnectTimeOut = CONNECT_TIMEOUT );
	DWORD SendBufferObject(int iID, PTBUFFER_OBJECT *pPtBuffObj, DWORD dwConnectTimeOut = CONNECT_TIMEOUT );
	DWORD GetStreamingMethod(PTSTREAMING *pPTStreaming, DWORD dwConnectTimeOut = CONNECT_TIMEOUT );

};

#endif
