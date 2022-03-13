#include "StdAfx.h"
#include "DataIO3.h"
#include <process.h>
#include <algorithm>

#define NOT_SYNC_BYTE		0x74

CDataIO3::CDataIO3(BOOL bMemStreaming)
 : CBaseIO(bMemStreaming)
{
	VIRTUAL_COUNT = 8*8;

	m_hThread1 = INVALID_HANDLE_VALUE;
	m_hThread2 = INVALID_HANDLE_VALUE;
	m_hThread3 = INVALID_HANDLE_VALUE;
	m_hThread4 = INVALID_HANDLE_VALUE;

	m_pcDevice = NULL;

	m_T0SetBuff = NULL;
	m_T1SetBuff = NULL;
	m_S0SetBuff = NULL;
	m_S1SetBuff = NULL;

	m_hWakeupEvent = _CreateEvent(FALSE, FALSE, NULL );

	m_hSetBuffEvent1 = _CreateEvent(FALSE, TRUE, NULL );
	m_hSetBuffEvent2 = _CreateEvent(FALSE, TRUE, NULL );
	m_hSetBuffEvent3 = _CreateEvent(FALSE, TRUE, NULL );
	m_hSetBuffEvent4 = _CreateEvent(FALSE, TRUE, NULL );

}

CDataIO3::~CDataIO3(void)
{
	Stop();

	if( m_hSetBuffEvent1 != NULL ){
		SetBuffUnLock1();
		CloseHandle(m_hSetBuffEvent1);
		m_hSetBuffEvent1 = NULL;
	}
	if( m_hSetBuffEvent2 != NULL ){
		SetBuffUnLock2();
		CloseHandle(m_hSetBuffEvent2);
		m_hSetBuffEvent2 = NULL;
	}
	if( m_hSetBuffEvent3 != NULL ){
		SetBuffUnLock3();
		CloseHandle(m_hSetBuffEvent3);
		m_hSetBuffEvent3 = NULL;
	}
	if( m_hSetBuffEvent4 != NULL ){
		SetBuffUnLock4();
		CloseHandle(m_hSetBuffEvent4);
		m_hSetBuffEvent4 = NULL;
	}

	if( m_hWakeupEvent != NULL){
		CloseHandle(m_hWakeupEvent);
		m_hWakeupEvent = NULL;
	}
}

bool CDataIO3::SetBuffLock1(DWORD timeout)
{
	if( m_hSetBuffEvent1 == NULL ){
		return false ;
	}
	if( WaitForSingleObject(m_hSetBuffEvent1, timeout) == WAIT_TIMEOUT ){
		OutputDebugString(L"time out1");
		return false ;
	}
	return true ;
}

void CDataIO3::SetBuffUnLock1()
{
	if( m_hSetBuffEvent1 != NULL ){
		SetEvent(m_hSetBuffEvent1);
	}
}

bool CDataIO3::SetBuffLock2(DWORD timeout)
{
	if( m_hSetBuffEvent2 == NULL ){
		return false ;
	}
	if( WaitForSingleObject(m_hSetBuffEvent2, timeout) == WAIT_TIMEOUT ){
		OutputDebugString(L"time out2");
		return false ;
	}
	return true ;
}

void CDataIO3::SetBuffUnLock2()
{
	if( m_hSetBuffEvent2 != NULL ){
		SetEvent(m_hSetBuffEvent2);
	}
}

bool CDataIO3::SetBuffLock3(DWORD timeout)
{
	if( m_hSetBuffEvent3 == NULL ){
		return false ;
	}
	if( WaitForSingleObject(m_hSetBuffEvent3, timeout) == WAIT_TIMEOUT ){
		OutputDebugString(L"time out3");
		return false ;
	}
	return true ;
}

void CDataIO3::SetBuffUnLock3()
{
	if( m_hSetBuffEvent3 != NULL ){
		SetEvent(m_hSetBuffEvent3);
	}
}

