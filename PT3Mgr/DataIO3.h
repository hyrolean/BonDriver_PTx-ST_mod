#pragma once

#include "inc/EARTH_PT3.h"
#include "inc/Prefix.h"
#include "inc/EX_Buffer.h"
#include "../Common/PTOutsideCtrlCmdDef.h"
#include "../Common/PipeServer.h"
#include "../Common/SharedMem.h"

#define DATA_TIMEOUT (10*1000)

using namespace EARTH3;

class CDataIO3
{
public:
	CDataIO3(BOOL bMemStreaming=FALSE);
	~CDataIO3(void);

	void SetDevice(PT::Device* pcDevice){ m_pcDevice = pcDevice; };
	void SetVirtualCount(UINT uiVirtualCount){ VIRTUAL_COUNT = uiVirtualCount*8; };
	void Run(int iID);
	void Stop();
	BOOL EnableTuner(int iID, BOOL bEnable);
	void StartPipeServer(int iID);
	void StopPipeServer(int iID);
	void ClearBuff(int iID);

	DWORD GetOverFlowCount(int iID);

protected:
	UINT VIRTUAL_COUNT;
	PT::Device* m_pcDevice;

	BOOL m_bThTerm;
	HANDLE m_hWakeupEvent;

	HANDLE m_hThread1;
	HANDLE m_hThread2;
	HANDLE m_hThread3;
	HANDLE m_hThread4;

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

	EARTH3::EX::Buffer* m_T0SetBuff;
	EARTH3::EX::Buffer* m_T1SetBuff;
	EARTH3::EX::Buffer* m_S0SetBuff;
	EARTH3::EX::Buffer* m_S1SetBuff;
	EARTH3::EX::Buffer* &SetBuff(DWORD dwID) {
		switch(dwID) {
		case 1: return m_T1SetBuff;
		case 2: return m_S0SetBuff;
		case 3: return m_S1SetBuff;
		default: return m_T0SetBuff;
		}
	}

	uint32 m_T0WriteIndex;
	uint32 m_T1WriteIndex;
	uint32 m_S0WriteIndex;
	uint32 m_S1WriteIndex;
	uint32 &WriteIndex(DWORD dwID) {
		switch(dwID) {
		case 1: return m_T1WriteIndex;
		case 2: return m_S0WriteIndex;
		case 3: return m_S1WriteIndex;
		default: return m_T0WriteIndex;
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

	HANDLE m_hSetBuffEvent1;
	HANDLE m_hSetBuffEvent2;
	HANDLE m_hSetBuffEvent3;
	HANDLE m_hSetBuffEvent4;

	HANDLE m_hBuffEvent1;
	HANDLE m_hBuffEvent2;
	HANDLE m_hBuffEvent3;
	HANDLE m_hBuffEvent4;

	bool m_fDataCarry[4];

protected:
	struct RECVTHREAD_PARAM {
		CDataIO3 *pSys;
		DWORD dwID;
		RECVTHREAD_PARAM(CDataIO3 *pSys_, DWORD dwID_)
		 : pSys(pSys_), dwID(dwID_) {}
	};
	static UINT WINAPI RecvThreadProc(LPVOID pParam);

	bool SetBuffLock1(DWORD timeout=DATA_TIMEOUT);
	void SetBuffUnLock1();
	bool SetBuffLock2(DWORD timeout=DATA_TIMEOUT);
	void SetBuffUnLock2();
	bool SetBuffLock3(DWORD timeout=DATA_TIMEOUT);
	void SetBuffUnLock3();
	bool SetBuffLock4(DWORD timeout=DATA_TIMEOUT);
	void SetBuffUnLock4();
	bool SetBuffLock(DWORD dwID, DWORD timeout=DATA_TIMEOUT) {
		switch(dwID) {
		case 1: return SetBuffLock2(timeout);
		case 2: return SetBuffLock3(timeout);
		case 3: return SetBuffLock4(timeout);
		default: return SetBuffLock1(timeout);
		}
	}
	void SetBuffUnLock(DWORD dwID) {
		switch(dwID) {
		case 1: SetBuffUnLock2(); break;
		case 2: SetBuffUnLock3(); break;
		case 3: SetBuffUnLock4(); break;
		default: SetBuffUnLock1(); break;
		}
	}

	bool BuffLock1(DWORD timeout=DATA_TIMEOUT);
	void BuffUnLock1();
	bool BuffLock2(DWORD timeout=DATA_TIMEOUT);
	void BuffUnLock2();
	bool BuffLock3(DWORD timeout=DATA_TIMEOUT);
	void BuffUnLock3();
	bool BuffLock4(DWORD timeout=DATA_TIMEOUT);
	void BuffUnLock4();
	bool BuffLock(DWORD dwID, DWORD timeout=DATA_TIMEOUT) {
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

	void ChkTransferInfo();

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

	bool CheckReady(DWORD dwID);
	bool ReadAddBuff(DWORD dwID);

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
		CDataIO3 *pSys;
		DWORD dwID;
		MEMSTREAMINGTHREAD_PARAM(CDataIO3 *pSys_, DWORD dwID_)
		 : pSys(pSys_), dwID(dwID_) {}
	};
	static UINT WINAPI MemStreamingThreadProc(LPVOID pParam);
	void StartMemStreaming(DWORD dwID);
	void StopMemStreaming();
};
