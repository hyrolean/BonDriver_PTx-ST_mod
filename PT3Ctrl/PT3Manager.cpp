#include "StdAfx.h"
#include <Shlwapi.h>
#pragma comment(lib, "Shlwapi.lib")

#include "PT3Manager.h"


CPT3Manager::CPT3Manager(void)
{
	m_cLibrary = NULL;
	m_cBus = NULL;

	WCHAR strExePath[512] = L"";
	GetModuleFileName(NULL, strExePath, 512);

	WCHAR szPath[_MAX_PATH];	// パス
	WCHAR szDrive[_MAX_DRIVE];
	WCHAR szDir[_MAX_DIR];
	WCHAR szFname[_MAX_FNAME];
	WCHAR szExt[_MAX_EXT];
	_tsplitpath_s( strExePath, szDrive, _MAX_DRIVE, szDir, _MAX_DIR, szFname, _MAX_FNAME, szExt, _MAX_EXT );
	_tmakepath_s(  szPath, _MAX_PATH, szDrive, szDir, NULL, NULL );

	wstring strIni;

	strIni = szPath;
	strIni += L"\\BonDriver_PT3-ST.ini";

	if(!PathFileExists(strIni.c_str())) {
		strIni = szPath;
		strIni += L"\\BonDriver_PTx-ST.ini";
	}

	m_bUseLNB = GetPrivateProfileInt(L"SET", L"UseLNB", 0, strIni.c_str());
	m_uiVirtualCount = GetPrivateProfileInt(L"SET", L"DMABuff", 8, strIni.c_str());
	if( m_uiVirtualCount == 0 ){
		m_uiVirtualCount = 8;
	}
}

BOOL CPT3Manager::LoadSDK()
{
	if( m_cLibrary == NULL ){
		m_cLibrary = new OS::Library();

		PT::Bus::NewBusFunction function = m_cLibrary->Function();
		if (function == NULL) {
			SAFE_DELETE(m_cLibrary);
			return FALSE;
		}
		status enStatus = function(&m_cBus);
		if( enStatus != PT::STATUS_OK ){
			SAFE_DELETE(m_cLibrary);
			return FALSE;
		}

		//バージョンチェック
		uint32 version;
		m_cBus->GetVersion(&version);
		if ((version >> 8) != 2) {
			m_cBus->Delete();
			m_cBus = NULL;
			SAFE_DELETE(m_cLibrary);
			return FALSE;
		}
	}
	return TRUE;
}

void CPT3Manager::FreeSDK()
{
	for( int i=0; i<(int)m_EnumDev.size(); i++ ){
		if( m_EnumDev[i]->bOpen ){
			m_EnumDev[i]->cDataIO.Stop();
			m_EnumDev[i]->pcDevice->Close();
			m_EnumDev[i]->pcDevice->Delete();
		}
		SAFE_DELETE(m_EnumDev[i]);
	}
	m_EnumDev.clear();

	if( m_cBus != NULL ){
		m_cBus->Delete();
		m_cBus = NULL;
	}
	if( m_cLibrary != NULL ){
		SAFE_DELETE(m_cLibrary);
	}
}

BOOL CPT3Manager::Init()
{
	if( m_cBus == NULL ){
		return FALSE;
	}
	m_EnumDev.clear();

	PT::Bus::DeviceInfo deviceInfo[9];
	uint32 deviceInfoCount = sizeof(deviceInfo)/sizeof(*deviceInfo);
	m_cBus->Scan(deviceInfo, &deviceInfoCount);

	for (uint32 i=0; i<deviceInfoCount; i++) {
		DEV_STATUS* pItem = new DEV_STATUS;
		pItem->stDevInfo = deviceInfo[i];
		m_EnumDev.push_back(pItem);
	}

	return TRUE;
}

void CPT3Manager::UnInit()
{
	FreeSDK();
}

BOOL CPT3Manager::IsFindOpen()
{
	BOOL bFind = FALSE;
	for( int i=0; i<(int)m_EnumDev.size(); i++ ){
		if( m_EnumDev[i]->bOpen ){
			bFind = TRUE;
		}
	}
	return bFind;
}

DWORD CPT3Manager::GetActiveTunerCount(BOOL bSate)
{
	DWORD total_used=0;
	for( int i=0; i<(int)m_EnumDev.size(); i++ ){
		if( bSate == FALSE ){
			if(m_EnumDev[i]->bUseT0) total_used++;
			if(m_EnumDev[i]->bUseT1) total_used++;
		}else {
			if(m_EnumDev[i]->bUseS0) total_used++;
			if(m_EnumDev[i]->bUseS1) total_used++;
		}
	}
	return total_used;
}