bool CDataIO3::SetBuffLock4(DWORD timeout)
{
	if( m_hSetBuffEvent4 == NULL ){
		return false ;
	}
	if( WaitForSingleObject(m_hSetBuffEvent4, timeout) == WAIT_TIMEOUT ){
		OutputDebugString(L"time out4");
		return false ;
	}
	return true ;
}

void CDataIO3::SetBuffUnLock4()
{
	if( m_hSetBuffEvent4 != NULL ){
		SetEvent(m_hSetBuffEvent4);
	}
}

void CDataIO3::ClearBuff(int iID)
{
	int iDevID = iID>>16;
	PT::Device::ISDB enISDB = (PT::Device::ISDB)((iID&0x0000FF00)>>8);
	uint32 iTuner = iID&0x000000FF;

	auto clear = [&](DWORD dwID) {
		SetBuffLock(dwID);
		BuffLock(dwID);
		OverFlowCount(dwID) = 0;
		Flush(Buff(dwID));
		BuffUnLock(dwID);
		SetBuffUnLock(dwID);
	};

	if( enISDB == PT::Device::ISDB_T )
		clear( iTuner==0 ? 0 : 1 );
	else
		clear( iTuner==0 ? 2 : 3 );
}

void CDataIO3::Run(int iID)
{
	int iDevID = iID>>16;
	PT::Device::ISDB enISDB = (PT::Device::ISDB)((iID&0x0000FF00)>>8);
	uint32 iTuner = iID&0x000000FF;

	if( m_hThread1 == INVALID_HANDLE_VALUE &&
		m_hThread2 == INVALID_HANDLE_VALUE &&
		m_hThread3 == INVALID_HANDLE_VALUE &&
		m_hThread4 == INVALID_HANDLE_VALUE ) {

		m_bThTerm=FALSE;
		::ResetEvent(m_hWakeupEvent);
	}

	if(enISDB == PT::Device::ISDB_T) {
		// Thread 1 - T0
		if(iTuner==0 && m_hThread1==INVALID_HANDLE_VALUE) {
			m_hThread1 = (HANDLE)_beginthreadex(NULL, 0, RecvThreadProc,
				(LPVOID) new RECVTHREAD_PARAM(this,0), CREATE_SUSPENDED, NULL);
			SetThreadPriority( m_hThread1, THREAD_PRIORITY_ABOVE_NORMAL );
			ResumeThread(m_hThread1);
			// MemStreaming
			StartMemStreaming(0);
		}
		// Thread 2 - T1
		else if(iTuner==1 && m_hThread2==INVALID_HANDLE_VALUE) {
			m_hThread2 = (HANDLE)_beginthreadex(NULL, 0, RecvThreadProc,
				(LPVOID) new RECVTHREAD_PARAM(this,1), CREATE_SUSPENDED, NULL);
			SetThreadPriority( m_hThread2, THREAD_PRIORITY_ABOVE_NORMAL );
			ResumeThread(m_hThread2);
			// MemStreaming
			StartMemStreaming(1);
		}
	}else if(enISDB == PT::Device::ISDB_S) {
		// Thread 3 - S0
		if(iTuner==0 && m_hThread3==INVALID_HANDLE_VALUE) {
			m_hThread3 = (HANDLE)_beginthreadex(NULL, 0, RecvThreadProc,
				(LPVOID) new RECVTHREAD_PARAM(this,2), CREATE_SUSPENDED, NULL);
			SetThreadPriority( m_hThread3, THREAD_PRIORITY_ABOVE_NORMAL );
			ResumeThread(m_hThread3);
			// MemStreaming
			StartMemStreaming(2);
		}
		// Thread 4 - S1
		else if(iTuner==1 && m_hThread4==INVALID_HANDLE_VALUE) {
			m_hThread4 = (HANDLE)_beginthreadex(NULL, 0, RecvThreadProc,
				(LPVOID) new RECVTHREAD_PARAM(this,3), CREATE_SUSPENDED, NULL);
			SetThreadPriority( m_hThread4, THREAD_PRIORITY_ABOVE_NORMAL );
			ResumeThread(m_hThread4);
			// MemStreaming
			StartMemStreaming(3);
		}
	}
}

