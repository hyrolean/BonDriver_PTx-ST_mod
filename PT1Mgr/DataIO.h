#pragma once

//#define USE_DEQUE

#include "inc/EARTH_PT.h"
#include "inc/Prefix.h"
#include "../Common/BaseIO.h"
#include "MicroPacketUtil.h"

#define TRANSFER_SIZE (4096*PT::Device::BUFFER_PAGE_COUNT)
#define VIRTUAL_IMAGE_COUNT 4
//#define VIRTUAL_COUNT 8
#define LOCK_SIZE 4
#define READ_BLOCK_COUNT 8
#define READ_BLOCK_SIZE (TRANSFER_SIZE / READ_BLOCK_COUNT)

using namespace EARTH;

class CDataIO : public CBaseIO
{
public:
	CDataIO(BOOL bMemStreaming=FALSE);
	virtual ~CDataIO(void);

	void SetDevice(PT::Device* pcDevice){ m_pcDevice = pcDevice; };
	void SetVirtualCount(UINT uiVirtualCount){ VIRTUAL_COUNT = uiVirtualCount; };
	void Run(int iID);
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

	BYTE* m_bDMABuff;

protected:
	static UINT WINAPI RecvThreadProc(LPVOID pParam);

	bool WaitBlock();
	void CopyBlock();
	bool DispatchBlock();
	void Clear(uint virtualIndex, uint imageIndex, uint blockIndex);
	uint Read(uint virtualIndex, uint imageIndex, uint blockIndex) const;
	uint Offset(uint imageIndex, uint blockIndex, uint additionalOffset = 0) const;
	void MicroPacket(BYTE* pbPacket);

	void ResetDMA();

};
