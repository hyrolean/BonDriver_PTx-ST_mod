// CPTxManager PT1,2 / PT3 共有ソース (#include して使う)

#if PT_VER==1 || PT_VER==2
	#define uint32 uint
	#define sint32 int
#endif

#if PT_VER==1
	#define CPTxManager CPT1Manager
#elif PT_VER==3
	#define CPTxManager CPT3Manager
#elif PT_VER==2
	#define CPTxManager CPTwManager
#else
	#error 判別できない PT_VER
#endif


CPTxManager::CPTxManager(void)
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
#if PT_VER==1
	strIni += L"\\BonDriver_PT-ST.ini";
#elif PT_VER==3
	strIni += L"\\BonDriver_PT3-ST.ini";
#elif PT_VER==2
	strIni += L"\\BonDriver_PTw-ST.ini";
#endif

	if(!PathFileExists(strIni.c_str())) {
		strIni = szPath;
		strIni += L"\\BonDriver_PTx-ST.ini";
	}

	m_bUseLNB = GetPrivateProfileInt(L"SET", L"UseLNB", 0, strIni.c_str());
	m_bLNB11V = GetPrivateProfileInt(L"SET", L"LNB11V", 0, strIni.c_str());
	m_uiVirtualCount = GetPrivateProfileInt(L"SET", L"DMABuff", 8, strIni.c_str());
	if( m_uiVirtualCount == 0 ){
		m_uiVirtualCount = 8;
	}
	m_bMemStreaming = GetPrivateProfileInt(L"SET", L"StreamingMethod", 0, strIni.c_str());

	SetHRTimerMode(GetPrivateProfileInt(L"SET", L"UseHRTimer", 0, strIni.c_str()));

	m_dwMaxDurFREQ = GetPrivateProfileInt(L"SET", L"MAXDUR_FREQ", 1000, strIni.c_str() ); //周波数調整に費やす最大時間(msec)
	m_dwMaxDurTMCC = GetPrivateProfileInt(L"SET", L"MAXDUR_TMCC", 1000, strIni.c_str() ); //TMCC取得に費やす最大時間(msec)
	m_dwMaxDurTMCC_S = GetPrivateProfileInt(L"SET", L"MAXDUR_TMCC_S", m_dwMaxDurTMCC, strIni.c_str() ); //TMCC(S側)取得に費やす最大時間(msec)
	m_dwMaxDurTSID = GetPrivateProfileInt(L"SET", L"MAXDUR_TSID", 1000, strIni.c_str() ); //TSID設定に費やす最大時間(msec)

	m_bNoCheckFREQ = GetPrivateProfileInt(L"SET", L"NoCheckFREQ", 1, strIni.c_str());
	m_bNoCheckTSID = GetPrivateProfileInt(L"SET", L"NoCheckTSID", 0, strIni.c_str());
}

BOOL CPTxManager::LoadSDK()
{
	if( m_cLibrary == NULL ){
		m_cLibrary = new OS::Library();

		PT::Bus::NewBusFunction function = m_cLibrary->Function();
		if (function == NULL) {
			SAFE_DELETE(m_cLibrary);
			DBGOUT("CPTxManager::LoadSDK failed(1)\n");
			return FALSE;
		}
		status enStatus = function(&m_cBus);
		if( enStatus != PT::STATUS_OK ){
			SAFE_DELETE(m_cLibrary);
			DBGOUT("CPTxManager::LoadSDK failed(2)\n");
			return FALSE;
		}

		//バージョンチェック
		uint32 version;
		m_cBus->GetVersion(&version);
		if ((version >> 8) != 2) {
			m_cBus->Delete();
			m_cBus = NULL;
			SAFE_DELETE(m_cLibrary);
			DBGOUT("CPTxManager::LoadSDK failed(3)\n");
			return FALSE;
		}
	}
	DBGOUT("CPTxManager::LoadSDK succeeded.\n");
	return TRUE;
}