void CDataIO3::Stop()
{
	if( m_hThread1 != INVALID_HANDLE_VALUE ||
		m_hThread2 != INVALID_HANDLE_VALUE ||
		m_hThread3 != INVALID_HANDLE_VALUE ||
		m_hThread4 != INVALID_HANDLE_VALUE ){
		// スレッド終了待ち
		HANDLE handles[4];
		DWORD cnt=0;
		if(m_hThread1!=INVALID_HANDLE_VALUE) handles[cnt++]=m_hThread1 ;
		if(m_hThread2!=INVALID_HANDLE_VALUE) handles[cnt++]=m_hThread2 ;
		if(m_hThread3!=INVALID_HANDLE_VALUE) handles[cnt++]=m_hThread3 ;
		if(m_hThread4!=INVALID_HANDLE_VALUE) handles[cnt++]=m_hThread4 ;
		m_bThTerm=TRUE;
		::SetEvent(m_hWakeupEvent);
		if ( ::WaitForMultipleObjects(cnt,handles,TRUE, 15000) == WAIT_TIMEOUT ){
			for(DWORD i=0;i<cnt;i++)
				if(::WaitForSingleObject(handles[i],0)!=WAIT_OBJECT_0)
					::TerminateThread(handles[i], 0xffffffff);
		}
		for(DWORD i=0;i<cnt;i++)
			CloseHandle(handles[i]);
		m_hThread1 = INVALID_HANDLE_VALUE;
		m_hThread2 = INVALID_HANDLE_VALUE;
		m_hThread3 = INVALID_HANDLE_VALUE;
		m_hThread4 = INVALID_HANDLE_VALUE;
	}

	// MemStreaming
	StopMemStreaming();
}

void CDataIO3::StartPipeServer(int iID)
{
	int iDevID = iID>>16;
	PT::Device::ISDB enISDB = (PT::Device::ISDB)((iID&0x0000FF00)>>8);
	uint32 iTuner = iID&0x000000FF;

	wstring strPipe = L"";
	wstring strEvent = L"";
	Format(strPipe, L"%s%d", CMD_PT_DATA_PIPE, iID );
	Format(strEvent, L"%s%d", CMD_PT_DATA_EVENT_WAIT_CONNECT, iID );

	// MemStreaming
	wstring strStreamerName;
	Format(strStreamerName, SHAREDMEM_TRANSPORT_STREAM_FORMAT, PT_VER, iID);

	status enStatus = m_pcDevice->SetTransferTestMode(enISDB, iTuner);
	if( enStatus != PT::STATUS_OK ){
		return ;
	}

	auto start = [&](DWORD dwID) {
		SetBuffLock(dwID);
		::SetEvent(m_hWakeupEvent);
		auto &buff = SetBuff(dwID);
		if( buff == NULL ){
			buff = new EARTH3::EX::Buffer(m_pcDevice);
			buff->Alloc(DATA_UNIT_SIZE, VIRTUAL_COUNT);
			WriteIndex(dwID) = 0;

			uint8 *ptr = static_cast<uint8 *>(buff->Ptr(0));
			for (uint32 i=0; i<VIRTUAL_COUNT; i++) {
				ptr[DATA_UNIT_SIZE*i] = NOT_SYNC_BYTE;	// 同期バイト部分に NOT_SYNC_BYTE を書き込む
				buff->SyncCpu(i);		// CPU キャッシュをフラッシュ
			}

			uint64 pageAddress = buff->PageDescriptorAddress();

			enStatus = m_pcDevice->SetTransferPageDescriptorAddress(
				enISDB, iTuner, pageAddress);

			BuffLock(dwID);
			OverFlowCount(dwID) = 0;
			auto &st = MemStreamer(dwID) ;
			if(m_bMemStreaming&&st==NULL)
				st = new CSharedTransportStreamer(
					strStreamerName, FALSE, SHAREDMEM_TRANSPORT_PACKET_SIZE,
					SHAREDMEM_TRANSPORT_PACKET_NUM);
			BuffUnLock(dwID);
		}
		::ResetEvent(m_hWakeupEvent);
		SetBuffUnLock(dwID);
		if(!m_bMemStreaming)
			Pipe(dwID).StartServer(strEvent.c_str(), strPipe.c_str(),
				OutsideCmdCallback(dwID), this, THREAD_PRIORITY_ABOVE_NORMAL);
	};


	if( enISDB == PT::Device::ISDB_T )
		start( iTuner == 0 ? 0 : 1 );
	else
		start( iTuner == 0 ? 2 : 3 );
}

