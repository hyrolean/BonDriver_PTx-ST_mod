// PTxScanS.cpp : コンソール アプリケーションのエントリ ポイントを定義します。
//

#include "stdafx.h"
#include <algorithm>
#include <vector>
#include <string>
#include <memory>
#include <deque>
#include <ctime>
#include "../Common/IBonTransponder.h"
#include "../Common/IBonPTx.h"

using namespace std;

// 周波数調整に費やす最大時間(ms)
DWORD MAXDUR_FREQ=1000;
// TMCC取得に費やす最大時間(ms)
DWORD MAXDUR_TMCC=1500;

using str_vector = vector<string> ;
using tsid_vector = vector<DWORD> ;
typedef IBonDriver *(*CREATEBONDRIVER)() ;

struct TRANSPONDER {
	string Name ;
	tsid_vector TSIDs;
	TRANSPONDER(string name="") : Name(name) {}
};
using TRANSPONDERS = vector<TRANSPONDER> ;

string AppPath = "" ;
string BonFile = "" ;

char vp_transponder = 0 ;
char vp_chset = 0 ;
char vp_tsid_stream = 'n' ;
char vp_deep_tuning = 'n' ;

HMODULE BonModule = NULL;
IBonDriver2 *BonTuner = NULL;
IBonTransponder *BonTransponder = NULL;
IBonPTx *BonPTx = NULL;

void help()
{
  puts(R"^(Usage:
  PTxScnanS [-param|+param] [--verbose-param] [BonDriver.dll]

    param, verbose-param:
      +c, --chset
        Enable writing the scanning results to the .ChSet.txt file.
      -c, --no-chset
        Disable writing the scanning results to the .ChSet.txt file.
      +t, --transponder
        Enable writing the transponder information to the scanning results.
      -t, --no-transponder
        Disable writing the transponder information to the scanning results.
      +s, --tsid-stream
        Write TSID columns as stream number columns to the scanning results.
      +d, --deep-tuning
        Test the integrity of the TSID referred from the TMCC-Info one by one.
      -?|+?, --help
        Show this help message and exit.

    BonDriver.dll:
      Load indicated .dll automatically if specified at the last argument.)^");
}

string wcs2mbcs(wstring src, UINT code_page=CP_ACP)
{
  int mbLen = WideCharToMultiByte(code_page, 0, src.c_str(), (int)src.length(), NULL, 0, NULL, NULL) ;
  if(mbLen>0) {
	char* mbcs = (char*) alloca(mbLen) ;
	WideCharToMultiByte(code_page, 0, src.c_str(), (int)src.length(), mbcs, mbLen, NULL, NULL);
	return string(mbcs,mbLen);
  }
  return string("");
}

int EnumCandidates(str_vector &candidates, string filter)
{
	int n = 0 ;
	WIN32_FIND_DATAA Data ;
	HANDLE HFind = FindFirstFileA((AppPath+filter).c_str(),&Data) ;
	if(HFind!=INVALID_HANDLE_VALUE) {
		do {
			if(Data.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY) continue ;
			candidates.push_back(Data.cFileName) ;
			n++;
		}while(FindNextFileA(HFind,&Data)) ;
		FindClose(HFind) ;
	}
	return n;
}

string DoSelectS()
{
	str_vector candidates;
	EnumCandidates(candidates,"BonDriver_PTx-S*.dll");
	EnumCandidates(candidates,"BonDriver_PT-S*.dll");
	EnumCandidates(candidates,"BonDriver_PT3-S*.dll");
	EnumCandidates(candidates,"BonDriver_PTw-S*.dll");
	if(candidates.empty()) {
		help();
		puts("\nPress <Ctrl+C> to exit.");
		for(int i=0;i<78;i++) {
		   putchar('.');
		   Sleep(1000);
		}
		return "";
	}
	int choice=-1;
	for(size_t i=0;i<candidates.size();i+=10) {
		size_t n=min(candidates.size(),i+10) ;
		for(;;) {
			for(size_t j=i;j<n;j++) {
				printf("  %d) %s\n",int(j-i),candidates[j].c_str());
			}
			if(n<candidates.size())
				printf("サテライトスキャンする候補を選択[0～%d][q=中断][n=次頁]> ",int(n-i-1));
			else
				printf("サテライトスキャンする候補を選択[0～%d][q=中断]> ",int(n-i-1));
			char c = tolower(getchar());
			while(c<=0x20) c = tolower(getchar());
			putchar('\n');
			if(n<candidates.size()&&c=='n') break ;
			else if(c=='q') {
				puts("中断しました。\n");
				return "";
			}
			else if(c>='0'&&c<'0'+char(n-i)) {
				choice = c-'0' + char(i);
				break;
			}else {
				puts("入力した値が正しくありません。もう一度やり直してください。\n");
			}
		}
		if(choice>=0) break;
	}
	if(choice>=0)
		return candidates[choice] ;
	return "";
}

