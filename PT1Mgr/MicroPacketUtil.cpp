#include "StdAfx.h"
#include "MicroPacketUtil.h"

CMicroPacketUtil::CMicroPacketUtil(void)
{
	Reset();
}

CMicroPacketUtil::~CMicroPacketUtil(void)
{
}

BOOL CMicroPacketUtil::MicroPacket(BYTE* pbPacket)
{
	BOOL bRet = FALSE;

	uint packetCounter = BIT_SHIFT_MASK(pbPacket[3], 2,  3);
	uint packetStart   = BIT_SHIFT_MASK(pbPacket[3], 1,  1);

	// カウンタ値を確認
	uint count = BIT_SHIFT_MASK(mCount, 0, 3);
	mCount++;

	if (packetCounter != count) {
		OutputDebugString(L"カウンタ値が異常です。\n");
		Reset();
		return FALSE;
	}

	// パケット開始位置フラグを確認
	if (packetStart) {
		// 同期バイトを確認
		if (pbPacket[2] != 0x47) {
			OutputDebugString(L"パケットの先頭バイトが 0x47 になっていません。\n");
		}

		if (mPacketOffset != 0) {
			OutputDebugString(L"前のパケットが完了しませんでした。\n");
		}
		mPacketOffset = 0;
	}

	// データをコピー
	for (int i=2; i>=0; i--) {
		if (mPacketOffset < 188) {
			m_b1TS[mPacketOffset] = pbPacket[i];
			mPacketOffset++;
		}
	}

	if (188 <= mPacketOffset) {
		// ひとつのパケットが完成
		mPacketOffset = 0;
		bRet = TRUE;
	}

	return bRet;
}
