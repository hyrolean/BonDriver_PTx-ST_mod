#include "StdAfx.h"
#include "ParseChSet.h"

CParseChSet::CParseChSet(void)
{
}

CParseChSet::~CParseChSet(void)
{
}

BOOL CParseChSet::ParseText(LPCWSTR filePath, LPCWSTR fileExt)
{
	if( filePath == NULL ){
		return FALSE;
	}

	this->spaceMap.clear();
	this->chMap.clear();
	this->filePath = filePath;
	if(fileExt!=NULL)
		this->filePath += fileExt;

	HANDLE hFile = _CreateFile2( this->filePath.c_str(), GENERIC_READ, FILE_SHARE_READ|FILE_SHARE_WRITE, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL );
	if( hFile == INVALID_HANDLE_VALUE ){
		return FALSE;
	}
	DWORD dwFileSize = GetFileSize( hFile, NULL );
	if( dwFileSize == 0 ){
		CloseHandle(hFile);
		return TRUE;
	}
	char* pszBuff = new char[dwFileSize+1];
	if( pszBuff == NULL ){
		CloseHandle(hFile);
		return FALSE;
	}
	ZeroMemory(pszBuff,dwFileSize+1);
	DWORD dwRead=0;
	ReadFile( hFile, pszBuff, dwFileSize, &dwRead, NULL );

	string strRead = pszBuff;

	CloseHandle(hFile);
	SAFE_DELETE_ARRAY(pszBuff);

	string parseLine="";
	size_t iIndex = 0;
	size_t iFind = 0;
	while( iFind != string::npos ){
		iFind = strRead.find("\r\n", iIndex);
		if( iFind == (int)string::npos ){
			parseLine = strRead.substr(iIndex);
			//strRead.clear();
		}else{
			parseLine = strRead.substr(iIndex,iFind-iIndex);
			//strRead.erase( 0, iIndex+2 );
			iIndex = iFind + 2;
		}
		//先頭；はコメント行
		if( parseLine.find(";") != 0 ){
			//空行？
			if( parseLine.find("\t") != string::npos ){
				if( parseLine.find("$") == 0 ){
					//チューナー空間
					SPACE_DATA item;
					if( Parse1Line(parseLine, &item) ){
						this->spaceMap.insert( pair<DWORD, SPACE_DATA>(item.dwSpace,item) );
					}
				}else if( parseLine.find("%") == 0 ){
					//トランスポンダ
					TP_DATA item;
					if( Parse1Line(parseLine, &item) ){
						DWORD iKey = (item.dwSpace<<16) | item.dwCh;
						this->tpMap.insert( pair<DWORD, TP_DATA>(iKey,item) );
					}
				}else{
					//チャンネル
					CH_DATA item;
					if( Parse1Line(parseLine, &item) ){
						DWORD iKey = (item.dwSpace<<16) | item.dwCh;
						this->chMap.insert( pair<DWORD, CH_DATA>(iKey,item) );
					}
				}
			}
		}
	}

	return TRUE;
}

