#include "stdafx.h"
#include <Windows.h>
#include <process.h>

#include <Shlwapi.h>
#pragma comment(lib, "Shlwapi.lib")

#include "BonTuner.h"

static BOOL isISDB_S;

#define DATA_BUFF_SIZE	(188*256)
#define MAX_BUFF_COUNT	500

#pragma warning( disable : 4273 )
extern "C" __declspec(dllexport) IBonDriver * CreateBonDriver()
{
	// ����v���Z�X����̕����C���X�^���X�擾�֎~
	// (�񓯊��Ŏ擾���ꂽ�ꍇ�̔r�������������Əo���Ă��Ȃ������u)
	CBonTuner *p = NULL;
	if (CBonTuner::m_pThis == NULL) {
		p = new CBonTuner;
	}
	return p;
}
#pragma warning( default : 4273 )

CBonTuner * CBonTuner::m_pThis = NULL;
HINSTANCE CBonTuner::m_hModule = NULL;


	//�p�C�v��
	LPCTSTR CMD_PT1_CTRL_PIPE, CMD_PT1_DATA_PIPE;

	//�ڑ��ҋ@�p�C�x���g
	LPCTSTR CMD_PT1_CTRL_EVENT_WAIT_CONNECT, CMD_PT1_DATA_EVENT_WAIT_CONNECT;

	static void SetupCmdPt1Defs(int iPTVer) {
	#define MK_CMD_PT1_(def, format) do { \
					wsprintf(defs[n],format,iPTVer); \
					CMD_PT1_##def=const_cast<LPCTSTR>(defs[n++]); \
				}while(0)
		static wchar_t defs[4][32];
		int n=0;
		MK_CMD_PT1_(CTRL_PIPE,_T("\\\\.\\pipe\\PT%dCtrlPipe"));
		MK_CMD_PT1_(DATA_PIPE,_T("\\\\.\\pipe\\PT%dDataPipe_"));
		MK_CMD_PT1_(CTRL_EVENT_WAIT_CONNECT,_T("Global\\PT%dCtrlConnect"));
		MK_CMD_PT1_(DATA_EVENT_WAIT_CONNECT,_T("Global\\PT%dDataConnect_"));
	#undef MK_CMD_PT1_
	}

