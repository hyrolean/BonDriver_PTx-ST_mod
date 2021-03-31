// BonTuner.h: CBonTuner クラスのインターフェイス
//
//////////////////////////////////////////////////////////////////////

#pragma once


#include "IBonDriver3.h"
#include "../Common/PTSendCtrlCmdUtil.h"
#include "../Common/PTxCtrlCmd.h"
#include "ParseChSet.h"

#define BUFF_SIZE (188*256)

void InitializeBonTuners(HMODULE hModule);
void FinalizeBonTuners();

class CBonTuner : public IBonDriver3
{
public:
	CBonTuner();
	virtual ~CBonTuner();

// IBonDriver
	const BOOL OpenTuner(void);
	void CloseTuner(void);

	const BOOL SetChannel(const BYTE bCh);
	const float GetSignalLevel(void);

	const DWORD WaitTsStream(const DWORD dwTimeOut = 0);
	const DWORD GetReadyCount(void);

	const BOOL GetTsStream(BYTE *pDst, DWORD *pdwSize, DWORD *pdwRemain);
	const BOOL GetTsStream(BYTE **ppDst, DWORD *pdwSize, DWORD *pdwRemain);

	void PurgeTsStream(void);

// IBonDriver2(暫定)
	LPCTSTR GetTunerName(void);

	const BOOL IsTunerOpening(void);

	LPCTSTR EnumTuningSpace(const DWORD dwSpace);
	LPCTSTR EnumChannelName(const DWORD dwSpace, const DWORD dwChannel);

	const BOOL SetChannel(const DWORD dwSpace, const DWORD dwChannel);

	const DWORD GetCurSpace(void);
	const DWORD GetCurChannel(void);

// IBonDriver3
	const DWORD GetTotalDeviceNum(void);
	const DWORD GetActiveDeviceNum(void);
	const BOOL SetLnbPower(const BOOL bEnable);

	void Release(void);

	static HINSTANCE m_hModule;

protected:
	CRITICAL_SECTION m_CriticalSection;
	HANDLE m_hOnStreamEvent;

	DWORD m_dwCurSpace;
	DWORD m_dwCurChannel;

	BOOL m_hasStream;

	PTBUFFER m_PtBuff;
	void FlushPtBuff(BOOL dispose=FALSE);

	HANDLE m_hStopEvent;
	HANDLE m_hThread;
	HANDLE m_hSharedMemTransportMutex;

	int m_iPT;
	int m_iID;
	int m_iTunerID;
	BOOL m_isPTxCtrl;
	BOOL m_isISDB_S;
	BOOL m_bBon3Lnb;
	BOOL m_bTrySpares;
	BOOL m_bFastScan;
	BOOL m_bXFirstPT3;
	DWORD m_dwSetChDelay;
	DWORD m_dwOpenTunerDuration;

	BOOL m_bExecPT[4] ;

	CPTSendCtrlCmd *m_pCmdSender ;
	CPTxCtrlCmdOperator PTxCtrlOp ;

	void GetTunerCounters(DWORD *lpdwTotal, DWORD *lpdwActive);

	wstring m_strDirPath;
	wstring m_strTunerName;

	CParseChSet m_chSet;
	void BuildDefSpace(wstring strIni);

	BOOL LaunchPTCtrl(int iPT);
	BOOL TryOpenTunerByID(int iTunerID, int *piID);
	BOOL TryOpenTuner();

protected:
	static UINT WINAPI RecvThreadPipeIOProc(LPVOID pParam);
	static UINT WINAPI RecvThreadSharedMemProc(LPVOID pParam);
};

