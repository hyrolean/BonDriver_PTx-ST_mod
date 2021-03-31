#include "StdAfx.h"
#include <Shlwapi.h>
#pragma comment(lib, "Shlwapi.lib")

#include "PT3Manager.h"

#include "../Common/PTxManager.cxx"

extern "C" IPTManager* CreatePT3Manager(void)
{
	return new CPT3Manager;
}