void CPTxManager::FreeSDK()
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

BOOL CPTxManager::Init()
{
	if( m_cBus == NULL ){
		DBGOUT("CPTxManager::Init failed(m_cBus==NULL).\n");
		return FALSE;
	}
	m_EnumDev.clear();

	PT::Bus::DeviceInfo deviceInfo[99];
	uint32 deviceInfoCount = sizeof(deviceInfo)/sizeof(*deviceInfo);
	m_cBus->Scan(deviceInfo, &deviceInfoCount);

	for (uint32 i=0; i<deviceInfoCount; i++) {
#if PT_VER==1
		if( deviceInfo[i].BadBitCount != 0 ) continue ;
#endif
		DEV_STATUS* pItem = new DEV_STATUS(m_bMemStreaming);
#if PT_VER==2
		#ifdef _DEBUG
		for(int j=0;j<4;j++) {
			string devName;
			WtoA(deviceInfo[i].DevName[j],devName) ;
			DBGOUT("\tEnumerated Device[%d].Name[%d]:\t%s\n",i,j,devName.c_str());
		}
		#endif
#endif
		pItem->stDevInfo = deviceInfo[i];
		m_EnumDev.push_back(pItem);
	}

	DBGOUT("CPTxManager::Init succeeded.\n");
	return TRUE;
}

void CPTxManager::UnInit()
{
	FreeSDK();
}

BOOL CPTxManager::IsFindOpen()
{
	BOOL bFind = FALSE;
	for( int i=0; i<(int)m_EnumDev.size(); i++ ){
		if( m_EnumDev[i]->bOpen ){
			bFind = TRUE;
		}
	}
	return bFind;
}

DWORD CPTxManager::GetActiveTunerCount(BOOL bSate)
{
	DWORD total_used=0;
	for( auto dev : m_EnumDev ){
		if( bSate == FALSE ){
			if(dev->bUseT0) total_used++;
			if(dev->bUseT1) total_used++;
		}else {
			if(dev->bUseS0) total_used++;
			if(dev->bUseS1) total_used++;
		}
	}
	return total_used;
}

BOOL CPTxManager::SetLnbPower(int iID, BOOL bEnabled)
{
	size_t iDevID = iID>>16;
	PT::Device::ISDB enISDB = (PT::Device::ISDB)((iID&0x0000FF00)>>8);
	uint32 iTuner = iID&0x000000FF;

	if( iDevID>=m_EnumDev.size() )
		return FALSE;

	if( m_EnumDev[iDevID]->pcDevice == NULL )
		return FALSE;

	if(enISDB == PT::Device::ISDB_S) {
		if( iTuner == 0 )
			m_EnumDev[iDevID]->bLnbS0 = bEnabled ;
		else
			m_EnumDev[iDevID]->bLnbS1 = bEnabled ;
	}

	m_EnumDev[iDevID]->pcDevice->SetLnbPower(
		m_EnumDev[iDevID]->bLnbS0 || m_EnumDev[iDevID]->bLnbS1 ?
			(m_bLNB11V? PT::Device::LNB_POWER_11V: PT::Device::LNB_POWER_15V) :
			PT::Device::LNB_POWER_OFF
	);

	return TRUE;
}

int CPTxManager::OpenTuner(BOOL bSate)
{
	int iTunerID = -1;
	//空きを探す
	if( bSate == FALSE ){
		//地デジ
		for( int i=0; i<(int)m_EnumDev.size(); i++ ){
			if( m_EnumDev[i]->bUseT0 == FALSE ){
				iTunerID = i<<1 | 0 ;
				break;
			}else if( m_EnumDev[i]->bUseT1 == FALSE ){
				iTunerID = i<<1 | 1 ;
				break;
			}
		}
	}else{
		//BS/CS
		for( int i=0; i<(int)m_EnumDev.size(); i++ ){
			if( m_EnumDev[i]->bUseS0 == FALSE ){
				iTunerID = i<<1 | 0 ;
				break;
			}else if( m_EnumDev[i]->bUseS1 == FALSE ){
				iTunerID = i<<1 | 1 ;
				break;
			}
		}
	}
	if( iTunerID == -1 ){
		return -1;
	}
	return OpenTuner2(bSate, iTunerID);
}

