#include "StdAfx.h"
#include "DataIO.h"
#include <process.h>
#include <algorithm>

CDataIO::CDataIO(BOOL bMemStreaming)
  : CBaseIO(bMemStreaming)
{
	VIRTUAL_COUNT = 8;

	m_hStopEvent = _CreateEvent(FALSE, FALSE, NULL);
	m_hThread = INVALID_HANDLE_VALUE;

	m_pcDevice = NULL;

	mQuit = false;

	m_bDMABuff = NULL;
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

	if( m_hStopEvent != NULL ){
		::CloseHandle(m_hStopEvent);
		m_hStopEvent = NULL;
	}

	SAFE_DELETE_ARRAY(m_bDMABuff);
}

void CDataIO::ClearBuff(int iID)
{
	int iDevID = iID>>16;
	PT::Device::ISDB enISDB = (PT::Device::ISDB)((iID&0x0000FF00)>>8);
	uint iTuner = iID&0x000000FF;

	auto clear = [&](DWORD dwID) {
		BuffLock(dwID);
		OverFlowCount(dwID) = 0;
		Flush(Buff(dwID));
		BuffUnLock(dwID);
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
			BuffLock(dwID);
			OverFlowCount(dwID) = 0;
			Flush(Buff(dwID), TRUE);
			auto &st = MemStreamer(dwID) ;
			if(m_bMemStreaming&&st==NULL)
				st = new CSharedTransportStreamer(
					strStreamerName, FALSE, SHAREDMEM_TRANSPORT_PACKET_SIZE,
					SHAREDMEM_TRANSPORT_PACKET_NUM);
			BuffUnLock(dwID);
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
			BuffLock(dwID);
			m_fDataCarry[dwID] = false;
			auto &st = MemStreamer(dwID);
			SAFE_DELETE(st);
			OverFlowCount(dwID) = 0;
			Flush(Buff(dwID));
			BuffUnLock(dwID);
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

	const DWORD MAX_WAIT = 200 ;
	const DWORD MIN_WAIT = 0 ;
	const size_t MAX_AVG = 10 ;
	fixed_queue<DWORD> avg(MAX_AVG+1) ;
	DWORD sleepy=0 ;

	for(;avg.size()<MAX_AVG;sleepy+=avg.front())
		avg.push_front(MIN_WAIT);

	DWORD s=GetTickCount(), t=MIN_WAIT;
	while (!pSys->mQuit) {
		DWORD dwRes = WaitForSingleObject(pSys->m_hStopEvent, 0);
		if( dwRes == WAIT_OBJECT_0 ){
			//STOP
			break;
		}

		bool b;

		if(b=pSys->WaitBlock()) {
			avg.push_front(t);
			sleepy+=avg.front();
			t=MIN_WAIT, s=GetTickCount();
		}
		else
			t=std::min<DWORD>(GetTickCount()-s, MAX_WAIT);

		if(b) {
			pSys->CopyBlock();
			if(!pSys->DispatchBlock()) break;
		}

		while(avg.size()>MAX_AVG) {
			sleepy-=avg.back();
			avg.pop_back();
		}

		if(DWORD wait = avg.size()>0 ? DWORD(sleepy/avg.size()) : 0)
			Sleep(wait);
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
		#if 0
		if (Read(mVirtualIndex, mImageIndex, mBlockIndex) != 0) break;
		Sleep(3);
		#else
		return Read(mVirtualIndex, mImageIndex, mBlockIndex) != 0 ;
		#endif
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
		BuffLock(dwID);
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
		BuffUnLock(dwID);
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

