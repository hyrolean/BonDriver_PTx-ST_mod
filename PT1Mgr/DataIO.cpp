#include "StdAfx.h"
#include "DataIO.h"
#include <process.h>

CDataIO::CDataIO(BOOL bMemStreaming)
  : m_T0Buff(MAX_DATA_BUFF_COUNT,1), m_T1Buff(MAX_DATA_BUFF_COUNT,1),
	m_S0Buff(MAX_DATA_BUFF_COUNT,1), m_S1Buff(MAX_DATA_BUFF_COUNT,1)
{
	VIRTUAL_COUNT = 8;

	m_hStopEvent = _CreateEvent(FALSE, FALSE, NULL);
	m_hThread = INVALID_HANDLE_VALUE;

	m_pcDevice = NULL;

	mQuit = false;

	m_hEvent1 = _CreateEvent(FALSE, TRUE, NULL );
	m_hEvent2 = _CreateEvent(FALSE, TRUE, NULL );
	m_hEvent3 = _CreateEvent(FALSE, TRUE, NULL );
	m_hEvent4 = _CreateEvent(FALSE, TRUE, NULL );

	m_dwT0OverFlowCount = 0;
	m_dwT1OverFlowCount = 0;
	m_dwS0OverFlowCount = 0;
	m_dwS1OverFlowCount = 0;

	m_bDMABuff = NULL;

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

CDataIO::~CDataIO(void)
{
	if( m_hThread != INVALID_HANDLE_VALUE ){
		::SetEvent(m_hStopEvent);
		// スレッド終了待ち
		if ( ::WaitForSingleObject(m_hThread, 15000) == WAIT_TIMEOUT ){
			::TerminateThread(m_hThread, 0xffffffff);
		}
		CloseHandle(m_hThread);
		m_hThread = INVALID_HANDLE_VALUE;
	}

	// MemStreaming
	StopMemStreaming();

	SAFE_DELETE(m_T0MemStreamer);
	SAFE_DELETE(m_T1MemStreamer);
	SAFE_DELETE(m_S0MemStreamer);
	SAFE_DELETE(m_S1MemStreamer);

	if( m_hStopEvent != NULL ){
		::CloseHandle(m_hStopEvent);
		m_hStopEvent = NULL;
	}

	if( m_hEvent1 != NULL ){
		UnLock1();
		CloseHandle(m_hEvent1);
		m_hEvent1 = NULL;
	}
	if( m_hEvent2 != NULL ){
		UnLock2();
		CloseHandle(m_hEvent2);
		m_hEvent2 = NULL;
	}
	if( m_hEvent3 != NULL ){
		UnLock3();
		CloseHandle(m_hEvent3);
		m_hEvent3 = NULL;
	}
	if( m_hEvent4 != NULL ){
		UnLock4();
		CloseHandle(m_hEvent4);
		m_hEvent4 = NULL;
	}

	SAFE_DELETE_ARRAY(m_bDMABuff);

}

bool CDataIO::Lock1(DWORD timeout)
{
	if( m_hEvent1 == NULL ){
		return false ;
	}
	if( WaitForSingleObject(m_hEvent1, timeout) == WAIT_TIMEOUT ){
		OutputDebugString(L"time out1");
		return false ;
	}
	return true ;
}

void CDataIO::UnLock1()
{
	if( m_hEvent1 != NULL ){
		SetEvent(m_hEvent1);
	}
}

bool CDataIO::Lock2(DWORD timeout)
{
	if( m_hEvent2 == NULL ){
		return false ;
	}
	if( WaitForSingleObject(m_hEvent2, timeout) == WAIT_TIMEOUT ){
		OutputDebugString(L"time out2");
		return false ;
	}
	return true ;
}

void CDataIO::UnLock2()
{
	if( m_hEvent2 != NULL ){
		SetEvent(m_hEvent2);
	}
}

bool CDataIO::Lock3(DWORD timeout)
{
	if( m_hEvent3 == NULL ){
		return false ;
	}
	if( WaitForSingleObject(m_hEvent3, timeout) == WAIT_TIMEOUT ){
		OutputDebugString(L"time out3");
		return false ;
	}
	return true ;
}

void CDataIO::UnLock3()
{
	if( m_hEvent3 != NULL ){
		SetEvent(m_hEvent3);
	}
}

bool CDataIO::Lock4(DWORD timeout)
{
	if( m_hEvent4 == NULL ){
		return false ;
	}
	if( WaitForSingleObject(m_hEvent4, timeout) == WAIT_TIMEOUT ){
		OutputDebugString(L"time out4");
		return false ;
	}
	return true ;
}

void CDataIO::UnLock4()
{
	if( m_hEvent4 != NULL ){
		SetEvent(m_hEvent4);
	}
}

void CDataIO::ClearBuff(int iID)
{
	int iDevID = iID>>16;
	PT::Device::ISDB enISDB = (PT::Device::ISDB)((iID&0x0000FF00)>>8);
	uint iTuner = iID&0x000000FF;

	auto clear = [&](DWORD dwID) {
		Lock(dwID);
		OverFlowCount(dwID) = 0;
		Flush(Buff(dwID));
		UnLock(dwID);
	};

	if( enISDB == PT::Device::ISDB_T )
		clear(iTuner == 0 ? 0 : 1 ) ;
	else
		clear(iTuner == 0 ? 2 : 3 ) ;
}

void CDataIO::Run(int iID)
{
	int iDevID = iID>>16;
	PT::Device::ISDB enISDB = (PT::Device::ISDB)((iID&0x0000FF00)>>8);
	uint iTuner = iID&0x000000FF;

	// MemStreaming
	if(enISDB == PT::Device::ISDB_T)
		StartMemStreaming( iTuner==0 ? 0 : 1 );
	else
		StartMemStreaming( iTuner==0 ? 2 : 3 );

	if( m_hThread != INVALID_HANDLE_VALUE ){
		return ;
	}
	status enStatus;

	bool bEnalbe = true;
	enStatus = m_pcDevice->GetTransferEnable(&bEnalbe);
	if( enStatus != PT::STATUS_OK ){
		return ;
	}
	if( bEnalbe ){
		enStatus = m_pcDevice->SetTransferEnable(false);
		if( enStatus != PT::STATUS_OK ){
			return ;
		}
	}

	enStatus = m_pcDevice->SetBufferInfo(NULL);
	if( enStatus != PT::STATUS_OK ){
		return ;
	}
	PT::Device::BufferInfo bufferInfo;
	bufferInfo.VirtualSize  = VIRTUAL_IMAGE_COUNT;
	bufferInfo.VirtualCount = VIRTUAL_COUNT;
	bufferInfo.LockSize     = LOCK_SIZE;
	enStatus = m_pcDevice->SetBufferInfo(&bufferInfo);
	if( enStatus != PT::STATUS_OK ){
		return ;
	}

	if( m_bDMABuff == NULL ){
		m_bDMABuff = new BYTE[READ_BLOCK_SIZE];
	}

	// DMA 転送がどこまで進んだかを調べるため、各ブロックの末尾を 0 でクリアする
	for (uint i=0; i<VIRTUAL_COUNT; i++) {
		for (uint j=0; j<VIRTUAL_IMAGE_COUNT; j++) {
			for (uint k=0; k<READ_BLOCK_COUNT; k++) {
				Clear(i, j, k);
			}
		}
	}

	// 転送カウンタをリセットする
	enStatus = m_pcDevice->ResetTransferCounter();
	if( enStatus != PT::STATUS_OK ){
		return ;
	}

	// 転送カウンタをインクリメントする
	for (uint i=0; i<VIRTUAL_IMAGE_COUNT*VIRTUAL_COUNT; i++) {
		enStatus = m_pcDevice->IncrementTransferCounter();
		if( enStatus != PT::STATUS_OK ){
			return ;
		}

	}

	// DMA 転送を許可する
	enStatus = m_pcDevice->SetTransferEnable(true);
	if( enStatus != PT::STATUS_OK ){
		return ;
	}

	::ResetEvent(m_hStopEvent);
	mQuit = false;
	m_hThread = (HANDLE)_beginthreadex(NULL, 0, RecvThreadProc, (LPVOID)this, CREATE_SUSPENDED, NULL);
	if(m_hThread != INVALID_HANDLE_VALUE) {
		SetThreadPriority( m_hThread, THREAD_PRIORITY_ABOVE_NORMAL );
		ResumeThread(m_hThread);
	}
}

void CDataIO::ResetDMA()
{
	status enStatus;

	bool bEnalbe = true;
	enStatus = m_pcDevice->GetTransferEnable(&bEnalbe);
	if( enStatus != PT::STATUS_OK ){
		return ;
	}
	if( bEnalbe ){
		enStatus = m_pcDevice->SetTransferEnable(false);
		if( enStatus != PT::STATUS_OK ){
			return ;
		}
	}

	mVirtualIndex=0;
	mImageIndex=0;
	mBlockIndex=0;

	// DMA 転送がどこまで進んだかを調べるため、各ブロックの末尾を 0 でクリアする
	for (uint i=0; i<VIRTUAL_COUNT; i++) {
		for (uint j=0; j<VIRTUAL_IMAGE_COUNT; j++) {
			for (uint k=0; k<READ_BLOCK_COUNT; k++) {
				Clear(i, j, k);
			}
		}
	}

	// 転送カウンタをリセットする
	enStatus = m_pcDevice->ResetTransferCounter();
	if( enStatus != PT::STATUS_OK ){
		return ;
	}

	// 転送カウンタをインクリメントする
	for (uint i=0; i<VIRTUAL_IMAGE_COUNT*VIRTUAL_COUNT; i++) {
		enStatus = m_pcDevice->IncrementTransferCounter();
		if( enStatus != PT::STATUS_OK ){
			return ;
		}

	}

	// DMA 転送を許可する
	enStatus = m_pcDevice->SetTransferEnable(true);
	if( enStatus != PT::STATUS_OK ){
		return ;
	}
}

void CDataIO::Stop()
{
	if( m_hThread != INVALID_HANDLE_VALUE ){
		mQuit = true;
		::SetEvent(m_hStopEvent);
		// スレッド終了待ち
		if ( ::WaitForSingleObject(m_hThread, 15000) == WAIT_TIMEOUT ){
			::TerminateThread(m_hThread, 0xffffffff);
		}
		CloseHandle(m_hThread);
		m_hThread = INVALID_HANDLE_VALUE;
	}

	// MemStreaming
	StopMemStreaming();

	status enStatus;

	bool bEnalbe = true;
	enStatus = m_pcDevice->GetTransferEnable(&bEnalbe);
	if( enStatus != PT::STATUS_OK ){
		return ;
	}
	if( bEnalbe ){
		enStatus = m_pcDevice->SetTransferEnable(false);
		if( enStatus != PT::STATUS_OK ){
			return ;
		}
	}
}

void CDataIO::EnableTuner(int iID, BOOL bEnable)
{
	int iDevID = iID>>16;
	PT::Device::ISDB enISDB = (PT::Device::ISDB)((iID&0x0000FF00)>>8);
	uint iTuner = iID&0x000000FF;

	wstring strPipe = L"";
	wstring strEvent = L"";
	Format(strPipe, L"%s%d", CMD_PT_DATA_PIPE, iID );
	Format(strEvent, L"%s%d", CMD_PT_DATA_EVENT_WAIT_CONNECT, iID );

	// MemStreaming
	wstring strStreamerName;
	Format(strStreamerName, SHAREDMEM_TRANSPORT_STREAM_FORMAT, PT_VER, iID);

	if( bEnable ){

		auto enable = [&](DWORD dwID) {
			Lock(dwID);
			OverFlowCount(dwID) = 0;
			Flush(Buff(dwID), TRUE);
			auto &st = MemStreamer(dwID) ;
			if(m_bMemStreaming&&st==NULL)
				st = new CSharedTransportStreamer(
					strStreamerName, FALSE, SHAREDMEM_TRANSPORT_PACKET_SIZE,
					SHAREDMEM_TRANSPORT_PACKET_NUM);
			UnLock(dwID);
			if(!m_bMemStreaming)
				Pipe(dwID).StartServer(strEvent.c_str(), strPipe.c_str(),
					OutsideCmdCallback(dwID), this, THREAD_PRIORITY_ABOVE_NORMAL);
		};

		if( enISDB == PT::Device::ISDB_T )
			enable(iTuner == 0 ? 0 : 1 ) ;
		else
			enable(iTuner == 0 ? 2 : 3 ) ;

	}else{ // Disable

		auto disable = [&](DWORD dwID) {
			if(!m_bMemStreaming) Pipe(dwID).StopServer();
			Lock(dwID);
			m_fDataCarry[dwID] = false;
			auto &st = MemStreamer(dwID);
			SAFE_DELETE(st);
			OverFlowCount(dwID) = 0;
			Flush(Buff(dwID));
			UnLock(dwID);
		};

		if( enISDB == PT::Device::ISDB_T )
			disable(iTuner == 0 ? 0 : 1 ) ;
		else
			disable(iTuner == 0 ? 2 : 3 ) ;

	}
}

UINT WINAPI CDataIO::RecvThreadProc(LPVOID pParam)
{
	CDataIO* pSys = (CDataIO*)pParam;

	HANDLE hCurThread = GetCurrentThread();
	SetThreadPriority(hCurThread, THREAD_PRIORITY_HIGHEST);

	pSys->mVirtualIndex = 0;
	pSys->mImageIndex = 0;
	pSys->mBlockIndex = 0;

	while (true) {
		DWORD dwRes = WaitForSingleObject(pSys->m_hStopEvent, 0);
		if( dwRes == WAIT_OBJECT_0 ){
			//STOP
			break;
		}

		bool b;

		b = pSys->WaitBlock();
		if (b == false) break;

		pSys->CopyBlock();

		b = pSys->DispatchBlock();
		if (b == false) break;
	}

	return 0;
}

// 1ブロック分 DMA 転送が終わるか mQuit が true になるまで待つ
bool CDataIO::WaitBlock()
{
	bool b = true;

	while (true) {
		if (mQuit) {
			b = false;
			break;
		}

		// ブロックの末尾が 0 でなければ、そのブロックの DMA 転送が完了したことになる
		if (Read(mVirtualIndex, mImageIndex, mBlockIndex) != 0) break;
		Sleep(3);
	}
	//::wprintf(L"(mVirtualIndex, mImageIndex, mBlockIndex) = (%d, %d, %d) の転送が終わりました。\n", mVirtualIndex, mImageIndex, mBlockIndex);

	return b;
}

// 1ブロック分のデータをテンポラリ領域にコピーする。CPU 側から見て DMA バッファはキャッシュが効かないため、
// キャッシュが効くメモリ領域にコピーしてからアクセスすると効率が高まります。
void CDataIO::CopyBlock()
{
	status enStatus;

	void *voidPtr;
	enStatus = m_pcDevice->GetBufferPtr(mVirtualIndex, &voidPtr);
	if( enStatus != PT::STATUS_OK ){
		return ;
	}
	DWORD dwOffset = ((TRANSFER_SIZE*mImageIndex) + (READ_BLOCK_SIZE*mBlockIndex));

	memcpy(m_bDMABuff, (BYTE*)voidPtr+dwOffset, READ_BLOCK_SIZE);

	// コピーし終わったので、ブロックの末尾を 0 にします。
	uint *ptr = static_cast<uint *>(voidPtr);
	ptr[Offset(mImageIndex, mBlockIndex, READ_BLOCK_SIZE)-1] = 0;

	if (READ_BLOCK_COUNT <= ++mBlockIndex) {
		mBlockIndex = 0;

		// 転送カウンタは OS::Memory::PAGE_SIZE * PT::Device::BUFFER_PAGE_COUNT バイトごとにインクリメントします。
		enStatus = m_pcDevice->IncrementTransferCounter();
		if( enStatus != PT::STATUS_OK ){
			return ;
		}

		if (VIRTUAL_IMAGE_COUNT <= ++mImageIndex) {
			mImageIndex = 0;
			if (VIRTUAL_COUNT <= ++mVirtualIndex) {
				mVirtualIndex = 0;
			}
		}
	}
}

void CDataIO::Clear(uint virtualIndex, uint imageIndex, uint blockIndex)
{
	void *voidPtr;
	status enStatus = m_pcDevice->GetBufferPtr(virtualIndex, &voidPtr);
	if( enStatus != PT::STATUS_OK ){
		return ;
	}

	uint *ptr = static_cast<uint *>(voidPtr);
	ptr[Offset(imageIndex, blockIndex, READ_BLOCK_SIZE)-1] = 0;
}

uint CDataIO::Read(uint virtualIndex, uint imageIndex, uint blockIndex) const
{
	void *voidPtr;
	status enStatus = m_pcDevice->GetBufferPtr(virtualIndex, &voidPtr);
	if( enStatus != PT::STATUS_OK ){
		return 0;
	}

	volatile const uint *ptr = static_cast<volatile const uint *>(voidPtr);
	return ptr[Offset(imageIndex, blockIndex, READ_BLOCK_SIZE)-1];
}

uint CDataIO::Offset(uint imageIndex, uint blockIndex, uint additionalOffset) const
{
	uint offset = ((TRANSFER_SIZE*imageIndex) + (READ_BLOCK_SIZE*blockIndex) + additionalOffset) / sizeof(uint);

	return offset;
}

bool CDataIO::DispatchBlock()
{
	const uint *ptr = (uint*)m_bDMABuff;

	for (uint i=0; i<READ_BLOCK_SIZE; i+=4) {
		uint packetError = BIT_SHIFT_MASK(m_bDMABuff[i+3], 0, 1);

		if (packetError) {
			// エラーの原因を調べる
			PT::Device::TransferInfo info;
			status enStatus = m_pcDevice->GetTransferInfo(&info);
			if( enStatus == PT::STATUS_OK ){
				if (info.TransferCounter0) {
					OutputDebugString(L"★転送カウンタが 0 であるのを検出した。");
					ResetDMA();
					break;
				} else if (info.TransferCounter1) {
					OutputDebugString(L"★転送カウンタが 1 以下になりました。");
					ResetDMA();
					break;
				} else if (info.BufferOverflow) {
					OutputDebugString(L"★PCI バスを長期に渡り確保できなかったため、ボード上の FIFO が溢れました。");
					ResetDMA();
					break;
				} else {
					OutputDebugString(L"★転送エラーが発生しました。");
					break;
				}
			}else{
				OutputDebugString(L"GetTransferInfo() err");
				break;
			}
		}else{
			MicroPacket(m_bDMABuff+i);
		}
	}

	return true;
}

void CDataIO::MicroPacket(BYTE* pbPacket)
{
	uint packetId      = BIT_SHIFT_MASK(pbPacket[3], 5,  3);

	auto init_head = [](PTBUFFER_OBJECT *head) {
		if(head->size()>=head->capacity()) {
			head->resize(0);
			head->growup(DATA_BUFF_SIZE);
		}
	};

	BOOL bCreate1TS = FALSE;

	DWORD dwID;
	switch(packetId){
	case 2: dwID=0; break;
	case 4: dwID=1; break;
	case 1: dwID=2; break;
	case 3: dwID=3; break;
	default: return;
	}

	auto &micro = Micro(dwID);
	auto &buf = Buff(dwID);
	auto &overflow = OverFlowCount(dwID);

	bCreate1TS = micro.MicroPacket(pbPacket);
	if( bCreate1TS && buf.head() != NULL){
		auto head = buf.head(); init_head(head);
		auto sz = head->size() ;
		head->resize(sz+188);
		memcpy(head->data()+sz, micro.Get1TS(), 188);
		Lock(dwID);
		if( head->size() >= head->capacity() ){
			buf.push();
			if(buf.no_pool()) { // overflow
				buf.pull();
				overflow++;
				OutputDebugString(IdentStr(dwID,L" Buff Full").c_str());
			}else{
				overflow ^= overflow;
			}
			m_fDataCarry[dwID] = true;
		}
		UnLock(dwID);
	}
}

int CALLBACK CDataIO::OutsideCmdCallbackT0(void* pParam, CMD_STREAM* pCmdParam, CMD_STREAM* pResParam, BOOL* pbResDataAbandon)
{
	CDataIO* pSys = (CDataIO*)pParam;
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

int CALLBACK CDataIO::OutsideCmdCallbackT1(void* pParam, CMD_STREAM* pCmdParam, CMD_STREAM* pResParam, BOOL* pbResDataAbandon)
{
	CDataIO* pSys = (CDataIO*)pParam;
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

int CALLBACK CDataIO::OutsideCmdCallbackS0(void* pParam, CMD_STREAM* pCmdParam, CMD_STREAM* pResParam, BOOL* pbResDataAbandon)
{
	CDataIO* pSys = (CDataIO*)pParam;
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

int CALLBACK CDataIO::OutsideCmdCallbackS1(void* pParam, CMD_STREAM* pCmdParam, CMD_STREAM* pResParam, BOOL* pbResDataAbandon)
{
	CDataIO* pSys = (CDataIO*)pParam;
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

void CDataIO::CmdSendData(DWORD dwID, CMD_STREAM* pCmdParam, CMD_STREAM* pResParam, BOOL* pbResDataAbandon)
{
	if(!m_fDataCarry[dwID]) {
		pResParam->dwParam = CMD_ERR_BUSY;
		return;
	}

	pResParam->dwParam = CMD_SUCCESS;
	BOOL bSend = FALSE;

	if(Lock(dwID)) {
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
		UnLock(dwID);
	}

	if( bSend == FALSE ){
		pResParam->dwParam = CMD_ERR_BUSY;
	}
}

DWORD CDataIO::GetOverFlowCount(int iID)
{
	int iDevID = iID>>16;
	PT::Device::ISDB enISDB = (PT::Device::ISDB)((iID&0x0000FF00)>>8);
	uint iTuner = iID&0x000000FF;

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

// MemStreaming
UINT CDataIO::MemStreamingThreadProcMain(DWORD dwID)
{
	const DWORD CmdWait = 100;
	const size_t sln = wstring(SHAREDMEM_TRANSPORT_STREAM_SUFFIX).length();

	while (!m_bMemStreamingTerm) {

		auto tx = [&](PTBUFFER &buf, CSharedTransportStreamer *st) -> bool {
			bool res = false;
			if (Lock(dwID, CmdWait)) {
				if (st != NULL) {
					wstring mutexName = st->Name().substr(0, st->Name().length() - sln);
					if (HANDLE hMutex = OpenMutex(MUTEX_ALL_ACCESS, FALSE, mutexName.c_str())) {
						CloseHandle(hMutex);
						while ((res = !buf.empty()) != false) {
							auto p = buf.pull();
							auto data = p->data();
							auto size = p->size();
							UnLock(dwID);
							if (!st->Tx(data, (DWORD)size, CmdWait)) {
								buf.pull_undo();  return false;
							}
							if (m_bMemStreamingTerm || !Lock(dwID, CmdWait))
								return res;
						}
					}
				}
				UnLock(dwID);
			}
			return res;
		};

		if (m_fDataCarry[dwID]) {
			if (!tx(Buff(dwID), MemStreamer(dwID)))
				m_fDataCarry[dwID] = false;
		}

		if (!m_fDataCarry[dwID])
			Sleep(MemStreamer(dwID) ? 10 : CmdWait);
	}

	return 0;
}

// MemStreaming
UINT WINAPI CDataIO::MemStreamingThreadProc(LPVOID pParam)
{
	auto p = static_cast<MEMSTREAMINGTHREAD_PARAM*>(pParam);
	CDataIO* pSys = p->pSys;
	DWORD dwID = p->dwID ;
	delete p;

	HANDLE hCurThread = GetCurrentThread();
	SetThreadPriority(hCurThread, THREAD_PRIORITY_HIGHEST);

	return pSys->MemStreamingThreadProcMain(dwID);
}

// MemStreaming
void CDataIO::StartMemStreaming(DWORD dwID)
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

// MemStreaming
void CDataIO::StopMemStreaming()
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

void CDataIO::Flush(PTBUFFER &buf, BOOL dispose)
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

