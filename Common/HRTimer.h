//===========================================================================
#pragma once

#ifndef _HRTIMER_187BE8C9_0DB1_4F50_B02E_B271FCB3B274_H_INCLUDED_
#define _HRTIMER_187BE8C9_0DB1_4F50_B02E_B271FCB3B274_H_INCLUDED_
//---------------------------------------------------------------------------

//===========================================================================
namespace PRY8EAlByw {
//---------------------------------------------------------------------------

#define PRY8EAlByw_HRTIMER

  // High Resolution Timer functions

void SetHRTimerMode(BOOL useHighResolution) ;
void HRSleep(DWORD msec, DWORD usec=0) ;
DWORD HRWaitForSingleObject(HANDLE hObj, DWORD msec, DWORD usec=0);
DWORD HRWaitForMultipleObjects(DWORD numObjs, const HANDLE *hObjs, BOOL waitAll,
  DWORD msec, DWORD usec=0);

//---------------------------------------------------------------------------
} // End of namespace PRY8EAlByw
//===========================================================================
using namespace PRY8EAlByw ;
//===========================================================================
#endif // _HRTIMER_187BE8C9_0DB1_4F50_B02E_B271FCB3B274_H_INCLUDED_