BOOL CPT3Manager::SetLnbPower(int iID, BOOL bEnabled)
{
	size_t iDevID = iID>>16;
	PT::Device::ISDB enISDB = (PT::Device::ISDB)((iID&0x0000FF00)>>8);
	uint32 iTuner = iID&0x000000FF;

	if( enISDB != PT::Device::ISDB_S || iDevID>=m_EnumDev.size() )
		return FALSE;

	if( !m_EnumDev[iDevID]->bUseS0 && !m_EnumDev[iDevID]->bUseS1 )
		return FALSE;

	BOOL bCurLnb = m_EnumDev[iDevID]->bLnbS0 || m_EnumDev[iDevID]->bLnbS1 ;
	if( iTuner == 0 )	m_EnumDev[iDevID]->bLnbS0 = bEnabled ;
	else				m_EnumDev[iDevID]->bLnbS1 = bEnabled ;
	BOOL bNewLnb = m_EnumDev[iDevID]->bLnbS0 || m_EnumDev[iDevID]->bLnbS1 ;

	if(bCurLnb != bNewLnb) {
		m_EnumDev[iDevID]->pcDevice->SetLnbPower(
			bNewLnb ?
				PT::Device::LNB_POWER_15V :
				PT::Device::LNB_POWER_OFF
		);
	}

	return TRUE;
}

int CPT3Manager::OpenTuner(BOOL bSate)
{
	int iID = -1;
	int iDevID = -1;
	PT::Device::ISDB enISDB;
	uint32 iTuner = -1;
	//空きを探す
	if( bSate == FALSE ){
		//地デジ
		for( int i=0; i<(int)m_EnumDev.size(); i++ ){
			if( m_EnumDev[i]->bUseT0 == FALSE ){
				iID = (i<<16) | (PT::Device::ISDB_T<<8) | 0;
				iDevID = i;
				enISDB = PT::Device::ISDB_T;
				iTuner = 0;
				break;
			}else if( m_EnumDev[i]->bUseT1 == FALSE ){
				iID = (i<<16) | (PT::Device::ISDB_T<<8) | 1;
				iDevID = i;
				enISDB = PT::Device::ISDB_T;
				iTuner = 1;
				break;
			}
		}
	}else{
		//BS/CS
		for( int i=0; i<(int)m_EnumDev.size(); i++ ){
			if( m_EnumDev[i]->bUseS0 == FALSE ){
				iID = (i<<16) | (PT::Device::ISDB_S<<8) | 0;
				iDevID = i;
				enISDB = PT::Device::ISDB_S;
				iTuner = 0;
				break;
			}else if( m_EnumDev[i]->bUseS1 == FALSE ){
				iID = (i<<16) | (PT::Device::ISDB_S<<8) | 1;
				iDevID = i;
				enISDB = PT::Device::ISDB_S;
				iTuner = 1;
				break;
			}
		}
	}
	if( iID == -1 ){
		return -1;
	}
	status enStatus;
	if( m_EnumDev[iDevID]->bOpen == FALSE ){
		//デバイス初オープン
		enStatus = m_cBus->NewDevice(&m_EnumDev[iDevID]->stDevInfo, &m_EnumDev[iDevID]->pcDevice, NULL);
		if( enStatus != PT::STATUS_OK ){
			return -1;
		}
		for( int i = 0; i < 5; i++ ){
			enStatus = m_EnumDev[iDevID]->pcDevice->Open();
			if( enStatus == PT::STATUS_OK ){
				break;
			}
			m_EnumDev[iDevID]->pcDevice->Close();
			Sleep(10);
		}
		if (enStatus != PT::STATUS_OK){
			m_EnumDev[iDevID]->pcDevice->Delete();
			m_EnumDev[iDevID]->pcDevice = NULL;
			return -1;
		}

		enStatus = m_EnumDev[iDevID]->pcDevice->InitTuner();
		if( enStatus != PT::STATUS_OK ){
			m_EnumDev[iDevID]->pcDevice->Close();
			m_EnumDev[iDevID]->pcDevice->Delete();
			m_EnumDev[iDevID]->pcDevice = NULL;
			return -1;
		}

		for (uint32 i=0; i<2; i++) {
			for (uint32 j=0; j<2; j++) {
				enStatus = m_EnumDev[iDevID]->pcDevice->SetTunerSleep(static_cast<PT::Device::ISDB>(j), i, true);
				if( enStatus != PT::STATUS_OK ){
					m_EnumDev[iDevID]->pcDevice->Close();
					m_EnumDev[iDevID]->pcDevice->Delete();
					m_EnumDev[iDevID]->pcDevice = NULL;
					return -1;
				}
			}
		}
		m_EnumDev[iDevID]->bOpen = TRUE;
		m_EnumDev[iDevID]->cDataIO.SetVirtualCount(m_uiVirtualCount);
		m_EnumDev[iDevID]->cDataIO.SetDevice(m_EnumDev[iDevID]->pcDevice);
		m_EnumDev[iDevID]->cDataIO.Run();
	}
	//スリープから復帰
	enStatus = m_EnumDev[iDevID]->pcDevice->SetTunerSleep(enISDB, iTuner, false);
	if( enStatus != PT::STATUS_OK ){
		return -1;
	}

	if( m_bUseLNB && enISDB == PT::Device::ISDB_S){
		if( iTuner == 0 )	m_EnumDev[iDevID]->bLnbS0 = TRUE;
		else				m_EnumDev[iDevID]->bLnbS1 = TRUE;
		m_EnumDev[iDevID]->pcDevice->SetLnbPower(PT::Device::LNB_POWER_15V);
	}

	if( enISDB == PT::Device::ISDB_T ){
		if( iTuner == 0 ){
			m_EnumDev[iDevID]->bUseT0 = TRUE;
		}else{
			m_EnumDev[iDevID]->bUseT1 = TRUE;
		}
	}else{
		if( iTuner == 0 ){
			m_EnumDev[iDevID]->bUseS0 = TRUE;
		}else{
			m_EnumDev[iDevID]->bUseS1 = TRUE;
		}
	}
	m_EnumDev[iDevID]->cDataIO.StartPipeServer(iID);
	m_EnumDev[iDevID]->cDataIO.EnableTuner(iID, TRUE);

	return iID;
}

