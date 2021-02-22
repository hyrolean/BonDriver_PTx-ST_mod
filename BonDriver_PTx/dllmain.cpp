// dllmain.cpp : DLL アプリケーションのエントリ ポイントを定義します。
#include "stdafx.h"
#include "BonTuner.h"

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
					 )
{
    switch(ul_reason_for_call){
		case DLL_PROCESS_ATTACH:
			// 初期化
			InitializeBonTuners(hModule);
			break;

		case DLL_PROCESS_DETACH:
			// 破棄
			FinalizeBonTuners();
			break;
	}
    return TRUE;
}

