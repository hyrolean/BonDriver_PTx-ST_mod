#pragma once

#include "inc/EarthPtIf.h"
#include "../Common/Util.h"
#include "../Common/PTManager.h"
#include "DataIO.h"

namespace EARTH {
	namespace OS {
		class Library {
			static	status NewBusFunction_(PT::Bus **lplpBus) {
				*lplpBus = new PT::Bus ;
				if(*lplpBus==nullptr)
					return PT::STATUS_OUT_OF_MEMORY_ERROR;
				return PT::STATUS_OK;
			}
		public:
			PT::Bus::NewBusFunction Function() const {
				return &NewBusFunction_;
			}
		};
	}
}

using namespace EARTH;

#ifndef PTW_GALAPAGOS // NOT galapagosization ( EARTH defact standard )

class CPTwManager : public IPTManager
{
public:
	CPTwManager(void);

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
		CDataIO cDataIO;
		_DEV_STATUS(void){
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

#else // begin of PTW_GALAPAGOS

#include "PTxWDMCmd.h"

class CPTwManager : public IPTManager
{
public:
	CPTwManager(void);

	BOOL LoadSDK();
	BOOL Init();
	void UnInit();

	PTSTREAMING GetStreamingMethod() {return PTSTREAMING_SHAREDMEM;}

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

	struct DEV_STATUS {
		CPTxWDMCmdOperator *cpOperatorT[2];
		HANDLE hProcessT[2];
		CPTxWDMCmdOperator *cpOperatorS[2];
		HANDLE hProcessS[2];
		bool Opened() {
			return cpOperatorT[0] || cpOperatorT[1] ||
				cpOperatorS[0] || cpOperatorS[1] ;
		}
		BOOL bLnbS0;
		BOOL bLnbS1;
		CDataIO cDataIO;
		DEV_STATUS(void){
			for(auto &v: cpOperatorT) v=nullptr;
			for(auto &v: cpOperatorS) v=nullptr;
			for(auto &h: hProcessT) h=NULL;
			for(auto &h: hProcessS) h=NULL;
			bLnbS0 = FALSE;
			bLnbS1 = FALSE;
		}
		~DEV_STATUS() {
			for(auto &v: cpOperatorT) if(v) {delete v; v=nullptr;}
			for(auto &v: cpOperatorS) if(v) {delete v; v=nullptr;}
			for(auto &h: hProcessT) if(h&&h!=INVALID_HANDLE_VALUE) {CloseHandle(h);h=NULL;}
			for(auto &h: hProcessS) if(h&&h!=INVALID_HANDLE_VALUE) {CloseHandle(h);h=NULL;}
		}
	};
	vector<DEV_STATUS*> m_EnumDev;

	wstring m_strExePath;

protected:
	void FreeSDK();
};

#endif // end of PTW_GALAPAGOS