BOOL CPT3Manager::CloseTuner(int iID)
{
	int iDevID = iID>>16;
	PT::Device::ISDB enISDB = (PT::Device::ISDB)((iID&0x0000FF00)>>8);
	uint32 iTuner = iID&0x000000FF;

	if( (int)m_EnumDev.size() <= iDevID ){
		return FALSE;
	}

	status enStatus;

	m_EnumDev[iDevID]->cDataIO.EnableTuner(iID, FALSE);
	m_EnumDev[iDevID]->cDataIO.StopPipeServer(iID);

	enStatus = m_EnumDev[iDevID]->pcDevice->SetTunerSleep(enISDB, iTuner, true);
	if( enStatus != PT::STATUS_OK ){
		return FALSE;
	}

	if( enISDB == PT::Device::ISDB_T ){
		if( iTuner == 0 ){
			m_EnumDev[iDevID]->bUseT0 = FALSE;
		}else{
			m_EnumDev[iDevID]->bUseT1 = FALSE;
		}
	}else{
		if( iTuner == 0 ){
			m_EnumDev[iDevID]->bUseS0 = FALSE;
		}else{
			m_EnumDev[iDevID]->bUseS1 = FALSE;
		}
	}

	if (m_bUseLNB) {
		if(enISDB == PT::Device::ISDB_S) {
			if( iTuner == 0 )	m_EnumDev[iDevID]->bLnbS0 = FALSE;
			else				m_EnumDev[iDevID]->bLnbS1 = FALSE;
		}
		if(m_EnumDev[iDevID]->bLnbS0 == FALSE && m_EnumDev[iDevID]->bLnbS1 == FALSE)
			m_EnumDev[iDevID]->pcDevice->SetLnbPower(PT::Device::LNB_POWER_OFF);
	}

	if( m_EnumDev[iDevID]->bUseT0 == FALSE &&
		m_EnumDev[iDevID]->bUseT1 == FALSE &&
		m_EnumDev[iDevID]->bUseS0 == FALSE &&
		m_EnumDev[iDevID]->bUseS1 == FALSE ){
			//全部使ってなければクローズ
			m_EnumDev[iDevID]->cDataIO.Stop();
			m_EnumDev[iDevID]->pcDevice->Close();
			m_EnumDev[iDevID]->pcDevice->Delete();
			m_EnumDev[iDevID]->pcDevice = NULL;
			m_EnumDev[iDevID]->bOpen = FALSE;
	}

	return TRUE;
}