CBonTuner::CBonTuner()
{
	m_pThis = this;
	m_hOnStreamEvent = NULL;

	m_LastBuff = NULL;

	m_dwCurSpace = 0xFF;
	m_dwCurChannel = 0xFF;
	m_hasStream = TRUE ;

	m_iID = -1;
	m_hStopEvent = _CreateEvent(FALSE, FALSE, NULL);
	m_hThread = NULL;

	::InitializeCriticalSection(&m_CriticalSection);

	WCHAR strExePath[512] = L"";
	GetModuleFileName(m_hModule, strExePath, 512);

	WCHAR szPath[_MAX_PATH];	// �p�X
	WCHAR szDrive[_MAX_DRIVE];
	WCHAR szDir[_MAX_DIR];
	WCHAR szFname[_MAX_FNAME];
	WCHAR szExt[_MAX_EXT];
	_tsplitpath_s( strExePath, szDrive, _MAX_DRIVE, szDir, _MAX_DIR, szFname, _MAX_FNAME, szExt, _MAX_EXT );
	_tmakepath_s(  szPath, _MAX_PATH, szDrive, szDir, NULL, NULL );
	m_strDirPath = szPath;

	wstring strIni;
	strIni = szPath;
	strIni += L"BonDriver_PTx-ST.ini";

	auto has_prefix = [](wstring target, wstring prefix) -> bool {
		return !CompareNoCase(prefix,target.substr(0,prefix.length())) ;
	};

	if(has_prefix(szFname,L"BonDriver_PTx"))
		m_iPT=0;
	else if(has_prefix(szFname,L"BonDriver_PT3"))
		m_iPT=3;
	else
		m_iPT=1;

	isISDB_S = TRUE;
	WCHAR szName[256];
	m_iTunerID = -1;

    _wcsupr_s( szFname, sizeof(szFname) ) ;

	if(m_iPT==0) { // PTx ( auto detect )

		int detection = GetPrivateProfileIntW(L"SET", L"xFirstPT3", -1, strIni.c_str());
		m_bXFirstPT3 = detection>=0 ? BOOL(detection) :
			PathFileExists((m_strDirPath+L"PT3Ctrl.exe").c_str()) ;

		WCHAR cTS=L'S'; int id=-1;
		if(swscanf_s(szFname,L"BONDRIVER_PTX-%1c%d",&cTS,1,&id)!=2) {
			id = -1 ;
			if(swscanf_s(szFname,L"BONDRIVER_PTX-%1c",&cTS,1)!=1)
				cTS = L'S' ;
		}
		if(cTS==L'T')	m_strTunerName = L"PTx ISDB-T" , isISDB_S = FALSE ;
		else 			m_strTunerName = L"PTx ISDB-S" ;
		if(id>=0) {
			wsprintfW(szName, L" (%d)", id);
		    m_strTunerName += szName ;
			m_iTunerID = id ;
		}

	}else if(m_iPT==3) { // PT3

		wstring strPT3ini = m_strDirPath + L"BonDriver_PT3-ST.ini";
		if(PathFileExists(strPT3ini.c_str())) strIni = strPT3ini;

		if( wcslen(szFname) == wcslen(L"BonDriver_PT3-**") ){
			const WCHAR *TUNER_NAME2;
			if (szFname[14] == L'T'){
				isISDB_S = FALSE;
				TUNER_NAME2 = L"PT3 ISDB-T (%d)";
			}else{
				TUNER_NAME2 = L"PT3 ISDB-S (%d)";
			}
			m_iTunerID = _wtoi(szFname+wcslen(L"BonDriver_PT3-*"));
			wsprintfW(szName, TUNER_NAME2, m_iTunerID);
			m_strTunerName = szName;
		}else if( wcslen(szFname) == wcslen(L"BonDriver_PT3-*") ){
			const WCHAR *TUNER_NAME;
			if (szFname[14] == L'T'){
				isISDB_S = FALSE;
				TUNER_NAME = L"PT3 ISDB-T";
			}else{
				TUNER_NAME = L"PT3 ISDB-S";
			}
			m_strTunerName = TUNER_NAME;
		}else{
			m_strTunerName = L"PT3 ISDB-S";
		}

	}else {  // PT1/2

		wstring strPTini = wstring(szPath) + L"BonDriver_PT-ST.ini";
		if(PathFileExists(strPTini.c_str())) strIni = strPTini;

		int iPTn = GetPrivateProfileIntW(L"SET", L"PT1Ver", 2, strIni.c_str());
		if(iPTn<1||iPTn>2) iPTn=2;

		if( wcslen(szFname) == wcslen(L"BonDriver_PT-**") ){
			const WCHAR *TUNER_NAME2;
			if (szFname[13] == L'T'){
				isISDB_S = FALSE;
				TUNER_NAME2 = L"PT%d ISDB-T (%d)";
			}else{
				TUNER_NAME2 = L"PT%d ISDB-S (%d)";
			}
			m_iTunerID = _wtoi(szFname+wcslen(L"BonDriver_PT-*"));
			wsprintfW(szName, TUNER_NAME2, iPTn, m_iTunerID);
			m_strTunerName = szName;
		}else if( wcslen(szFname) == wcslen(L"BonDriver_PT-*") ){
			const WCHAR *TUNER_NAME;
			if (szFname[13] == L'T'){
				isISDB_S = FALSE;
				TUNER_NAME = L"PT%d ISDB-T";
			}else{
				TUNER_NAME = L"PT%d ISDB-S";
			}
			wsprintfW(szName, TUNER_NAME, iPTn);
			m_strTunerName = szName;
		}else{
			wsprintfW(szName, L"PT%d ISDB-S", iPTn);
			m_strTunerName = szName;
		}

	}

	m_dwSetChDelay = GetPrivateProfileIntW(L"SET", L"SetChDelay", 0, strIni.c_str());

	wstring strChSet;

	//dll���Ɠ������O��.ChSet.txt���ɗD�悵�ēǂݍ��݂����s����
	//(fixed by 2020 LVhJPic0JSk5LiQ1ITskKVk9UGBg)
	strChSet = szPath;	strChSet += szFname;	strChSet += L".ChSet.txt";
	if(!m_chSet.ParseText(strChSet.c_str())) {
		strChSet = szPath;
		if(m_iPT==3) {
			if (isISDB_S)
				strChSet += L"BonDriver_PT3-S.ChSet.txt";
			else
				strChSet += L"BonDriver_PT3-T.ChSet.txt";
		}else if(m_iPT==1) {
			if (isISDB_S)
				strChSet += L"BonDriver_PT-S.ChSet.txt";
			else
				strChSet += L"BonDriver_PT-T.ChSet.txt";
		}
		if(!m_iPT||!m_chSet.ParseText(strChSet.c_str())) {
			strChSet = szPath;
			if (isISDB_S)
				strChSet += L"BonDriver_PTx-S.ChSet.txt";
			else
				strChSet += L"BonDriver_PTx-T.ChSet.txt";
			if(!m_chSet.ParseText(strChSet.c_str()))
				BuildDefSpace(strIni);
		}
	}

	SetupCmdPt1Defs( m_iPT ? m_iPT : m_bXFirstPT3 ? 3 : 1 );
}

