#include "stdafx.h"
#include <Windows.h>
#include <process.h>
#include <algorithm>
#include <iterator>
#include <vector>
#include <set>

#include <Shlwapi.h>
#pragma comment(lib, "Shlwapi.lib")

#include "BonTuner.h"
#include "../Common/SharedMem.h"

using namespace std;

static CRITICAL_SECTION secBonTuners;
static set<CBonTuner*> BonTuners ;

void InitializeBonTuners(HMODULE hModule)
{
	::InitializeCriticalSection(&secBonTuners);
	CBonTuner::m_hModule = hModule;
}

void FinalizeBonTuners()
{
	::EnterCriticalSection(&secBonTuners);
	vector<CBonTuner*> clone;
	copy(BonTuners.begin(),BonTuners.end(),back_inserter(clone));
	for(auto bon: clone) if(bon!=NULL) bon->Release();
	::LeaveCriticalSection(&secBonTuners);
	::DeleteCriticalSection(&secBonTuners);
}

#pragma warning( disable : 4273 )
extern "C" __declspec(dllexport) IBonDriver * CreateBonDriver()
{
	// 同一プロセスからの複数インスタンス取得可能(IBonDriver3対応により)
	::EnterCriticalSection(&secBonTuners);
	CBonTuner *p = new CBonTuner ;
	if(p!=NULL) BonTuners.insert(p);
	::LeaveCriticalSection(&secBonTuners);
	return p;
}
#pragma warning( default : 4273 )


HINSTANCE CBonTuner::m_hModule = NULL;

	//PTxCtrl実行ファイルのミューテックス名
	#define PT0_CTRL_MUTEX L"PT0_CTRL_EXE_MUTEX" // PTxCtrl.exe
	#define PT1_CTRL_MUTEX L"PT1_CTRL_EXE_MUTEX" // PTCtrl.exe
	#define PT3_CTRL_MUTEX L"PT3_CTRL_EXE_MUTEX" // PT3Ctrl.exe
	#define PT2_CTRL_MUTEX L"PT2_CTRL_EXE_MUTEX" // PTwCtrl.exe

	//PTxCtrlへのコマンド送信用オブジェクト
	CPTSendCtrlCmd
		PT1CmdSender(1), PT3CmdSender(3), // PT1/2/3
		PTwCmdSender(2); // pt2wdm