// MARK : BOOL CPT3Manager::SetCh(int iID, unsigned long ulCh, DWORD dwTSID, BOOL &hasStream)
BOOL CPT3Manager::SetCh(int iID, unsigned long ulCh, DWORD dwTSID, BOOL &hasStream)
{
	const DWORD MAXDUR_FREQ = 1000; //周波数調整に費やす最大時間(msec)
	const DWORD MAXDUR_TMCC = 1500; //TMCC取得に費やす最大時間(msec)
	const DWORD MAXDUR_TSID = 3000; //TSID設定に費やす最大時間(msec)

	auto dur =[](DWORD s=0, DWORD e=GetTickCount()) -> DWORD {
		// duration ( s -> e )
		return s <= e ? e - s : 0xFFFFFFFFUL - s + 1 + e;
	};

	uint32 ch = ulCh & 0xffff;
	sint32 offset = (ulCh >> 16) & 0xffff;
	if (offset >= 32768) offset -= 65536;

	_OutputDebugString(L"CPT3Manager::SetCh: iID=%d ch=%d offset=%d dwTSID=%d\n",iID,ch,offset,dwTSID) ;

	int iDevID = iID>>16;
	PT::Device::ISDB enISDB = (PT::Device::ISDB)((iID&0x0000FF00)>>8);
	uint32 iTuner = iID&0x000000FF;

    hasStream = TRUE ;

	if( (int)m_EnumDev.size() <= iDevID ){
		return FALSE;
	}

	status enStatus;
	for (DWORD t=0,s=dur(),n=0; t<MAXDUR_FREQ; t=dur(s)) {
		enStatus = m_EnumDev[iDevID]->pcDevice->SetFrequency(enISDB, iTuner, ch, offset);
		if( enStatus == PT::STATUS_OK ) {
			if(++n>=2) {
				_OutputDebugString(L"CPT3Manager::SetCh: Device::SetFrequency: ISDB:%d tuner:%d ch:%d\n",enISDB,iTuner,ch);
				break ;
			}
		}
		Sleep(50);
	}
	if( enStatus != PT::STATUS_OK ) {
		_OutputDebugString(L"CPT3Manager::SetCh: Device::SetFrequency failure!\n") ;
		return FALSE ;
	}
	if( enISDB == PT::Device::ISDB_S ){
		hasStream = FALSE ;
		if(!(dwTSID&~7UL)) {
			//dwTSIDに0～7が指定された場合は、TSIDをチューナーから取得して
			//下位３ビットと一致するものに書き換える
			// by 2020 LVhJPic0JSk5LiQ1ITskKVk9UGBg
			Sleep(50);
			for (DWORD t=0,s=dur(); t<MAXDUR_TMCC; t=dur(s)) {
				PT::Device::TmccS tmcc;
				ZeroMemory(&tmcc,sizeof(tmcc));
				//std::fill_n(tmcc.Id,8,0xffff) ;
				enStatus = m_EnumDev[iDevID]->pcDevice->GetTmccS(iTuner, &tmcc);
				if (enStatus == PT::STATUS_OK) {
					for (uint32 i=0; i<8; i++) {
						uint32 id = tmcc.Id[i]&0xffff;
						if ((id&0xff00) && (id^0xffff)) {
							if( (id&7) == dwTSID ) { //ストリームに一致した
								//一致したidに書き換える
								dwTSID = id ;
								_OutputDebugString(L"CPT3Manager::SetCh: replaced TSID=%d\n",id) ;
								break;
							}
						}
					}
					if(dwTSID&~7UL) break ;
				}
				Sleep(50);
			}
		}
		if(dwTSID&~7UL) {
			uint32 uiGetID=0xffff;
			Sleep(50);
			for (DWORD t=0,s=dur(),n=0; t<MAXDUR_TSID ; t=dur(s)) {
				enStatus = m_EnumDev[iDevID]->pcDevice->SetIdS(iTuner, dwTSID);
				if( enStatus == PT::STATUS_OK ) { if(++n>=2) break ; }
				Sleep(50);
			}
			Sleep(10);
			for (DWORD t=0,s=dur(); t<MAXDUR_TSID && dwTSID != uiGetID ; t=dur(s)) {
				enStatus = m_EnumDev[iDevID]->pcDevice->GetIdS(iTuner, &uiGetID);
				Sleep(10);
			}
			if(dwTSID==uiGetID) hasStream = TRUE ;
			if( !hasStream && enStatus != PT::STATUS_OK ){
				_OutputDebugString(L"CPT3Manager::SetCh: failure!\n") ;
				return FALSE;
			}
		}
	}

	m_EnumDev[iDevID]->cDataIO.ClearBuff(iID);

	_OutputDebugString(L"CPT3Manager::SetCh: success!\n") ;
	return TRUE;
}

