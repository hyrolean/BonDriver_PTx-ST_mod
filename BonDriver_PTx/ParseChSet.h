#pragma once

#include "../Common/Util.h"
#include "../Common/StringUtil.h"

struct SPACE_DATA{
	wstring wszName;
	DWORD dwSpace;
	//=オペレーターの処理
	SPACE_DATA(void){
		wszName = L"";
		dwSpace = 0;
	};
	~SPACE_DATA(void){
	}
	SPACE_DATA & operator= (const SPACE_DATA & o) {
		wszName = o.wszName;
		dwSpace = o.dwSpace;
		return *this;
	}
};

struct CH_DATA{
	wstring wszName;
	DWORD dwSpace;
	DWORD dwCh;
	DWORD dwPT1Ch;
	DWORD dwTSID;
	//=オペレーターの処理
	CH_DATA(void){
		wszName = L"";
		dwSpace = 0;
		dwCh = 0;
		dwPT1Ch = 0;
		dwTSID = 0;
	};
	~CH_DATA(void){
	}
	CH_DATA & operator= (const CH_DATA & o) {
		wszName = o.wszName;
		dwSpace = o.dwSpace;
		dwCh = o.dwCh;
		dwPT1Ch = o.dwPT1Ch;
		dwTSID = o.dwTSID;
		return *this;
	}
};

struct TP_DATA {
	wstring wszName;
	DWORD dwSpace;
	DWORD dwCh;
	DWORD dwPT1Ch;
	TP_DATA()
	 : wszName(L"") {
		dwSpace = dwCh = dwPT1Ch = 0;
	}
	~TP_DATA(){}
	TP_DATA & operator= (const TP_DATA & o) {
		wszName = o.wszName ;
		dwSpace = o.dwSpace ;
		dwCh = o.dwCh ;
		dwPT1Ch = o.dwPT1Ch ;
		return *this;
	}
	CH_DATA ToChData() {
		CH_DATA dataCh ;
		dataCh.wszName = wszName ;
		dataCh.dwSpace = dwSpace ;
		dataCh.dwCh = dwCh ;
		dataCh.dwPT1Ch = dwPT1Ch ;
		return dataCh ;
	}
};

class CParseChSet
{
public:
	map<DWORD, SPACE_DATA> spaceMap;
	map<DWORD, TP_DATA> tpMap;
	map<DWORD, CH_DATA> chMap;

public:
	CParseChSet(void);
	~CParseChSet(void);

	//ChSet.txtの読み込みを行う
	//戻り値：
	// TRUE（成功）、FALSE（失敗）
	//引数：
	// file_path		[IN]ChSet.txtのフルパス
	BOOL ParseText(
		LPCWSTR filePath,
		LPCWSTR fileExt=NULL
		);
	BOOL ParseTextCSV(
		LPCWSTR filePath,
		BOOL isISDBS,
		LPCWSTR fileExt=NULL
		);

protected:
	wstring filePath;
	CH_DATA chLast;

protected:
	BOOL Parse1Line(string parseLine, SPACE_DATA* info );
	BOOL Parse1Line(string parseLine, TP_DATA* tpInfo );
	BOOL Parse1Line(string parseLine, CH_DATA* chInfo );
};
