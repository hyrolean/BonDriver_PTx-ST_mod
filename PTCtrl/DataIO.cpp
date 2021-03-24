#include "StdAfx.h"
#include "DataIO.h"
#include <process.h>

#define DATA_BUFF_SIZE 188*256
#define INI_DATA_BUFF_COUNT 50
#define MAX_DATA_BUFF_COUNT 500

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

	// MemStreamer
	m_bMemStreaming = bMemStreaming ;
	m_bMemStreamingTerm = TRUE ;
	m_hMemStreamingThread = INVALID_HANDLE_VALUE ;
	m_T0MemStreamer = NULL ;
	m_T1MemStreamer = NULL ;
	m_S0MemStreamer = NULL ;
	m_S1MemStreamer = NULL ;
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

	// MemStreamer
	if( m_hMemStreamingThread != INVALID_HANDLE_VALUE) {
		// スレッド終了待ち
		m_bMemStreamingTerm = TRUE ;
		if ( ::WaitForSingleObject(m_hMemStreamingThread, 15000) == WAIT_TIMEOUT ){
			::TerminateThread(m_hMemStreamingThread, 0xffffffff);
		}
		CloseHandle(m_hMemStreamingThread) ;
		m_hMemStreamingThread = INVALID_HANDLE_VALUE ;
	}
	if(m_T0MemStreamer != NULL) {
		SAFE_DELETE(m_T0MemStreamer);
	}
	if(m_T1MemStreamer != NULL) {
		SAFE_DELETE(m_T1MemStreamer);
	}
	if(m_S0MemStreamer != NULL) {
		SAFE_DELETE(m_S0MemStreamer);
	}
	if(m_S1MemStreamer != NULL) {
		SAFE_DELETE(m_S1MemStreamer);
	}

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

	if( enISDB == PT::Device::ISDB_T ){
		if( iTuner == 0 ){
			Lock1();
			m_dwT0OverFlowCount = 0;
			Flush(m_T0Buff);
			UnLock1();
		}else{
			Lock2();
			m_dwT1OverFlowCount = 0;
			Flush(m_T1Buff);
			UnLock2();
		}
	}else{
		if( iTuner == 0 ){
			Lock3();
			m_dwS0OverFlowCount = 0;
			Flush(m_S0Buff);
			UnLock3();
		}else{
			Lock4();
			m_dwS1OverFlowCount = 0;
			Flush(m_S1Buff);
			UnLock4();
		}
	}
}