CBonTuner::~CBonTuner()
{
	CloseTuner();

	::EnterCriticalSection(&m_CriticalSection);
	SAFE_DELETE(m_LastBuff);
	::LeaveCriticalSection(&m_CriticalSection);

	::CloseHandle(m_hStopEvent);
	m_hStopEvent = NULL;

	::DeleteCriticalSection(&m_CriticalSection);

	m_pThis = NULL;
}

void CBonTuner::BuildDefSpace(wstring strIni)
{
	//.ChSet.txt�����݂��Ȃ��ꍇ�́A����̃`�����l�������\�z����
	//(added by 2021 LVhJPic0JSk5LiQ1ITskKVk9UGBg)

	BOOL UHF=TRUE, CATV=FALSE, VHF=FALSE, BS=TRUE, CS110=TRUE;
	DWORD BSStreams=8, CS110Streams=8;
	BOOL BSStreamStride=FALSE, CS110StreamStride=FALSE;

#define LOADDW(nam) do {\
		nam=(DWORD)GetPrivateProfileIntW(L"DefSpace", L#nam, nam, strIni.c_str()); \
	}while(0)

	LOADDW(UHF);
	LOADDW(CATV);
	LOADDW(VHF);
	LOADDW(BS);
	LOADDW(CS110);
	LOADDW(BSStreams);
	LOADDW(CS110Streams);
	LOADDW(BSStreamStride);
	LOADDW(CS110StreamStride);

#undef LOADDW

	DWORD spc=0 ;
	auto entry_spc = [&](const wchar_t *space_name) {
		SPACE_DATA item;
		item.wszName=space_name;
		item.dwSpace=spc++;
		m_chSet.spaceMap.insert( pair<DWORD, SPACE_DATA>(item.dwSpace,item) );
	};

	if(isISDB_S) {  // BS / CS110

		DWORD i,ch,ts,pt1offs;
		auto entry_ch = [&](const wchar_t *prefix, bool suffix) {
			CH_DATA item ;
			Format(item.wszName,suffix?L"%s%02d/TS%d":L"%s%02d",prefix,ch,ts);
			item.dwSpace=spc;
			item.dwCh=i;
			item.dwPT1Ch=(ch-1)/2+pt1offs;
			item.dwTSID=ts;
			DWORD iKey = (item.dwSpace<<16) | item.dwCh;
			m_chSet.chMap.insert( pair<DWORD, CH_DATA>(iKey,item) );
		};

		if(BS) {
			pt1offs=0;
			if(BSStreamStride) {
				for(i=0,ts=0;ts<(BSStreams>0?BSStreams:1);ts++)
				for(ch=1;ch<=23;ch+=2,i++)
					entry_ch(L"BS",BSStreams>0);
			}else {
				for(i=0,ch=1;ch<=23;ch+=2)
				for(ts=0;ts<(BSStreams>0?BSStreams:1);ts++,i++)
					entry_ch(L"BS",BSStreams>0);
			}
			entry_spc(L"BS");
		}

		if(CS110) {
			pt1offs=12;
			if(CS110StreamStride) {
				for(i=0,ts=0;ts<(CS110Streams>0?CS110Streams:1);ts++)
				for(ch=2;ch<=24;ch+=2,i++)
					entry_ch(L"ND",CS110Streams>0);
			}else {
				for(i=0,ch=2;ch<=24;ch+=2)
				for(ts=0;ts<(CS110Streams>0?CS110Streams:1);ts++,i++)
					entry_ch(L"ND",CS110Streams>0);
			}
			entry_spc(L"CS110");
		}

	}else { // �n�f�W

		DWORD i,offs,C;
		auto entry_ch = [&](DWORD (*pt1conv)(DWORD i)) {
			CH_DATA item;
			Format(item.wszName,C?L"C%dCh":L"%dCh",i+offs);
			item.dwSpace=spc;
			item.dwCh=i;
			item.dwPT1Ch=pt1conv(i);
			DWORD iKey = (item.dwSpace<<16) | item.dwCh;
			m_chSet.chMap.insert( pair<DWORD, CH_DATA>(iKey,item) );
		};

		if(UHF) {
			for(offs=13,C=i=0;i<50;i++) entry_ch([](DWORD i){return i+63;});
			entry_spc(L"�n�f�W(UHF)") ;
		}

		if(CATV) {
			for(offs=13,C=1,i=0;i<51;i++) entry_ch([](DWORD i){return i+(i>=10?12:3);});
			entry_spc(L"�n�f�W(CATV)") ;
		}

		if(VHF) {
			for(offs=1,C=i=0;i<12;i++) entry_ch([](DWORD i){return i+(i>=3?10:0);});
			entry_spc(L"�n�f�W(VHF)") ;
		}

	}
}

