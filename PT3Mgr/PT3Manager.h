#pragma once

#include "inc/EARTH_PT3.h"
#include "inc/OS_Library.h"
#include "../Common/Util.h"
#include "../Common/PTManager.h"
#include "DataIO3.h"

using namespace EARTH3;

class CPT3Manager : public IPTManager
{
public:
	CPT3Manager(void);

	BOOL LoadSDK();
	BOOL Init();
	void UnInit();

	DWORD GetTotalTunerCount() { return DWORD(m_EnumDev.size()<<1) ; }
	DWORD GetActiveTunerCount(BOOL bSate);
	BOOL SetLnbPower(int iID, BOOL bEnabled);

	int OpenTuner(BOOL bSate);
	int OpenTuner2(BOOL bSate, int iTunerID);
	BOOL CloseTuner(int iID);

	BOOL SetCh(int iID, unsigned long ulCh, DWORD dwTSID, BOOL &hasStream);
	DWORD GetSignal(int iID);

	BOOL IsFindOpen();

	BOOL CloseChk();

protected:
	OS::Library* m_cLibrary;
	PT::Bus* m_cBus;

	typedef struct _DEV_STATUS{
		PT::Bus::DeviceInfo stDevInfo;
		PT::Device* pcDevice;
		BOOL bOpen;
		BOOL bUseT0;
		BOOL bUseT1;
		BOOL bUseS0;
		BOOL bUseS1;
		BOOL bLnbS0;
		BOOL bLnbS1;
		CDataIO3 cDataIO;
		_DEV_STATUS(BOOL bMemStreaming=FALSE) : cDataIO(bMemStreaming) {
			bOpen = FALSE;
			pcDevice = NULL;
			bUseT0 = FALSE;
			bUseT1 = FALSE;
			bUseS0 = FALSE;
			bUseS1 = FALSE;
			bLnbS0 = FALSE;
			bLnbS1 = FALSE;
		}
	}DEV_STATUS;
	vector<DEV_STATUS*> m_EnumDev;

protected:
	void FreeSDK();
};
