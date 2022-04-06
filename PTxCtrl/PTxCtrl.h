#pragma once

#include "resource.h"
#include "../Common/PTxCtrlCmd.h"

void WINAPI service_main(DWORD dwArgc, LPTSTR *lpszArgv);
DWORD WINAPI service_ctrl(DWORD dwControl, DWORD dwEventType, LPVOID lpEventData, LPVOID lpContext);
BOOL SendStatusScm(int iState, int iExitcode, int iProgress);

void StartMain(BOOL bService);
void StopMain();


  // CPTxCtrlCmdServiceOperator

class CPTxCtrlCmdServiceOperator : public CPTxCtrlCmdOperator
{
	IPTManager *Pt1Manager, *Pt3Manager ;
	CPipeServer *PtPipeServer1, *PtPipeServer3 ;
	BOOL PtService;
	DWORD PtSupported;
	DWORD PtActivated;
	DWORD LastActivated;
	static BOOL PtTerminated;
	HANDLE HRTimerHandle;
	void ActivateHRTimer();
	void DeactivateHRTimer();
public:
	CPTxCtrlCmdServiceOperator(std::wstring name, BOOL bService);
	virtual ~CPTxCtrlCmdServiceOperator();
protected:
	// for Server Operations (override)
	BOOL ResSupported(DWORD &PtBits);
	BOOL ResActivatePt(DWORD PtVer);
public:
	void Main();
	static void Stop();
};

