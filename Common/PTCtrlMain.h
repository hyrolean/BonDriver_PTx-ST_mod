#pragma once

#include "PTSendCtrlCmdUtil.h"
#include "PipeServer.h"
#include "PTManager.h"
#include <string>

class CPTCtrlMain
{
public:
	CPTCtrlMain(
		wstring strGlobalLockMutex,
		wstring strPipeEvent,
		wstring strPipeName, BOOL bResetStartEnableOnClose=TRUE );

	~CPTCtrlMain(void);

	BOOL Init(BOOL bService, IPTManager *pManager);
	void UnInit();

	CPipeServer *MakePipeServer();

	void StartMain(BOOL bService, IPTManager *pManager);
	void StopMain();

	BOOL IsFindOpen();

	HANDLE GetStopEvent() { return m_hStopEvent; }

protected:
	HANDLE m_hStopEvent;
	IPTManager *m_pManager;

	BOOL m_bService;
	BOOL m_bResetStartEnableOnClose;

	wstring m_strGlobalLockMutex ;
	wstring m_strPipeEvent, m_strPipeName ;

protected:
	static int CALLBACK OutsideCmdCallback(void* pParam, CMD_STREAM* pCmdParam, CMD_STREAM* pResParam, BOOL* pbResDataAbandon);

	//CMD_CLOSE_EXE PT1Ctrl.exeの強制終了コマンド 通常は使用しない
	void CmdCloseExe(CMD_STREAM* pCmdParam, CMD_STREAM* pResParam);
	//CMD_GET_TOTAL_TUNER_COUNT GetTotalTunerCount
	void CmdGetTotalTunerCount(CMD_STREAM* pCmdParam, CMD_STREAM* pResParam);
	//CMD_GET_ACTIVE_TUNER_COUNT GetActiveTunerCount
	void CmdGetActiveTunerCount(CMD_STREAM* pCmdParam, CMD_STREAM* pResParam);
	//CMD_SET_LNB_POWER SetLnbPower
	void CmdSetLnbPower(CMD_STREAM* pCmdParam, CMD_STREAM* pResParam);
	//CMD_OPEN_TUNER OpenTuner
	void CmdOpenTuner(CMD_STREAM* pCmdParam, CMD_STREAM* pResParam);
	//CMD_CLOSE_TUNER CloseTuner
	void CmdCloseTuner(CMD_STREAM* pCmdParam, CMD_STREAM* pResParam);
	//CMD_SET_CH SetChannel
	void CmdSetCh(CMD_STREAM* pCmdParam, CMD_STREAM* pResParam);
	//CMD_GET_SIGNAL GetSignalLevel
	void CmdGetSignal(CMD_STREAM* pCmdParam, CMD_STREAM* pResParam);
	//CMD_OPEN_TUNER2 OpenTuner2
	void CmdOpenTuner2(CMD_STREAM* pCmdParam, CMD_STREAM* pResParam);
	//CMD_GET_STREAMING_METHOD GetStreamingMethod
	void CmdGetStreamingMethod(CMD_STREAM* pCmdParam, CMD_STREAM* pResParam);
};
