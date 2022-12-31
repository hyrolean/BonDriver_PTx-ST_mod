#include "StdAfx.h"

#include "PT3Manager.h"

#include "../Common/PTxManager.cxx"

extern "C" IPTManager* CreatePT3Manager(void)
{
	return new CPT3Manager;
}