void CDataIO3::StopPipeServer(int iID)
{
	int iDevID = iID>>16;
	PT::Device::ISDB enISDB = (PT::Device::ISDB)((iID&0x0000FF00)>>8);
	uint32 iTuner = iID&0x000000FF;

	auto stop = [&](DWORD dwID) {
		if(!m_bMemStreaming) Pipe(dwID).StopServer();
		SetBuffLock(dwID);
		BuffLock(dwID);
		m_fDataCarry[dwID] = false;
		auto &st = MemStreamer(dwID);
		SAFE_DELETE(st);
		OverFlowCount(dwID) = 0;
		auto &buff = SetBuff(dwID);
		SAFE_DELETE(buff);
		Flush(Buff(dwID));
		BuffUnLock(dwID);
		SetBuffUnLock(dwID);
	};

	if( enISDB == PT::Device::ISDB_T )
		stop( iTuner == 0 ? 0 : 1 );
	else
		stop( iTuner == 0 ? 2 : 3 );
}

BOOL CDataIO3::EnableTuner(int iID, BOOL bEnable)
{
	int iDevID = iID>>16;
	PT::Device::ISDB enISDB = (PT::Device::ISDB)((iID&0x0000FF00)>>8);
	uint32 iTuner = iID&0x000000FF;

	status enStatus = PT::STATUS_OK;

	if( bEnable ){

		auto enable = [&](DWORD dwID) {
			SetBuffLock(dwID);
			BuffLock(dwID);
			if( SetBuff(dwID) != NULL ){
				OverFlowCount(dwID) = 0;
				WriteIndex(dwID) = 0;
			}
			Flush(Buff(dwID), TRUE);
			BuffUnLock(dwID);
			SetBuffUnLock(dwID);
		};

		if( enISDB == PT::Device::ISDB_T )
			enable( iTuner == 0 ? 0 : 1 );
		else
			enable( iTuner == 0 ? 2 : 3 );

		enStatus = m_pcDevice->SetTransferTestMode(enISDB, iTuner);
		enStatus = m_pcDevice->SetTransferEnabled(enISDB, iTuner, true);
		_OutputDebugString(L"Device::SetTransferEnabled ISDB:%d tuner:%d enable:true●",enISDB,iTuner);
		if( enStatus != PT::STATUS_OK ){
			_OutputDebugString(L"★SetTransferEnabled true err");
			return FALSE;
		}

	}else{

		enStatus = m_pcDevice->SetTransferEnabled(enISDB, iTuner, false);
		_OutputDebugString(L"Device::SetTransferEnabled ISDB:%d tuner:%d enable:false■",enISDB,iTuner);
		if( enStatus != PT::STATUS_OK ){
			_OutputDebugString(L"★SetTransferEnabled false err");
//			return FALSE;
		}

		auto disable = [&](DWORD dwID) {
			SetBuffLock(dwID);
			BuffLock(dwID);
			m_fDataCarry[dwID] = false;
			OverFlowCount(dwID) = 0;
			Flush(Buff(dwID));
			BuffUnLock(dwID);
			SetBuffUnLock(dwID);
		};

		if( enISDB == PT::Device::ISDB_T )
			disable( iTuner == 0 ? 0 : 1 );
		else
			disable( iTuner == 0 ? 2 : 3 );

	}
	return TRUE;
}