CBonTuner::CBonTuner()
  : m_PtBuff(MAX_DATA_BUFF_COUNT,1)
{
	m_hOnStreamEvent = NULL;
	m_pPTxCtrlOp = NULL;

	m_dwCurSpace = 0xFF;
	m_dwCurChannel = 0xFF;
	m_hasStream = TRUE ;

	m_dwStartBuffBorder = 0 ;

	m_iID = -1;
	m_hStopEvent = _CreateEvent(FALSE, FALSE, NULL);
	m_hThread = INVALID_HANDLE_VALUE;
	m_hSharedMemTransportMutex = NULL;

	::InitializeCriticalSection(&m_CriticalSection);

	WCHAR strExePath[512] = L"";
	GetModuleFileName(m_hModule, strExePath, 512);

	WCHAR szPath[_MAX_PATH];	// パス
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
	else if(has_prefix(szFname,L"BonDriver_PTw"))
		m_iPT=2;
	else if(has_prefix(szFname,L"BonDriver_PT3"))
		m_iPT=3;
	else
		m_iPT=1;

	m_isISDB_S = TRUE;
	WCHAR szName[256];
	m_iTunerID = -1;

	_wcsupr_s( szFname, sizeof(szFname) / sizeof(WCHAR) ) ;

	auto parse_fname = [&](wstring ptx, wstring prefix=L"") -> void {
	    WCHAR cTS=L'S'; int id=-1;
		wstring ident = L"BONDRIVER_" + ptx ;
		for(auto &v: ident) v=towupper(v);
		wchar_t wcsID[11]; wcsID[0]=wcsID[10]=0;
		if(swscanf_s(szFname,(ident+L"-%1c%[0-9]%*s").c_str(),&cTS,1,wcsID,10)!=2) {
			id = -1 ;
			if(swscanf_s(szFname,(ident+L"-%1c%*s").c_str(),&cTS,1)!=1)
				cTS = L'S' ;
		}else if(wcsID[0]>=L'0'&&wcsID[0]<=L'9')
			id = _wtoi(wcsID);
	    if(prefix==L"") prefix=ptx ;
		if(cTS==L'T')	m_strTunerName = prefix + L" ISDB-T" , m_isISDB_S = FALSE ;
		else 			m_strTunerName = prefix + L" ISDB-S" ;
		if(id>=0) {
			wsprintfW(szName, L" (%d)", id);
			m_strTunerName += szName ;
			m_iTunerID = id ;
		}
	};

	if(m_iPT==0) { // PTx Tuner ( auto detect )

		int detection = GetPrivateProfileIntW(L"SET", L"xFirstPT3", -1, strIni.c_str());
		int bypassPTw = GetPrivateProfileIntW(L"SET", L"xSparePTw", 0, strIni.c_str());
		m_bXFirstPT3 = detection>=0 ? BOOL(detection) :
			PathFileExists((m_strDirPath+L"PT3Ctrl.exe").c_str()) ;
		m_bXSparePTw = bypassPTw==0 ? FALSE:
			PathFileExists((m_strDirPath+L"PTwCtrl.exe").c_str()) ;

		parse_fname(L"PTx");

	}else if(m_iPT==2) { // pt2wdm Tuner

		wstring strPTwini = m_strDirPath + L"BonDriver_PTw-ST.ini";
		if(PathFileExists(strPTwini.c_str())) strIni = strPTwini;

		parse_fname(L"PTw");

	}else if(m_iPT==3) { // PT3 Tuner

		wstring strPT3ini = m_strDirPath + L"BonDriver_PT3-ST.ini";
		if(PathFileExists(strPT3ini.c_str())) strIni = strPT3ini;

		parse_fname(L"PT3");

	}else {  // PT Tuner (PT1/2)

		wstring strPTini = wstring(szPath) + L"BonDriver_PT-ST.ini";
		if(PathFileExists(strPTini.c_str())) strIni = strPTini;

		int iPTn = GetPrivateProfileIntW(L"SET", L"PT1Ver", 2, strIni.c_str());
		if(iPTn<1||iPTn>2) iPTn=2;

		parse_fname(L"PT", iPTn==1 ? L"PT1" : L"PT2");
	}

	m_bTrySpares = GetPrivateProfileIntW(L"SET", L"TrySpares", 0, strIni.c_str());
	m_bBon3Lnb = GetPrivateProfileIntW(L"SET", L"Bon3Lnb", 0, strIni.c_str());
	m_bFastScan = GetPrivateProfileIntW(L"SET", L"FastScan", 0, strIni.c_str());
	m_dwSetChDelay = GetPrivateProfileIntW(L"SET", L"SetChDelay", 0, strIni.c_str());
	m_dwRetryDur = GetPrivateProfileIntW(L"SET", L"RetryDur", 3000, strIni.c_str());
	m_dwStartBuff = GetPrivateProfileIntW(L"SET", L"StartBuff", 8, strIni.c_str());
	SetHRTimerMode(GetPrivateProfileIntW(L"SET", L"UseHRTimer", 0, strIni.c_str()));

	wstring strChSet;

	//dll名と同じ名前の.ChSet.txtを先に優先して読み込みを試行する
	//(fixed by 2020 LVhJPic0JSk5LiQ1ITskKVk9UGBg)
	strChSet = szPath;	strChSet += szFname;	strChSet += L".ChSet.txt";
	if(!m_chSet.ParseText(strChSet.c_str())) {
		strChSet = szPath;
		if(m_iPT==3) {
			if (m_isISDB_S)
				strChSet += L"BonDriver_PT3-S.ChSet.txt";
			else
				strChSet += L"BonDriver_PT3-T.ChSet.txt";
		}else if(m_iPT==1) {
			if (m_isISDB_S)
				strChSet += L"BonDriver_PT-S.ChSet.txt";
			else
				strChSet += L"BonDriver_PT-T.ChSet.txt";
		}else if(m_iPT==2) {
			if (m_isISDB_S)
				strChSet += L"BonDriver_PTw-S.ChSet.txt";
			else
				strChSet += L"BonDriver_PTw-T.ChSet.txt";
		}
		if(!m_iPT||!m_chSet.ParseText(strChSet.c_str())) {
			strChSet = szPath;
			if (m_isISDB_S)
				strChSet += L"BonDriver_PTx-S.ChSet.txt";
			else
				strChSet += L"BonDriver_PTx-T.ChSet.txt";
			if(!m_chSet.ParseText(strChSet.c_str()))
				BuildDefSpace(strIni);
		}
	}

	switch(m_iPT ? m_iPT : m_bXFirstPT3 ? 3 : 1) {
	case 1:	m_pCmdSender = &PT1CmdSender; break;
	case 3:	m_pCmdSender = &PT3CmdSender; break;
	case 2:	m_pCmdSender = &PTwCmdSender; break; // pt2wdm
	}
}