bool LoadBon()
{
	printf("凡ドライバ \"%s\" をロードしています...\n\n",BonFile.c_str());
	string path = AppPath + BonFile ;
	BonModule = LoadLibraryA(path.c_str()) ;
	if(BonModule==NULL) {
		printf("チューナーファイル \"%s\" のロードに失敗。\n\n",path.c_str());
		return false ;
	}
	CREATEBONDRIVER CreateBonDriver_
	  = (CREATEBONDRIVER) GetProcAddress(BonModule,"CreateBonDriver") ;
	if(!CreateBonDriver_) {
		printf("凡ドライバ \"%s\" のインタンス生成に失敗。\n\n",BonFile.c_str());
		return false ;
	}
	try {
		BonTuner = dynamic_cast<IBonDriver2*>(CreateBonDriver_()) ;
	}catch(bad_cast &e) {
		printf("Cast error: %s\n",e.what());
		puts("IBonDriver2インターフェイスを確認できませんでした。\n");
		return false;
	}
	printf("凡ドライバ \"%s\" のインタンス生成に成功。\n\n",BonFile.c_str());
	return true;
}

void FreeBon()
{
	if(BonTuner!=NULL) { BonTuner->Release(); BonTuner=NULL; }
	if(BonModule!=NULL) { FreeLibrary(BonModule); BonModule=NULL; }
}

bool SelectTransponder(DWORD spc, DWORD tp)
{
	BOOL res;
	if(!(res=BonTransponder->TransponderSelect(spc,tp))) {
	  Sleep(MAXDUR_FREQ);
	  res = BonTransponder->TransponderSelect(spc,tp) ;
	}
	if(res)
		BonTuner->PurgeTsStream();
	return res ? true:false ;
}

bool MakeIDList(tsid_vector &idList)
{
	DWORD num_ids=0;
	if(!BonTransponder->TransponderGetIDList(NULL, &num_ids)||!num_ids)
		return false;

	DWORD *ids = (DWORD*) alloca(sizeof(DWORD)*num_ids);

	Sleep(MAXDUR_TMCC);

	if(!BonTransponder->TransponderGetIDList(ids, &num_ids))
		return false;

	for(DWORD i=0;i<num_ids;i++) {
		DWORD id = ids[i]&0xFFFF ;
		if(id==0||id==0xFFFF) continue;
		idList.push_back(id);
	}

	//念の為、ソートしておく
	stable_sort(idList.begin(), idList.end(),
		[](DWORD lhs, DWORD rhs) -> bool {
			return (lhs&7) < (rhs&7) ;
		}
	);

	return true ;
}

bool CheckID(DWORD id)
{
	if(!BonTransponder->TransponderSetCurID(id)) return false;
	DWORD curID = 0 ;
	if(!BonTransponder->TransponderGetCurID(&curID)) return false;
	if(curID!=id) return false;

	auto dur = [](DWORD s=0, DWORD e=GetTickCount()) -> DWORD {
		// duration ( s -> e )
		return s <= e ? e - s : 0xFFFFFFFFUL - s + 1 + e;
	};

	DWORD s = dur(), u = s ;
	using rate_t = pair<float,DWORD> ;
	deque<rate_t> avg;
	rate_t sum = pair<float,DWORD>(0.f,0), cur = pair<float,DWORD>(-1.f,0) ;

	const DWORD wait = 500, max_wait=5000;
	char status[80] = "\0" ;

	for(;;) { // signal and bps testing
		DWORD e = dur();
		if(dur(s,e)>=max_wait) break;
		DWORD w = dur(u,e);
		if(w>=wait) {
			cur.second = cur.second * 1000 / w ;
			avg.push_front(cur);
			sum.first += cur.first ; sum.second += cur.second ;
			cur = pair<float,DWORD>(-1.f,0) ;
			if(avg.size()>10) {
				sum.first -= avg.back().first ;
				sum.second -= avg.back().second ;
				avg.pop_back();
			}
			float sig = sum.first / float(avg.size()) ;
			float mbps = (sum.second * 8.f) / (1024.f * 1024.f * float(avg.size())) ;
			for(size_t i=0;status[i];i++) putchar('\b');
			for(size_t i=0;status[i];i++) putchar(' ');
			for(size_t i=0;status[i];i++) putchar('\b');
			sprintf_s(status,"%.2f dB / %.2f Mbps", sig, mbps);
			printf("%s",status);
			u=e ;
		}
		int n=BonTuner->GetReadyCount();
		if(!n) {
			BonTuner->WaitTsStream(wait);
			n=BonTuner->GetReadyCount();
		}
		while(n>0) {
			BYTE *buf=NULL; DWORD sz=0, rem=0;
			if(BonTuner->GetTsStream(&buf, &sz, &rem)) {
				cur.second += sz ;
			}
			if(!--n) {
				float sig = BonTuner->GetSignalLevel();
				if(sig>cur.first) cur.first = sig ;
			}
		}
	}

	for(size_t i=0;status[i];i++) putchar('\b');
	for(size_t i=0;status[i];i++) putchar(' ');
	for(size_t i=0;status[i];i++) putchar('\b');

	return sum.first  > 0.f  && sum.second > 0 ;
}