BOOL CPTxManager::CloseTuner(int iID)
{
	int iDevID = iID>>16;
	PT::Device::ISDB enISDB = (PT::Device::ISDB)((iID&0x0000FF00)>>8);
	uint32 iTuner = iID&0x000000FF;

	if( (int)m_EnumDev.size() <= iDevID ){
		return FALSE;
	}

	status enStatus;

#if PT_VER==1 || PT_VER==2
	enStatus = m_EnumDev[iDevID]->pcDevice->SetStreamEnable(iTuner, enISDB, false);
	if( enStatus != PT::STATUS_OK ){
		return FALSE;
	}

	enStatus = m_EnumDev[iDevID]->pcDevice->SetTunerSleep(iTuner, enISDB, true);
#elif PT_VER==3
	m_EnumDev[iDevID]->cDataIO.EnableTuner(iID, FALSE);
	m_EnumDev[iDevID]->cDataIO.StopPipeServer(iID);

	enStatus = m_EnumDev[iDevID]->pcDevice->SetTunerSleep(enISDB, iTuner, true);
#endif
	if( enStatus != PT::STATUS_OK ){
		return FALSE;
	}
#if PT_VER==1 || PT_VER==2
	m_EnumDev[iDevID]->cDataIO.EnableTuner(iID, FALSE);
#endif

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

	if( m_bUseLNB )
		SetLnbPower(iID, FALSE);

	if( m_EnumDev[iDevID]->bUseT0 == FALSE &&
		m_EnumDev[iDevID]->bUseT1 == FALSE &&
		m_EnumDev[iDevID]->bUseS0 == FALSE &&
		m_EnumDev[iDevID]->bUseS1 == FALSE ){
			//全部使ってなければクローズ
			m_EnumDev[iDevID]->cDataIO.Stop();
#if PT_VER==1 || PT_VER==2
			m_EnumDev[iDevID]->pcDevice->SetTransferEnable(false);
			m_EnumDev[iDevID]->pcDevice->SetBufferInfo(NULL);
#endif
			m_EnumDev[iDevID]->pcDevice->Close();
			m_EnumDev[iDevID]->pcDevice->Delete();
			m_EnumDev[iDevID]->pcDevice = NULL;
			m_EnumDev[iDevID]->bOpen = FALSE;
	}

	return TRUE;
}

BOOL CPTxManager::SetFreq(int iID, unsigned long ulCh)
{
	int iDevID = iID>>16;
	PT::Device::ISDB enISDB = (PT::Device::ISDB)((iID&0x0000FF00)>>8);
	uint32 iTuner = iID&0x000000FF;

	if( (int)m_EnumDev.size() <= iDevID ){
		return FALSE;
	}

	uint32 ch = ulCh & 0xffff;
	sint32 offset = (ulCh >> 16) & 0xffff;
	if (offset >= 32768) offset -= 65536;

	status enStatus = PT::STATUS_GENERAL_ERROR ;
	for (DWORD t=0,s=dur(); t<m_dwMaxDurFREQ; t=dur(s)) {
		if(t) HRSleep(enStatus==PT::STATUS_OK?10:40);
		if( enStatus!=PT::STATUS_OK )
#if PT_VER==1 || PT_VER==2
			enStatus = m_EnumDev[iDevID]->pcDevice->SetFrequency(iTuner, enISDB, ch, offset);
#elif PT_VER==3
			enStatus = m_EnumDev[iDevID]->pcDevice->SetFrequency(enISDB, iTuner, ch, offset);
#endif
		if( enStatus == PT::STATUS_OK ) {
			if(m_bNoCheckFREQ) return TRUE; // no check
			uint32 cur_ch = ch ;
			sint32 cur_offset = offset ;
			status enCurStatus = PT::STATUS_OK ;
#if PT_VER==1 || PT_VER==2
			enCurStatus = m_EnumDev[iDevID]->pcDevice->GetFrequency(iTuner, enISDB, &cur_ch, &cur_offset);
#elif PT_VER==3
			enCurStatus = m_EnumDev[iDevID]->pcDevice->GetFrequency(enISDB, iTuner, &cur_ch, &cur_offset);
#endif
			if(enCurStatus == PT::STATUS_OK) {
				if(cur_ch==ch && cur_offset==offset)
					return TRUE ;
			}
		}
	}

	return FALSE ;
}