void CDataIO3::ChkTransferInfo()
{
	BOOL err = FALSE;
	PT::Device::TransferInfo transferInfo;

	if( m_T0SetBuff != NULL ){
		ZeroMemory(&transferInfo, sizeof(PT::Device::TransferInfo));
		m_pcDevice->GetTransferInfo((PT::Device::ISDB)0, 0, &transferInfo);

		if( transferInfo.InternalFIFO_A_Overflow ||
			transferInfo.InternalFIFO_A_Underflow ||
			transferInfo.InternalFIFO_B_Overflow ||
			transferInfo.InternalFIFO_B_Underflow ||
			transferInfo.ExternalFIFO_Overflow ||
			transferInfo.Status >= 0x100
			){
				_OutputDebugString(L"★TransferInfo err : isdb:%d, tunerIndex:%d status:%d InternalFIFO_A_Overflow:%d InternalFIFO_A_Underflow:%d InternalFIFO_B_Overflow:%d InternalFIFO_B_Underflow:%d ExternalFIFO_Overflow:%d",
					0, 0,transferInfo.Status,
					transferInfo.InternalFIFO_A_Overflow,
					transferInfo.InternalFIFO_A_Underflow,
					transferInfo.InternalFIFO_B_Overflow,
					transferInfo.InternalFIFO_B_Underflow,
					transferInfo.ExternalFIFO_Overflow
					);
				err = TRUE;
		}
	}
	if( m_T1SetBuff != NULL ){
		ZeroMemory(&transferInfo, sizeof(PT::Device::TransferInfo));
		m_pcDevice->GetTransferInfo((PT::Device::ISDB)0, 1, &transferInfo);

		if( transferInfo.InternalFIFO_A_Overflow ||
			transferInfo.InternalFIFO_A_Underflow ||
			transferInfo.InternalFIFO_B_Overflow ||
			transferInfo.InternalFIFO_B_Underflow ||
			transferInfo.ExternalFIFO_Overflow ||
			transferInfo.Status >= 0x100
			){
				_OutputDebugString(L"★TransferInfo err : isdb:%d, tunerIndex:%d status:%d InternalFIFO_A_Overflow:%d InternalFIFO_A_Underflow:%d InternalFIFO_B_Overflow:%d InternalFIFO_B_Underflow:%d ExternalFIFO_Overflow:%d",
					0, 1,transferInfo.Status,
					transferInfo.InternalFIFO_A_Overflow,
					transferInfo.InternalFIFO_A_Underflow,
					transferInfo.InternalFIFO_B_Overflow,
					transferInfo.InternalFIFO_B_Underflow,
					transferInfo.ExternalFIFO_Overflow
					);
				err = TRUE;
		}
	}
	if( m_S0SetBuff != NULL ){
		ZeroMemory(&transferInfo, sizeof(PT::Device::TransferInfo));
		m_pcDevice->GetTransferInfo((PT::Device::ISDB)1, 0, &transferInfo);

		if( transferInfo.InternalFIFO_A_Overflow ||
			transferInfo.InternalFIFO_A_Underflow ||
			transferInfo.InternalFIFO_B_Overflow ||
			transferInfo.InternalFIFO_B_Underflow ||
			transferInfo.ExternalFIFO_Overflow ||
			transferInfo.Status >= 0x100
			){
				_OutputDebugString(L"★TransferInfo err : isdb:%d, tunerIndex:%d status:%d InternalFIFO_A_Overflow:%d InternalFIFO_A_Underflow:%d InternalFIFO_B_Overflow:%d InternalFIFO_B_Underflow:%d ExternalFIFO_Overflow:%d",
					1, 0,transferInfo.Status,
					transferInfo.InternalFIFO_A_Overflow,
					transferInfo.InternalFIFO_A_Underflow,
					transferInfo.InternalFIFO_B_Overflow,
					transferInfo.InternalFIFO_B_Underflow,
					transferInfo.ExternalFIFO_Overflow
					);
				err = TRUE;
		}
	}
	if( m_S1SetBuff != NULL ){
		ZeroMemory(&transferInfo, sizeof(PT::Device::TransferInfo));
		m_pcDevice->GetTransferInfo((PT::Device::ISDB)1, 1, &transferInfo);

		if( transferInfo.InternalFIFO_A_Overflow ||
			transferInfo.InternalFIFO_A_Underflow ||
			transferInfo.InternalFIFO_B_Overflow ||
			transferInfo.InternalFIFO_B_Underflow ||
			transferInfo.ExternalFIFO_Overflow ||
			transferInfo.Status >= 0x100
			){
				_OutputDebugString(L"★TransferInfo err : isdb:%d, tunerIndex:%d status:%d InternalFIFO_A_Overflow:%d InternalFIFO_A_Underflow:%d InternalFIFO_B_Overflow:%d InternalFIFO_B_Underflow:%d ExternalFIFO_Overflow:%d",
					1, 1,transferInfo.Status,
					transferInfo.InternalFIFO_A_Overflow,
					transferInfo.InternalFIFO_A_Underflow,
					transferInfo.InternalFIFO_B_Overflow,
					transferInfo.InternalFIFO_B_Underflow,
					transferInfo.ExternalFIFO_Overflow
					);
				err = TRUE;
		}
	}

	if( err ){
		for(int i=0; i<2; i++ ){
			for(int j=0; j<2; j++ ){
				int iID = (i<<8) | (j&0x000000FF);
				EnableTuner(iID, false);
			}
		}
		if( m_T0SetBuff != NULL ){
			int iID = (0<<8) | (0&0x000000FF);
			EnableTuner(iID, true);
		}
		if( m_T1SetBuff != NULL ){
			int iID = (0<<8) | (1&0x000000FF);
			EnableTuner(iID, true);
		}
		if( m_S0SetBuff != NULL ){
			int iID = (1<<8) | (0&0x000000FF);
			EnableTuner(iID, true);
		}
		if( m_S1SetBuff != NULL ){
			int iID = (1<<8) | (1&0x000000FF);
			EnableTuner(iID, true);
		}
	}
}