DWORD CPT3Manager::GetSignal(int iID)
{
	int iDevID = iID>>16;
	PT::Device::ISDB enISDB = (PT::Device::ISDB)((iID&0x0000FF00)>>8);
	uint32 iTuner = iID&0x000000FF;

	if( (int)m_EnumDev.size() <= iDevID ){
		return 0;
	}

	uint32 cn100;
	uint32 currentAgc;
	uint32 maxAgc;

	status enStatus;
	enStatus = m_EnumDev[iDevID]->pcDevice->GetCnAgc(enISDB, iTuner, &cn100, &currentAgc, &maxAgc);
	if( enStatus != PT::STATUS_OK ){
		return 0;
	}

	return cn100;
}

BOOL CPT3Manager::CloseChk()
{
	BOOL bRet = FALSE;
	int iID;
	for(int i=0; i<(int)m_EnumDev.size(); i++ ){
		if( m_EnumDev[i]->bUseT0 ){
			iID = (i<<16) | (PT::Device::ISDB_T<<8) | 0;
			if(m_EnumDev[i]->cDataIO.GetOverFlowCount(iID) > 100){
				OutputDebugString(L"T0 OverFlow Close");
				CloseTuner(iID);
			}
		}
		if( m_EnumDev[i]->bUseT1 ){
			iID = (i<<16) | (PT::Device::ISDB_T<<8) | 1;
			if(m_EnumDev[i]->cDataIO.GetOverFlowCount(iID) > 100){
				OutputDebugString(L"T1 OverFlow Close");
				CloseTuner(iID);
			}
		}
		if( m_EnumDev[i]->bUseS0 ){
			iID = (i<<16) | (PT::Device::ISDB_S<<8) | 0;
			if(m_EnumDev[i]->cDataIO.GetOverFlowCount(iID) > 100){
				OutputDebugString(L"S0 OverFlow Close");
				CloseTuner(iID);
			}
		}
		if( m_EnumDev[i]->bUseS1 ){
			iID = (i<<16) | (PT::Device::ISDB_S<<8) | 1;
			if(m_EnumDev[i]->cDataIO.GetOverFlowCount(iID) > 100){
				OutputDebugString(L"S1 OverFlow Close");
				CloseTuner(iID);
			}
		}
	}
	for(int i=0; i<(int)m_EnumDev.size(); i++ ){
		if( m_EnumDev[i]->bOpen ){
			bRet = TRUE;
		}
	}
	return bRet;
}