BOOL CPTxManager::GetIdListS(int iID, PTTSIDLIST* pPtTSIDList)
{
	int iDevID = iID>>16;
	PT::Device::ISDB enISDB = (PT::Device::ISDB)((iID&0x0000FF00)>>8);
	uint32 iTuner = iID&0x000000FF;

	if( (int)m_EnumDev.size() <= iDevID || pPtTSIDList==NULL || enISDB != PT::Device::ISDB_S ){
		return FALSE;
	}

	PT::Device::TmccS tmcc;
	status enStatus = m_EnumDev[iDevID]->pcDevice->GetTmccS(iTuner, &tmcc);
	if (enStatus != PT::STATUS_OK) {
		return FALSE ;
	}

	PDWORD pdwId = &pPtTSIDList->dwId[0];
	std::copy(&tmcc.Id[0], &tmcc.Id[8], pdwId);

	return TRUE ;
}

BOOL CPTxManager::GetIdS(int iID, DWORD *pdwTSID)
{
	int iDevID = iID>>16;
	PT::Device::ISDB enISDB = (PT::Device::ISDB)((iID&0x0000FF00)>>8);
	uint32 iTuner = iID&0x000000FF;

	if( (int)m_EnumDev.size() <= iDevID || pdwTSID==NULL || enISDB != PT::Device::ISDB_S ){
		return FALSE;
	}

	uint32 uiGetID ;
	status enStatus = m_EnumDev[iDevID]->pcDevice->GetIdS(iTuner, &uiGetID);
	if (enStatus != PT::STATUS_OK) {
		return FALSE ;
	}

	*pdwTSID = (DWORD) uiGetID ;
	return TRUE ;
}

BOOL CPTxManager::SetIdS(int iID, DWORD dwTSID)
{
	int iDevID = iID>>16;
	PT::Device::ISDB enISDB = (PT::Device::ISDB)((iID&0x0000FF00)>>8);
	uint32 iTuner = iID&0x000000FF;

	if( (int)m_EnumDev.size() <= iDevID || enISDB != PT::Device::ISDB_S ){
		return FALSE;
	}

	DWORD dwGetID=0xffff;
	BOOL bRes = FALSE ;

	status enStatus = PT::STATUS_GENERAL_ERROR ;
	for (DWORD t=0,s=dur(); t<m_dwMaxDurTSID && dwTSID != dwGetID ; t=dur(s)) {
		if(t) HRSleep(enStatus==PT::STATUS_OK?10:40);
		if( enStatus != PT::STATUS_OK )
			enStatus = m_EnumDev[iDevID]->pcDevice->SetIdS(iTuner, dwTSID);
		if( enStatus == PT::STATUS_OK ) {
			if(GetIdS(iID, &dwGetID)) bRes=TRUE;
		}
	}

	return (dwTSID==dwGetID) ? bRes : FALSE ;
}