bool CDataIO3::CheckReady(DWORD dwID)
{
	auto buffer = SetBuff(dwID) ;
	auto index = WriteIndex(dwID) ;

	status status = PT::STATUS_OK;
	uint32 nextIndex = index+1;
	if( nextIndex >= VIRTUAL_COUNT ){
		nextIndex = 0;
	}
	volatile uint8 *ptr = static_cast<uint8 *>(buffer->Ptr(nextIndex));
	status = buffer->SyncCpu(nextIndex);
	if( status != PT::STATUS_OK){
		return false;
	}

	uint8 data = ptr[0];	// 順番的にSyncCpu() -> 読み出しの方が直観的なので…

	if (data == 0x47) return true;
//	if (data == NOT_SYNC_BYTE) return false;

	return false;
}

bool CDataIO3::ReadAddBuff(DWORD dwID)
{
	auto buffer = SetBuff(dwID) ;
	auto index = WriteIndex(dwID) ;
	auto &tsBuff = Buff(dwID) ;
	auto &OverFlow = OverFlowCount(dwID) ;

	status status = PT::STATUS_OK;
	status = buffer->SyncIo(index);
	if( status != PT::STATUS_OK){
		return false;
	}
	uint8 *ptr = static_cast<uint8 *>(buffer->Ptr(index));
	/*
	HANDLE hFile = CreateFile(L"test.ts", GENERIC_WRITE , FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	SetFilePointer(hFile, 0, 0, FILE_END);
	DWORD w;
	WriteFile(hFile, ptr, UNIT_SIZE, &w, NULL);
	CloseHandle(hFile);
	*/
	BuffLock(dwID);

	for (uint32 i = 0; i<DATA_UNIT_SIZE; i += DATA_BUFF_SIZE){
		if(tsBuff.no_pool()) { // overflow
			tsBuff.pull();
			OverFlow++ ;
			OutputDebugString(IdentStr(dwID,L" Buff Full").c_str());
		}else {
			OverFlow = 0 ;
		}
		BuffUnLock(dwID);
		auto head = tsBuff.head();
		head->resize(DATA_BUFF_SIZE);
		memcpy(head->data(), ptr+i, DATA_BUFF_SIZE);
		BuffLock(dwID);
		tsBuff.push();
	}

	m_fDataCarry[dwID] = true ;
	BuffUnLock(dwID);

	ptr[0] = NOT_SYNC_BYTE;
	status = buffer->SyncCpu(index);

	return true;
}