BOOL CBonTuner::LaunchPTCtrl(int iPT)
{
	SetupCmdPt1Defs(iPT);

	PROCESS_INFORMATION pi;
	STARTUPINFO si;
	ZeroMemory(&si,sizeof(si));
	si.cb=sizeof(si);

	wstring strPTCtrlExe = m_strDirPath ;

	switch(iPT) {
	case 1: strPTCtrlExe += L"PTCtrl.exe" ; break ;
	case 3: strPTCtrlExe += L"PT3Ctrl.exe" ; break ;
	}

	BOOL bRet = CreateProcessW( NULL, (LPWSTR)strPTCtrlExe.c_str(), NULL, NULL, FALSE, GetPriorityClass(GetCurrentProcess()), NULL, NULL, &si, &pi );
	CloseHandle(pi.hThread);
	CloseHandle(pi.hProcess);

	_RPT3(_CRT_WARN, "*** CBonTuner::LaunchPTCtrl() ***\nbRet[%s]", bRet ? "TRUE" : "FALSE");

	return bRet ;
}

BOOL CBonTuner::TryOpenTuner(int iTunerID, int *piID)
{
	DWORD dwRet;
	if( iTunerID >= 0 ){
		dwRet = SendOpenTuner2(isISDB_S, iTunerID, piID);
	}else{
		dwRet = SendOpenTuner(isISDB_S, piID);
	}

	_RPT3(_CRT_WARN, "*** CBonTuner::TryOpenTuner() ***\ndwRet[%u]\n", dwRet);

	if( dwRet != CMD_SUCCESS ){
		return FALSE;
	}

	return TRUE;
}

const BOOL CBonTuner::OpenTuner(void)
{
	//�C�x���g
	m_hOnStreamEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);

	_RPT3(_CRT_WARN, "*** CBonTuner::OpenTuner() ***\nm_hOnStreamEvent[%p]\n", m_hOnStreamEvent);

	if(!m_iPT) { // PTx ( PT1/2/3 - auto detect )

		//PTx�������o�@�\�̒ǉ�
		//(added by 2021 LVhJPic0JSk5LiQ1ITskKVk9UGBg)
		BOOL opened = FALSE ;
		int tid = m_iTunerID ;
		for(int i=0;i<2;i++) {
			int iPT = m_bXFirstPT3 ? (i?1:3) : (i?3:1) ;
			LaunchPTCtrl(iPT);
			DWORD dwNumTuner=0;
			if(SendGetTunerCount(&dwNumTuner) == CMD_SUCCESS) {
				if(tid>=0 && DWORD(tid)>=dwNumTuner) {
					tid-=dwNumTuner ;
					continue;
				}
				m_iID=-1 ;
				if(TryOpenTuner(tid, &m_iID)) {
					opened = TRUE; break;
				}
			}
		}
		if(!opened) return FALSE ;

	}else { // PT1/2/3 ( manual )

		LaunchPTCtrl(m_iPT);
		if(!TryOpenTuner(m_iTunerID, &m_iID)){
			return FALSE;
		}

	}

	m_hThread = (HANDLE)_beginthreadex(NULL, 0, RecvThread, (LPVOID)this, CREATE_SUSPENDED, NULL);
	ResumeThread(m_hThread);

	return TRUE;
}