BOOL CParseChSet::ParseTextCSV(LPCWSTR filePath, BOOL isISDBS, LPCWSTR fileExt)
{
	if( filePath == NULL ){
		return FALSE;
	}

	this->spaceMap.clear();
	this->chMap.clear();
	this->filePath = filePath;
	if(fileExt!=NULL)
		this->filePath += fileExt;

	// BAND
	enum BAND {
		BAND_na, // [n/a]
		BAND_VU, // VHS, UHF or CATV
		BAND_BS, // BS
		BAND_ND  // CS110
	};
	// CHANNEL/CHANNELS
	struct CHANNEL {
		std::wstring Space ;
		BAND        Band ;
		int         Channel ;
		WORD        Stream ;
        WORD        TSID ;
		DWORD       Freq ;
		std::wstring	Name ;
		CHANNEL() = default;
		CHANNEL(std::wstring space, BAND band, int channel, std::wstring name,unsigned stream=0,unsigned tsid=0) {
			Space = space ;
			Band = band ;
			Name = name ;
			Channel = channel ;
			Freq = FreqFromBandCh(band,channel) ;
			Stream = stream ;
			TSID = tsid ;
		}
		CHANNEL(std::wstring space, DWORD freq,std::wstring name,unsigned stream=0,unsigned tsid=0) {
			Space = space ;
			Band = BandFromFreq(freq) ;
			Name = name ;
			Channel = 0 ;
			Freq = freq ;
			Stream = stream ;
			TSID = tsid ;
		}
		CHANNEL(const CHANNEL &src) {
			Space = src.Space ;
			Band = src.Band ;
			Name = src.Name ;
			Channel = src.Channel ;
			Freq = src.Freq ;
			Stream = src.Stream ;
			TSID = src.TSID;
		}
		static DWORD FreqFromBandCh(BAND band,int ch) {
			DWORD freq =0 ;
			switch(band) {
				case BAND_VU:
					if(ch < 4)          freq =  93UL + (ch - 1)   * 6UL ;
					else if(ch < 8)     freq = 173UL + (ch - 4)   * 6UL ;
					else if(ch < 13)    freq = 195UL + (ch - 8)   * 6UL ;
					else if(ch < 63)    freq = 473UL + (ch - 13)  * 6UL ;
					else if(ch < 122)   freq = 111UL + (ch - 113) * 6UL ;
					else if(ch ==122)   freq = 167UL ; // C22
					else if(ch < 136)   freq = 225UL + (ch - 123) * 6UL ;
					else                freq = 303UL + (ch - 136) * 6UL ;
					freq *= 1000UL ; // kHz
					freq +=  143UL ; // + 1000/7 kHz
					break ;
				case BAND_BS:
					freq = ch * 19180UL + 1030300UL ;
					break ;
				case BAND_ND:
					freq = ch * 20000UL + 1573000UL ;
					break ;
			}
			return freq ;
		}
		bool isISDBT() const { return Band==BAND_VU; }
		bool isISDBS() const { return Band==BAND_BS||Band==BAND_ND; }
		static BAND BandFromFreq(DWORD freq) {
			if(freq < 60000UL || freq > 2456123UL )
				return BAND_na ;
			if(freq < 900000UL )
				return BAND_VU ;
			if(freq < 1573000UL )
				return BAND_BS ;
			return BAND_ND ;
		}
		CH_DATA ToChData(DWORD dwSpace, DWORD dwCh) const {
			CH_DATA data;
			data.dwSpace = dwSpace ;
			data.dwCh = dwCh ;
			data.wszName = Name ;
			data.dwTSID = TSID!=0 ? TSID : Stream ;
			int ch = Channel, ofs=0 ;
			if(!ch) { // Freq enabled
				auto vuFreq = [](DWORD kHz, DWORD d6=0, DWORD delta=143) -> DWORD
					{ return (kHz + d6*6)*1000+delta; };
				switch(Band) {
					case BAND_VU: {
						DWORD baseFreq = 0 ;
						int baseCh = 0 ;
						if( Freq < vuFreq(111UL-3UL)) // VHF 1 - 3
							baseFreq = vuFreq(93UL), baseCh=1 ;
						else if(Freq < vuFreq(167UL-3UL)) // C13 - C21
							baseFreq = vuFreq(111UL), baseCh=113 ;
						else if(Freq < vuFreq(173UL-3UL)) // C22
							baseFreq = vuFreq(167UL), baseCh=122 ;
						else if(Freq < vuFreq(195UL-3UL)) // VHF 4 - 7
							baseFreq = vuFreq(173UL), baseCh=4 ;
						else if(Freq < vuFreq(225UL-3UL)) // VHF 8 - 12
							baseFreq = vuFreq(195UL), baseCh=8 ;
						else if(Freq < vuFreq(303UL-3UL)) // C23 - C35
							baseFreq = vuFreq(225UL), baseCh=123 ;
						else if(Freq < vuFreq(473UL-1UL)) // C36 - C63
							baseFreq = vuFreq(303UL), baseCh=136 ;
						else // UHF 13 - 62
							baseFreq = vuFreq(473), baseCh=13 ;
						ch = int( (Freq - baseFreq +3000) / 6000 ) + baseCh ;
						ofs = (int(Freq) - (int(ch)-baseCh)*6000 - int(baseFreq)) / 143 ;
						break;
					}
					case BAND_BS:
						ch = (Freq - 1030300UL + 19180UL/2UL)/19180UL ;
						if(ch<1) ch = 1 ;
						ofs = (int(Freq) - int(1030300UL+19180UL*ch)) / 1000 ;
						break;
					case BAND_ND:
						ch = (Freq - 1573000UL + 10000UL)/20000UL ;
						if(ch<1) ch = 1 ;
						ofs = (int(Freq) - int(1573000UL+20000UL*ch)) / 1000 ;
						break;
				}
			}
			switch(Band) {
				case BAND_VU:
					if(ch<=3) // VHF 1 - 3
						data.dwPT1Ch = ch-1 ;
					else if(ch>=4&&ch<=12)  // VHF 4 - 12
						data.dwPT1Ch = ch+9 ;
					else if(ch>=113&&ch<=122) // C13 - C22
						data.dwPT1Ch = ch-110 ;
					else if(ch>=123&&ch<=163) // C23 - C63
						data.dwPT1Ch = ch-101 ;
					else // UHF 13 - 62
						data.dwPT1Ch = ch+50 ;
					break;
				case BAND_BS:
					data.dwPT1Ch = (ch-1)/2;
					break;
				case BAND_ND:
					if(ch&1)
						data.dwPT1Ch = (ch-1)/2 + 24;
					else
						data.dwPT1Ch = (ch-2)/2 + 12;
			}
			data.dwPT1Ch |= WORD(ofs)<<16 ;
#ifdef _DEBUG
			DBGOUT("Space=%d, Ch=%d, Name=%s, PT1Ch=%d\n",
				data.dwSpace, data.dwCh, wcs2mbcs(data.wszName).c_str(), data.dwPT1Ch);
#endif
			return data;
		}
	} ;

	auto trim = [](const wstring &str) ->wstring {
		wstring str2=L"" ;
		for(wstring::size_type i=0;i<str.size();i++)
			if(unsigned(str[i])>0x20UL)
				{ str2 = str.substr(i,str.size()-i) ; break ; }
		if(str2.empty()) return L"" ;
		for(wstring::size_type i=str2.size();i>0;i--)
			if(unsigned(str2[i-1])>0x20UL)
				return str2.substr(0,i) ;
		return L"" ;
	};

	auto split = [trim](vector<wstring> &params, wstring src, wchar_t dlmt) {
		size_t n=0; wstring r=src, l, sep=L"" ; sep.push_back(dlmt);
		while(Separate(r,sep,l,r)) params.push_back(trim(l));
		if(!trim(l).empty()) params.push_back(trim(l));
	};

	auto itows = [](int d) ->wstring
		{ wstring r; Format(r, L"%d", d); return r; };

	FILE *st=NULL ;
	_wfopen_s(&st,this->filePath.c_str(),L"rt") ;
	if(!st) return FALSE ;
	char s[512] ;

	CHANNEL chObj;
	std::wstring space_name=L"" ;
	int num_space=0;

	for(DWORD ch=0xFFFFFFFF,sp=0xFFFFFFFF;!feof(st);) {
		s[0]='\0' ;
		fgets(s,512,st) ;
		wstring wstrLine = trim(mbcs2wcs(s)) ;
		if(wstrLine.empty()) continue ;
		int t=0 ;
		vector<wstring> params ;
		split(params,wstrLine,L';') ;
		wstrLine = params[0] ; params.clear() ;
		split(params,wstrLine,L',') ;
		if(params.size()>=2&&!params[0].empty()) {
			BAND band = BAND_na ;
			int channel = 0 ;
			DWORD freq = 0 ;
			int stream = 0 ;
			int tsid = 0 ;
			wstring &space = params[0] ;
			wstring name = params.size()>=3 ? params[2] : wstring(L"") ;
			wstring subname = params[1] ;
			vector<wstring> phyChDiv ;
			split(phyChDiv,params[1],L'/') ;
			for(size_t i=0;i<phyChDiv.size();i++) {
				wstring phyCh = phyChDiv[i] ;
				if( phyCh.length()>3&&
						phyCh.substr(phyCh.length()-3)==L"MHz" ) {
					float megaHz = 0.f ;
					if(swscanf_s(phyCh.c_str(),L"%fMHz",&megaHz)==1) {
						freq=DWORD(megaHz*1000.f) ;
						channel = CHANNEL::BandFromFreq(freq)!=BAND_na ? -1 : 0 ;
					}
				}else {
					if(swscanf_s(phyCh.c_str(),L"TS%d",&stream)==1)
						;
					else if(swscanf_s(phyCh.c_str(),L"ID%i",&tsid)==1)
						;
					else if(band==BAND_na) {
						if(swscanf_s(phyCh.c_str(),L"BS%d",&channel)==1)
							band = BAND_BS ;
						else if(swscanf_s(phyCh.c_str(),L"ND%d",&channel)==1)
							band = BAND_ND ;
						else if(swscanf_s(phyCh.c_str(),L"C%d",&channel)==1)
							band = BAND_VU, subname=L"C"+itows(channel)+L"ch", channel+=100 ;
						else if(swscanf_s(phyCh.c_str(),L"%d",&channel)==1)
							band = BAND_VU, subname=itows(channel)+L"ch" ;
					}
				}
			}
			if(name==L"")
				name=subname ;
			if(freq>0&&channel<0)
				chObj=CHANNEL(space,freq,name,stream,tsid) ;
			else if(band!=BAND_na&&channel>0)
				chObj=CHANNEL(space,band,channel,name,stream,tsid) ;
			else
				continue ;
			if(chObj.Band==BAND_na || (chObj.isISDBS() && !isISDBS) || (chObj.isISDBT() && isISDBS))
				continue ;
			if(space_name!=space) {
				SPACE_DATA item;
				item.dwSpace = num_space ;
				item.wszName = space ;
				this->spaceMap.insert( pair<DWORD, SPACE_DATA>(item.dwSpace,item) );
				space_name = space ;
				ch=0; sp=num_space++;
			}
			this->chMap.insert( pair<DWORD, CH_DATA>(sp<<16|ch,chObj.ToChData(sp,ch)) );
			ch++;
		}
	}

	fclose(st) ;

	return TRUE ;
}