UINT WINAPI CDataIO3::RecvThreadProc(LPVOID pParam)
{
	RECVTHREAD_PARAM *p = (RECVTHREAD_PARAM*) pParam;
	CDataIO3* pSys = p->pSys ;
	DWORD dwID = p->dwID ;
	delete p;

	HANDLE hCurThread = GetCurrentThread();
	SetThreadPriority(hCurThread, THREAD_PRIORITY_HIGHEST);

	const DWORD IDLE_WAIT = 5000 ;
	const DWORD MAX_WAIT = 200 ;
	const DWORD MIN_WAIT = 0 ;
	const size_t MAX_AVG = 10 ;
	fixed_queue<DWORD> avg(MAX_AVG+2) ;
	DWORD sleepy=0 ;
	bool idle = true ;

	DWORD s=GetTickCount();
	while(!pSys->m_bThTerm) {
		pSys->SetBuffLock(dwID);
		if( pSys->SetBuff(dwID) != NULL ){
			if(idle) {
				avg.clear();
				for(sleepy=0;avg.size()<MAX_AVG;sleepy+=avg.front())
					avg.push_front(MIN_WAIT);
				idle=false ;
				s=GetTickCount();
			}
			if( pSys->CheckReady(dwID) ){
				DWORD e=GetTickCount();
				avg.push_front(std::min<DWORD>(e-s,MAX_WAIT));
				sleepy+=avg.front();
				if( pSys->ReadAddBuff(dwID) ){
					pSys->WriteIndex(dwID)++;
					if (pSys->VIRTUAL_COUNT <= pSys->WriteIndex(dwID)) {
						pSys->WriteIndex(dwID) = 0;
					}
					avg.push_front(MIN_WAIT) ;
					sleepy+=avg.front();
				}
				s=GetTickCount();
			}
		}else {
			avg.push_front(IDLE_WAIT) ;
			sleepy+=avg.front() ;
			idle = true ;
		}
		pSys->SetBuffUnLock(dwID);
		while(avg.size()>MAX_AVG) {
			sleepy-=avg.back();
			avg.pop_back();
		}
		if(DWORD wait = avg.size()>0 ? DWORD(sleepy/avg.size()) : 0)
			WaitForSingleObject(pSys->m_hWakeupEvent, wait)==WAIT_TIMEOUT || pSys->m_bThTerm || !SleepEx(10, TRUE) ;
	}

	return 0;
}

DWORD CDataIO3::GetOverFlowCount(int iID)
{
	int iDevID = iID>>16;
	PT::Device::ISDB enISDB = (PT::Device::ISDB)((iID&0x0000FF00)>>8);
	uint32 iTuner = iID&0x000000FF;

	DWORD dwRet = 0;
	if( enISDB == PT::Device::ISDB_T ){
		if( iTuner == 0 ){
			dwRet = m_dwT0OverFlowCount;
		}else{
			dwRet = m_dwT1OverFlowCount;
		}
	}else{
		if( iTuner == 0 ){
			dwRet = m_dwS0OverFlowCount;
		}else{
			dwRet = m_dwS1OverFlowCount;
		}
	}
	return dwRet;
}