CBonTuner::~CBonTuner()
{
	CloseTuner();

	::CloseHandle(m_hStopEvent);
	m_hStopEvent = NULL;

	::DeleteCriticalSection(&m_CriticalSection);

	::EnterCriticalSection(&secBonTuners);
	BonTuners.erase(this);
	::LeaveCriticalSection(&secBonTuners);
}

void CBonTuner::BuildDefSpace(wstring strIni)
{
	//.ChSet.txtが存在しない場合は、既定のチャンネル情報を構築する
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

	if(m_isISDB_S) {  // BS / CS110

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

		auto entry_tp = [&](const wchar_t *prefix) {
			TP_DATA item;
			Format(item.wszName,L"%s%02d",prefix,ch);
			item.dwSpace=spc;
			item.dwCh=i;
			item.dwPT1Ch=(ch-1)/2+pt1offs;
			DWORD iKey = (item.dwSpace<<16) | item.dwCh;
			m_chSet.tpMap.insert( pair<DWORD, TP_DATA>(iKey,item) );
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
			for(i=0,ch=1;ch<=23;ch+=2,i++)
				entry_tp(L"BS");
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
			for(i=0,ch=2;ch<=24;ch+=2,i++)
				entry_tp(L"ND");
			entry_spc(L"CS110");
		}

	}else { // 地デジ

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
			entry_spc(L"地デジ(UHF)") ;
		}

		if(CATV) {
			for(offs=13,C=1,i=0;i<51;i++) entry_ch([](DWORD i){return i+(i>=10?12:3);});
			entry_spc(L"地デジ(CATV)") ;
		}

		if(VHF) {
			for(offs=1,C=i=0;i<12;i++) entry_ch([](DWORD i){return i+(i>=3?10:0);});
			entry_spc(L"地デジ(VHF)") ;
		}

	}
}

void CBonTuner::FlushPtBuff(BOOL dispose)
{
	PTBUFFER &buf = m_PtBuff ;
	if(dispose) {
		buf.dispose();
		for(size_t i=0; i<INI_DATA_BUFF_COUNT; i++) {
			buf.head()->growup(DATA_BUFF_SIZE);
			buf.push();
		}
	}
	buf.clear();
	m_dwStartBuffBorder = m_dwStartBuff ;
}

BOOL CBonTuner::LaunchPTCtrl(int iPT)
{
	PROCESS_INFORMATION pi;
	STARTUPINFO si;
	ZeroMemory(&si,sizeof(si));
	si.cb=sizeof(si);

	wstring strPTCtrlExe = m_strDirPath ;
	wstring mutexName ;

	switch(iPT) {
	case 0:
		strPTCtrlExe += L"PTxCtrl.exe" ;
		mutexName = PT0_CTRL_MUTEX ;
		break ;
	case 1:
		strPTCtrlExe += L"PTCtrl.exe" ;
		mutexName = PT1_CTRL_MUTEX ;
		break ;
	case 3:
		strPTCtrlExe += L"PT3Ctrl.exe" ;
		mutexName = PT3_CTRL_MUTEX ;
		break ;
	case 2:
		strPTCtrlExe += L"PTwCtrl.exe" ;
		mutexName = PT2_CTRL_MUTEX ;
		break ;
	}

	bool hasMutex = FALSE ;
	if(HANDLE Mutex = OpenMutex(MUTEX_ALL_ACCESS, FALSE, mutexName.c_str())) {
		// 既に起動中
		hasMutex = TRUE ;
		CloseHandle(Mutex) ;
		if(m_bExecPT[iPT])
			return TRUE ;
	}

	if(!PathFileExists(strPTCtrlExe.c_str())) {
		// 実行ファイルが存在しない
		return hasMutex ;
	}

	strPTCtrlExe = L"\""+strPTCtrlExe+L"\"" ;
	BOOL bRet = CreateProcessW( NULL, (LPWSTR)strPTCtrlExe.c_str(), NULL, NULL, FALSE, GetPriorityClass(GetCurrentProcess()), NULL, NULL, &si, &pi );
	CloseHandle(pi.hThread);
	CloseHandle(pi.hProcess);

	_RPT3(_CRT_WARN, "*** CBonTuner::LaunchPTCtrl() ***\nbRet[%s]", bRet ? "TRUE" : "FALSE");

	if(!bRet) bRet = hasMutex ;
	m_bExecPT[iPT] = bRet ;
	return bRet ;
}