BOOL CParseChSet::Parse1Line(string parseLine, SPACE_DATA* info )
{
	if( parseLine.empty() || info == NULL ){
		return FALSE;
	}
	Replace(parseLine, "$", "");
	string strBuff="";

	Separate( parseLine, "\t", strBuff, parseLine);

	//Ch名
	AtoW(strBuff, info->wszName);

	Separate( parseLine, "\t", strBuff, parseLine);

	//Space
	info->dwSpace = atoi(strBuff.c_str());

	return TRUE;
}

BOOL CParseChSet::Parse1Line(string parseLine, TP_DATA* tpInfo )
{
	auto ParseData=[](string strBuff, DWORD lastVal) -> DWORD {
		if(strBuff=="-") return lastVal ;
		if(strBuff=="+") return lastVal+1 ;
		return DWORD(atoi(strBuff.c_str()));
	};

	auto ParsePT1Ch=[](string strBuff, DWORD lastVal) -> DWORD {
		DWORD ch=0; int offset=0;
		if(strBuff.size()>0) {
			if(strBuff[0]=='-'||strBuff[0]=='+') {
				ch= lastVal;
				offset= (ch>>16) & 0xffff;
				if (offset >= 32768) offset -= 65536;
				ch&= 0xffff;
				if(strBuff[0]=='+') ch++;
				strBuff=strBuff.substr(1,strBuff.size()-1);
			}
		}
		string prefix="", suffix="" ;
		Separate( strBuff, "+", prefix, suffix);
		if(suffix!="") {
			if(prefix!="") ch= atoi(prefix.c_str());
			offset += atoi(suffix.c_str());
		}else {
			Separate( strBuff, "-", prefix, suffix);
			if(suffix!="") {
				if(prefix!="") ch= atoi(prefix.c_str());
				offset -= atoi(suffix.c_str());
			}else {
				if(prefix!="") ch= atoi(strBuff.c_str()) ;
			}
		}
		return ch | WORD(offset)<<16 ;
	};

	if( parseLine.empty() || tpInfo == NULL ){
		return FALSE;
	}
	Replace(parseLine, "%", "");
	string strBuff="";

	Separate( parseLine, "\t", strBuff, parseLine);

	//Ch名
	AtoW(strBuff, tpInfo->wszName);

	Separate( parseLine, "\t", strBuff, parseLine);

	//Space
	tpInfo->dwSpace = ParseData(strBuff, chLast.dwSpace);

	Separate( parseLine, "\t", strBuff, parseLine);

	//Ch
	tpInfo->dwCh = ParseData(strBuff, chLast.dwCh);

	Separate( parseLine, "\t", strBuff, parseLine);

	//PTxのチャンネル
	//ch+-offsetで周波数オフセット可に
    //(fixed by 2020 LVhJPic0JSk5LiQ1ITskKVk9UGBg)
	tpInfo->dwPT1Ch = ParsePT1Ch(strBuff, chLast.dwPT1Ch);

	chLast = tpInfo->ToChData();

	return TRUE;
}

