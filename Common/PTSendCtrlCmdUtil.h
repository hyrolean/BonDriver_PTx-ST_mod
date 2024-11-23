#ifndef __PT1_SEND_CTRL_CMD_UTIL_H__
#define __PT1_SEND_CTRL_CMD_UTIL_H__

#include "PTOutsideCtrlCmdDef.h"
#include "PTCreateCtrlCmdUtil.h"
#include "StringUtil.h"
#include "PTManager.h"

#define CONNECT_TIMEOUT 30*1000

class CPTSendCtrlCmdBase
{
public:
	CPTSendCtrlCmdBase(int iPT,DWORD Timeout__ = CONNECT_TIMEOUT);
	virtual ~CPTSendCtrlCmdBase(){}
protected:
	int m_iPT;
	DWORD Timeout_;
	virtual DWORD SendCmd(CMD_STREAM &stSend, CMD_STREAM &stRes) = 0;
public:
	int PTKind() {return m_iPT;}
	DWORD Timeout() { return Timeout_ ; }
	void SetTimeout(DWORD Timeout__) { Timeout_ = Timeout__ ; }
	// main commands
	DWORD CloseExe();
	DWORD GetTotalTunerCount(DWORD* pdwNumTuner);
	DWORD GetActiveTunerCount(BOOL bSate, DWORD* pdwNumTuner);
	DWORD SetLnbPower(int iID, BOOL bEnabled);
	DWORD OpenTuner(BOOL bSate, int* piID);
	DWORD OpenTuner2(BOOL bSate, int iTunerID, int* piID);
	DWORD CloseTuner(int iID);
	DWORD SetCh(int iID, DWORD dwCh, DWORD dwTSID);
	DWORD GetSignal(int iID, DWORD* pdwCn100);
	DWORD GetStreamingMethod(PTSTREAMING *pPTStreaming);
	// data transmitter
	virtual DWORD SendData(int iID, BYTE** pbData, DWORD* pdwSize) = 0;
	virtual DWORD SendBufferObject(int iID, PTBUFFER_OBJECT *pPtBuffObj) = 0 ;
    // for IBonTransponder
	DWORD SetFreq(int iID, DWORD dwCh);
	DWORD GetIdListS(int iID, PTTSIDLIST *pPtTSIDList);
	DWORD GetIdS(int iID, DWORD *pdwTSID);
	DWORD SetIdS(int iID, DWORD dwTSID);
};


class CPTSendCtrlCmdPipe : public CPTSendCtrlCmdBase
{
public:
	/*
	CPTSendCtrlCmdUtil(
		int iPT, wstring strCmdEvent=CMD_PT_CTRL_EVENT_WAIT_CONNECT,
		wstring strCmdPipe=CMD_PT_CTRL_PIPE)
		: m_iPT(iPT), m_strCmdEvent(strCmdEvent), m_strCmdPipe(strCmdPipe) {}
                                                                           */
	CPTSendCtrlCmdPipe(int iPT, DWORD Timeout__ = CONNECT_TIMEOUT);

protected:
	wstring m_strCmdEvent, m_strCmdPipe;
	DWORD SendCmd(CMD_STREAM &stSend, CMD_STREAM &stRes) ;
public:
	DWORD SendData(int iID, BYTE** pbData, DWORD* pdwSize);
	DWORD SendBufferObject(int iID, PTBUFFER_OBJECT *pPtBuffObj);
};

#endif
