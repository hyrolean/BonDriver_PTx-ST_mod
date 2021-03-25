#pragma once

//#define USE_DEQUE

#include "inc/EARTH_PT.h"
#include "inc/Prefix.h"
#include "../Common/PTOutsideCtrlCmdDef.h"
#include "../Common/PipeServer.h"
#include "../Common/SharedMem.h"
#include "MicroPacketUtil.h"

#define TRANSFER_SIZE (4096*PT::Device::BUFFER_PAGE_COUNT)
#define VIRTUAL_IMAGE_COUNT 4
//#define VIRTUAL_COUNT 8
#define LOCK_SIZE 4
#define READ_BLOCK_COUNT 8
#define READ_BLOCK_SIZE (TRANSFER_SIZE / READ_BLOCK_COUNT)
#define DATA_TIMEOUT (10*1000)

using namespace EARTH;

class CDataIO
{
public:
	CDataIO(BOOL bMemStreaming=FALSE);
	~CDataIO(void);

	void SetDevice(PT::Device* pcDevice){ m_pcDevice = pcDevice; };
	void SetVirtualCount(UINT uiVirtualCount){ VIRTUAL_COUNT = uiVirtualCount; };
	void Run();
	void Stop();
	void EnableTuner(int iID, BOOL bEnable);
	void ClearBuff(int iID);

	DWORD GetOverFlowCount(int iID);

protected:
	UINT VIRTUAL_COUNT;
	PT::Device* m_pcDevice;

	uint mVirtualIndex;
	uint mImageIndex;
	uint mBlockIndex;
	volatile bool mQuit;

	HANDLE m_hStopEvent;
	HANDLE m_hThread;

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

	CMicroPacketUtil m_cT0Micro;
	CMicroPacketUtil m_cT1Micro;
	CMicroPacketUtil m_cS0Micro;
	CMicroPacketUtil m_cS1Micro;
	CMicroPacketUtil &Micro(DWORD dwID) {
		switch(dwID) {
		case 1: return m_cT1Micro;
		case 2: return m_cS0Micro;
		case 3: return m_cS1Micro;
		default: return m_cT0Micro;
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

	HANDLE m_hEvent1;
	HANDLE m_hEvent2;
	HANDLE m_hEvent3;
	HANDLE m_hEvent4;

	BYTE* m_bDMABuff;

protected:
	static UINT WINAPI RecvThread(LPVOID pParam);

	bool Lock1(DWORD timeout=DATA_TIMEOUT);
	void UnLock1();
	bool Lock2(DWORD timeout=DATA_TIMEOUT);
	void UnLock2();
	bool Lock3(DWORD timeout=DATA_TIMEOUT);
	void UnLock3();
	bool Lock4(DWORD timeout=DATA_TIMEOUT);
	void UnLock4();
	bool Lock(DWORD dwID, DWORD timeout=DATA_TIMEOUT) {
		switch(dwID) {
		case 1: return Lock2(timeout);
		case 2: return Lock3(timeout);
		case 3: return Lock4(timeout);
		default: return Lock1(timeout);
		}
	}
	void UnLock(DWORD dwID) {
		switch(dwID) {
		case 1: UnLock2(); break;
		case 2: UnLock3(); break;
		case 3: UnLock4(); break;
		default: UnLock1(); break;
		}
	}

	bool WaitBlock();
	void CopyBlock();
	bool DispatchBlock();
	void Clear(uint virtualIndex, uint imageIndex, uint blockIndex);
	uint Read(uint virtualIndex, uint imageIndex, uint blockIndex) const;
	uint Offset(uint imageIndex, uint blockIndex, uint additionalOffset = 0) const;
	void MicroPacket(BYTE* pbPacket);

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

	void ResetDMA();

	void Flush(PTBUFFER &buf, BOOL dispose = FALSE );

protected:
	// MemStreamer
	BOOL m_bMemStreaming;
	BOOL m_bMemStreamingTerm;
	HANDLE m_hMemStreamingThread;
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
	UINT MemStreamingThreadMain();
	static UINT WINAPI MemStreamingThread(LPVOID pParam);
};
