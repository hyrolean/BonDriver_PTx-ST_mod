// IBonPTx.h: IBonPTx クラスのインターフェイス
//
//////////////////////////////////////////////////////////////////////

#if !defined(_IBONPTX_H_)
#define _IBONPTX_H_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

// 凡PTxインターフェイス (PTx固有の機能をまとめたもの)
class IBonPTx
{
public:
	// 各トランスポンダに対応するPTx固有のチャンネル番号を返す
	virtual const DWORD TransponderGetPTxCh(const DWORD dwSpace, const DWORD dwTransponder) = 0;
	// 各チャンネルに対応するPTx固有のチャンネル情報を返す
	virtual const DWORD GetPTxCh(const DWORD dwSpace, const DWORD dwChannel, DWORD *lpdwTSID=NULL) = 0;
};

#endif // !defined(_IBONPTX_H_)