bool OutChSet(string out_file, const str_vector &spaces, const vector<TRANSPONDERS> &transponders_list, bool out_transponders)
{
	string filename = AppPath + out_file ;
	FILE *st=NULL;
	fopen_s(&st,filename.c_str(),"wt");
	if(!st) return false ;
	fputs("; -*- This file was generated automatically by PTxScanS -*-\n",st);
	time_t now = time(NULL) ;
	char asct_buff[80];
	tm now_tm; localtime_s(&now_tm,&now);
	asctime_s(asct_buff,80,&now_tm);
	fputs(";\t@ ",st); fputs(asct_buff,st);
	fputs(";BS/CS用\n",st);
	fputs(";チューナー空間(タブ区切り：$名称\tBonDriverとしてのチューナ空間\n",st);
	for(size_t spc=0;spc<spaces.size();spc++)
		fprintf(st,"$%s\t%d\n",spaces[spc].c_str(),(int)spc);
	if(out_transponders) {
		fputs(";BS/CS110トランスポンダ情報[mod5]\n",st);
		fputs(
			";トランスポンダ(タブ区切り：%名称\t"
			"BonDriverとしてのチューナ空間\t"
			"BonDriverとしてのチャンネル\t"
			"PTxとしてのチャンネル)\n",st);
		for(size_t spc=0;spc<transponders_list.size();spc++) {
			const TRANSPONDERS &trapons = transponders_list[spc];
			for(size_t tp=0;tp<trapons.size();tp++) {
				fputc('%',st) ;
				const TRANSPONDER &trpn = trapons[tp] ;
				DWORD ptxch = BonPTx->TransponderGetPTxCh((DWORD)spc,(DWORD)tp);
				fprintf(st,"%s\t%d\t%d\t%d\n",trpn.Name.c_str(),(int)spc,(int)tp,(int)ptxch);
			}
		}
	}
	fputs(
		";チャンネル(タブ区切り：名称\t"
		"BonDriverとしてのチューナ空間\t"
		"BonDriverとしてのチャンネル\t"
		"PTxとしてのチャンネル\t"
		"TSID(10進数で衛星波以外は0))\n",st);
	if(vp_tsid_stream=='y')
		fputs(";(※ただし、TSIDの記述が7以下の場合は、"
			"ストリーム番号を意味する[mod])\n",st);
	for(size_t spc=0;spc<transponders_list.size();spc++) {
		const TRANSPONDERS &trapons = transponders_list[spc];
		for(size_t tp=0,n=0;tp<trapons.size();tp++) {
			const TRANSPONDER &trpn = trapons[tp] ;
			DWORD ptxch = BonPTx->TransponderGetPTxCh((DWORD)spc,(DWORD)tp);
			for(auto id : trpn.TSIDs) {
				fprintf(st,"%s/TS%d\t%d\t%d\t%d\t%d\n",
						trpn.Name.c_str(),
						(int)id&7,(int)spc,(int)n,(int)ptxch,
						(int)(vp_tsid_stream=='y'?id&7:id)
					);
				n++;
			}
		}
	}
	fclose(st);
	return true;
}

