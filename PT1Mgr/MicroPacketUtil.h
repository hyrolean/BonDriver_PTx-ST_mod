#pragma once

#include "inc/EARTH_PT.h"
#include "inc/Prefix.h"
#include "../Common/Util.h"

using namespace EARTH;

class CMicroPacketUtil
{
public:
	CMicroPacketUtil(void);
	~CMicroPacketUtil(void);

	void Reset()
	{
		mCount = 0;
		mPacketOffset = 0;
	};

	BOOL MicroPacket(BYTE* pbPacket);
	BYTE* Get1TS(){ return m_b1TS; };

protected:
	BYTE m_b1TS[188];

	uint mCount;
	uint mPacketOffset;
};
