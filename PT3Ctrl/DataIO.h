#pragma once

#include "inc/EARTH_PT3.h"
#include "inc/Prefix.h"
#include "inc/EX_Buffer.h"
#include "../Common/PTOutsideCtrlCmdDef.h"
#include "../Common/PipeServer.h"
#include "../Common/SharedMem.h"

#define DATA_TIMEOUT (10*1000)

using namespace EARTH;

class CDataIO
{
public:
	CDataIO(BOOL bMemStreaming=FALSE);
	~CDataIO(void);

	void SetDevice(PT::Device* pcDevice){ m_pcDevice = pcDevice; };
	void SetVirtualCount(UINT uiVirtualCount){ VIRTUAL_COUNT = uiVirtualCount*8; };
	void Run(PT::Device::ISDB enISDB, uint32 iTuner);
	void Stop();
	BOOL EnableTuner(int iID, BOOL bEnable);
	void StartPipeServer(int iID);
	void StopPipeServer(int iID);
	void ClearBuff(int iID);

	DWORD GetOverFlowCount(int iID);

protected:
	UINT VIRTUAL_COUNT;
	PT::Device* m_pcDevice;

	//HANDLE m_hStopEvent;
	BOOL m_bThTerm;
	HANDLE m_hThread1;
	HANDLE m_hThread2;
	HANDLE m_hThread3;
	HANDLE m_hThread4;

	CPipeServer m_cPipeT0;
	CPipeServer m_cPipeT1;
	CPipeServer m_cPipeS0;
	CPipeServer m_cPipeS1;

	/*
	typedef struct _BUFF_DATA {
		BYTE* pbBuff;
		DWORD dwSize;
		_BUFF_DATA(DWORD dw) : dwSize(dw){
			pbBuff = new BYTE[dw];
		}
		~_BUFF_DATA(){
			delete[] pbBuff;
		}
	} BUFF_DATA;

	deque<BUFF_DATA*> m_T0Buff;
	deque<BUFF_DATA*> m_T1Buff;
	deque<BUFF_DATA*> m_S0Buff;
	deque<BUFF_DATA*> m_S1Buff;
	*/

	PTBUFFER m_T0Buff;
	PTBUFFER m_T1Buff;
	PTBUFFER m_S0Buff;
	PTBUFFER m_S1Buff;

	EARTH::EX::Buffer* m_T0SetBuff;
	EARTH::EX::Buffer* m_T1SetBuff;
	EARTH::EX::Buffer* m_S0SetBuff;
	EARTH::EX::Buffer* m_S1SetBuff;

	uint32 m_T0WriteIndex;
	uint32 m_T1WriteIndex;
	uint32 m_S0WriteIndex;
	uint32 m_S1WriteIndex;

	DWORD m_dwT0OverFlowCount;
	DWORD m_dwT1OverFlowCount;
	DWORD m_dwS0OverFlowCount;
	DWORD m_dwS1OverFlowCount;

	HANDLE m_hEvent1;
	HANDLE m_hEvent2;
	HANDLE m_hEvent3;
	HANDLE m_hEvent4;

	HANDLE m_hBuffEvent1;
	HANDLE m_hBuffEvent2;
	HANDLE m_hBuffEvent3;
	HANDLE m_hBuffEvent4;

protected:
	static UINT WINAPI RecvThread1(LPVOID pParam);
	static UINT WINAPI RecvThread2(LPVOID pParam);
	static UINT WINAPI RecvThread3(LPVOID pParam);
	static UINT WINAPI RecvThread4(LPVOID pParam);

	bool Lock1(DWORD timeout=DATA_TIMEOUT);
	void UnLock1();
	bool Lock2(DWORD timeout=DATA_TIMEOUT);
	void UnLock2();
	bool Lock3(DWORD timeout=DATA_TIMEOUT);
	void UnLock3();
	bool Lock4(DWORD timeout=DATA_TIMEOUT);
	void UnLock4();

	bool BuffLock1(DWORD timeout=DATA_TIMEOUT);
	void BuffUnLock1();
	bool BuffLock2(DWORD timeout=DATA_TIMEOUT);
	void BuffUnLock2();
	bool BuffLock3(DWORD timeout=DATA_TIMEOUT);
	void BuffUnLock3();
	bool BuffLock4(DWORD timeout=DATA_TIMEOUT);
	void BuffUnLock4();

	void ChkTransferInfo();

	static int CALLBACK OutsideCmdCallbackT0(void* pParam, CMD_STREAM* pCmdParam, CMD_STREAM* pResParam, BOOL* pbResDataAbandon);
	static int CALLBACK OutsideCmdCallbackT1(void* pParam, CMD_STREAM* pCmdParam, CMD_STREAM* pResParam, BOOL* pbResDataAbandon);
	static int CALLBACK OutsideCmdCallbackS0(void* pParam, CMD_STREAM* pCmdParam, CMD_STREAM* pResParam, BOOL* pbResDataAbandon);
	static int CALLBACK OutsideCmdCallbackS1(void* pParam, CMD_STREAM* pCmdParam, CMD_STREAM* pResParam, BOOL* pbResDataAbandon);

	void CmdSendData(DWORD dwID, CMD_STREAM* pCmdParam, CMD_STREAM* pResParam, BOOL* pbResDataAbandon);

	bool CheckReady(EARTH::EX::Buffer* buffer, uint32 index);
	bool ReadAddBuff(EARTH::EX::Buffer* buffer, uint32 index, PTBUFFER &tsBuff, DWORD dwID, DWORD &OverFlow);

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
