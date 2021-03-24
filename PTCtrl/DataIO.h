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

	PTBUFFER m_T0Buff;
	PTBUFFER m_T1Buff;
	PTBUFFER m_S0Buff;
	PTBUFFER m_S1Buff;

	CMicroPacketUtil m_cT0Micro;
	CMicroPacketUtil m_cT1Micro;
	CMicroPacketUtil m_cS0Micro;
	CMicroPacketUtil m_cS1Micro;

	DWORD m_dwT0OverFlowCount;
	DWORD m_dwT1OverFlowCount;
	DWORD m_dwS0OverFlowCount;
	DWORD m_dwS1OverFlowCount;

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
	UINT MemStreamingThreadMain();
	static UINT WINAPI MemStreamingThread(LPVOID pParam);
};
