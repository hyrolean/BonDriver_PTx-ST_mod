//===========================================================================
#include "StdAfx.h"
#include <process.h>
#include <algorithm>

#include "BaseIO.h"
//---------------------------------------------------------------------------

//===========================================================================
CBaseIO::CBaseIO(BOOL bMemStreaming)
  : m_T0Buff(MAX_DATA_BUFF_COUNT,1), m_T1Buff(MAX_DATA_BUFF_COUNT,1),
	m_S0Buff(MAX_DATA_BUFF_COUNT,1), m_S1Buff(MAX_DATA_BUFF_COUNT,1)
{
	m_hBuffEvent1 = _CreateEvent(FALSE, TRUE, NULL );
	m_hBuffEvent2 = _CreateEvent(FALSE, TRUE, NULL );
	m_hBuffEvent3 = _CreateEvent(FALSE, TRUE, NULL );
	m_hBuffEvent4 = _CreateEvent(FALSE, TRUE, NULL );

	m_dwT0OverFlowCount = 0;
	m_dwT1OverFlowCount = 0;
	m_dwS0OverFlowCount = 0;
	m_dwS1OverFlowCount = 0;

	for(auto &c: m_fDataCarry) c = false ;

	// MemStreaming
	m_bMemStreaming = bMemStreaming ;
	m_bMemStreamingTerm = TRUE ;
	m_T0MemStreamer = NULL ;
	m_T1MemStreamer = NULL ;
	m_S0MemStreamer = NULL ;
	m_S1MemStreamer = NULL ;
	m_hT0MemStreamingThread = INVALID_HANDLE_VALUE ;
	m_hT1MemStreamingThread = INVALID_HANDLE_VALUE ;
	m_hS0MemStreamingThread = INVALID_HANDLE_VALUE ;
	m_hS1MemStreamingThread = INVALID_HANDLE_VALUE ;
}
//---------------------------------------------------------------------------
CBaseIO::~CBaseIO(void)
{
	// MemStreaming
	StopMemStreaming();

	SAFE_DELETE(m_T0MemStreamer);
	SAFE_DELETE(m_T1MemStreamer);
	SAFE_DELETE(m_S0MemStreamer);
	SAFE_DELETE(m_S1MemStreamer);

	if( m_hBuffEvent1 != NULL ){
		BuffUnLock1();
		CloseHandle(m_hBuffEvent1);
		m_hBuffEvent1 = NULL;
	}
	if( m_hBuffEvent2 != NULL ){
		BuffUnLock2();
		CloseHandle(m_hBuffEvent2);
		m_hBuffEvent2 = NULL;
	}
	if( m_hBuffEvent3 != NULL ){
		BuffUnLock3();
		CloseHandle(m_hBuffEvent3);
		m_hBuffEvent3 = NULL;
	}
	if( m_hBuffEvent4 != NULL ){
		BuffUnLock4();
		CloseHandle(m_hBuffEvent4);
		m_hBuffEvent4 = NULL;
	}
}
//---------------------------------------------------------------------------
bool CBaseIO::BuffLock1(DWORD timeout)
{
	if( m_hBuffEvent1 == NULL ){
		return false ;
	}
	if( WaitForSingleObject(m_hBuffEvent1, timeout) == WAIT_TIMEOUT ){
		OutputDebugString(L"time out1");
		return false ;
	}
	return true ;
}
//---------------------------------------------------------------------------
void CBaseIO::BuffUnLock1()
{
	if( m_hBuffEvent1 != NULL ){
		SetEvent(m_hBuffEvent1);
	}
}
//---------------------------------------------------------------------------
bool CBaseIO::BuffLock2(DWORD timeout)
{
	if( m_hBuffEvent2 == NULL ){
		return false ;
	}
	if( WaitForSingleObject(m_hBuffEvent2, timeout) == WAIT_TIMEOUT ){
		OutputDebugString(L"time out2");
		return false ;
	}
	return true ;
}
//---------------------------------------------------------------------------
void CBaseIO::BuffUnLock2()
{
	if( m_hBuffEvent2 != NULL ){
		SetEvent(m_hBuffEvent2);
	}
}
//---------------------------------------------------------------------------
bool CBaseIO::BuffLock3(DWORD timeout)
{
	if( m_hBuffEvent3 == NULL ){
		return false ;
	}
	if( WaitForSingleObject(m_hBuffEvent3, timeout) == WAIT_TIMEOUT ){
		OutputDebugString(L"time out3");
		return false ;
	}
	return true ;
}
//---------------------------------------------------------------------------
void CBaseIO::BuffUnLock3()
{
	if( m_hBuffEvent3 != NULL ){
		SetEvent(m_hBuffEvent3);
	}
}
//---------------------------------------------------------------------------
bool CBaseIO::BuffLock4(DWORD timeout)
{
	if( m_hBuffEvent4 == NULL ){
		return false ;
	}
	if( WaitForSingleObject(m_hBuffEvent4, timeout) == WAIT_TIMEOUT ){
		OutputDebugString(L"time out4");
		return false ;
	}
	return true ;
}
//---------------------------------------------------------------------------
void CBaseIO::BuffUnLock4()
{
	if( m_hBuffEvent4 != NULL ){
		SetEvent(m_hBuffEvent4);
	}
}
//---------------------------------------------------------------------------
int CALLBACK CBaseIO::OutsideCmdCallbackT0(void* pParam, CMD_STREAM* pCmdParam, CMD_STREAM* pResParam, BOOL* pbResDataAbandon)
{
	CBaseIO* pSys = (CBaseIO*)pParam;
	switch( pCmdParam->dwParam ){
		case CMD_SEND_DATA:
			pSys->CmdSendData(0, pCmdParam, pResParam, pbResDataAbandon);
			break;
		default:
			pResParam->dwParam = CMD_NON_SUPPORT;
			break;
	}
	return 0;
}
//---------------------------------------------------------------------------
int CALLBACK CBaseIO::OutsideCmdCallbackT1(void* pParam, CMD_STREAM* pCmdParam, CMD_STREAM* pResParam, BOOL* pbResDataAbandon)
{
	CBaseIO* pSys = (CBaseIO*)pParam;
	switch( pCmdParam->dwParam ){
		case CMD_SEND_DATA:
			pSys->CmdSendData(1, pCmdParam, pResParam, pbResDataAbandon);
			break;
		default:
			pResParam->dwParam = CMD_NON_SUPPORT;
			break;
	}
	return 0;
}
//---------------------------------------------------------------------------
int CALLBACK CBaseIO::OutsideCmdCallbackS0(void* pParam, CMD_STREAM* pCmdParam, CMD_STREAM* pResParam, BOOL* pbResDataAbandon)
{
	CBaseIO* pSys = (CBaseIO*)pParam;
	switch( pCmdParam->dwParam ){
		case CMD_SEND_DATA:
			pSys->CmdSendData(2, pCmdParam, pResParam, pbResDataAbandon);
			break;
		default:
			pResParam->dwParam = CMD_NON_SUPPORT;
			break;
	}
	return 0;
}
//---------------------------------------------------------------------------
int CALLBACK CBaseIO::OutsideCmdCallbackS1(void* pParam, CMD_STREAM* pCmdParam, CMD_STREAM* pResParam, BOOL* pbResDataAbandon)
{
	CBaseIO* pSys = (CBaseIO*)pParam;
	switch( pCmdParam->dwParam ){
		case CMD_SEND_DATA:
			pSys->CmdSendData(3, pCmdParam, pResParam, pbResDataAbandon);
			break;
		default:
			pResParam->dwParam = CMD_NON_SUPPORT;
			break;
	}
	return 0;
}
//---------------------------------------------------------------------------
void CBaseIO::CmdSendData(DWORD dwID, CMD_STREAM* pCmdParam, CMD_STREAM* pResParam, BOOL* pbResDataAbandon)
{
	if(!m_fDataCarry[dwID]) {
		pResParam->dwParam = CMD_ERR_BUSY;
		return;
	}

	pResParam->dwParam = CMD_SUCCESS;
	BOOL bSend = FALSE;

	if(BuffLock(dwID)) {
		auto &buf = Buff(dwID);
		if( buf.size() > 0 ){
			auto p = buf.pull();
			pResParam->dwSize = (DWORD)p->size();
			pResParam->bData = p->data();
			*pbResDataAbandon = TRUE;
			bSend = TRUE;
		}
		if(buf.empty())
			m_fDataCarry[dwID] = false ;
		BuffUnLock(dwID);
	}

	if( bSend == FALSE ){
		pResParam->dwParam = CMD_ERR_BUSY;
	}
}
//---------------------------------------------------------------------------
void CBaseIO::Flush(PTBUFFER &buf, BOOL dispose)
{
	if(dispose) {
		buf.dispose();
		for(size_t i=0; i<INI_DATA_BUFF_COUNT; i++) {
			buf.head()->growup(DATA_BUFF_SIZE);
			buf.push();
		}
	}
	buf.clear();
}
//---------------------------------------------------------------------------
// MemStreaming
//-----
UINT CBaseIO::MemStreamingThreadProcMain(DWORD dwID)
{
	const DWORD CmdWait = 100 ;
	const size_t sln = wstring(SHAREDMEM_TRANSPORT_STREAM_SUFFIX).length();

	PTBUFFER_OBJECT *obj = nullptr ;

	while (!m_bMemStreamingTerm) {

		auto tx = [&](PTBUFFER &buf, CSharedTransportStreamer *st) -> void {
			if(BuffLock(dwID,CmdWait)) {
				if(obj!=nullptr||!buf.empty()) {
					if(st!=NULL) {
						wstring mutexName = st->Name().substr(0, st->Name().length() - sln);
						if(HANDLE hMutex = OpenMutex(MUTEX_ALL_ACCESS,FALSE,mutexName.c_str())) {
							CloseHandle(hMutex);
							do {
								if(obj==nullptr) obj = buf.pull() ;
								auto data = obj->data() ;
								auto size = obj->size() ;
								if(buf.empty()) m_fDataCarry[dwID] = false ;
								BuffUnLock(dwID);
								if(!st->Tx(data,(DWORD)size,CmdWait))
									return ;
								obj=nullptr;
								if(m_bMemStreamingTerm||!BuffLock(dwID,CmdWait))
									return ;
							}while(!buf.empty());
						}
					}
				}else
					m_fDataCarry[dwID] = false ;
				BuffUnLock(dwID);
			}
		};

		if (!MemStreamer(dwID))
			obj=nullptr, m_fDataCarry[dwID] = false ;

		if (obj!=nullptr||m_fDataCarry[dwID])
			tx(Buff(dwID), MemStreamer(dwID));

		if (obj!=nullptr||!m_fDataCarry[dwID])
			Sleep(MemStreamer(dwID) ? 10 : CmdWait);
	}

	return 0;
}
//-----
UINT WINAPI CBaseIO::MemStreamingThreadProc(LPVOID pParam)
{
	auto p = static_cast<MEMSTREAMINGTHREAD_PARAM*>(pParam);
	CBaseIO* pSys = p->pSys;
	DWORD dwID = p->dwID ;
	delete p;

	HANDLE hCurThread = GetCurrentThread();
	SetThreadPriority(hCurThread, THREAD_PRIORITY_HIGHEST);

	return pSys->MemStreamingThreadProcMain(dwID);
}
//-----
void CBaseIO::StartMemStreaming(DWORD dwID)
{
	if( MemStreamingThread(0) == INVALID_HANDLE_VALUE &&
		MemStreamingThread(1) == INVALID_HANDLE_VALUE &&
		MemStreamingThread(2) == INVALID_HANDLE_VALUE &&
		MemStreamingThread(3) == INVALID_HANDLE_VALUE )
			m_bMemStreamingTerm = FALSE;

	HANDLE &hThread = MemStreamingThread(dwID) ;
	if(m_bMemStreaming&&hThread==INVALID_HANDLE_VALUE) {
		hThread = (HANDLE)_beginthreadex(NULL, 0, MemStreamingThreadProc,
			(LPVOID) new MEMSTREAMINGTHREAD_PARAM(this, dwID), CREATE_SUSPENDED, NULL);
		if(hThread != INVALID_HANDLE_VALUE) {
			SetThreadPriority( hThread, THREAD_PRIORITY_ABOVE_NORMAL );
			ResumeThread(hThread);
		}
	}
}
//-----
void CBaseIO::StopMemStreaming()
{
	if( MemStreamingThread(0) != INVALID_HANDLE_VALUE ||
		MemStreamingThread(1) != INVALID_HANDLE_VALUE ||
		MemStreamingThread(2) != INVALID_HANDLE_VALUE ||
		MemStreamingThread(3) != INVALID_HANDLE_VALUE ){
		// スレッド終了待ち
		HANDLE handles[4];
		DWORD cnt=0;
		for(DWORD dwID=0;dwID<4;dwID++)
			if(MemStreamingThread(dwID)!=INVALID_HANDLE_VALUE)
				handles[cnt++]=MemStreamingThread(dwID);
		m_bMemStreamingTerm=TRUE;
		if ( ::WaitForMultipleObjects(cnt,handles,TRUE, 15000) == WAIT_TIMEOUT ){
			for(DWORD i=0;i<cnt;i++)
				if(::WaitForSingleObject(handles[i],0)!=WAIT_OBJECT_0)
					::TerminateThread(handles[i], 0xffffffff);
		}
		for(DWORD i=0;i<cnt;i++)
			CloseHandle(handles[i]);
		for(DWORD dwID=0;dwID<4;dwID++)
			MemStreamingThread(dwID)=INVALID_HANDLE_VALUE;
	}
}
//===========================================================================