bool DoScanS()
{
	try {
		BonTransponder = dynamic_cast<IBonTransponder*>(BonTuner) ;
	}catch(bad_cast &e) {
		printf("Cast error: %s\n",e.what());
		puts("IBonTransponderインターフェイスを確認できませんでした。\n");
		BonTransponder = NULL ;
		return false;
	}
	puts("IBonTransponderインターフェイス: 有効\n");

	try {
		BonPTx = dynamic_cast<IBonPTx*>(BonTuner) ;
	}catch(bad_cast &e) {
		printf("Cast error: %s\n",e.what());
		puts("IBonPTxインターフェイスを確認できませんでした。\n");
		BonTransponder = NULL ;
		BonPTx = NULL ;
		return false;
	}
	puts("IBonPTxインターフェイス: 有効\n");

	puts("スペース名を列挙しています。");
	str_vector spaces ;
	for(DWORD spc=0;;spc++) {
		LPCTSTR space = BonTuner->EnumTuningSpace(spc);
		if(!space) break;
		string name = wcs2mbcs(space);
		printf(" %s",name.c_str());
		spaces.push_back(name);
	}
	printf("\n計 %d のスペースを集計しました。\n\n",(int)spaces.size());

	puts("トランスポンダを列挙しています。");
	vector<TRANSPONDERS> transponders_list;
	int ntrapons=0;
	for(DWORD spc=0;spc<(DWORD)spaces.size();spc++) {
		printf(" スペース(%d): %s\n ",spc,spaces[spc].c_str());
		TRANSPONDERS trapons;
		for(DWORD tp=0;;tp++) {
			LPCTSTR tra = BonTransponder->TransponderEnumerate(spc,tp);
			if(!tra) break;
			string name = wcs2mbcs(tra);
			printf(" %s",name.c_str());
			trapons.push_back(TRANSPONDER(name));
			ntrapons++;
		}
		transponders_list.push_back(trapons);
		putchar('\n');
	}
	printf("計 %d のトランスポンダを集計しました。\n\n",ntrapons);

	puts("チューナーをオープンしています...");
	BOOL opened = BonTuner->OpenTuner();
	bool res = true ;
	if(!opened) {
		puts("チューナーのオープンに失敗。");
		res=false;
	}else do {
		puts("チューナーのオープンに成功しました。");
		puts("トランスポンダスキャンを開始します。");
		for(DWORD spc=0;spc<(DWORD)transponders_list.size();spc++) {
			printf(" スペース(%d): %s\n",spc,spaces[spc].c_str());
			TRANSPONDERS &trapons = transponders_list[spc];
			for(DWORD tp=0;tp<trapons.size();tp++) {
				printf("  トランスポンダ(%d): %s\n",tp,trapons[tp].Name.c_str());
				if(SelectTransponder(spc,tp)) {
					tsid_vector idList;
					if(MakeIDList(idList)) {
						if(idList.empty())
							puts("\t<空>");
						else {
							putchar('\t');
							for(auto id : idList)
								printf("TS%d:%04x ",(int)id&7,(int)id);
							putchar('\n');
						}
						trapons[tp].TSIDs = idList ;
					}else {
						puts("\t<TSID抽出不可>");
					}
				}else {
					puts("\t<選択不可>");
				}
			}
		}
		puts("トランスポンダスキャンを完了しました。");
		if(vp_deep_tuning!='y') break;
		puts("TSID整合性の検証を開始します。");
		for(DWORD spc=0;spc<(DWORD)transponders_list.size();spc++) {
			printf(" スペース(%d): %s\n",spc,spaces[spc].c_str());
			TRANSPONDERS &trapons = transponders_list[spc];
			for(DWORD tp=0;tp<trapons.size();tp++) {
				printf("  トランスポンダ(%d): %s\n",tp,trapons[tp].Name.c_str());
				if(SelectTransponder(spc,tp)) {
					Sleep(MAXDUR_TMCC);
					tsid_vector &idList = trapons[tp].TSIDs, passedIDList ;
					if(idList.empty())
						puts("\t<空>");
					else {
						for(auto id : idList) {
							printf("\tTS%d:%04x ⇒ ",(int)id&7,(int)id);
							if(CheckID(id)) {
								passedIDList.push_back(id) ;
								puts("○");
							}else
								puts("×");
							BonTuner->PurgeTsStream();
						}
						idList.swap(passedIDList);
					}
				}else {
					puts("\t<選択不可>");
				}
			}
		}
		puts("TSID整合性の検証を完了しました。");
	}while(0);
	if(opened) {
		puts("チューナーをクローズしています...");
		BonTuner->CloseTuner();
		puts("チューナーをクローズしました。");
	}

	if(res) {
		string out_file ;
		{

			CHAR szFname[_MAX_FNAME];
			CHAR szExt[_MAX_EXT];
			_splitpath_s( (AppPath+BonFile).c_str(), NULL, 0, NULL, 0, szFname, _MAX_FNAME, szExt, _MAX_EXT );
			out_file = szFname ;
			out_file += ".ChSet.txt" ;
		}
		char c = vp_chset ;
		if(!c) {
			printf("\nファイル \"%s\" にスキャン結果を出力しますか？[y/N]> ",out_file.c_str());
			while((c=tolower(getchar()))<=0x20) ;
		}
		if(c=='y') {
			c = vp_transponder ;
			if(!c) {
				printf("トランスポンダ情報も出力しますか？[y/N]> ");
				while((c=tolower(getchar()))<=0x20) ;
			}
			printf("スキャン結果をファイル \"%s\" に出力しています...\n",out_file.c_str());
			if(!OutChSet(out_file, spaces, transponders_list, c=='y')) {
				puts("スキャン結果の出力に失敗。") ;
				res=false ;
			}else
				printf("スキャン結果をファイル \"%s\" に出力しました。\n",out_file.c_str());
			Sleep(1000);
		}
	}

	BonTransponder=NULL;
	BonPTx=NULL;

	return res;
}