void CBonTuner::CloseTuner(void)
{
	if( m_hThread != NULL ){
		::SetEvent(m_hStopEvent);
		// �X���b�h�I���҂�
		if ( ::WaitForSingleObject(m_hThread, 15000) == WAIT_TIMEOUT ){
			::TerminateThread(m_hThread, 0xffffffff);
		}
		CloseHandle(m_hThread);
		m_hThread = NULL;
	}

	m_dwCurSpace = 0xFF;
	m_dwCurChannel = 0xFF;
    m_hasStream = TRUE;

	::CloseHandle(m_hOnStreamEvent);
	m_hOnStreamEvent = NULL;

	if( m_iID != -1 ){
		SendCloseTuner(m_iID);
		m_iID = -1;
	}

	//�o�b�t�@���
	::EnterCriticalSection(&m_CriticalSection);
	while (!m_TsBuff.empty()){
		TS_DATA *p = m_TsBuff.front();
		m_TsBuff.pop_front();
		delete p;
	}
	::LeaveCriticalSection(&m_CriticalSection);
}

const BOOL CBonTuner::SetChannel(const BYTE bCh)
{
	return TRUE;
}

const float CBonTuner::GetSignalLevel(void)
{
	if( m_iID == -1 || !m_hasStream){
		return 0;
	}
	DWORD dwCn100;
	if( SendGetSignal(m_iID, &dwCn100) == CMD_SUCCESS ){
		return ((float)dwCn100) / 100.0f;
	}else{
		return 0;
	}
}

const DWORD CBonTuner::WaitTsStream(const DWORD dwTimeOut)
{
	if( m_hOnStreamEvent == NULL ){
		return WAIT_ABANDONED;
	}
	// �C�x���g���V�O�i����ԂɂȂ�̂�҂�
	const DWORD dwRet = ::WaitForSingleObject(m_hOnStreamEvent, (dwTimeOut)? dwTimeOut : INFINITE);

	switch(dwRet){
		case WAIT_ABANDONED :
			// �`���[�i������ꂽ
			return WAIT_ABANDONED;

		case WAIT_OBJECT_0 :
		case WAIT_TIMEOUT :
			// �X�g���[���擾�\
			return dwRet;

		case WAIT_FAILED :
		default:
			// ��O
			return WAIT_FAILED;
	}
}

const DWORD CBonTuner::GetReadyCount(void)
{
	DWORD dwCount = 0;
	::EnterCriticalSection(&m_CriticalSection);
	if(m_hasStream) dwCount = (DWORD)m_TsBuff.size();
	::LeaveCriticalSection(&m_CriticalSection);
	return dwCount;
}

const BOOL CBonTuner::GetTsStream(BYTE *pDst, DWORD *pdwSize, DWORD *pdwRemain)
{
	BYTE *pSrc = NULL;

	if(GetTsStream(&pSrc, pdwSize, pdwRemain)){
		if(*pdwSize){
			::CopyMemory(pDst, pSrc, *pdwSize);
		}
		return TRUE;
	}
	return FALSE;
}

const BOOL CBonTuner::GetTsStream(BYTE **ppDst, DWORD *pdwSize, DWORD *pdwRemain)
{
	BOOL bRet;
	::EnterCriticalSection(&m_CriticalSection);
	if( m_hasStream && m_TsBuff.size() != 0 ){
		delete m_LastBuff;
		m_LastBuff = m_TsBuff.front();
		m_TsBuff.pop_front();
		*pdwSize = m_LastBuff->dwSize;
		*ppDst = m_LastBuff->pbBuff;
		*pdwRemain = (DWORD)m_TsBuff.size();
		bRet = TRUE;
	}else{
		*pdwSize = 0;
		*pdwRemain = 0;
		bRet = FALSE;
	}
	::LeaveCriticalSection(&m_CriticalSection);
	return bRet;
}

