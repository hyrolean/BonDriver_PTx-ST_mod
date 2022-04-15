#pragma once

enum PTSTREAMING : DWORD {
	PTSTREAMING_PIPEIO=0,
	PTSTREAMING_SHAREDMEM=1
};

struct PTTSIDLIST { DWORD dwId[8]; };

class IPTManager
{
public:

	virtual ~IPTManager(void) { FreeSDK(); }

public:

	virtual BOOL LoadSDK() = 0 ;
	virtual BOOL Init() = 0 ;
	virtual void UnInit() = 0 ;

	virtual PTSTREAMING GetStreamingMethod()
	{ return m_bMemStreaming? PTSTREAMING_SHAREDMEM: PTSTREAMING_PIPEIO; }

	virtual DWORD GetTotalTunerCount() = 0 ;
	virtual DWORD GetActiveTunerCount(BOOL bSate) = 0 ;
	virtual BOOL SetLnbPower(int iID, BOOL bEnabled) = 0 ;

	virtual int OpenTuner(BOOL bSate) = 0 ;
	virtual int OpenTuner2(BOOL bSate, int iTunerID) = 0 ;
	virtual BOOL CloseTuner(int iID) = 0 ;

	virtual BOOL SetCh(int iID, unsigned long ulCh, DWORD dwTSID, BOOL &hasStream) = 0 ;
	virtual DWORD GetSignal(int iID) = 0 ;

	virtual BOOL SetFreq(int iID, unsigned long ulCh) = 0 ;
	virtual BOOL GetIdListS(int iID, PTTSIDLIST* pPtTSIDList) = 0 ;
	virtual BOOL GetIdS(int iID, DWORD *pdwTSID) = 0 ;
	virtual BOOL SetIdS(int iID, DWORD dwTSID) = 0 ;

	virtual BOOL IsFindOpen() = 0 ;

	virtual BOOL CloseChk() = 0 ;

protected:

	BOOL m_bUseLNB;
	BOOL m_bLNB11V;
	UINT m_uiVirtualCount;
	BOOL m_bMemStreaming;

	DWORD m_dwMaxDurFREQ;
	DWORD m_dwMaxDurTMCC, m_dwMaxDurTMCC_S;
	DWORD m_dwMaxDurTSID;

	BOOL m_bNoCheckFREQ;
	BOOL m_bNoCheckTSID;

	virtual void FreeSDK() {}
};

