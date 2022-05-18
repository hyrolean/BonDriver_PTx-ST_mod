// PTxScanS.cpp : コンソール アプリケーションのエントリ ポイントを定義します。
//

#include "stdafx.h"
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <algorithm>
#include <vector>
#include <string>
#include <memory>
#include <deque>
#include <set>
#include <ctime>
#include <cctype>
#include <conio.h>
#include <winsock2.h>
#include "../Common/IBonTransponder.h"
#include "../Common/IBonPTx.h"

#pragma comment(lib, "WSock32.Lib")

using namespace std;

// 周波数調整に費やす最大時間(ms)
DWORD MAXDUR_FREQ=1000;
// TMCC取得に費やす最大時間(ms)
DWORD MAXDUR_TMCC=1000;

using str_vector = vector<string> ;
using tsid_vector = vector<DWORD> ;
typedef IBonDriver *(*CREATEBONDRIVER)() ;

struct TRANSPONDER {
	string Name ;
	tsid_vector TSIDs;
	TRANSPONDER(string name="") : Name(name) {}
};
using TRANSPONDERS = vector<TRANSPONDER> ;

using CHANNELSET = set<DWORD> ;

struct SPACE {
	string Name ;
	TRANSPONDERS Transponders ;
	CHANNELSET NGChannels ;
};
using SPACES = vector<SPACE> ;

string AppPath = "" ;
string BonFile = "" ;

char   vp_transponder          = 0           ;
char   vp_chset                = 0           ;
char   vp_tsid_stream          = 'n'         ;
char   vp_force_CSV            = 'n'         ;
char   vp_deep_tuning          = 'n'         ;
int    vp_deep_tuning_sec      = 5           ;
float  vp_deep_tuning_db       = 5.f         ;
string vp_tuning_udp_ip        = "127.0.0.1" ;
int    vp_tuning_udp_port      = 1234        ;
BOOL   vp_tuning_udp_broadcast = FALSE       ;

HMODULE BonModule = NULL;
IBonDriver2 *BonTuner = NULL;
IBonTransponder *BonTransponder = NULL;
IBonPTx *BonPTx = NULL;

#define UDP_PACKET_SIZE		48128

  // CUDPTransmitter
class CUDPTransmitter
{
protected:
	BOOL Broadcast ;
	sockaddr_in Addr ;
	SOCKET Sock ;
public:
	CUDPTransmitter(string ip, WORD port, BOOL broadcast=FALSE) {
		Sock = INVALID_SOCKET; Broadcast=broadcast ;
		WSAData wsaData;
		WSAStartup(MAKEWORD(2, 0), &wsaData);
		ZeroMemory(&Addr, sizeof(Addr)) ;
		Addr.sin_family = AF_INET;
		Addr.sin_port = htons((WORD)port);
		Addr.sin_addr.S_un.S_addr = inet_addr(ip.c_str());
	}
	~CUDPTransmitter() {
		Close();
		WSACleanup();
	}
	BOOL Open() {
		if ( Sock != INVALID_SOCKET ) {
			return FALSE;
		}
		Sock = socket(AF_INET, SOCK_DGRAM, 0);
		if ( Sock == INVALID_SOCKET ) {
			return FALSE;
		}
		if(Broadcast) {
			BYTE yes = 1 ;
			setsockopt(Sock, SOL_SOCKET, SO_BROADCAST, (char*)&yes, sizeof(yes) );
		}
		return TRUE;
	}
	BOOL Close() {
		if ( Sock != INVALID_SOCKET ) {
			closesocket(Sock);
			Sock = INVALID_SOCKET;
		}
		return TRUE;
	}
	BOOL Send(char *data, int size) {
		return sendto(Sock, data, size, 0,
			(sockaddr*)&Addr, sizeof(Addr)) == SOCKET_ERROR ? FALSE : TRUE ;
	}
	int Select(UINT wait, bool *sendOk = nullptr, bool *excepted = nullptr) {
		fd_set fdSend, fdExcept ;
		ZeroMemory(&fdSend, sizeof fdSend) ;
		FD_ZERO(&fdSend);
		FD_SET(Sock, &fdSend);
		fdExcept = fdSend ;
		timeval time_limit ;
		time_limit.tv_sec = 0 ;
		time_limit.tv_usec = wait * 1000 ;
		int r = select(int(Sock+1), NULL, &fdSend, &fdExcept, &time_limit) ;
		if (sendOk)
			*sendOk = FD_ISSET(Sock, &fdSend) ? true : false ;
		if (excepted)
			*excepted = FD_ISSET(Sock, &fdExcept) ? true : false ;
		return r ;
	};
};


