// IBonTransponder.h: IBonTransponder クラスのインターフェイス
//
//////////////////////////////////////////////////////////////////////

#if !defined(_IBONTRANSPONDER_H_)
#define _IBONTRANSPONDER_H_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

// 凡トランスポンダインターフェイス (TSIDによるチューニング機能をまとめたもの)
class IBonTransponder
{
public:

	// ※ここで扱っているIDとは、トランスポートストリームID(TSID)のことを表す

	// トランスポンダ名を列挙する
	//  ※ スペースの範囲は、IBonDriver2::EnumTuningSpace で取得したものでそれに同期する
	//  ※ dwTransponderは、0～指定してNULLを返すまでの範囲で有効
	virtual LPCTSTR TransponderEnumerate(const DWORD dwSpace, const DWORD dwTransponder) = 0;

	// 現在のスペースと現行トランスポンダを選択する
	//  ※ スペースの範囲は、IBonDriver2::EnumTuningSpace で取得したものでそれに同期する
	//  ※ dwTransponderは、0～指定してFALSEを返すまでの範囲で有効
	//  ※ トランスポンダデコーダ自体が有効でない（地デジなど）場合は、常にFALSEを返す
	virtual const BOOL TransponderSelect(const DWORD dwSpace, const DWORD dwTransponder) = 0;

	// 現在のトランスポンダデコーダから設定できるストリームIDの一覧を取得する
	//  lpIDList に NULL指定:
	//    *lpdwNumIDに割り当てに必要なストリーム数(default:8)が返る
	//  それ以外:
	//    lpIDList配列にIDリストを,*lpdwNumIDに取得した個数をセットして返す
	//  ※ 現在放送されていないストリームには、0xFFFFFFFF が入ることとする
	//  ※ トランスポンダデコーダ自体が有効でない（地デジなど）場合は、常にFALSEを返す
	virtual const BOOL TransponderGetIDList(LPDWORD lpIDList, LPDWORD lpdwNumID) = 0;

	// 現行トランスポンダのデコード対象ストリームIDを設定する
	//  ※ トランスポンダデコーダ自体が有効でない（地デジなど）場合は、常にFALSEを返す
	virtual const BOOL TransponderSetCurID(const DWORD dwID) = 0;

	// デコード対象ストリームIDを現行トランスポンダのデコーダから取得する
	//  ※ ただし、ストリームが無効な場合には、0xFFFFFFFF を *lpdwID に代入する
	//  ※ トランスポンダデコーダ自体が有効でない（地デジなど）場合は、常にFALSEを返す
	virtual const BOOL TransponderGetCurID(LPDWORD lpdwID) = 0;

	// ※ TransponderSelect が呼ばれてチャンネルが変更された場合、
	//    IBonDriver2::GetCurChannel は、チャンネル番号ではなく、
	//    Transponderのインデックスに 0x80000000 をマスクしたものを返す
	//
	//    つまり、IBonDriver2::GetCurChannelで現在の選局状態が把握可能
	//
	//    ・IBonDriver2::GetCurChannelの返却値の31ビット目が立っていない
	//       →IBonDriver::SetChannel/IBonDriver2::SetChannelでチャンネル切替されている
	//
	//    ・IBonDriver2::GetCurChannelの返却値の31ビット目が立っている
	//       →TransponderSelectでチャンネル切替されている
	//       ※ 0～30ビット部分はチャンネル番号ではなく、Transponderのインデックスを意味する

};

// トランスポンダのチャンネルマスク
// (トランスポンダ選択時にこの値が現在のチャンネルにマスクされる)
#define TRANSPONDER_CHMASK 0x80000000

#include "IBonDriver2.h"
// 凡(ドライバ2+トランスポンダ)複合インターフェイス
class IBonDriver2Transponder : public IBonDriver2, public IBonTransponder {};

#include "IBonDriver3.h"
// 凡(ドライバ3+トランスポンダ)複合インターフェイス
class IBonDriver3Transponder : public IBonDriver3, public IBonTransponder {};

#endif // !defined(_IBONTRANSPONDER_H_)