BOOL CPTxManager::SetCh(int iID, unsigned long ulCh, DWORD dwTSID, BOOL &hasStream)
{
	uint32 ch = ulCh & 0xffff;
	sint32 offset = (ulCh >> 16) & 0xffff;
	if (offset >= 32768) offset -= 65536;

	_OutputDebugString(L"CPT%dManager::SetCh: iID=%d ch=%d offset=%d dwTSID=%d\n",PT_VER,iID,ch,offset,dwTSID) ;

	int iDevID = iID>>16;
	PT::Device::ISDB enISDB = (PT::Device::ISDB)((iID&0x0000FF00)>>8);
	uint32 iTuner = iID&0x000000FF;

	hasStream = FALSE ;

	BOOL result = FALSE ;
	do {

		if( (int)m_EnumDev.size() <= iDevID ){
			break;
		}

		BOOL bRes=FALSE;
		bRes = SetFreq(iID, ulCh);
		if( bRes ) {
			_OutputDebugString(L"CPT%dManager::SetFreq: Device::SetFrequency: ISDB:%d tuner:%d ch:%d\n",PT_VER,enISDB,iTuner,ch);
		}else {
			_OutputDebugString(L"CPT%dManager::SetFreq: Device::SetFrequency failure!\n",PT_VER) ;
			break ;
		}

		if( enISDB == PT::Device::ISDB_S ){
			bool checkOnly = (dwTSID&~7UL)!=0 ,checkFinished = checkOnly&&m_bNoCheckTSID ;
			for (DWORD t=0,s=dur(); !checkFinished && t<m_dwMaxDurTMCC_S; t=dur(s)) {
				PTTSIDLIST PtTSIDList = {0};
				bRes = GetIdListS(iID, &PtTSIDList);
				if (bRes) {
					for (uint32 i=0; i<8; i++) {
						DWORD id = PtTSIDList.dwId[i]&0xffff;
						if ((id&0xff00) && (id^0xffff)) {
							if(checkOnly) {
								//dwTSIDに既にTSIDを記述してある場合は、IDの一致だけを確かめる
								// mod6 by 2022 LVhJPic0JSk5LiQ1ITskKVk9UGBg
								if(checkFinished = dwTSID == id) break;
							}
							else if( (id&7) == dwTSID ) {
								//dwTSIDに0～7が指定された場合は、TSIDをチューナーから取得して
								//下位３ビットと一致するものに書き換える
								// mod by 2020 LVhJPic0JSk5LiQ1ITskKVk9UGBg
								dwTSID = id ;  checkFinished = true ;
								_OutputDebugString(L"CPT%dManager::SetCh: replaced TSID=%d\n",PT_VER,id) ;
								break;
							}
						}
					}
					HRSleep(0,500);
				}else
					HRSleep(10);
			}
			if(!checkFinished) {
				_OutputDebugString(L"CPT%dManager::SetCh: TSID:%04x was not found on TMCC!\n",PT_VER,dwTSID) ;
				break;
			}
			else {
				if(!SetIdS(iID, dwTSID)) {
					_OutputDebugString(L"CPT%dManager::SetCh: SetIdS failure!\n",PT_VER) ;
					break;
				}
				hasStream = TRUE ;
			}
		}else {
			for (DWORD t=0,s=dur(); t<m_dwMaxDurTMCC; t=dur(s)) {
				PT::Device::TmccT tmcc;
				ZeroMemory(&tmcc,sizeof(tmcc));
				//std::fill_n(tmcc.Id,8,0xffff) ;
				if(t) HRSleep(40);
				status enStatus = m_EnumDev[iDevID]->pcDevice->GetTmccT(iTuner, &tmcc);
				if (enStatus == PT::STATUS_OK) {
					hasStream = TRUE ; break;
				}
			}
		}

		result = TRUE;
		_OutputDebugString(L"CPT%dManager::SetCh: success!\n",PT_VER) ;

	}while(0);

	m_EnumDev[iDevID]->cDataIO.ClearBuff(iID);

	return result ;
}

DWORD CPTxManager::GetSignal(int iID)
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
#if PT_VER==1 || PT_VER==2
	enStatus = m_EnumDev[iDevID]->pcDevice->GetCnAgc(iTuner, enISDB, &cn100, &currentAgc, &maxAgc);