BOOL CParseChSet::Parse1Line(string parseLine, CH_DATA* chInfo )
{
	auto ParseData=[](string strBuff, DWORD lastVal) -> DWORD {
		if(strBuff=="-") return lastVal ;
		if(strBuff=="+") return lastVal+1 ;
		return DWORD(atoi(strBuff.c_str()));
	};

	auto ParsePT1Ch=[](string strBuff, DWORD lastVal) -> DWORD {
		DWORD ch=0; int offset=0;
		if(strBuff.size()>0) {
			if(strBuff[0]=='-'||strBuff[0]=='+') {
				ch= lastVal;
				offset= (ch>>16) & 0xffff;
				if (offset >= 32768) offset -= 65536;
				ch&= 0xffff;
				if(strBuff[0]=='+') ch++;
				strBuff=strBuff.substr(1,strBuff.size()-1);
			}
		}
		string prefix="", suffix="" ;
		Separate( strBuff, "+", prefix, suffix);
		if(suffix!="") {
			if(prefix!="") ch= atoi(prefix.c_str());
			offset += atoi(suffix.c_str());
		}else {
			Separate( strBuff, "-", prefix, suffix);
			if(suffix!="") {
				if(prefix!="") ch= atoi(prefix.c_str());
				offset -= atoi(suffix.c_str());
			}else {
				if(prefix!="") ch= atoi(strBuff.c_str()) ;
			}
		}
		return ch | WORD(offset)<<16 ;
	};

	if( parseLine.empty() || chInfo == NULL ){
		return FALSE;
	}
	string strBuff="";

	Separate( parseLine, "\t", strBuff, parseLine);

	//Ch名
	AtoW(strBuff, chInfo->wszName);

	Separate( parseLine, "\t", strBuff, parseLine);

	//Space
	chInfo->dwSpace = ParseData(strBuff, chLast.dwSpace);

	Separate( parseLine, "\t", strBuff, parseLine);

	//Ch
	chInfo->dwCh = ParseData(strBuff, chLast.dwCh);

	Separate( parseLine, "\t", strBuff, parseLine);

	//PTxのチャンネル
	//ch+-offsetで周波数オフセット可に
    //(fixed by 2020 LVhJPic0JSk5LiQ1ITskKVk9UGBg)
	chInfo->dwPT1Ch = ParsePT1Ch(strBuff, chLast.dwPT1Ch);

	Separate( parseLine, "\t", strBuff, parseLine);

	//TSID
	chInfo->dwTSID = (WORD)ParseData(strBuff, chLast.dwTSID);

	//直近のchデータを複製
	chLast = *chInfo;

	return TRUE;
}
