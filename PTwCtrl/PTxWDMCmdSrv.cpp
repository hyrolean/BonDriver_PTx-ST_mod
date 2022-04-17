//===========================================================================
#include "stdafx.h"
#include "PtDrvWrap.h"

#include "PTxWDMCmdSrv.h"
#include "../Common/Util.h"
#include "../Common/PTOutsideCtrlCmdDef.h"
//---------------------------------------------------------------------------

using namespace std ;

//===========================================================================
namespace PRY8EAlByw {
//===========================================================================
// CPTxWDMCmdServiceOperator
//---------------------------------------------------------------------------
CPTxWDMCmdServiceOperator::CPTxWDMCmdServiceOperator(wstring name)
  : CPTxWDMCmdOperator(name,true)
{
	Tuner_ = NULL ;
	Terminated_ = FALSE ;
	StreamingEnabled_ = FALSE ;
	Settings_.dwSize = sizeof(Settings_);
	Settings_.CtrlPackets = SHAREDMEM_TRANSPORT_PACKET_NUM ;
	Settings_.StreamerThreadPriority = THREAD_PRIORITY_HIGHEST ;
	Settings_.MAXDUR_FREQ = 1000; //周波数調整に費やす最大時間(msec)
	Settings_.MAXDUR_TMCC = 1000; //TMCC取得に費やす最大時間(msec)
	Settings_.MAXDUR_TMCC_S = 1000; //TMCC(S側)取得に費やす最大時間(msec)
	Settings_.MAXDUR_TSID = 1000; //TSID設定に費やす最大時間(msec)
	Settings_.StreamerPacketSize = SHAREDMEM_TRANSPORT_PACKET_SIZE ;
	Settings_.LNB11V = FALSE;
	Settings_.PipeStreaming = FALSE;
	InitializeCriticalSection(&Critical_);
	KeepAlive();
}
//---------------------------------------------------------------------------
CPTxWDMCmdServiceOperator::~CPTxWDMCmdServiceOperator()
{
	ResCloseTuner();
	DeleteCriticalSection(&Critical_);
}
//---------------------------------------------------------------------------
BOOL CPTxWDMCmdServiceOperator::ResTerminate()
{
	critical_lock lock(&Critical_);
	KeepAlive();
	Terminated_=TRUE;
	return TRUE;
}
//---------------------------------------------------------------------------
BOOL CPTxWDMCmdServiceOperator::ResOpenTuner(BOOL Sate, DWORD TunerID)
{
	critical_lock lock(&Critical_);
	KeepAlive();
	ResCloseTuner();

	LINEDEBUG;
	auto tuner = new CPtDrvWrapper(Sate,TunerID) ;
	if(!tuner->HandleAllocated()) {
		LINEDEBUG;
		delete tuner;
		return FALSE;
	}

	Tuner_ = tuner;
	Sate_ = Sate;

	LINEDEBUG;
	return TRUE;
}
//---------------------------------------------------------------------------
BOOL CPTxWDMCmdServiceOperator::ResCloseTuner()
{
	critical_lock lock(&Critical_);
	KeepAlive();
	if(Tuner_) {
		delete Tuner_;
		Tuner_ = NULL;
	}
	return TRUE;
}
//---------------------------------------------------------------------------
BOOL CPTxWDMCmdServiceOperator::ResGetTunerCount(DWORD &Count)
{
	critical_lock lock(&Critical_);
	KeepAlive();
	Count = CPtDrvWrapper::TunerCount() ;
	return TRUE;
}
//---------------------------------------------------------------------------
BOOL CPTxWDMCmdServiceOperator::ResSetTunerSleep(BOOL Sleep)
{
	critical_lock lock(&Critical_);
	KeepAlive();
	if(Tuner_) {
		if(Tuner_->SetTunerSleep(Sleep?true:false))
			return TRUE;
	}
	return FALSE;
}
//---------------------------------------------------------------------------
BOOL CPTxWDMCmdServiceOperator::ResSetStreamEnable(BOOL Enable)
{
	critical_lock lock(&Critical_);
	KeepAlive();
	if(Tuner_) {
		if(Tuner_->SetStreamEnable(Enable?true:false)) {
			StreamingEnabled_ = Enable ;
			DBGOUT("service streaming: %s.\n",StreamingEnabled_?"Enabled":"Disabled");
			return TRUE;
		}
	}
	return FALSE;
}
//---------------------------------------------------------------------------
BOOL CPTxWDMCmdServiceOperator::ResIsStreamEnabled(BOOL &Enable)
{
	critical_lock lock(&Critical_);
	KeepAlive();
	if(Tuner_) {
		Enable = Tuner_->IsStreamEnabled();
		return Tuner_->LastErrorCode()==NOERROR? TRUE: FALSE;
	}
	return FALSE;
}
//---------------------------------------------------------------------------
BOOL CPTxWDMCmdServiceOperator::ResSetChannel(BOOL &Tuned, DWORD Freq,
	DWORD TSID, DWORD Stream)
{
	critical_lock lock(&Critical_);
	KeepAlive();
	if(!Tuner_) return FALSE ;

	const DWORD MAXDUR_FREQ = Settings_.MAXDUR_FREQ; //周波数調整に費やす最大時間(msec)
	const DWORD MAXDUR_TMCC = Settings_.MAXDUR_TMCC; //TMCC取得に費やす最大時間(msec)
	const DWORD MAXDUR_TMCC_S = Settings_.MAXDUR_TMCC_S; //TMCC(S側)取得に費やす最大時間(msec)

	//チューニング
	bool tuned=false ;

	do {

		{
			bool done=false ;
			for (DWORD t=0,s=dur(),n=0; t<MAXDUR_FREQ; t=dur(s)) {
				if(Tuner_->SetFrequency(Freq))
				{done=true;break;}
			}
			if(!done) break ;
		}

		HRSleep(60);

		if(Sate_) {
			if(!TSID) {
				for (DWORD t=0,s=dur(); t<MAXDUR_TMCC_S; t=dur(s)) {
					TMCC_STATUS tmcc;
					ZeroMemory(&tmcc,sizeof(tmcc));
					//std::fill_n(tmcc.Id,8,0xffff) ;
					if(Tuner_->GetTmcc(&tmcc)) {
						for (int i=0; i<8; i++) {
							WORD id = tmcc.u.bs.tsId[i]&0xffff;
							if ((id&0xff00) && (id^0xffff)) {
								if(Stream<=7) { // 下位３ビット一致対象
									if( (id&7) == Stream ) { //ストリームの下位3ビットに一致した
										//一致したidに書き換える
										TSID = id ;
										break;
									}
								}else { // 完全一致対象
									if( id == Stream ) { //ストリームと完全に一致した
										//一致したidに書き換える
										TSID = id ;
										break;
									}
								}
							}
						}
						if(TSID&~7UL) break ;
					}
					HRSleep(0,500);
				}
			}
			if(!TSID) break ;
			if(ResSetIdS(TSID)) tuned = true ;
		}else {
			TMCC_STATUS tmcc;
			for (DWORD t=0,s=dur(); t<MAXDUR_TMCC; t=dur(s)) {
				if(Tuner_->GetTmcc(&tmcc)) {tuned=true;break;}
				HRSleep(50);
			}
		}

	}while(0);

	Tuned = tuned ? TRUE : FALSE ;

	return TRUE ;
}
//---------------------------------------------------------------------------
BOOL CPTxWDMCmdServiceOperator::ResSetFreq(DWORD Freq)
{
	critical_lock lock(&Critical_);
	KeepAlive();
	if(Tuner_) {
		if(Tuner_->SetFrequency(Freq))
			return TRUE ;
	}
	return FALSE;
}
//---------------------------------------------------------------------------
BOOL CPTxWDMCmdServiceOperator::ResCurFreq(DWORD &Freq)
{
	critical_lock lock(&Critical_);
	KeepAlive();
	if(Tuner_) {
		Freq = Tuner_->CurFrequency();
		return Tuner_->LastErrorCode()==NOERROR? TRUE: FALSE;
	}
	return FALSE;
}
//---------------------------------------------------------------------------
BOOL CPTxWDMCmdServiceOperator::ResGetIdListS(TSIDLIST &TSIDList)
{
	critical_lock lock(&Critical_);
	KeepAlive();
	if(Tuner_) {
		TMCC_STATUS tmcc;
		ZeroMemory(&tmcc,sizeof(tmcc));
		if(Tuner_->GetTmcc(&tmcc)) {
			for (int i=0 ; i<8 ; i++)
				TSIDList.Id[i] = tmcc.u.bs.tsId[i] ;
            return TRUE;
		}
	}
	return FALSE;
}
//---------------------------------------------------------------------------
BOOL CPTxWDMCmdServiceOperator::ResGetIdS(DWORD &TSID)
{
	critical_lock lock(&Critical_);
	KeepAlive();
	if(Tuner_) {
		TSID = Tuner_->CurIdS();
		return Tuner_->LastErrorCode()==NOERROR? TRUE: FALSE;
	}
	return FALSE;
}
//---------------------------------------------------------------------------
BOOL CPTxWDMCmdServiceOperator::ResSetIdS(DWORD TSID)
{
	const DWORD MAXDUR_TSID = Settings_.MAXDUR_TSID; //TSID設定に費やす最大時間(msec)
	critical_lock lock(&Critical_);
	KeepAlive();
	if(Tuner_) {
		BOOL bRes = FALSE ;
		for (DWORD t=0,s=dur(),n=0; t<MAXDUR_TSID ; t=dur(s)) {
			if(Tuner_->SetIdS(TSID)) { if(++n>=2) {bRes=TRUE;break;} }
			HRSleep(50);
		}
		return bRes;
	}
	return FALSE;
}
//---------------------------------------------------------------------------
BOOL CPTxWDMCmdServiceOperator::ResSetLnbPower(BOOL Power)
{
	critical_lock lock(&Critical_);
	KeepAlive();
	if(Tuner_) {
		if(Tuner_->SetLnbPower(
			Power?
				(Settings_.LNB11V?BS_LNB_POWER_11V:BS_LNB_POWER_15V):
				BS_LNB_POWER_OFF	))
			return TRUE;
	}
	return FALSE;
}
//---------------------------------------------------------------------------
BOOL CPTxWDMCmdServiceOperator::ResGetCnAgc(DWORD &Cn100, DWORD &CurAgc,
	DWORD &MaxAgc)
{
	critical_lock lock(&Critical_);
	KeepAlive();
	if(Tuner_) {
		UINT cn100, curAgc, maxAgc;
		if(Tuner_->GetCnAgc(&cn100,&curAgc,&maxAgc)) {
			Cn100 = cn100; CurAgc = curAgc; MaxAgc = maxAgc;
			return TRUE;
		}
	}
	return FALSE;
}
//---------------------------------------------------------------------------
BOOL CPTxWDMCmdServiceOperator::ResPurgeStream()
{
	critical_lock lock(&Critical_);
	KeepAlive();
	if(Tuner_) {
		if(Tuner_->PurgeStream())
			return TRUE;
	}
	return FALSE;
}
//---------------------------------------------------------------------------
BOOL CPTxWDMCmdServiceOperator::ResSetupServer(const SERVER_SETTINGS *Options)
{
	critical_lock lock(&Critical_);
	KeepAlive();
	if(Options->dwSize<=sizeof(SERVER_SETTINGS)) {
		CopyMemory(&Settings_,Options,Options->dwSize);
		Settings_.dwSize = sizeof(Settings_);
		return TRUE ;
	}
	return FALSE;
}
//---------------------------------------------------------------------------
DWORD CPTxWDMCmdServiceOperator::CurStreamSize()
{
	critical_lock lock(&Critical_);
	if(Tuner_) {
		return Tuner_->CurStreamSize();
	}
	return 0;
}
//---------------------------------------------------------------------------
BOOL CPTxWDMCmdServiceOperator::GetStreamData(LPVOID data, DWORD &size)
{
	critical_lock lock(&Critical_);
	if(Tuner_) {
		UINT szOut = 0 ;
		BOOL res = Tuner_->GetStreamData(data, size, &szOut) ? TRUE : FALSE ;
		if(res) size = szOut ;
		return res;
	}
	return FALSE;
}
//---------------------------------------------------------------------------
} // End of namespace PRY8EAlByw
//===========================================================================