void CDataIO::Run()
{
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
	m_hThread = (HANDLE)_beginthreadex(NULL, 0, RecvThread, (LPVOID)this, CREATE_SUSPENDED, NULL);
	if(m_hThread != INVALID_HANDLE_VALUE) {
		SetThreadPriority( m_hThread, THREAD_PRIORITY_ABOVE_NORMAL );
		ResumeThread(m_hThread);
	}

	// MemStreamer
	if(m_bMemStreaming) {
		m_hMemStreamingThread = (HANDLE)_beginthreadex(NULL, 0, MemStreamingThread, (LPVOID)this, CREATE_SUSPENDED, NULL);
		if(m_hMemStreamingThread != INVALID_HANDLE_VALUE) {
			m_bMemStreamingTerm = FALSE;
			SetThreadPriority( m_hMemStreamingThread, THREAD_PRIORITY_ABOVE_NORMAL );
			ResumeThread(m_hMemStreamingThread);
		}
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

	// MemStreamer
	if( m_hMemStreamingThread != INVALID_HANDLE_VALUE) {
		// スレッド終了待ち
		m_bMemStreamingTerm = TRUE ;
		if ( ::WaitForSingleObject(m_hMemStreamingThread, 15000) == WAIT_TIMEOUT ){
			::TerminateThread(m_hMemStreamingThread, 0xffffffff);
		}
		CloseHandle(m_hMemStreamingThread) ;
		m_hMemStreamingThread = INVALID_HANDLE_VALUE ;
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
}

void CDataIO::EnableTuner(int iID, BOOL bEnable)
{
	int iDevID = iID>>16;
	PT::Device::ISDB enISDB = (PT::Device::ISDB)((iID&0x0000FF00)>>8);
	uint iTuner = iID&0x000000FF;

	wstring strPipe = L"";
	wstring strEvent = L"";
	Format(strPipe, L"%s%d", CMD_PT1_DATA_PIPE, iID );
	Format(strEvent, L"%s%d", CMD_PT1_DATA_EVENT_WAIT_CONNECT, iID );

	// MemStreamer
	wstring strStreamerName;
	Format(strStreamerName, SHAREDMEM_TRANSPORT_STREAM_FORMAT, PT_VER, iID);

	if( bEnable ){
		if( enISDB == PT::Device::ISDB_T ){
			if( iTuner == 0 ){
				Lock1();
				m_dwT0OverFlowCount = 0;
				Flush(m_T0Buff, TRUE);
				if(m_bMemStreaming&&m_T0MemStreamer==NULL)
					m_T0MemStreamer = new CSharedTransportStreamer(
						strStreamerName, FALSE, SHAREDMEM_TRANSPORT_PACKET_SIZE,
						SHAREDMEM_TRANSPORT_PACKET_NUM);
				UnLock1();
				if(!m_bMemStreaming)
					m_cPipeT0.StartServer(strEvent.c_str(), strPipe.c_str(), OutsideCmdCallbackT0, this, THREAD_PRIORITY_ABOVE_NORMAL);
			}else{
				Lock2();
				m_dwT1OverFlowCount = 0;
				Flush(m_T1Buff, TRUE);
				if(m_bMemStreaming&&m_T1MemStreamer==NULL)
					m_T1MemStreamer = new CSharedTransportStreamer(
						strStreamerName, FALSE, SHAREDMEM_TRANSPORT_PACKET_SIZE,
						SHAREDMEM_TRANSPORT_PACKET_NUM);
				UnLock2();
				if(!m_bMemStreaming)
					m_cPipeT1.StartServer(strEvent.c_str(), strPipe.c_str(), OutsideCmdCallbackT1, this, THREAD_PRIORITY_ABOVE_NORMAL);
			}
		}else{
			if( iTuner == 0 ){
				Lock3();
				m_dwS0OverFlowCount = 0;
				Flush(m_S0Buff, TRUE);
				if(m_bMemStreaming&&m_S0MemStreamer==NULL)
					m_S0MemStreamer = new CSharedTransportStreamer(
						strStreamerName, FALSE, SHAREDMEM_TRANSPORT_PACKET_SIZE,
						SHAREDMEM_TRANSPORT_PACKET_NUM);
				UnLock3();
				if(!m_bMemStreaming)
					m_cPipeS0.StartServer(strEvent.c_str(), strPipe.c_str(), OutsideCmdCallbackS0, this, THREAD_PRIORITY_ABOVE_NORMAL);
			}else{
				Lock4();
				m_dwS1OverFlowCount = 0;
				Flush(m_S1Buff, TRUE);
				if(m_bMemStreaming&&m_S1MemStreamer==NULL)
					m_S1MemStreamer = new CSharedTransportStreamer(
						strStreamerName, FALSE, SHAREDMEM_TRANSPORT_PACKET_SIZE,
						SHAREDMEM_TRANSPORT_PACKET_NUM);
				UnLock4();
				if(!m_bMemStreaming)
					m_cPipeS1.StartServer(strEvent.c_str(), strPipe.c_str(), OutsideCmdCallbackS1, this, THREAD_PRIORITY_ABOVE_NORMAL);
			}
		}
	}else{ // Disable
		if( enISDB == PT::Device::ISDB_T ){
			if( iTuner == 0 ){
				if(!m_bMemStreaming) m_cPipeT0.StopServer();
				Lock1();
				if(m_bMemStreaming&&m_T0MemStreamer)
					SAFE_DELETE(m_T0MemStreamer);
				m_dwT0OverFlowCount = 0;
				Flush(m_T0Buff);
				UnLock1();
			}else{
				if(!m_bMemStreaming) m_cPipeT1.StopServer();
				Lock2();
				if(m_bMemStreaming&&m_T1MemStreamer)
					SAFE_DELETE(m_T1MemStreamer);
				m_dwT1OverFlowCount = 0;
				Flush(m_T1Buff);
				UnLock2();
			}
		}else{
			if( iTuner == 0 ){
				if(!m_bMemStreaming) m_cPipeS0.StopServer();
				Lock3();
				if(m_bMemStreaming&&m_S0MemStreamer)
					SAFE_DELETE(m_S0MemStreamer);
				m_dwS0OverFlowCount = 0;
				Flush(m_S0Buff);
				UnLock3();
			}else{
				if(!m_bMemStreaming) m_cPipeS1.StopServer();
				Lock4();
				if(m_bMemStreaming&&m_S1MemStreamer)
					SAFE_DELETE(m_S1MemStreamer);
				m_dwS1OverFlowCount = 0;
				Flush(m_S1Buff);
				UnLock4();
			}
		}
	}
}

UINT WINAPI CDataIO::RecvThread(LPVOID pParam)
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
	switch(packetId){
		case 2:
			bCreate1TS = m_cT0Micro.MicroPacket(pbPacket);
			if( bCreate1TS && m_T0Buff.head() != NULL){
				Lock1();
				auto head = m_T0Buff.head(); init_head(head);
				auto sz = head->size() ;
				head->resize(sz+188);
				memcpy(head->data()+sz, m_cT0Micro.Get1TS(), 188);
				if( head->size() >= head->capacity() ){
					m_T0Buff.push();
					if(m_T0Buff.no_pool()) { // overflow
						m_T0Buff.pull();
						m_dwT0OverFlowCount++;
						OutputDebugString(L"T0 Buff Full");
					}else{
						m_dwT0OverFlowCount = 0;
					}
				}
				UnLock1();
			}
			break;
		case 4:
			bCreate1TS = m_cT1Micro.MicroPacket(pbPacket);
			if( bCreate1TS && m_T1Buff.head() != NULL){
				Lock2();
				auto head = m_T1Buff.head(); init_head(head);
				auto sz = head->size() ;
				head->resize(sz+188);
				memcpy(head->data()+sz, m_cT1Micro.Get1TS(), 188);
				if( head->size() >= head->capacity() ){
					m_T1Buff.push();
					if(m_T1Buff.no_pool()) { // overflow
						m_T1Buff.pull();
						m_dwT1OverFlowCount++;
						OutputDebugString(L"T1 Buff Full");
					}else{
						m_dwT1OverFlowCount = 0;
					}
				}
				UnLock2();
			}
			break;
		case 1:
			bCreate1TS = m_cS0Micro.MicroPacket(pbPacket);
			if( bCreate1TS && m_S0Buff.head() != NULL){
				Lock3();
				auto head = m_S0Buff.head(); init_head(head);
				auto sz = head->size() ;
				head->resize(sz+188);
				memcpy(head->data()+sz, m_cS0Micro.Get1TS(), 188);
				if( head->size() >= head->capacity() ){
					m_S0Buff.push();
					if(m_S0Buff.no_pool()) { // overflow
						m_S0Buff.pull();
						m_dwS0OverFlowCount++;
						OutputDebugString(L"S0 Buff Full");
					}else{
						m_dwS0OverFlowCount = 0;
					}
				}
				UnLock3();
			}
			break;
		case 3:
			bCreate1TS = m_cS1Micro.MicroPacket(pbPacket);
			if( bCreate1TS && m_S1Buff.head() != NULL){
				Lock4();
				auto head = m_S1Buff.head(); init_head(head);
				auto sz = head->size() ;
				head->resize(sz+188);
				memcpy(head->data()+sz, m_cS1Micro.Get1TS(), 188);
				if( head->size() >= head->capacity() ){
					m_S1Buff.push();
					if(m_S1Buff.no_pool()) { // overflow
						m_S1Buff.pull();
						m_dwS1OverFlowCount++;
						OutputDebugString(L"S1 Buff Full");
					}else{
						m_dwS1OverFlowCount = 0;
					}
				}
				UnLock4();
			}
			break;
		default:
			return;
			break;
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
	pResParam->dwParam = CMD_SUCCESS;
	BOOL bSend = FALSE;

	switch(dwID){
		case 0:
			Lock1();
			if( m_T0Buff.size() > 0 ){
				auto p = m_T0Buff.pull();
				pResParam->dwSize = (DWORD)p->size();
				pResParam->bData = p->data();
				*pbResDataAbandon = TRUE;
				bSend = TRUE;
			}
			UnLock1();
			break;
		case 1:
			Lock2();
			if( m_T1Buff.size() > 0 ){
				auto p = m_T1Buff.pull();
				pResParam->dwSize = (DWORD)p->size();
				pResParam->bData = p->data();
				*pbResDataAbandon = TRUE;
				bSend = TRUE;
			}
			UnLock2();
			break;
		case 2:
			Lock3();
			if( m_S0Buff.size() > 0 ){
				auto p = m_S0Buff.pull();
				pResParam->dwSize = (DWORD)p->size();
				pResParam->bData = p->data();
				*pbResDataAbandon = TRUE;
				bSend = TRUE;
			}
			UnLock3();
			break;
		case 3:
			Lock4();
			if( m_S1Buff.size() > 0 ){
				auto p = m_S1Buff.pull();
				pResParam->dwSize = (DWORD)p->size();
				pResParam->bData = p->data();
				*pbResDataAbandon = TRUE;
				bSend = TRUE;
			}
			UnLock4();
			break;
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

// MemStreamer
UINT CDataIO::MemStreamingThreadMain()
{
	const DWORD CmdWait = 50 ;

	while (!m_bMemStreamingTerm) {
		int cnt=0;

		if(Lock1(CmdWait)) {
			if(!m_T0Buff.empty()) {
				if(m_T0MemStreamer!=NULL) {
					auto p = m_T0Buff.pull() ;
					if(!m_T0MemStreamer->Tx(p->data(),(DWORD)p->size(),CmdWait))
						m_T0Buff.pull_undo();
					if(!m_T0Buff.empty()) cnt++;
				}
			}
			UnLock1();
		}else cnt++;

		if(Lock2(CmdWait)){
			if(!m_T1Buff.empty()) {
				if(m_T1MemStreamer!=NULL) {
					auto p = m_T1Buff.pull() ;
					if(!m_T1MemStreamer->Tx(p->data(),(DWORD)p->size(),CmdWait))
						m_T1Buff.pull_undo();
					if(!m_T1Buff.empty()) cnt++;
				}
			}
			UnLock2();
		}else cnt++;

		if(Lock3(CmdWait)) {
			if(!m_S0Buff.empty()) {
				if(m_S0MemStreamer!=NULL) {
					auto p = m_S0Buff.pull() ;
					if(!m_S0MemStreamer->Tx(p->data(),(DWORD)p->size(),CmdWait))
						m_S0Buff.pull_undo();
					if(!m_S0Buff.empty()) cnt++;
				}
			}
			UnLock3();
		}else cnt++;

		if(Lock4(CmdWait)) {
			if(!m_S1Buff.empty()) {
				if(m_S1MemStreamer!=NULL) {
					auto p = m_S1Buff.pull() ;
					if(!m_S1MemStreamer->Tx(p->data(),(DWORD)p->size(),CmdWait))
						m_S1Buff.pull_undo();
					if(!m_S1Buff.empty()) cnt++;
				}
			}
			UnLock4();
		}else cnt++;

		if(!cnt) Sleep(10);
	}

	return 0;
}

// MemStreamer
UINT WINAPI CDataIO::MemStreamingThread(LPVOID pParam)
{
	CDataIO* pSys = (CDataIO*)pParam;

	HANDLE hCurThread = GetCurrentThread();
	SetThreadPriority(hCurThread, THREAD_PRIORITY_HIGHEST);

	return pSys->MemStreamingThreadMain();
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