#elif PT_VER==3
	enStatus = m_EnumDev[iDevID]->pcDevice->GetCnAgc(enISDB, iTuner, &cn100, &currentAgc, &maxAgc);
#endif
	if( enStatus != PT::STATUS_OK ){
		return 0;
	}

	return cn100;
}

BOOL CPTxManager::CloseChk()
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

int CPTxManager::OpenTuner2(BOOL bSate, int iTunerID)
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
	iDevID = iTunerID>>1;
	if( bSate == FALSE ){
		//地デジ
		if( !(iTunerID&1) ){
			if( m_EnumDev[iDevID]->bUseT0 == FALSE )
				iID = iDevID<<16 | (enISDB = PT::Device::ISDB_T)<<8 | (iTuner = 0);
		}else{
			if( m_EnumDev[iDevID]->bUseT1 == FALSE )
				iID = iDevID<<16 | (enISDB = PT::Device::ISDB_T)<<8 | (iTuner = 1);
		}
	}else{
		//BS/CS
		if( !(iTunerID&1) ){
			if( m_EnumDev[iDevID]->bUseS0 == FALSE )
				iID = iDevID<<16 | (enISDB = PT::Device::ISDB_S)<<8 | (iTuner = 0);
		}else{
			if( m_EnumDev[iDevID]->bUseS1 == FALSE )
				iID = iDevID<<16 | (enISDB = PT::Device::ISDB_S)<<8 | (iTuner = 1);
		}
	}
	_RPT3(_CRT_WARN, "*** CPT%dManager::OpenTuner2() : iDevID[%d] bSate[%d] iTunerID[%d] ***\n", PT_VER, iDevID, bSate, iTunerID);
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
#if PT_VER==1 || PT_VER==3
			enStatus = m_EnumDev[iDevID]->pcDevice->Open();
#elif PT_VER==2
			// NOTE: なぜかココでエラーが発生する(原因不明) @ pt2wdm1.1.0.0
			enStatus = m_EnumDev[iDevID]->pcDevice->Open(&m_EnumDev[iDevID]->stDevInfo);
#endif
			if( enStatus == PT::STATUS_OK ){
				DBGOUT("CPTxManager::OpenTuner2 Open failed(%x).\n",enStatus);
				break;
			}
			wsprintfA(log, "%d: pcDevice->Open() error : enStatus[0x%x]\n", i, enStatus);
			OutputDebugStringA(log);
			// PT::STATUS_DEVICE_IS_ALREADY_OPEN_ERRORの場合は考慮しない
			m_EnumDev[iDevID]->pcDevice->Close();
			HRSleep(10);	// 保険
		}
		if( enStatus != PT::STATUS_OK ){
			m_EnumDev[iDevID]->pcDevice->Delete();
			m_EnumDev[iDevID]->pcDevice = NULL;
			return -1;
		}

#if PT_VER==1 || PT_VER==2
		enStatus = m_EnumDev[iDevID]->pcDevice->SetTunerPowerReset(PT::Device::TUNER_POWER_ON_RESET_ENABLE);
		if( enStatus != PT::STATUS_OK ){
			m_EnumDev[iDevID]->pcDevice->Close();
			m_EnumDev[iDevID]->pcDevice->Delete();
			m_EnumDev[iDevID]->pcDevice = NULL;
			wsprintfA(log, "pcDevice->SetTunerPowerReset(ENABLE) error : enStatus[0x%x]\n", enStatus);
			OutputDebugStringA(log);
			return -1;
		}
		HRSleep(20);
		enStatus = m_EnumDev[iDevID]->pcDevice->SetTunerPowerReset(PT::Device::TUNER_POWER_ON_RESET_DISABLE);
		if( enStatus != PT::STATUS_OK ){
			m_EnumDev[iDevID]->pcDevice->Close();
			m_EnumDev[iDevID]->pcDevice->Delete();
			m_EnumDev[iDevID]->pcDevice = NULL;
			wsprintfA(log, "pcDevice->SetTunerPowerReset(DISABLE) error : enStatus[0x%x]\n", enStatus);
			OutputDebugStringA(log);
			return -1;
		}
		HRSleep(1);
		for( uint32 i=0; i<2; i++ ){
			enStatus = m_EnumDev[iDevID]->pcDevice->InitTuner(i);
			if( enStatus != PT::STATUS_OK ){
				m_EnumDev[iDevID]->pcDevice->Close();
				m_EnumDev[iDevID]->pcDevice->Delete();
				m_EnumDev[iDevID]->pcDevice = NULL;
				wsprintfA(log, "pcDevice->InitTuner(%d) error : enStatus[0x%x]\n", i, enStatus);
				OutputDebugStringA(log);
				return -1;
			}
		}
