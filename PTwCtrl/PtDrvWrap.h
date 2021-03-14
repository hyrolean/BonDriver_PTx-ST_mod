#pragma once

#include <Windows.h>
#include <strsafe.h>
#include <string>
#include <map>

#ifndef	NTSTATUS
#define	NTSTATUS	long
#endif

#include "inc/PtDrvIfLib.h"


namespace PRY8EAlByw {

class CPtDrvWrapper
{
protected:
	using NTSTATUSMAP = std::map<DWORD,NTSTATUS> ;
	NTSTATUSMAP StatusMap_;
	CRITICAL_SECTION Critical_;
	std::wstring DevName_;
	HANDLE Handle_;

protected:
	void Initialize() {
		InitializeCriticalSection(&Critical_);
	}
	void Finalize() {
		DeleteCriticalSection(&Critical_);
	}
	bool MakeStatus(DWORD status) {
		EnterCriticalSection(&Critical_);
		StatusMap_[GetCurrentThreadId()] = status ;
		LeaveCriticalSection(&Critical_);
		return status==NOERROR;
	}

public:
	CPtDrvWrapper(BOOL sate, int id) {
		Initialize();
		WCHAR   wcTunerName[32];
		ZeroMemory(wcTunerName,sizeof(wcTunerName));
		StringCchPrintf( wcTunerName, 32,
			sate ? PT_DEV_BS_NAME : PT_DEV_BT_NAME , (id>>1)+1 , (id&1)+1 ) ;
		DevName_ = wcTunerName ;
		Handle_ = PtDrvOpen(wcTunerName);
	}
	~CPtDrvWrapper() {
		Close();
		Finalize();
	}
	std::wstring DevName() { return DevName_; }
	HANDLE Handle() { return Handle_; }
	bool HandleAllocated() {
		return Handle_!=INVALID_HANDLE_VALUE ;
	}
	BOOL Close() {
		BOOL result=FALSE ;
		if(HandleAllocated()) {
	  		if(result = PtDrvClose(Handle_))
				Handle_=INVALID_HANDLE_VALUE;
		}
		return result ;
	}
	static int TunerCount() { return PtDrvGetDevCount() <<1 ; }
	NTSTATUS LastErrorCode() {
		NTSTATUS code = NOERROR ;
		EnterCriticalSection(&Critical_);
		auto pos = StatusMap_.find(GetCurrentThreadId()) ;
		if(pos!=StatusMap_.end())
			code= pos->second ;
		LeaveCriticalSection(&Critical_);
		return code;
	}
	UINT Version() {
		UINT version=0;
		MakeStatus(PtDrvGetVersion(Handle_, &version));
		return version;
	}
	UINT PciClockCounter() {
		UINT counter=0;
		MakeStatus(PtDrvGetPciClockCounter(Handle_, &counter));
		return counter;
	}
	bool SetPciLatencyTimer(BYTE latencyTimer) {
		return MakeStatus(PtDrvSetPciLatencyTimer(Handle_,latencyTimer));
	}
	BYTE PciLatencyTimer() {
		BYTE latencyTimer=0;
		MakeStatus(PtDrvGetPciLatencyTimer(Handle_,&latencyTimer));
		return latencyTimer;
	}
	bool SetLnbPower(BS_LNB_POWER lnbPower) {
		return MakeStatus(PtDrvSetLnbPower(Handle_,lnbPower));
	}
	BS_LNB_POWER CurLnbPower() {
		BS_LNB_POWER lnbPower=BS_LNB_POWER_OFF;
		MakeStatus(PtDrvGetLnbPower(Handle_,&lnbPower));
		return lnbPower;
	}
	bool SetTunerSleep(bool sleep) {
		return MakeStatus(PtDrvSetTunerSleep(Handle_,sleep));
	}
	bool IsTunerSleeping() {
		bool sleep=false;
		MakeStatus(PtDrvGetTunerSleep(Handle_,&sleep));
		return sleep;
	}
	int MaxChannels() {
		int channelMax=0;
		MakeStatus(PtDrvGetChannelMax(Handle_,&channelMax));
		return channelMax;
	}
	bool GetChannelInfo(UINT channel, int *pFrequency, std::string *pName) {
		const char *name=NULL;
		bool res = MakeStatus(PtDrvGetChannelInfo(Handle_,channel,pFrequency,&name));
		if(pName) *pName = std::string(name?name:"");
		return res;
	}
	bool SetFrequency(UINT channel) {
		return MakeStatus(PtDrvSetFrequency(Handle_,channel));
	}
	UINT CurFrequency() {
		UINT channel=0;
		MakeStatus(PtDrvGetFrequency(Handle_,&channel));
		return channel;
	}
	bool GetFrequencyStatus(int *clock,int *carrier) {
		return MakeStatus(PtDrvGetFrequencyStatus(Handle_,clock,carrier));
	}
	bool GetCnAgc(UINT *cn100,UINT *curAgc,UINT *maxAgc) {
		return MakeStatus(PtDrvGetCnAgc(Handle_,cn100,curAgc,maxAgc));
	}
	bool SetIdS(UINT id) {
		return MakeStatus(PtDrvSetIdS(Handle_,id));
	}
	UINT CurIdS() {
		UINT id=0;
		MakeStatus(PtDrvGetIdS(Handle_,&id));
		return id;
	}
	bool GetCorrectedErrorRate(LAYER_INDEX layIndex, TS_ERR_RATE *errRate) {
		return MakeStatus(PtDrvGetCorrectedErrorRate(Handle_,layIndex,errRate));
	}
	bool ResetCorrectedErrorCount() {
		return MakeStatus(PtDrvResetCorrectedErrorCount(Handle_));
	}
	UINT ErrorCount() {
		UINT count=0;
		MakeStatus(PtDrvGetErrorCount(Handle_,&count));
		return count;
	}
	bool GetTunerStatus(BT_TUNER_STATUS *status) {
		return MakeStatus(PtDrvGetTunerStatus(Handle_,status));
	}
	bool GetTmcc(TMCC_STATUS *tmcc) {
		return MakeStatus(PtDrvGetTmcc(Handle_,tmcc));
	}
	bool GetLayerS(BS_LAYER_STATUS *layerS) {
		return MakeStatus(PtDrvGetLayerS(Handle_,layerS));
	}
	bool GetLockedT(bool locked[3]) {
		return MakeStatus(PtDrvGetLockedT(Handle_,locked));
	}
	bool SetLayerEnable(LAYER_MASK *layerMask) {
		return MakeStatus(PtDrvSetLayerEnable(Handle_,layerMask));
	}
	bool GetLayerEnable(LAYER_MASK *layerMask) {
		return MakeStatus(PtDrvGetLayerEnable(Handle_,layerMask));
	}
	bool SetStreamEnable(bool enable) {
		return MakeStatus(PtDrvSetStreamEnable(Handle_,enable));
	}
	bool IsStreamEnabled() {
		bool enable=false;
		MakeStatus(PtDrvGetStreamEnable(Handle_,&enable));
		return enable;
	}
	bool GetTransferInfo(DMA_INFOMATION *pInfo) {
		return MakeStatus(PtDrvGetTransferInfo(Handle_,pInfo));
	}
	bool HasTransferPktErr() {
	    bool dmaPktErr=false;
		MakeStatus(PtDrvGetTransferPktErr(Handle_,&dmaPktErr));
		return dmaPktErr;
	}
	UINT CurStreamSize() {
		UINT tsSize=0;
		MakeStatus(PtDrvGetStreamSize(Handle_,&tsSize));
		return tsSize;
	}
	bool GetStreamData(void *ts,UINT size,UINT *outSize) {
		return MakeStatus(PtDrvGetStreamData(Handle_,ts,size,outSize));
	}
	bool PurgeStream() {
		return MakeStatus(PtDrvGetStreamFlush(Handle_));
	}

};

} // End of namespace PRY8EAlByw

using namespace PRY8EAlByw ;