void help()
{
  puts(R"^(Usage:
  PTxScnanS [-param|+param] [--verbose-param] [BonDriver.dll]

    param, verbose-param:
      +c, --chset
        Enable writing the scanning results to the .ChSet.txt file.
      -c, --no-chset
        Disable writing the scanning results to the .ChSet.txt file.
      +f, --force-CSV
        Force writing the scanning results to the .CSV.txt file.
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

char prompt(char def=0)
{
	char c;
	printf("> ");
	do {
		c = tolower(_getch());
		if(c<=0x20) c=def;
	} while(!isalnum(c));
	printf("%c\n",c);
	return c;
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
	#if 0
	EnumCandidates(candidates,"BonDriver_PTx-S*.dll");
	EnumCandidates(candidates,"BonDriver_PT-S*.dll");
	EnumCandidates(candidates,"BonDriver_PT3-S*.dll");
	EnumCandidates(candidates,"BonDriver_PTw-S*.dll");
	#elif 0
	EnumCandidates(candidates,"BonDriver_PT*.dll");
	EnumCandidates(candidates,"BonDriver_Bulldog*.dll");
	EnumCandidates(candidates,"BonDriver_FSUSB2*.dll");
	EnumCandidates(candidates,"BonDriver_uSUNpTV*.dll");
	#else
	EnumCandidates(candidates,"BonDriver_*.dll");
	#endif
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
				printf("サテライトスキャンする候補を選択[0～%d][q=中断][n=次頁]",int(n-i-1));
			else
				printf("サテライトスキャンする候補を選択[0～%d][q=中断]",int(n-i-1));
			char c = prompt();
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
		BonTuner = NULL ;
	}
	if(!BonTuner) {
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
	return true ;
}

bool TuningTest(DWORD spc, DWORD ch)
{
	auto dur = [](DWORD s=0, DWORD e=GetTickCount()) -> DWORD {
		// duration ( s -> e )
		return s <= e ? e - s : 0xFFFFFFFFUL - s + 1 + e;
	};

	using rate_t = pair<float,DWORD> ;
	deque<rate_t> avg;
	rate_t sum = pair<float,DWORD>(0.f,0), cur = pair<float,DWORD>(0.f,0) ;

	const DWORD wait = 500, max_wait = vp_deep_tuning_sec*1000 ;
	char status[80] = "\0" ;

	Sleep(wait);
	BonTuner->PurgeTsStream(); // 初回受信分を殺しておく

	// チャンネル情報を確認する
	if(BonTuner->GetCurSpace()!=spc || BonTuner->GetCurChannel()!=ch) {
		puts("× <スペース、または、チャンネルが不正>");
		return FALSE;
	}

	CUDPTransmitter udptx(vp_tuning_udp_ip, vp_tuning_udp_port, vp_tuning_udp_broadcast) ;
	udptx.Open();

	for(DWORD s = dur(), u = s ;;) { // signal and bps testing
		int n=BonTuner->GetReadyCount();
		if(!n) {
			BonTuner->WaitTsStream(wait);
			n=BonTuner->GetReadyCount();
		}
		while(n>0) {
			BYTE *buf=NULL; DWORD sz=0, rem=0;
			if(BonTuner->GetTsStream(&buf, &sz, &rem)) {
				if(sz>0) {
					for(DWORD i=0;i<sz;i+=UDP_PACKET_SIZE) {
						udptx.Select(wait);
						int ln = min<int>(sz-i,UDP_PACKET_SIZE);
						udptx.Send((char*)buf+i,ln);
					}
				}
				cur.second += sz ;
			}
			if(!--n) {
				float sig = BonTuner->GetSignalLevel();
				if(sig>cur.first) cur.first = sig ;
			}
		}
		DWORD e = dur();
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
		if(dur(s,e)>=max_wait) break;
	}

	udptx.Close();

	for(size_t i=0;status[i];i++) putchar('\b');
	for(size_t i=0;status[i];i++) putchar(' ');
	for(size_t i=0;status[i];i++) putchar('\b');

	float sig = sum.first/float(avg.size()) ;
	float mbps = (sum.second * 8.f) / (1024.f * 1024.f * float(avg.size())) ;
	bool r = true ;
	string reason="";
	if(sig<vp_deep_tuning_db)
		reason += " <シグナル下限超過>", r=false ;
	if(!sum.second)
		reason += " <ストリーム未到達>", r=false ;
	printf("%s %.2f dB / %.2f Mbps%s\n", r?"○":"×", sig, mbps, reason.c_str()) ;
	return  r ;
}

bool OutChSet(string out_file, const SPACES &spaces, bool out_transponders)
{
	auto print_ptxch = [](DWORD ptxch, FILE *st) {
		int ch = ptxch & 0xffff;
		int offset = (ptxch >> 16) & 0xffff;
		if (offset >= 32768) offset -= 65536;
		if(offset!=0)
			fprintf(st,"%d%+d",ch,offset);
		else
			fprintf(st,"%d",ch);
	};

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
	//fputs(";BS/CS用\n",st);
	fputs(";チューナー空間(タブ区切り：$名称\tBonDriverとしてのチューナ空間\n",st);
	for(size_t spc=0;spc<spaces.size();spc++)
		fprintf(st,"$%s\t%d\n",spaces[spc].Name.c_str(),(int)spc);
	if(out_transponders) {
		fputs(";BS/CS110トランスポンダ情報[mod5]\n",st);
		fputs(
			";トランスポンダ(タブ区切り：%名称\t"
			"BonDriverとしてのチューナ空間\t"
			"BonDriverとしてのチャンネル\t"
			"PTxとしてのチャンネル)\n",st);
		for(size_t spc=0;spc<spaces.size();spc++) {
			const TRANSPONDERS &trapons = spaces[spc].Transponders ;
			for(size_t tp=0;tp<trapons.size();tp++) {
				fputc('%',st) ;
				const TRANSPONDER &trpn = trapons[tp] ;
				DWORD ptxch = BonPTx->TransponderGetPTxCh((DWORD)spc,(DWORD)tp);
				fprintf(st,"%s\t%d\t%d\t",trpn.Name.c_str(),(int)spc,(int)tp);
				print_ptxch(ptxch,st);
				putc('\n',st);
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
	for(size_t spc=0;spc<spaces.size();spc++) {
		bool hasTransponder = BonTransponder->TransponderEnumerate((DWORD)spc,0) != NULL ;
		if(hasTransponder) {
			const TRANSPONDERS &trapons = spaces[spc].Transponders ;
			for(size_t tp=0,n=0;tp<trapons.size();tp++) {
				const TRANSPONDER &trpn = trapons[tp] ;
				DWORD ptxch = BonPTx->TransponderGetPTxCh((DWORD)spc,(DWORD)tp);
				for(auto id : trpn.TSIDs) {
					fprintf(st,"%s/TS%d\t%d\t%d\t",
							trpn.Name.c_str(),(int)id&7,(int)spc,(int)n);
					print_ptxch(ptxch,st);
					fprintf(st,"\t%d\n",(int)(vp_tsid_stream=='y'?id&7:id));
					n++;
				}
			}
		}else {
			const CHANNELSET &ngchs = spaces[spc].NGChannels ;
			for(DWORD n=0,i=0;;n++) {
				LPCTSTR pStr = BonTuner->EnumChannelName((DWORD)spc,n);
				if(!pStr) break;
				if(ngchs.find(n)!=ngchs.end()) continue;
				string nam = wcs2mbcs(pStr);
				DWORD id=0, ptxch = BonPTx->GetPTxCh((DWORD)spc,n,&id);
				fprintf(st,"%s\t%d\t%d\t",nam.c_str(),(int)spc,(int)i++);
				print_ptxch(ptxch,st);
				fprintf(st,"\t%d\n",(int)(vp_tsid_stream=='y'?id&7:id));
			}
		}
	}
	fclose(st);
	return true;
}

bool OutCSV(string out_file, const SPACES &spaces)
{
	string filename = AppPath + out_file ;
	FILE *st=NULL;
	fopen_s(&st,filename.c_str(),"wt");
	if(!st) return false ;
	fputs("; -*- This CSV file was generated automatically by PTxScanS -*-\n",st);
	time_t now = time(NULL) ;
	char asct_buff[80];
	tm now_tm; localtime_s(&now_tm,&now);
	asctime_s(asct_buff,80,&now_tm);
	fputs(";\t@ ",st); fputs(asct_buff,st);
fputs(R"^(;
;       チャンネル情報を変更する場合は、このファイルを編集して
;       プレフィックスが同じ名前のドライバと同ディレクトリに
;       拡張子 .ch.txt としてこのファイルを置くこと。
;
; ※ このファイルを利用する場合は、必ずチャンネルスキャンし直して下さい。
; ※ スペースの順番はブロックごとカット＆ペーストして入れ替えることができます。
;    （CATVの記述の途中にVHFを挿入したり(ミックス)することはできません。）
; ※ 利用しないスペースは丸ごと抹消するかコメントアウトしてください。
;
; 物理チャンネル番号の表記について：
;
;   1～63               : 地デジ(VHF/UHF帯域)の物理チャンネル
;   C13～C63            : CATVパススルー(UHF帯域)の物理チャンネル
;   BS[1～23]/TS[0～7]  : BSの物理チャンネル(チャンネル番号/ストリーム番号)
;   BS[1～23]/ID[整数]  : BSの物理チャンネル(チャンネル番号/ストリームＩＤ)
;   ND[2～24]           : CSの物理チャンネル(チャンネル番号)
;
;     ※TS[0～7]、ID[整数] は省略可。
;
; スペース名, 物理チャンネル番号or周波数MHz[, チャンネル名(無くても可)]
)^",st);
	for(size_t spc=0;spc<spaces.size();spc++) {
		string space = spaces[spc].Name ;
		fprintf(st,"\n\t; %s\n\n",space.c_str());
		bool hasTransponder = BonTransponder->TransponderEnumerate((DWORD)spc,0) != NULL ;
		if(hasTransponder) {
			const TRANSPONDERS &trapons = spaces[spc].Transponders ;
			for(size_t tp=0,n=0;tp<trapons.size();tp++) {
				const TRANSPONDER &trpn = trapons[tp] ;
				for(auto id : trpn.TSIDs) {
					if(vp_tsid_stream=='y') {
						fprintf(st,"%s, %s/TS%d\n",
								space.c_str(),
								trpn.Name.c_str(),
								(int)id&7
							);
					}else {
						fprintf(st,"%s, %s/ID0x%04X, %s/TS%d\n",
								space.c_str(),
								trpn.Name.c_str(),
								(int)id,
								trpn.Name.c_str(),
								(int)id&7
							);
					}
				}
			}
		}else {
			const CHANNELSET &ngchs = spaces[spc].NGChannels ;
			auto i2s = [](int v) -> string {
				char s[10]; sprintf_s(s,"%d",v); return s;
			};
			for(DWORD n=0;;n++) {
				LPCTSTR pStr = BonTuner->EnumChannelName((DWORD)spc,n);
				if(!pStr) break;
				if(ngchs.find(n)!=ngchs.end()) continue;
				string nam = wcs2mbcs(pStr), ch = nam ;
				int tp,id;
				if(sscanf_s(ch.c_str(),"BS%d/TS%d",&tp,&id)==2) {
					// BS
					ch = "BS"+i2s(tp)+"/TS"+i2s(id);
				}else if(sscanf_s(ch.c_str(),"ND%d/TS%d",&tp,&id)==2) {
					// CS110
					ch = "ND"+i2s(tp)+"/TS"+i2s(id);
				}else if(sscanf_s(ch.c_str(),"ND%d",&tp)==1) {
					// CS110
					ch = "ND"+i2s(tp);
				}else if(sscanf_s(ch.c_str(),"C%d",&id)==1) {
					// CATV
					ch = "C"+i2s(id);
				}else if(sscanf_s(ch.c_str(),"%d",&id)==1) {
					// VHF/UHF
					ch = i2s(id);
				}
				if(ch == nam)
					fprintf(st,"%s, %s\n", space.c_str(), ch.c_str() );
				else
					fprintf(st,"%s, %s, %s\n", space.c_str(), ch.c_str(), nam.c_str() );
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
		BonTransponder = NULL ;
	}
	if(BonTransponder==NULL) {
		puts("IBonTransponderインターフェイスを確認できませんでした。\n");
		return false;
	}
	puts("IBonTransponderインターフェイス: 有効\n");

	try {
		BonPTx = dynamic_cast<IBonPTx*>(BonTuner) ;
	}catch(bad_cast &e) {
		printf("Cast error: %s\n",e.what());
		BonPTx = NULL ;
	}
	if(BonPTx==NULL) {
		puts("IBonPTxインターフェイスを確認できませんでした。\n"
			"(※チャンネル情報出力機能はCSV形式に強制されます。)\n");
	}else
		puts("IBonPTxインターフェイス: 有効\n");

	SPACES spaces;
    {
		puts("スペース名を列挙しています。");
		str_vector space_names ;
		for(DWORD spc=0;;spc++) {
			LPCTSTR lpwcnam = BonTuner->EnumTuningSpace(spc);
			if(!lpwcnam) break;
			string name = wcs2mbcs(lpwcnam);
			printf(" %s",name.c_str());
			space_names.push_back(name);
		}
		printf("\n計 %d のスペースを集計しました。\n\n",(int)space_names.size());

		puts("トランスポンダを列挙しています。");
		int ntrapons=0;
		for(DWORD spc=0;spc<(DWORD)space_names.size();spc++) {
			SPACE space ;
			space.Name = space_names[spc] ;
			printf(" スペース(%d): %s\n ",spc,space.Name.c_str());
			for(DWORD tp=0;;tp++) {
				LPCTSTR tra = BonTransponder->TransponderEnumerate(spc,tp);
				if(!tra) {
					if(!tp) fputs(" <空>",stdout);
					break;
				}
				string name = wcs2mbcs(tra);
				printf(" %s",name.c_str());
				space.Transponders.push_back(TRANSPONDER(name));
				ntrapons++;
			}
			spaces.push_back(space);
			putchar('\n');
		}
		printf("計 %d のトランスポンダを集計しました。\n\n",ntrapons);
    }

	puts("チューナーをオープンしています...");
	BOOL opened = BonTuner->OpenTuner();
	bool res = true ;
	if(!opened) {
		puts("チューナーのオープンに失敗。");
		res=false;
	}else do {
		puts("チューナーのオープンに成功しました。");
		puts("トランスポンダスキャンを開始します。");
		for(DWORD spc=0;spc<(DWORD)spaces.size();spc++) {
			printf(" スペース(%d): %s\n",spc,spaces[spc].Name.c_str());
			TRANSPONDERS &trapons = spaces[spc].Transponders;
			if(trapons.empty()) {
				puts("  <空>");
			}else for(DWORD tp=0;tp<trapons.size();tp++) {
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
		for(DWORD spc=0;spc<(DWORD)spaces.size();spc++) {
			printf(" スペース(%d): %s\n",spc,spaces[spc].Name.c_str());
			bool hasTransponder = BonTransponder->TransponderEnumerate((DWORD)spc,0) != NULL ;
			TRANSPONDERS &trapons = spaces[spc].Transponders;
			if(hasTransponder) {
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
								if(!CheckID(id))
									puts("× <IDが不正>");
								else if(TuningTest((DWORD)spc,tp|TRANSPONDER_CHMASK))
									passedIDList.push_back(id) ;
								BonTuner->PurgeTsStream();
							}
							idList.swap(passedIDList);
						}
					}else {
						puts("\t<選択不可>");
					}
				}
			}else {
				puts(" (※トランスポンダが存在しません。チャンネルスキャンモードに切り替えます。)");
				CHANNELSET &ngchs = spaces[spc].NGChannels ;
				for(DWORD n=0;;n++) {
					LPCTSTR pStr = BonTuner->EnumChannelName((DWORD)spc,n);
					if(!pStr) break;
					printf("  チャンネル(%d): %s ⇒ ",n,wcs2mbcs(pStr).c_str());
					bool done = false ;
					if(!BonTuner->SetChannel((DWORD)spc,n))
						puts("× <チャンネル切替に失敗>");
					else if(TuningTest((DWORD)spc,n))
						done = true;
					if(!done)
						ngchs.insert(n);
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
		bool csv = !BonPTx || vp_force_CSV=='y' ;
		{
			CHAR szFname[_MAX_FNAME];
			CHAR szExt[_MAX_EXT];
			_splitpath_s( (AppPath+BonFile).c_str(), NULL, 0, NULL, 0, szFname, _MAX_FNAME, szExt, _MAX_EXT );
			out_file = szFname ;
			out_file += csv ? ".CSV.txt" /*csv*/ : ".ChSet.txt" /*ptx*/ ;
		}
		char c = vp_chset ;
		if(!c) {
			printf("\nファイル \"%s\" にスキャン結果を出力しますか？[y/N]",out_file.c_str());
			c=prompt('n') ;
		}
		if(c=='y') {
			if(BonPTx) {
				c = vp_transponder ;
				if(!c) {
					if(csv) c='n';
					else {
						printf("トランスポンダ情報も出力しますか？[y/N]");
						c=prompt('n') ;
					}
				}
			}
			printf("スキャン結果をファイル \"%s\" に出力しています...\n",out_file.c_str());
			if(!(csv?OutCSV(out_file, spaces):OutChSet(out_file, spaces, c=='y'))) {
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
	MAXDUR_TMCC = GetPrivateProfileIntA("SET", "MAXDUR_TMCC_S", MAXDUR_TMCC, iniFile.c_str() ); //TMCC(S側)取得に費やす最大時間(msec)

	int err=1,dig; wchar_t sub[80]; float fval;
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
			else if(vparam==L"force-CSV")      vp_force_CSV = vp_chset = 'y' ;
			else if(vparam==L"tsid-stream")    vp_tsid_stream = 'y' ;
			else if(vparam==L"deep-tuning")    vp_deep_tuning = 'y' ;
			else if(swscanf_s(vparam.c_str(),L"deep-tuning-sec=%d",&dig)==1)
				vp_deep_tuning_sec = dig ;
			else if(swscanf_s(vparam.c_str(),L"deep-tuning-db=%f",&fval)==1)
				vp_deep_tuning_db = fval ;
			else if(swscanf_s(vparam.c_str(),L"tuning-udp-ip=%s",sub,(unsigned)_countof(sub))==1)
				vp_tuning_udp_ip = wcs2mbcs(sub) ;
			else if(swscanf_s(vparam.c_str(),L"tuning-udp-port=%d",&dig)==1)
				vp_tuning_udp_port = dig ;
			else if(vparam==L"tuning-udp-broadcast")
				vp_tuning_udp_broadcast = TRUE ;
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
					case 'f': vp_force_CSV   = boolean ; break;
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