BOOL CBonTuner::TryOpenTunerByID(int iTunerID, int *piID)
{
	DWORD dwRet;
	if( iTunerID >= 0 ){
		dwRet = m_pCmdSender->OpenTuner2(m_isISDB_S, iTunerID, piID);
	}else{
		dwRet = m_pCmdSender->OpenTuner(m_isISDB_S, piID);
	}

	_RPT3(_CRT_WARN, "*** CBonTuner::TryOpenTunerByID() ***\ndwRet[%u]\n", dwRet);

	if( dwRet != CMD_SUCCESS ){
		return FALSE;
	}

	return TRUE;
}

BOOL CBonTuner::TryOpenTuner()
{
	//イベント
	m_hOnStreamEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);

	_RPT3(_CRT_WARN, "*** CBonTuner::TryOpenTuner() ***\nm_hOnStreamEvent[%p]\n", m_hOnStreamEvent);

	for(auto &v: m_bExecPT) v = FALSE ;

	auto launchPTxCtrl = [&](int iPT) -> bool {
		DWORD bits=0 ;
		if(!LaunchPTCtrl(0))
			return false;
		if(m_pPTxCtrlOp==NULL)
			m_pPTxCtrlOp = new CPTxCtrlCmdOperator(CMD_PTX_CTRL_OP);
		if( !m_pPTxCtrlOp->CmdSupported(bits) ||
			!(bits&(1<<(iPT-1))) ||
			!m_pPTxCtrlOp->CmdActivatePt(iPT) ) {
				return false;
		}
		return true;
	};

	for( bool opened=false, retry=true ; retry ; ) {

		retry=false ;

		if(!m_iPT) { // PTx ( PT1/2/3 - auto detect )

			//PTx自動検出機能の追加
			//(added by 2021 LVhJPic0JSk5LiQ1ITskKVk9UGBg)
			int tid = m_iTunerID ;
			for(int i=0;i<2;i++) {
				int iPT = m_bXFirstPT3 ? (i?1:3) : (i?3:1) ;
				BOOL ptx = FALSE ;
				if(!LaunchPTCtrl(iPT)) {
					if(!launchPTxCtrl(iPT))
						continue;
				}else {
					SAFE_DELETE(m_pPTxCtrlOp);
				}
				switch(iPT) {
				case 1:	m_pCmdSender = &PT1CmdSender; break;
				case 3:	m_pCmdSender = &PT3CmdSender; break;
				}
				DWORD dwNumTuner=0;
				if(m_pCmdSender->GetTotalTunerCount(&dwNumTuner) == CMD_SUCCESS) {
					if(tid>=0 && DWORD(tid)>=dwNumTuner) {
						tid-=dwNumTuner ;
						continue;
					}
					m_iID=-1 ;
					if(TryOpenTunerByID(tid, &m_iID)) {
						opened = true; break;
					}else if(m_bTrySpares) {
						if(tid>=0) tid=-1, i=-1 ;
					}
					if(tid>=0) break;
				}
			}

		}else do { // PT1/2/3 or pt2wdm ( manual )

			if(!LaunchPTCtrl(m_iPT)) {
				if(!launchPTxCtrl(m_iPT))
					break;
			}
			if(!TryOpenTunerByID(m_iTunerID, &m_iID)){
				if(m_iTunerID<0 || !m_bTrySpares || !TryOpenTunerByID(-1, &m_iID))
					break;
			}

			opened = true ;

		}while(0);

		if(!opened) {
			SAFE_DELETE(m_pPTxCtrlOp);
			if(!m_iPT) {
				if(m_bXSparePTw) { // Make a bypass to pt2wdm
					m_iPT = 2 ;
					m_pCmdSender = &PTwCmdSender ;
					retry = true ;
				}
			}
			if(!retry) return FALSE ;
		}
	}


	PTSTREAMING streaming_method=PTSTREAMING_PIPEIO;
	m_pCmdSender->GetStreamingMethod(&streaming_method);
	switch(streaming_method) {
	case PTSTREAMING_PIPEIO:
		m_hThread = (HANDLE)_beginthreadex(NULL, 0, RecvThreadPipeIOProc, (LPVOID)this, CREATE_SUSPENDED, NULL);
		break;
	case PTSTREAMING_SHAREDMEM:
		m_hThread = (HANDLE)_beginthreadex(NULL, 0, RecvThreadSharedMemProc, (LPVOID)this, CREATE_SUSPENDED, NULL);
		if(m_hThread!=INVALID_HANDLE_VALUE) {
			wstring memName;
			Format(memName,SHAREDMEM_TRANSPORT_FORMAT,m_pCmdSender->GetPTKind(),m_iID) ;
			m_hSharedMemTransportMutex = _CreateMutex(TRUE, memName.c_str());
		}
		break;
	}
	if(m_hThread!=INVALID_HANDLE_VALUE) {
		::EnterCriticalSection(&m_CriticalSection);
		FlushPtBuff(TRUE);
		::LeaveCriticalSection(&m_CriticalSection);
		ResumeThread(m_hThread);
	}

	return TRUE;
}