void CBonTuner::PurgeTsStream(void)
{
	//�o�b�t�@���
	::EnterCriticalSection(&m_CriticalSection);
	while (!m_TsBuff.empty()){
		TS_DATA *p = m_TsBuff.front();
		m_TsBuff.pop_front();
		delete p;
	}
	::LeaveCriticalSection(&m_CriticalSection);
}

LPCTSTR CBonTuner::GetTunerName(void)
{
	return m_strTunerName.c_str();
}

const BOOL CBonTuner::IsTunerOpening(void)
{
	return FALSE;
}

LPCTSTR CBonTuner::EnumTuningSpace(const DWORD dwSpace)
{
	map<DWORD, SPACE_DATA>::iterator itr;
	itr = m_chSet.spaceMap.find(dwSpace);
	if( itr == m_chSet.spaceMap.end() ){
		return NULL;
	}else{
		return itr->second.wszName.c_str();
	}
}

LPCTSTR CBonTuner::EnumChannelName(const DWORD dwSpace, const DWORD dwChannel)
{
	DWORD key = dwSpace<<16 | dwChannel;
	map<DWORD, CH_DATA>::iterator itr;
	itr = m_chSet.chMap.find(key);
	if( itr == m_chSet.chMap.end() ){
		return NULL;
	}else{
		return itr->second.wszName.c_str();
	}
}

const BOOL CBonTuner::SetChannel(const DWORD dwSpace, const DWORD dwChannel)
{
	DWORD key = dwSpace<<16 | dwChannel;
	map<DWORD, CH_DATA>::iterator itr;
	itr = m_chSet.chMap.find(key);
	if (itr == m_chSet.chMap.end()) {
		return FALSE;
	}

	m_hasStream=FALSE ;

	DWORD dwRet=CMD_ERR;
	if( m_iID != -1 ){
		dwRet=SendSetCh(m_iID, itr->second.dwPT1Ch, itr->second.dwTSID);
	}else{
		return FALSE;
	}

	if (m_dwSetChDelay)
		Sleep(m_dwSetChDelay);

	PurgeTsStream();

	m_hasStream = (dwRet&CMD_BIT_NON_STREAM) ? FALSE : TRUE ;
	dwRet &= ~CMD_BIT_NON_STREAM ;

	if( dwRet==CMD_SUCCESS ){
		m_dwCurSpace = dwSpace;
		m_dwCurChannel = dwChannel;
		return TRUE;
	}

	return FALSE;
}

const DWORD CBonTuner::GetCurSpace(void)
{
	return m_dwCurSpace;
}

const DWORD CBonTuner::GetCurChannel(void)
{
	return m_dwCurChannel;
}

void CBonTuner::Release()
{
	delete this;
}

UINT WINAPI CBonTuner::RecvThread(LPVOID pParam)
{
	CBonTuner* pSys = (CBonTuner*)pParam;

	wstring strEvent;
	wstring strPipe;
	Format(strEvent, L"%s%d", CMD_PT1_DATA_EVENT_WAIT_CONNECT, pSys->m_iID);
	Format(strPipe, L"%s%d", CMD_PT1_DATA_PIPE, pSys->m_iID);

	while (1) {
		if (::WaitForSingleObject( pSys->m_hStopEvent, 0 ) != WAIT_TIMEOUT) {
			//���~
			break;
		}
		DWORD dwSize;
		BYTE *pbBuff;
		if ((SendSendData(pSys->m_iID, &pbBuff, &dwSize, strEvent, strPipe) == CMD_SUCCESS) && (dwSize != 0)) {
			if(pSys->m_hasStream) {
				TS_DATA *pData = new TS_DATA(pbBuff, dwSize);
				::EnterCriticalSection(&pSys->m_CriticalSection);
				while (pSys->m_TsBuff.size() > MAX_BUFF_COUNT) {
					TS_DATA *p = pSys->m_TsBuff.front();
					pSys->m_TsBuff.pop_front();
					delete p;
				}
				pSys->m_TsBuff.push_back(pData);
				::LeaveCriticalSection(&pSys->m_CriticalSection);
				::SetEvent(pSys->m_hOnStreamEvent);
			}else {
				//�x�~
				delete [] pbBuff ;
			}
		}else{
			if(!pSys->m_hasStream) pSys->PurgeTsStream();
			::Sleep(5);
		}
	}

	return 0;
}