int _tmain(int argc, _TCHAR* argv[])
{
	{
		WCHAR szPath[_MAX_PATH];    // パス
		WCHAR szDrive[_MAX_DRIVE];
		WCHAR szDir[_MAX_DIR];
		WCHAR szFname[_MAX_FNAME];
		WCHAR szExt[_MAX_EXT];
		_tsplitpath_s( argv[0], szDrive, _MAX_DRIVE, szDir, _MAX_DIR, szFname, _MAX_FNAME, szExt, _MAX_EXT );
		_tmakepath_s(  szPath, _MAX_PATH, szDrive, szDir, NULL, NULL );
		AppPath = wcs2mbcs(szPath);
	}

	string iniFile = AppPath;
	iniFile += "BonDriver_PTx-ST.ini";
	MAXDUR_FREQ = GetPrivateProfileIntA("SET", "MAXDUR_FREQ", MAXDUR_FREQ, iniFile.c_str() ); //周波数調整に費やす最大時間(msec)
	MAXDUR_TMCC = GetPrivateProfileIntA("SET", "MAXDUR_TMCC", MAXDUR_TMCC, iniFile.c_str() ); //TMCC取得に費やす最大時間(msec)

	int err=1;
	for(int i=1;i<argc;i++) { // param analyzer
		wstring param = argv[i] ;
		if(param.empty()) continue;
		if(param.size()>=3&&param.substr(0,2)==L"--") { // -- verbose param
			wstring vparam = param.substr(2,param.size()-2);
			if(vparam==L"help") { // --help
				help();
				return err;
			}
			else if(vparam==L"transponder")    vp_transponder = 'y' ;
			else if(vparam==L"no-transponder") vp_transponder = 'n' ;
			else if(vparam==L"chset")          vp_chset       = 'y' ;
			else if(vparam==L"no-chset")       vp_chset       = 'n' ;
			else if(vparam==L"tsid-stream")    vp_tsid_stream = 'y' ;
			else if(vparam==L"deep-tuning")    vp_deep_tuning = 'y' ;
			else {
				printf("不明なパラメタ: %s\n\n",wcs2mbcs(param).c_str());
				help();
				return err;
			}
		}else if(param.size()>=2&&(param[0]==L'-'||param[0]==L'+')) { // -/+ param
			string cparam = wcs2mbcs(param.substr(1,param.size()-1));
			char boolean = param[0]==L'-' ? 'n' : 'y' ;
			for(auto c : cparam) {
				switch(tolower(c)) {
					case 't': vp_transponder = boolean ; break;
					case 'c': vp_chset       = boolean ; break;
					case 's': vp_tsid_stream = boolean ; break;
					case 'd': vp_deep_tuning = boolean ; break;
					case '?': help() ; return err;
					default:
						printf("不明なパラメタ: %s\n\n",wcs2mbcs(param).c_str());
						help();
						return err;
				}
			}
		}else {
			if(BonFile.empty())
				BonFile = wcs2mbcs(param) ;
			else {
				printf("BonDriverの重複参照は許されません。(NG:%s)\n\n",wcs2mbcs(param).c_str());
				help();
				return err;
			}
		}
	}

	if(BonFile.empty()) BonFile = DoSelectS() ;
	err++;
	if(BonFile.empty()) return err ;
	err++;
	if(!LoadBon()) { FreeBon(); return err ;}
	err++;
	if(!DoScanS()) { FreeBon(); return err ;}
	FreeBon();
	return 0;
}