const BOOL CBonTuner::OpenTuner(void)
{
	CloseTuner();

	for(DWORD s=dur(),e=s;dur(s,e)<=m_dwRetryDur;e=dur()) {
		if(TryOpenTuner()) return TRUE ;
	}

	return FALSE ;
}

void CBonTuner::CloseTuner(void)
{
	auto closeThread = [&]() {
		if( m_hThread != INVALID_HANDLE_VALUE ){
			::SetEvent(m_hStopEvent);
			// スレッド終了待ち
			if ( ::HRWaitForSingleObject(m_hThread, 15000) == WAIT_TIMEOUT ){
				::TerminateThread(m_hThread, 0xffffffff);
			}
			CloseHandle(m_hThread);
			m_hThread = INVALID_HANDLE_VALUE ;
		}
		if(m_hSharedMemTransportMutex != NULL) {
			ReleaseMutex(m_hSharedMemTransportMutex) ;
			CloseHandle(m_hSharedMemTransportMutex) ;
			m_hSharedMemTransportMutex=NULL;
		}
	};

	auto closeTuner = [&]() {
		if( m_iID != -1 ){
			m_pCmdSender->CloseTuner(m_iID);
			m_iID = -1;
		}
		if(m_pPTxCtrlOp!=NULL)
			m_pPTxCtrlOp->CmdIdle();
		SAFE_DELETE(m_pPTxCtrlOp);
	};

	// ストリーミングの種類によって閉じ方のパターンを変える
	if(m_hSharedMemTransportMutex!=NULL) {
		// チューナーを閉じてからスレッドを閉じる [PTSTREAMING_SHAREDMEM]
		closeTuner();
		closeThread();
	}else {
		// スレッドを閉じてからチューナーを閉じる [PTSTREAMING_PIPEIO]
		closeThread();
		closeTuner();
	}

	m_dwCurSpace = 0xFF;
	m_dwCurChannel = 0xFF;
	m_hasStream = TRUE;

	if(m_hOnStreamEvent!=NULL) {
		::CloseHandle(m_hOnStreamEvent);
		m_hOnStreamEvent = NULL;
	}

	//バッファ解放
	::EnterCriticalSection(&m_CriticalSection);
	FlushPtBuff();
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
	if( m_pCmdSender->GetSignal(m_iID, &dwCn100) == CMD_SUCCESS ){
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
	// イベントがシグナル状態になるのを待つ
	const DWORD dwRet = ::HRWaitForSingleObject(m_hOnStreamEvent, (dwTimeOut)? dwTimeOut : INFINITE);

	switch(dwRet){
		case WAIT_ABANDONED :
			// チューナが閉じられた
			return WAIT_ABANDONED;

		case WAIT_OBJECT_0 :
		case WAIT_TIMEOUT :
			// ストリーム取得可能
			return dwRet;

		case WAIT_FAILED :
		default:
			// 例外
			return WAIT_FAILED;
	}
}

const DWORD CBonTuner::GetReadyCount(void)
{
	DWORD dwCount = 0;
	::EnterCriticalSection(&m_CriticalSection);
	if(m_hasStream) dwCount = (DWORD)m_PtBuff.size();
	dwCount = dwCount>m_dwStartBuffBorder ? dwCount-m_dwStartBuffBorder : 0 ;
	::LeaveCriticalSection(&m_CriticalSection);
	return dwCount ;
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
	if( m_hasStream && m_PtBuff.size() > m_dwStartBuffBorder ){
		PTBUFFER_OBJECT *buf = m_PtBuff.pull() ;
		*pdwSize = (DWORD)buf->size();
		*ppDst = buf->data() ;
		if(m_PtBuff.size()>m_dwStartBuffBorder)
			*pdwRemain = (DWORD)m_PtBuff.size()-m_dwStartBuffBorder;
		else
			*pdwRemain = 0;
		if(m_dwStartBuffBorder) m_dwStartBuffBorder-- ;
		bRet = TRUE;
	}else{
		*ppDst = NULL;
		*pdwSize = 0;
		*pdwRemain = 0;
		bRet = FALSE;
	}
	::LeaveCriticalSection(&m_CriticalSection);
	return bRet;
}

void CBonTuner::PurgeTsStream(void)
{
	//バッファ解放
	::EnterCriticalSection(&m_CriticalSection);
	FlushPtBuff();
	::LeaveCriticalSection(&m_CriticalSection);
}

LPCTSTR CBonTuner::GetTunerName(void)
{
	return m_strTunerName.c_str();
}

const BOOL CBonTuner::IsTunerOpening(void)
{
	return m_iID>=0 ? TRUE : FALSE ;
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
		dwRet=m_pCmdSender->SetCh(m_iID, itr->second.dwPT1Ch, itr->second.dwTSID);
	}else{
		return FALSE;
	}

	if (m_dwSetChDelay)
		HRSleep(m_dwSetChDelay);

	PurgeTsStream();

	m_hasStream = (dwRet&CMD_BIT_NON_STREAM) ? FALSE : TRUE ;
	dwRet &= ~CMD_BIT_NON_STREAM ;

	if( dwRet==CMD_SUCCESS ){
		m_dwCurSpace = dwSpace;
		m_dwCurChannel = dwChannel;
		if(m_bFastScan) return m_hasStream ? TRUE : FALSE ;
		return TRUE ;
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

UINT WINAPI CBonTuner::RecvThreadPipeIOProc(LPVOID pParam)
{
	CBonTuner* pSys = (CBonTuner*)pParam;


	PTBUFFER_OBJECT *pPtBuffObj=nullptr;
	for (;;) {
		if (::HRWaitForSingleObject( pSys->m_hStopEvent, 0 ) != WAIT_TIMEOUT) {
			//中止
			break;
		}
		if(pPtBuffObj==nullptr) {
			::EnterCriticalSection(&pSys->m_CriticalSection);
			pPtBuffObj = pSys->m_PtBuff.head() ;
			if(pPtBuffObj==nullptr&&pSys->m_PtBuff.no_pool()) {
				// buffer overflow
				pSys->m_PtBuff.pull();
				pPtBuffObj = pSys->m_PtBuff.head() ;
			}
			::LeaveCriticalSection(&pSys->m_CriticalSection);
			if(pPtBuffObj==nullptr) {
				//中止
				break;
			}
		}
		if (pSys->m_pCmdSender->SendBufferObject(pSys->m_iID, pPtBuffObj) == CMD_SUCCESS) {
			if(pSys->m_hasStream) {
				::EnterCriticalSection(&pSys->m_CriticalSection);
				bool done = pSys->m_PtBuff.push();
				DWORD sz = (DWORD)pSys->m_PtBuff.size();
				::LeaveCriticalSection(&pSys->m_CriticalSection);
				if(done) {
					if(sz>pSys->m_dwStartBuffBorder) {
						::SetEvent(pSys->m_hOnStreamEvent);
					}
					pPtBuffObj=nullptr;
				}
			}
		}else{
			if(!pSys->m_hasStream) pSys->PurgeTsStream();
			HRSleep(5);
		}
	}

	return 0;
}

UINT WINAPI CBonTuner::RecvThreadSharedMemProc(LPVOID pParam)
{
	const DWORD MAXWAIT = 250;
	CBonTuner* pSys = (CBonTuner*)pParam;

	wstring strStreamerName;
	Format(strStreamerName, SHAREDMEM_TRANSPORT_STREAM_FORMAT,
		pSys->m_pCmdSender->GetPTKind(), pSys->m_iID);
	CSharedTransportStreamer streamer(strStreamerName,
		TRUE, SHAREDMEM_TRANSPORT_PACKET_SIZE, SHAREDMEM_TRANSPORT_PACKET_NUM);
	DBGOUT("BON Streamer memName: %s\n",wcs2mbcs(streamer.Name()).c_str());

	DWORD rem=0;
	PTBUFFER_OBJECT *pPtBuffObj=nullptr;
	for (;;) {
		if (::HRWaitForSingleObject( pSys->m_hStopEvent, 0 ) != WAIT_TIMEOUT) {
			//中止
			break;
		}
		DWORD wait_res = rem ? WAIT_OBJECT_0 : streamer.WaitForCmd(MAXWAIT);
		if(wait_res==WAIT_TIMEOUT) {
			if(!pSys->m_hasStream) pSys->PurgeTsStream();
			continue;
		}
		if(wait_res==WAIT_OBJECT_0) {
			if(pPtBuffObj==nullptr) {
				::EnterCriticalSection(&pSys->m_CriticalSection);
				pPtBuffObj = pSys->m_PtBuff.head() ;
				if(pPtBuffObj==nullptr&&pSys->m_PtBuff.no_pool()) {
					// buffer overflow
					pSys->m_PtBuff.pull();
					pPtBuffObj = pSys->m_PtBuff.head() ;
				}
				::LeaveCriticalSection(&pSys->m_CriticalSection);
				if(pPtBuffObj==nullptr) {
					//中止
					break;
				}
			}
			if(!pPtBuffObj->resize(SHAREDMEM_TRANSPORT_PACKET_SIZE)) {
				//中止
				break;
			}
			DWORD dwSize=0;
			if(streamer.Rx(pPtBuffObj->data(), dwSize, MAXWAIT)&&pSys->m_hasStream) {
				pPtBuffObj->resize(dwSize);
				::EnterCriticalSection(&pSys->m_CriticalSection);
				bool done = pSys->m_PtBuff.push();
				DWORD sz = (DWORD)pSys->m_PtBuff.size();
				::LeaveCriticalSection(&pSys->m_CriticalSection);
				if(done) {
					if(sz>pSys->m_dwStartBuffBorder) {
						::SetEvent(pSys->m_hOnStreamEvent);
					}
					pPtBuffObj=nullptr;
				}
			}
			if(!rem)
				rem=streamer.PacketRemain(MAXWAIT);
			else
				rem--;
		}else break;
	}

	return 0;
}

void CBonTuner::GetTunerCounters(DWORD *lpdwTotal, DWORD *lpdwActive)
{
	if(m_iTunerID>=0) { // ID固定チューナー
		if(lpdwTotal) *lpdwTotal = 1 ;
		if(lpdwActive) *lpdwActive = m_hThread ? 1 : 0 ;
	}else { // ID自動割り当てチューナー
		if(lpdwTotal) *lpdwTotal=0;
		if(lpdwActive) *lpdwActive=0;
		for(int i=1;i<=3;i++) {
			if((!m_iPT&&i!=2)||m_iPT==i) {
				if(LaunchPTCtrl(i)) {
					CPTSendCtrlCmd *sender;
					switch(i) {
					case 1:	sender = &PT1CmdSender; break;
					case 3:	sender = &PT3CmdSender; break;
					case 2:	sender = &PTwCmdSender; break;
					}
					DWORD dwNumTuner=0;
					if(lpdwTotal && sender->GetTotalTunerCount(&dwNumTuner) == CMD_SUCCESS) {
						*lpdwTotal += dwNumTuner ;
					}
					dwNumTuner=0;
					if(lpdwActive && sender->GetActiveTunerCount(m_isISDB_S,&dwNumTuner) == CMD_SUCCESS) {
						*lpdwActive += dwNumTuner ;
					}
				}
			}
		}
	}
}

	//IBonDriver3の機能を追加
	//(added by 2021 LVhJPic0JSk5LiQ1ITskKVk9UGBg)

const DWORD CBonTuner::GetTotalDeviceNum(void)
{
	DWORD nTotal=0;
	GetTunerCounters(&nTotal,NULL);
	return nTotal;
}

const DWORD CBonTuner::GetActiveDeviceNum(void)
{
	DWORD nActive=0;
	GetTunerCounters(NULL,&nActive);
	return nActive;
}

const BOOL CBonTuner::SetLnbPower(const BOOL bEnable)
{
	//チューナーをオープンした状態で呼ばないと正しい動作は期待できない
	if(!m_bBon3Lnb) return TRUE;
	if(!m_hThread) return FALSE;
	if(m_iID<0) return FALSE;
	return m_pCmdSender->SetLnbPower(m_iID,bEnable) == CMD_SUCCESS ? TRUE : FALSE ;
}

	//IBonTransponderの機能を追加
	//(added by 2021 LVhJPic0JSk5LiQ1ITskKVk9UGBg)

	#define TRANSPONDER_CHMASK 0x80000000

LPCTSTR CBonTuner::TransponderEnumerate(const DWORD dwSpace, const DWORD dwTransponder)
{
	DWORD key = dwSpace<<16 | dwTransponder;
	map<DWORD, TP_DATA>::iterator itr;
	itr = m_chSet.tpMap.find(key);
	if( itr == m_chSet.tpMap.end() ){
		return NULL;
	}
	return itr->second.wszName.c_str();
}

const BOOL CBonTuner::TransponderSelect(const DWORD dwSpace, const DWORD dwTransponder)
{
	if(!m_isISDB_S) return FALSE;

	DWORD key = dwSpace<<16 | dwTransponder;
	map<DWORD, TP_DATA>::iterator itr;
	itr = m_chSet.tpMap.find(key);
	if( itr == m_chSet.tpMap.end() ){
		return FALSE;
	}

	DWORD dwRet=CMD_ERR;
	if(m_iID!=-1){
		dwRet=m_pCmdSender->SetFreq(m_iID, itr->second.dwPT1Ch);
	}

	if( dwRet==CMD_SUCCESS ) {
		m_dwCurSpace = dwSpace;
		m_dwCurChannel = dwTransponder | TRANSPONDER_CHMASK ;
		m_hasStream = FALSE; // TransponderSetCurID はまだ行っていないので
		return TRUE;
	}

	return FALSE;
}

const BOOL CBonTuner::TransponderGetIDList(LPDWORD lpIDList, LPDWORD lpdwNumID)
{
	if(!m_isISDB_S) return FALSE;

	DWORD dwRet=CMD_ERR;
	PTTSIDLIST PtTSIDList;
	const DWORD numId = sizeof(PtTSIDList) / sizeof(DWORD) ;

	if(lpdwNumID==NULL) {
		return FALSE ;
	}else if(lpIDList==NULL) {
		*lpdwNumID = numId ;
		return TRUE ;
	}

	if(m_iID!=-1){
		dwRet=m_pCmdSender->GetIdListS(m_iID, &PtTSIDList);
	}

	if(dwRet==CMD_SUCCESS) {
		if(*lpdwNumID>numId) *lpdwNumID = numId ;
		for(DWORD i=0;i<*lpdwNumID;i++) {
			if(!PtTSIDList.dwId[i]||(PtTSIDList.dwId[i]&0xFFFF)==0xFFFF)
				lpIDList[i] = 0xFFFFFFFF;
			else
				lpIDList[i] = PtTSIDList.dwId[i] ;
		}
		return TRUE;
	}

	return FALSE;
}

const BOOL CBonTuner::TransponderSetCurID(const DWORD dwID)
{
	if(!m_isISDB_S) return FALSE;

	DWORD dwRet=CMD_ERR;
	if(m_iID!=-1){
		dwRet=m_pCmdSender->SetIdS(m_iID, dwID);
	}

	if (m_dwSetChDelay)
		HRSleep(m_dwSetChDelay);

	PurgeTsStream();

	if(dwRet==CMD_SUCCESS) {
		m_hasStream=TRUE;
		return TRUE;
	}

	m_hasStream=FALSE;
	return FALSE;
}

const BOOL CBonTuner::TransponderGetCurID(LPDWORD lpdwID)
{
	if(lpdwID==NULL||!m_isISDB_S) return FALSE;

	DWORD dwRet=CMD_ERR;
	if(m_iID!=-1){
		if(!m_hasStream) {
			*lpdwID=0xFFFFFFFF;
			return TRUE;
		}
		else
			dwRet=m_pCmdSender->GetIdS(m_iID, lpdwID);
	}

	if(dwRet==CMD_SUCCESS) {
		return TRUE;
	}

	return FALSE;
}

	//IBonPTxの機能を追加
	//(added by 2021 LVhJPic0JSk5LiQ1ITskKVk9UGBg)

const DWORD CBonTuner::TransponderGetPTxCh(const DWORD dwSpace, const DWORD dwTransponder)
{
	DWORD key = dwSpace<<16 | dwTransponder;
	map<DWORD, TP_DATA>::iterator itr;
	itr = m_chSet.tpMap.find(key);
	if( itr == m_chSet.tpMap.end() ){
		return 0xFFFFFFFF;
	}
	return itr->second.dwPT1Ch;
}

