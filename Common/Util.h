#ifndef __UTIL_H__
#define __UTIL_H__

// MFC‚ÅŽg‚¤Žž—p
//#define _MFC
#ifdef _MFC
#ifdef _DEBUG
#undef new
#endif
#include <string>
#include <map>
#include <vector>
#ifdef _DEBUG
#define new DEBUG_NEW
#endif
#else
#include <string>
#include <map>
#include <vector>
#include <deque>
#endif
using namespace std;
#include <TCHAR.h>

#define SAFE_DELETE(p)       do { if(p) { delete (p);     (p)=NULL; } } while(0)
#define SAFE_DELETE_ARRAY(p) do { if(p) { delete[] (p);   (p)=NULL; } } while(0)

HANDLE _CreateEvent(BOOL bManualReset, BOOL bInitialState, LPCTSTR lpName );
HANDLE _CreateFile( LPCTSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile );
HANDLE _CreateMutex( BOOL bInitialOwner, LPCTSTR lpName );
HANDLE _CreateFileMapping( HANDLE hFile, DWORD flProtect, DWORD dwMaximumSizeHigh, DWORD dwMaximumSizeLow, LPCTSTR lpName );
HANDLE _CreateNamedPipe( LPCTSTR lpName, DWORD dwOpenMode, DWORD dwPipeMode, DWORD nMaxInstances, DWORD nOutBufferSize, DWORD nInBufferSize, DWORD nDefaultTimeOut );
BOOL _CreateDirectory( LPCTSTR lpPathName );
HANDLE _CreateFile2( LPCTSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile );

void _OutputDebugString(const TCHAR *pOutputString, ...);

#ifdef _DEBUG
#include <stdio.h>
#include <stdarg.h>
#include <malloc.h>

	#define DEBUG_TO_X_DRIVE

	void __inline DBGOUT( const char* format,... )
	{
		va_list marker ;
		va_start( marker, format ) ;
		int edit_ln = _vscprintf(format, marker);
		if(edit_ln++>0) {
			char *edit_str = static_cast<char*>(alloca(edit_ln)) ;
			vsprintf_s( edit_str, edit_ln, format, marker ) ;
			va_end( marker ) ;
			#ifndef DEBUG_TO_X_DRIVE
			OutputDebugStringA(edit_str) ;
			#else
			{
				FILE *fp = NULL ;
				fopen_s(&fp,"X:\\Debug.txt","a+t") ;
				if(fp) {
					fputs(edit_str,fp) ;
					fclose(fp) ;
				}
			}
			#endif
		}
	}
	#define LINEDEBUG DBGOUT("%s(%d): passed.\n",__FILE__,__LINE__)

#else

	#define DBGOUT(...) /*empty*/
	#define LINEDEBUG   /*empty*/

#endif

	auto inline dur(DWORD s=0, DWORD e=GetTickCount()) -> DWORD {
		// duration ( s -> e )
		return s <= e ? e - s : 0xFFFFFFFFUL - s + 1 + e;
	};

#endif