#elif PT_VER==3
		enStatus = m_EnumDev[iDevID]->pcDevice->InitTuner();
		if( enStatus != PT::STATUS_OK ){
			m_EnumDev[iDevID]->pcDevice->Close();
			m_EnumDev[iDevID]->pcDevice->Delete();
			m_EnumDev[iDevID]->pcDevice = NULL;
			wsprintfA(log, "pcDevice->InitTuner() error : enStatus[0x%x]\n", enStatus);
			OutputDebugStringA(log);
			return -1;
		}
#endif
		for (uint32 i=0; i<2; i++) {
			for (uint32 j=0; j<2; j++) {
#if PT_VER==1 || PT_VER==2
				enStatus = m_EnumDev[iDevID]->pcDevice->SetTunerSleep(i, static_cast<PT::Device::ISDB>(j), true);
#elif PT_VER==3
				enStatus = m_EnumDev[iDevID]->pcDevice->SetTunerSleep(static_cast<PT::Device::ISDB>(j), i, true);
#endif
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
	}
	m_EnumDev[iDevID]->cDataIO.Run(iID);

	//スリープから復帰
#if PT_VER==1 || PT_VER==2
	enStatus = m_EnumDev[iDevID]->pcDevice->SetTunerSleep(iTuner, enISDB, false);
#elif PT_VER==3
	enStatus = m_EnumDev[iDevID]->pcDevice->SetTunerSleep(enISDB, iTuner, false);
#endif
	if( enStatus != PT::STATUS_OK ){
		wsprintfA(log, "pcDevice->SetTunerSleep(%d, %d, false) error : enStatus[0x%x]\n", enISDB, iTuner, enStatus);
		OutputDebugStringA(log);
		return -1;
	}

	if( m_bUseLNB )
		SetLnbPower(iID, TRUE);

#if PT_VER==1 || PT_VER==2
	enStatus = m_EnumDev[iDevID]->pcDevice->SetStreamEnable(iTuner, enISDB, true);
	if( enStatus != PT::STATUS_OK ){
		wsprintfA(log, "pcDevice->SetStreamEnable(%d, %d, true) error : enStatus[0x%x]\n", iTuner, enISDB, enStatus);
		OutputDebugStringA(log);
		return -1;
	}
#endif
	if( enISDB == PT::Device::ISDB_T ){
		if( iTuner == 0 ){
			m_EnumDev[iDevID]->bUseT0 = TRUE;
		}else{
			m_EnumDev[iDevID]->bUseT1 = TRUE;
		}
#if PT_VER==1 || PT_VER==2
		HRSleep(10);	// PT1/2のT側はこの後の選局までが早すぎるとエラーになる場合があるので
#endif
	}else{
		if( iTuner == 0 ){
			m_EnumDev[iDevID]->bUseS0 = TRUE;
		}else{
			m_EnumDev[iDevID]->bUseS1 = TRUE;
		}
	}
#if PT_VER==3
	m_EnumDev[iDevID]->cDataIO.StartPipeServer(iID);
#endif
	m_EnumDev[iDevID]->cDataIO.EnableTuner(iID, TRUE);
	return iID;
}
