#pragma once

#include "inc/EARTH_PT3.h"
#include "inc/Prefix.h"
#include "inc/EX_Buffer.h"
#include "../Common/BaseIO.h"

using namespace EARTH3;

class CDataIO3 : public CBaseIO
{
public:
	CDataIO3(BOOL bMemStreaming=FALSE);
	virtual ~CDataIO3(void);

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

	HANDLE m_hSetBuffEvent1;
	HANDLE m_hSetBuffEvent2;
	HANDLE m_hSetBuffEvent3;
	HANDLE m_hSetBuffEvent4;

protected:
	struct RECVTHREAD_PARAM {
		CDataIO3 *pSys;
		DWORD dwID;
		RECVTHREAD_PARAM(CDataIO3 *pSys_, DWORD dwID_)
		 : pSys(pSys_), dwID(dwID_) {}
	};
	static UINT WINAPI RecvThreadProc(LPVOID pParam);

	bool SetBuffLock1(DWORD timeout=IO_DEFAULT_TIMEOUT);
	void SetBuffUnLock1();
	bool SetBuffLock2(DWORD timeout=IO_DEFAULT_TIMEOUT);
	void SetBuffUnLock2();
	bool SetBuffLock3(DWORD timeout=IO_DEFAULT_TIMEOUT);
	void SetBuffUnLock3();
	bool SetBuffLock4(DWORD timeout=IO_DEFAULT_TIMEOUT);
	void SetBuffUnLock4();
	bool SetBuffLock(DWORD dwID, DWORD timeout=IO_DEFAULT_TIMEOUT) {
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

	void ChkTransferInfo();

	bool CheckReady(DWORD dwID);
	bool ReadAddBuff(DWORD dwID);

};
