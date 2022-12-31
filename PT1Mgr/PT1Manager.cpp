#include "StdAfx.h"

#include "PT1Manager.h"

#include "../Common/PTxManager.cxx"

extern "C" IPTManager* CreatePT1Manager(void)
{
	return new CPT1Manager;
}