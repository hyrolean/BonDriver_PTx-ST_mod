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
	Settings_.MAXDUR_FREQ = 1000; //Žü”g”’²®‚É”ï‚â‚·Å‘åŽžŠÔ(msec)
	Settings_.MAXDUR_TMCC = 1500; //TMCCŽæ“¾‚É”ï‚â‚·Å‘åŽžŠÔ(msec)
	Settings_.MAXDUR_TSID = 3000; //TSIDÝ’è‚É”ï‚â‚·Å‘åŽžŠÔ(msec)
	Settings_.StreamerPacketSize = SHAREDMEM_TRANSPORT_PACKET_SIZE ;
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

	const DWORD MAXDUR_FREQ = Settings_.MAXDUR_FREQ; //Žü”g”’²®‚É”ï‚â‚·Å‘åŽžŠÔ(msec)
	const DWORD MAXDUR_TMCC = Settings_.MAXDUR_TMCC; //TMCCŽæ“¾‚É”ï‚â‚·Å‘åŽžŠÔ(msec)
	const DWORD MAXDUR_TSID = Settings_.MAXDUR_TSID; //TSIDÝ’è‚É”ï‚â‚·Å‘åŽžŠÔ(msec)

	auto dur =[](DWORD s=0, DWORD e=GetTickCount()) -> DWORD {
		// duration ( s -> e )
		return s <= e ? e - s : 0xFFFFFFFFUL - s + 1 + e;
	};

	//ƒ`ƒ…[ƒjƒ“ƒO
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

		Sleep(60);

		if(Sate_) {
			if(!TSID) {
				for (DWORD t=0,s=dur(); t<MAXDUR_TMCC; t=dur(s)) {
					TMCC_STATUS tmcc;
					ZeroMemory(&tmcc,sizeof(tmcc));
					//std::fill_n(tmcc.Id,8,0xffff) ;
					if(Tuner_->GetTmcc(&tmcc)) {
						for (int i=0; i<8; i++) {
							WORD id = tmcc.u.bs.tsId[i]&0xffff;
							if ((id&0xff00) && (id^0xffff)) {
								if( (id&7) == Stream ) { //ƒXƒgƒŠ[ƒ€‚Éˆê’v‚µ‚½
									//ˆê’v‚µ‚½id‚É‘‚«Š·‚¦‚é
									TSID = id ;
									break;
								}
							}
						}
						if(TSID&~7UL) break ;
					}
					Sleep(50);
				}
			}
			if(!TSID) break ;
			for (DWORD t=0,s=dur(),n=0; t<MAXDUR_TSID ; t=dur(s)) {
				if(Tuner_->SetIdS(TSID)) { if(++n>=2) {tuned=true;break;} }
				Sleep(50);
			}
		}else {
			TMCC_STATUS tmcc;
			for (DWORD t=0,s=dur(); t<MAXDUR_TMCC; t=dur(s)) {
				if(Tuner_->GetTmcc(&tmcc)) {tuned=true;break;}
				Sleep(50);
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
BOOL CPTxWDMCmdServiceOperator::ResGetIdListS(TSIDLIST &TSIDList)
{
	critical_lock lock(&Critical_);
	KeepAlive();
	if(Tuner_) {
		TMCC_STATUS tmcc;
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
	critical_lock lock(&Critical_);
	KeepAlive();
	if(Tuner_) {
		if(Tuner_->SetIdS(TSID))
			return TRUE;
	}
	return FALSE;
}
//---------------------------------------------------------------------------
BOOL CPTxWDMCmdServiceOperator::ResSetLnbPower(BOOL Power)
{
	critical_lock lock(&Critical_);
	KeepAlive();
	if(Tuner_) {
		if(Tuner_->SetLnbPower(Power?BS_LNB_POWER_15V:BS_LNB_POWER_OFF))
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