int CPT3Manager::OpenTuner2(BOOL bSate, int iTunerID)
{
	int iID = -1;
	int iDevID = -1;
	PT::Device::ISDB enISDB;
	uint32 iTuner = -1;
	char log[256];

	//指定チューナーが空いてるか確認
	if( (int)m_EnumDev.size() <= iTunerID/2 ){
		wsprintfA(log, "tuner not found: m_EnumDev.size()[%d] iTunerID[%d]\n", (int)m_EnumDev.size(), iTunerID);
		OutputDebugStringA(log);
		return -1;
	}
	iDevID = iTunerID/2;
	if( bSate == FALSE ){
		//地デジ
		if( iTunerID%2 == 0 ){
			if( m_EnumDev[iDevID]->bUseT0 == FALSE ){
				iID = (iDevID<<16) | (PT::Device::ISDB_T<<8) | 0;
				enISDB = PT::Device::ISDB_T;
				iTuner = 0;
			}
		}else{
			if( m_EnumDev[iDevID]->bUseT1 == FALSE ){
				iID = (iDevID<<16) | (PT::Device::ISDB_T<<8) | 1;
				enISDB = PT::Device::ISDB_T;
				iTuner = 1;
			}
		}
	}else{
		//BS/CS
		if( iTunerID%2 == 0 ){
			if( m_EnumDev[iDevID]->bUseS0 == FALSE ){
				iID = (iDevID<<16) | (PT::Device::ISDB_S<<8) | 0;
				enISDB = PT::Device::ISDB_S;
				iTuner = 0;
			}
		}else{
			if( m_EnumDev[iDevID]->bUseS1 == FALSE ){
				iID = (iDevID<<16) | (PT::Device::ISDB_S<<8) | 1;
				enISDB = PT::Device::ISDB_S;
				iTuner = 1;
			}
		}
	}
	_RPT3(_CRT_WARN, "*** CPT3Manager::OpenTuner2() : iDevID[%d] bSate[%d] iTunerID[%d] ***\n", iDevID, bSate, iTunerID);
	if( iTuner == -1 ){
		OutputDebugStringA("unused tuner not found\n");
		return -1;
	}
	status enStatus;
	if( m_EnumDev[iDevID]->bOpen == FALSE ){
		//デバイス初オープン
		enStatus = m_cBus->NewDevice(&m_EnumDev[iDevID]->stDevInfo, &m_EnumDev[iDevID]->pcDevice, NULL);
		if( enStatus != PT::STATUS_OK ){
			wsprintfA(log, "m_cBus->NewDevice() error : enStatus[0x%x]\n", enStatus);
			OutputDebugStringA(log);
			return -1;
		}
		for( int i = 0; i < 5; i++ ){
			enStatus = m_EnumDev[iDevID]->pcDevice->Open();
			if( enStatus == PT::STATUS_OK ){
				break;
			}
			wsprintfA(log, "%d: pcDevice->Open() error : enStatus[0x%x]\n", i, enStatus);
			OutputDebugStringA(log);
			// PT::STATUS_DEVICE_IS_ALREADY_OPEN_ERRORの場合は考慮しない
			m_EnumDev[iDevID]->pcDevice->Close();
			Sleep(10);	// 保険
		}
		if( enStatus != PT::STATUS_OK ){
			m_EnumDev[iDevID]->pcDevice->Delete();
			m_EnumDev[iDevID]->pcDevice = NULL;
			return -1;
		}
		enStatus = m_EnumDev[iDevID]->pcDevice->InitTuner();
		if( enStatus != PT::STATUS_OK ){
			m_EnumDev[iDevID]->pcDevice->Close();
			m_EnumDev[iDevID]->pcDevice->Delete();
			m_EnumDev[iDevID]->pcDevice = NULL;
			wsprintfA(log, "pcDevice->InitTuner() error : enStatus[0x%x]\n", enStatus);
			OutputDebugStringA(log);
			return -1;
		}
		for (uint32 i=0; i<2; i++) {
			for (uint32 j=0; j<2; j++) {
				enStatus = m_EnumDev[iDevID]->pcDevice->SetTunerSleep(static_cast<PT::Device::ISDB>(j), i, true);
				if( enStatus != PT::STATUS_OK ){
					m_EnumDev[iDevID]->pcDevice->Close();
					m_EnumDev[iDevID]->pcDevice->Delete();
					m_EnumDev[iDevID]->pcDevice = NULL;
					wsprintfA(log, "pcDevice->SetTunerSleep(%d, %d, true) error : enStatus[0x%x]\n", j, i, enStatus);
					OutputDebugStringA(log);
					return -1;
				}
			}
		}
		m_EnumDev[iDevID]->bOpen = TRUE;
		m_EnumDev[iDevID]->cDataIO.SetVirtualCount(m_uiVirtualCount);
		m_EnumDev[iDevID]->cDataIO.SetDevice(m_EnumDev[iDevID]->pcDevice);
		m_EnumDev[iDevID]->cDataIO.Run();
	}
	//スリープから復帰
	enStatus = m_EnumDev[iDevID]->pcDevice->SetTunerSleep(enISDB, iTuner, false);
	if( enStatus != PT::STATUS_OK ){
		wsprintfA(log, "pcDevice->SetTunerSleep(%d, %d, false) error : enStatus[0x%x]\n", enISDB, iTuner, enStatus);
		OutputDebugStringA(log);
		return -1;
	}

	if( m_bUseLNB && enISDB == PT::Device::ISDB_S){
		m_EnumDev[iDevID]->pcDevice->SetLnbPower(PT::Device::LNB_POWER_15V);
	}

	if( enISDB == PT::Device::ISDB_T ){
		if( iTuner == 0 ){
			m_EnumDev[iDevID]->bUseT0 = TRUE;
		}else{
			m_EnumDev[iDevID]->bUseT1 = TRUE;
		}
	}else{
		if( iTuner == 0 ){
			m_EnumDev[iDevID]->bUseS0 = TRUE;
		}else{
			m_EnumDev[iDevID]->bUseS1 = TRUE;
		}
	}
	m_EnumDev[iDevID]->cDataIO.StartPipeServer(iID);
	m_EnumDev[iDevID]->cDataIO.EnableTuner(iID, TRUE);
	return iID;
}
