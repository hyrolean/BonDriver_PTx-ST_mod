//===========================================================================
#pragma once

#ifndef _BASEIO_A7E2875D_30EF_4185_AE03_67047135B8F7_H_INCLUDED_
#define _BASEIO_A7E2875D_30EF_4185_AE03_67047135B8F7_H_INCLUDED_
//---------------------------------------------------------------------------

#include "PTOutsideCtrlCmdDef.h"
#include "PipeServer.h"
#include "SharedMem.h"

#define IO_DEFAULT_TIMEOUT (10*1000)

class CBaseIO
{
protected:
	CBaseIO(BOOL bMemStreaming);
	virtual ~CBaseIO(void);

	CPipeServer m_cPipeT0;
	CPipeServer m_cPipeT1;
	CPipeServer m_cPipeS0;
	CPipeServer m_cPipeS1;
	CPipeServer &Pipe(DWORD dwID) {
		switch(dwID) {
		case 1: return m_cPipeT1;
		case 2: return m_cPipeS0;
		case 3: return m_cPipeS1;
		default: return m_cPipeT0;
		}
	}

	PTBUFFER m_T0Buff;
	PTBUFFER m_T1Buff;
	PTBUFFER m_S0Buff;
	PTBUFFER m_S1Buff;
	PTBUFFER &Buff(DWORD dwID) {
		switch(dwID) {
		case 1: return m_T1Buff;
		case 2: return m_S0Buff;
		case 3: return m_S1Buff;
		default: return m_T0Buff;
		}
	}

	DWORD m_dwT0OverFlowCount;
	DWORD m_dwT1OverFlowCount;
	DWORD m_dwS0OverFlowCount;
	DWORD m_dwS1OverFlowCount;
	DWORD &OverFlowCount(DWORD dwID) {
		switch(dwID) {
		case 1: return m_dwT1OverFlowCount;
		case 2: return m_dwS0OverFlowCount;
		case 3: return m_dwS1OverFlowCount;
		default: return m_dwT0OverFlowCount;
		}
	}

	std::wstring IdentStr(DWORD dwID, std::wstring suffix=L"") {
		switch(dwID) {
		case 1: return L"T1"+suffix;
		case 2: return L"S0"+suffix;
		case 3: return L"S1"+suffix;
		default: return L"T0"+suffix;
		}
	}

	HANDLE m_hBuffEvent1;
	HANDLE m_hBuffEvent2;
	HANDLE m_hBuffEvent3;
	HANDLE m_hBuffEvent4;

	bool m_fDataCarry[4];

protected:

	bool BuffLock1(DWORD timeout=IO_DEFAULT_TIMEOUT);
	void BuffUnLock1();
	bool BuffLock2(DWORD timeout=IO_DEFAULT_TIMEOUT);
	void BuffUnLock2();
	bool BuffLock3(DWORD timeout=IO_DEFAULT_TIMEOUT);
	void BuffUnLock3();
	bool BuffLock4(DWORD timeout=IO_DEFAULT_TIMEOUT);
	void BuffUnLock4();
	bool BuffLock(DWORD dwID, DWORD timeout=IO_DEFAULT_TIMEOUT) {
		switch(dwID) {
		case 1: return BuffLock2(timeout);
		case 2: return BuffLock3(timeout);
		case 3: return BuffLock4(timeout);
		default: return BuffLock1(timeout);
		}
	}
	void BuffUnLock(DWORD dwID) {
		switch(dwID) {
		case 1: BuffUnLock2(); break;
		case 2: BuffUnLock3(); break;
		case 3: BuffUnLock4(); break;
		default: BuffUnLock1(); break;
		}
	}

	static int CALLBACK OutsideCmdCallbackT0(void* pParam, CMD_STREAM* pCmdParam, CMD_STREAM* pResParam, BOOL* pbResDataAbandon);
	static int CALLBACK OutsideCmdCallbackT1(void* pParam, CMD_STREAM* pCmdParam, CMD_STREAM* pResParam, BOOL* pbResDataAbandon);
	static int CALLBACK OutsideCmdCallbackS0(void* pParam, CMD_STREAM* pCmdParam, CMD_STREAM* pResParam, BOOL* pbResDataAbandon);
	static int CALLBACK OutsideCmdCallbackS1(void* pParam, CMD_STREAM* pCmdParam, CMD_STREAM* pResParam, BOOL* pbResDataAbandon);
    CMD_CALLBACK_PROC OutsideCmdCallback(DWORD dwID) {
		switch(dwID) {
		case 1: return &OutsideCmdCallbackT1;
		case 2: return &OutsideCmdCallbackS0;
		case 3: return &OutsideCmdCallbackS1;
		default: return &OutsideCmdCallbackT0;
		}
	}

	void CmdSendData(DWORD dwID, CMD_STREAM* pCmdParam, CMD_STREAM* pResParam, BOOL* pbResDataAbandon);

	void Flush(PTBUFFER &buf, BOOL dispose = FALSE );

protected:
	// MemStreaming
	BOOL m_bMemStreaming;
	BOOL m_bMemStreamingTerm;
	CSharedTransportStreamer *m_T0MemStreamer;
	CSharedTransportStreamer *m_T1MemStreamer;
	CSharedTransportStreamer *m_S0MemStreamer;
	CSharedTransportStreamer *m_S1MemStreamer;
	CSharedTransportStreamer *&MemStreamer(DWORD dwID) {
		switch(dwID) {
		case 1: return m_T1MemStreamer;
		case 2: return m_S0MemStreamer;
		case 3: return m_S1MemStreamer;
		default: return m_T0MemStreamer;
		}
	}
	HANDLE m_hT0MemStreamingThread;
	HANDLE m_hT1MemStreamingThread;
	HANDLE m_hS0MemStreamingThread;
	HANDLE m_hS1MemStreamingThread;
	HANDLE &MemStreamingThread(DWORD dwID) {
		switch(dwID) {
		case 1: return m_hT1MemStreamingThread;
		case 2: return m_hS0MemStreamingThread;
		case 3: return m_hS1MemStreamingThread;
		default: return m_hT0MemStreamingThread;
		}
	}
	UINT MemStreamingThreadProcMain(DWORD dwID);
	struct MEMSTREAMINGTHREAD_PARAM {
		CBaseIO *pSys;
		DWORD dwID;
		MEMSTREAMINGTHREAD_PARAM(CBaseIO *pSys_, DWORD dwID_)
		 : pSys(pSys_), dwID(dwID_) {}
	};
	static UINT WINAPI MemStreamingThreadProc(LPVOID pParam);
	void StartMemStreaming(DWORD dwID);
	void StopMemStreaming();
};


//===========================================================================
#endif // _BASEIO_A7E2875D_30EF_4185_AE03_67047135B8F7_H_INCLUDED_
