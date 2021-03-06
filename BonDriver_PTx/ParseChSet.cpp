#include "StdAfx.h"
#include "ParseChSet.h"

CParseChSet::CParseChSet(void)
{
}

CParseChSet::~CParseChSet(void)
{
}

BOOL CParseChSet::ParseText(LPCWSTR filePath)
{
	if( filePath == NULL ){
		return FALSE;
	}

	this->spaceMap.clear();
	this->chMap.clear();
	this->filePath = filePath;

	HANDLE hFile = _CreateFile2( filePath, GENERIC_READ, FILE_SHARE_READ|FILE_SHARE_WRITE, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL );
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

// MARK: BOOL CParseChSet::Parse1Line(string parseLine, CH_DATA* chInfo )
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
