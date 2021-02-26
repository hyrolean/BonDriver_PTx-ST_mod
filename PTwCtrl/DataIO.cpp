#include "StdAfx.h"
#include "DataIO.h"
#include <process.h>

#define DATA_BUFF_SIZE 188*256
#define MAX_DATA_BUFF_COUNT 500

CDataIO::CDataIO(void)
{
	VIRTUAL_COUNT = 8;

	m_hStopEvent = _CreateEvent(FALSE, FALSE, NULL);
	m_hThread = NULL;

	m_pcDevice = NULL;

	mQuit = false;

	m_T0SetBuff = NULL;
	m_T1SetBuff = NULL;
	m_S0SetBuff = NULL;
	m_S1SetBuff = NULL;

	for( int i=0; i<4; i++ ){
#ifndef USE_DEQUE
		BUFF_DATA* pDataBuff = new BUFF_DATA;
		pDataBuff->dwSize = DATA_BUFF_SIZE;
		pDataBuff->pbBuff = new BYTE[DATA_BUFF_SIZE];
#else
		BUFF_DATA *pDataBuff = new BUFF_DATA(DATA_BUFF_SIZE);
#endif
		switch(i){
			case 0:
				m_T0SetBuff = pDataBuff;
				break;
			case 1:
				m_T1SetBuff = pDataBuff;
				break;
			case 2:
				m_S0SetBuff = pDataBuff;
				break;
			case 3:
				m_S1SetBuff = pDataBuff;
				break;
		}
	}

	m_hEvent1 = _CreateEvent(FALSE, TRUE, NULL );
	m_hEvent2 = _CreateEvent(FALSE, TRUE, NULL );
	m_hEvent3 = _CreateEvent(FALSE, TRUE, NULL );
	m_hEvent4 = _CreateEvent(FALSE, TRUE, NULL );

	m_dwT0OverFlowCount = 0;
	m_dwT1OverFlowCount = 0;
	m_dwS0OverFlowCount = 0;
	m_dwS1OverFlowCount = 0;

	m_bDMABuff = NULL;
}

CDataIO::~CDataIO(void)
{
	if( m_hThread != NULL ){
		::SetEvent(m_hStopEvent);
		// スレッド終了待ち
		if ( ::WaitForSingleObject(m_hThread, 15000) == WAIT_TIMEOUT ){
			::TerminateThread(m_hThread, 0xffffffff);
		}
		CloseHandle(m_hThread);
		m_hThread = NULL;
	}
	if( m_hStopEvent != NULL ){
		::CloseHandle(m_hStopEvent);
		m_hStopEvent = NULL;
	}

	SAFE_DELETE(m_T0SetBuff);
	SAFE_DELETE(m_T1SetBuff);
	SAFE_DELETE(m_S0SetBuff);
	SAFE_DELETE(m_S1SetBuff);

#ifndef USE_DEQUE
	for( int i=0; i<(int)m_T0Buff.size(); i++ ){
		SAFE_DELETE(m_T0Buff[i]);
	}
	for( int i=0; i<(int)m_T1Buff.size(); i++ ){
		SAFE_DELETE(m_T1Buff[i]);
	}
	for( int i=0; i<(int)m_S0Buff.size(); i++ ){
		SAFE_DELETE(m_S0Buff[i]);
	}
	for( int i=0; i<(int)m_S1Buff.size(); i++ ){
		SAFE_DELETE(m_S1Buff[i]);
	}
#else
	while (!m_T0Buff.empty()){
		BUFF_DATA *p = m_T0Buff.front();
		m_T0Buff.pop_front();
		delete p;
	}
	while (!m_T1Buff.empty()){
		BUFF_DATA *p = m_T1Buff.front();
		m_T1Buff.pop_front();
		delete p;
	}
	while (!m_S0Buff.empty()){
		BUFF_DATA *p = m_S0Buff.front();
		m_S0Buff.pop_front();
		delete p;
	}
	while (!m_S1Buff.empty()){
		BUFF_DATA *p = m_S1Buff.front();
		m_S1Buff.pop_front();
		delete p;
	}
#endif
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

void CDataIO::Lock1()
{
	if( m_hEvent1 == NULL ){
		return ;
	}
	if( WaitForSingleObject(m_hEvent1, 10*1000) == WAIT_TIMEOUT ){
		OutputDebugString(L"time out1");
	}
}

void CDataIO::UnLock1()
{
	if( m_hEvent1 != NULL ){
		SetEvent(m_hEvent1);
	}
}

void CDataIO::Lock2()
{
	if( m_hEvent2 == NULL ){
		return ;
	}
	if( WaitForSingleObject(m_hEvent2, 10*1000) == WAIT_TIMEOUT ){
		OutputDebugString(L"time out2");
	}
}

void CDataIO::UnLock2()
{
	if( m_hEvent2 != NULL ){
		SetEvent(m_hEvent2);
	}
}

void CDataIO::Lock3()
{
	if( m_hEvent3 == NULL ){
		return ;
	}
	if( WaitForSingleObject(m_hEvent3, 10*1000) == WAIT_TIMEOUT ){
		OutputDebugString(L"time out3");
	}
}

void CDataIO::UnLock3()
{
	if( m_hEvent3 != NULL ){
		SetEvent(m_hEvent3);
	}
}

void CDataIO::Lock4()
{
	if( m_hEvent4 == NULL ){
		return ;
	}
	if( WaitForSingleObject(m_hEvent4, 10*1000) == WAIT_TIMEOUT ){
		OutputDebugString(L"time out4");
	}
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
#ifndef USE_DEQUE
			SAFE_DELETE(m_T0SetBuff);
			for( int i=0; i<(int)m_T0Buff.size(); i++ ){
				SAFE_DELETE(m_T0Buff[i]);
			}
			m_T0Buff.clear();
#else
			m_T0SetBuff->dwSetSize = 0;
			while (!m_T0Buff.empty()){
				BUFF_DATA *p = m_T0Buff.front();
				m_T0Buff.pop_front();
				delete p;
			}
#endif
			UnLock1();
		}else{
			Lock2();
			m_dwT1OverFlowCount = 0;
#ifndef USE_DEQUE
			SAFE_DELETE(m_T1SetBuff);
			for( int i=0; i<(int)m_T1Buff.size(); i++ ){
				SAFE_DELETE(m_T1Buff[i]);
			}
			m_T1Buff.clear();
#else
			m_T1SetBuff->dwSetSize = 0;
			while (!m_T1Buff.empty()){
				BUFF_DATA *p = m_T1Buff.front();
				m_T1Buff.pop_front();
				delete p;
			}
#endif
			UnLock2();
		}
	}else{
		if( iTuner == 0 ){
			Lock3();
			m_dwS0OverFlowCount = 0;
#ifndef USE_DEQUE
			SAFE_DELETE(m_S0SetBuff);
			for( int i=0; i<(int)m_S0Buff.size(); i++ ){
				SAFE_DELETE(m_S0Buff[i]);
			}
			m_S0Buff.clear();
#else
			m_S0SetBuff->dwSetSize = 0;
			while (!m_S0Buff.empty()){
				BUFF_DATA *p = m_S0Buff.front();
				m_S0Buff.pop_front();
				delete p;
			}
#endif
			UnLock3();
		}else{
			Lock4();
			m_dwS1OverFlowCount = 0;
#ifndef USE_DEQUE
			SAFE_DELETE(m_S1SetBuff);
			for( int i=0; i<(int)m_S1Buff.size(); i++ ){
				SAFE_DELETE(m_S1Buff[i]);
			}
			m_S1Buff.clear();
#else
			m_S1SetBuff->dwSetSize = 0;
			while (!m_S1Buff.empty()){
				BUFF_DATA *p = m_S1Buff.front();
				m_S1Buff.pop_front();
				delete p;
			}
#endif
			UnLock4();
		}
	}
}

void CDataIO::Run()
{
	if( m_hThread != NULL ){
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
	SetThreadPriority( m_hThread, THREAD_PRIORITY_ABOVE_NORMAL );
	ResumeThread(m_hThread);
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
	if( m_hThread != NULL ){
		mQuit = true;
		::SetEvent(m_hStopEvent);
		// スレッド終了待ち
		if ( ::WaitForSingleObject(m_hThread, 15000) == WAIT_TIMEOUT ){
			::TerminateThread(m_hThread, 0xffffffff);
		}
		CloseHandle(m_hThread);
		m_hThread = NULL;
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
	if( bEnable ){
		if( enISDB == PT::Device::ISDB_T ){
			if( iTuner == 0 ){
#ifndef USE_DEQUE
				if( m_T0SetBuff == NULL ){
					Lock1();
					BUFF_DATA* pDataBuff = new BUFF_DATA;
					pDataBuff->dwSize = DATA_BUFF_SIZE;
					pDataBuff->pbBuff = new BYTE[DATA_BUFF_SIZE];
					m_T0SetBuff = pDataBuff;
					m_dwT0OverFlowCount = 0;
					UnLock1();
				}
#endif
				m_cPipeT0.StartServer(strEvent.c_str(), strPipe.c_str(), OutsideCmdCallbackT0, this, THREAD_PRIORITY_ABOVE_NORMAL);
			}else{
#ifndef USE_DEQUE
				if( m_T1SetBuff == NULL ){
					Lock2();
					BUFF_DATA* pDataBuff = new BUFF_DATA;
					pDataBuff->dwSize = DATA_BUFF_SIZE;
					pDataBuff->pbBuff = new BYTE[DATA_BUFF_SIZE];
					m_T1SetBuff = pDataBuff;
					m_dwT1OverFlowCount = 0;
					UnLock2();
				}
#endif
				m_cPipeT1.StartServer(strEvent.c_str(), strPipe.c_str(), OutsideCmdCallbackT1, this, THREAD_PRIORITY_ABOVE_NORMAL);
			}
		}else{
			if( iTuner == 0 ){
#ifndef USE_DEQUE
				if( m_S0SetBuff == NULL ){
					Lock3();
					BUFF_DATA* pDataBuff = new BUFF_DATA;
					pDataBuff->dwSize = DATA_BUFF_SIZE;
					pDataBuff->pbBuff = new BYTE[DATA_BUFF_SIZE];
					m_S0SetBuff = pDataBuff;
					m_dwS0OverFlowCount = 0;
					UnLock3();
				}
#endif
				m_cPipeS0.StartServer(strEvent.c_str(), strPipe.c_str(), OutsideCmdCallbackS0, this, THREAD_PRIORITY_ABOVE_NORMAL);
			}else{
#ifndef USE_DEQUE
				if( m_S1SetBuff == NULL ){
					Lock4();
					BUFF_DATA* pDataBuff = new BUFF_DATA;
					pDataBuff->dwSize = DATA_BUFF_SIZE;
					pDataBuff->pbBuff = new BYTE[DATA_BUFF_SIZE];
					m_S1SetBuff = pDataBuff;
					m_dwS1OverFlowCount = 0;
					UnLock4();
				}
#endif
				m_cPipeS1.StartServer(strEvent.c_str(), strPipe.c_str(), OutsideCmdCallbackS1, this, THREAD_PRIORITY_ABOVE_NORMAL);
			}
		}
	}else{
		if( enISDB == PT::Device::ISDB_T ){
			if( iTuner == 0 ){
				m_cPipeT0.StopServer();
				Lock1();
				m_dwT0OverFlowCount = 0;
#ifndef USE_DEQUE
				SAFE_DELETE(m_T0SetBuff);
				for( int i=0; i<(int)m_T0Buff.size(); i++ ){
					SAFE_DELETE(m_T0Buff[i]);
				}
				m_T0Buff.clear();
#else
				m_T0SetBuff->dwSetSize = 0;
				while (!m_T0Buff.empty()){
					BUFF_DATA *p = m_T0Buff.front();
					m_T0Buff.pop_front();
					delete p;
				}
#endif
				UnLock1();
			}else{
				m_cPipeT1.StopServer();
				Lock2();
				m_dwT1OverFlowCount = 0;
#ifndef USE_DEQUE
				SAFE_DELETE(m_T1SetBuff);
				for( int i=0; i<(int)m_T1Buff.size(); i++ ){
					SAFE_DELETE(m_T1Buff[i]);
				}
				m_T1Buff.clear();
#else
				m_T1SetBuff->dwSetSize = 0;
				while (!m_T1Buff.empty()){
					BUFF_DATA *p = m_T1Buff.front();
					m_T1Buff.pop_front();
					delete p;
				}
#endif
				UnLock2();
			}
		}else{
			if( iTuner == 0 ){
				m_cPipeS0.StopServer();
				Lock3();
				m_dwS0OverFlowCount = 0;
#ifndef USE_DEQUE
				SAFE_DELETE(m_S0SetBuff);
				for( int i=0; i<(int)m_S0Buff.size(); i++ ){
					SAFE_DELETE(m_S0Buff[i]);
				}
				m_S0Buff.clear();
#else
				m_S0SetBuff->dwSetSize = 0;
				while (!m_S0Buff.empty()){
					BUFF_DATA *p = m_S0Buff.front();
					m_S0Buff.pop_front();
					delete p;
				}
#endif
				UnLock3();
			}else{
				m_cPipeS1.StopServer();
				Lock4();
				m_dwS1OverFlowCount = 0;
#ifndef USE_DEQUE
				SAFE_DELETE(m_S1SetBuff);
				for( int i=0; i<(int)m_S1Buff.size(); i++ ){
					SAFE_DELETE(m_S1Buff[i]);
				}
				m_S1Buff.clear();
#else
				m_S1SetBuff->dwSetSize = 0;
				while (!m_S1Buff.empty()){
					BUFF_DATA *p = m_S1Buff.front();
					m_S1Buff.pop_front();
					delete p;
				}
#endif
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

	BOOL bCreate1TS = FALSE;
	switch(packetId){
		case 2:
			bCreate1TS = m_cT0Micro.MicroPacket(pbPacket);
			if( bCreate1TS && m_T0SetBuff != NULL){
				Lock1();
#ifndef USE_DEQUE
				if( m_T0SetBuff == NULL ){
					UnLock1();
					return ;
				}
#endif
				memcpy(m_T0SetBuff->pbBuff+m_T0SetBuff->dwSetSize, m_cT0Micro.Get1TS(), 188);
				m_T0SetBuff->dwSetSize+=188;
				if( m_T0SetBuff->dwSetSize >= m_T0SetBuff->dwSize ){
					m_T0Buff.push_back(m_T0SetBuff);

#ifndef USE_DEQUE
					BUFF_DATA* pDataBuff = new BUFF_DATA;
					pDataBuff->dwSize = DATA_BUFF_SIZE;
					pDataBuff->pbBuff = new BYTE[DATA_BUFF_SIZE];
					m_T0SetBuff = pDataBuff;
#else
					m_T0SetBuff = new BUFF_DATA(DATA_BUFF_SIZE);
#endif

					if( m_T0Buff.size() > MAX_DATA_BUFF_COUNT ){
#ifndef USE_DEQUE
						SAFE_DELETE(m_T0Buff[0]);
						m_T0Buff.erase(m_T0Buff.begin());
#else
						BUFF_DATA *p = m_T0Buff.front();
						m_T0Buff.pop_front();
						delete p;
#endif
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
			if( bCreate1TS && m_T1SetBuff != NULL){
				Lock2();
#ifndef USE_DEQUE
				if( m_T1SetBuff == NULL ){
					UnLock2();
					return ;
				}
#endif
				memcpy(m_T1SetBuff->pbBuff+m_T1SetBuff->dwSetSize, m_cT1Micro.Get1TS(), 188);
				m_T1SetBuff->dwSetSize+=188;
				if( m_T1SetBuff->dwSetSize >= m_T1SetBuff->dwSize ){
					m_T1Buff.push_back(m_T1SetBuff);

#ifndef USE_DEQUE
					BUFF_DATA* pDataBuff = new BUFF_DATA;
					pDataBuff->dwSize = DATA_BUFF_SIZE;
					pDataBuff->pbBuff = new BYTE[DATA_BUFF_SIZE];
					m_T1SetBuff = pDataBuff;
#else
					m_T1SetBuff = new BUFF_DATA(DATA_BUFF_SIZE);
#endif

					if( m_T1Buff.size() > MAX_DATA_BUFF_COUNT ){
#ifndef USE_DEQUE
						SAFE_DELETE(m_T1Buff[0]);
						m_T1Buff.erase(m_T1Buff.begin());
#else
						BUFF_DATA *p = m_T1Buff.front();
						m_T1Buff.pop_front();
						delete p;
#endif
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
			if( bCreate1TS && m_S0SetBuff != NULL){
				Lock3();
#ifndef USE_DEQUE
				if( m_S0SetBuff == NULL ){
					UnLock3();
					return ;
				}
#endif
				memcpy(m_S0SetBuff->pbBuff+m_S0SetBuff->dwSetSize, m_cS0Micro.Get1TS(), 188);
				m_S0SetBuff->dwSetSize+=188;
				if( m_S0SetBuff->dwSetSize >= m_S0SetBuff->dwSize ){
					m_S0Buff.push_back(m_S0SetBuff);

#ifndef USE_DEQUE
					BUFF_DATA* pDataBuff = new BUFF_DATA;
					pDataBuff->dwSize = DATA_BUFF_SIZE;
					pDataBuff->pbBuff = new BYTE[DATA_BUFF_SIZE];
					m_S0SetBuff = pDataBuff;
#else
					m_S0SetBuff = new BUFF_DATA(DATA_BUFF_SIZE);
#endif

					if( m_S0Buff.size() > MAX_DATA_BUFF_COUNT ){
#ifndef USE_DEQUE
						SAFE_DELETE(m_S0Buff[0]);
						m_S0Buff.erase(m_S0Buff.begin());
#else
						BUFF_DATA *p = m_S0Buff.front();
						m_S0Buff.pop_front();
						delete p;
#endif
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
			if( bCreate1TS && m_S1SetBuff != NULL){
				Lock4();
#ifndef USE_DEQUE
				if( m_S1SetBuff == NULL ){
					UnLock4();
					return ;
				}
#endif
				memcpy(m_S1SetBuff->pbBuff+m_S1SetBuff->dwSetSize, m_cS1Micro.Get1TS(), 188);
				m_S1SetBuff->dwSetSize+=188;
				if( m_S1SetBuff->dwSetSize >= m_S1SetBuff->dwSize ){
					m_S1Buff.push_back(m_S1SetBuff);

#ifndef USE_DEQUE
					BUFF_DATA* pDataBuff = new BUFF_DATA;
					pDataBuff->dwSize = DATA_BUFF_SIZE;
					pDataBuff->pbBuff = new BYTE[DATA_BUFF_SIZE];
					m_S1SetBuff = pDataBuff;
#else
					m_S1SetBuff = new BUFF_DATA(DATA_BUFF_SIZE);
#endif

					if( m_S1Buff.size() > MAX_DATA_BUFF_COUNT ){
#ifndef USE_DEQUE
						SAFE_DELETE(m_S1Buff[0]);
						m_S1Buff.erase(m_S1Buff.begin());
#else
						BUFF_DATA *p = m_S1Buff.front();
						m_S1Buff.pop_front();
						delete p;
#endif
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

int CALLBACK CDataIO::OutsideCmdCallbackT0(void* pParam, CMD_STREAM* pCmdParam, CMD_STREAM* pResParam)
{
	CDataIO* pSys = (CDataIO*)pParam;
	switch( pCmdParam->dwParam ){
		case CMD_SEND_DATA:
			pSys->CmdSendData(0, pCmdParam, pResParam);
			break;
		default:
			pResParam->dwParam = CMD_NON_SUPPORT;
			break;
	}
	return 0;
}

int CALLBACK CDataIO::OutsideCmdCallbackT1(void* pParam, CMD_STREAM* pCmdParam, CMD_STREAM* pResParam)
{
	CDataIO* pSys = (CDataIO*)pParam;
	switch( pCmdParam->dwParam ){
		case CMD_SEND_DATA:
			pSys->CmdSendData(1, pCmdParam, pResParam);
			break;
		default:
			pResParam->dwParam = CMD_NON_SUPPORT;
			break;
	}
	return 0;
}

int CALLBACK CDataIO::OutsideCmdCallbackS0(void* pParam, CMD_STREAM* pCmdParam, CMD_STREAM* pResParam)
{
	CDataIO* pSys = (CDataIO*)pParam;
	switch( pCmdParam->dwParam ){
		case CMD_SEND_DATA:
			pSys->CmdSendData(2, pCmdParam, pResParam);
			break;
		default:
			pResParam->dwParam = CMD_NON_SUPPORT;
			break;
	}
	return 0;
}

int CALLBACK CDataIO::OutsideCmdCallbackS1(void* pParam, CMD_STREAM* pCmdParam, CMD_STREAM* pResParam)
{
	CDataIO* pSys = (CDataIO*)pParam;
	switch( pCmdParam->dwParam ){
		case CMD_SEND_DATA:
			pSys->CmdSendData(3, pCmdParam, pResParam);
			break;
		default:
			pResParam->dwParam = CMD_NON_SUPPORT;
			break;
	}
	return 0;
}

void CDataIO::CmdSendData(DWORD dwID, CMD_STREAM* pCmdParam, CMD_STREAM* pResParam)
{
	pResParam->dwParam = CMD_SUCCESS;
	BOOL bSend = FALSE;

	switch(dwID){
		case 0:
			Lock1();
			if( m_T0Buff.size() > 0 ){
#ifndef USE_DEQUE
				pResParam->dwSize = m_T0Buff[0]->dwSize;
				pResParam->bData = new BYTE[pResParam->dwSize];
				memcpy(pResParam->bData, m_T0Buff[0]->pbBuff, pResParam->dwSize);
				SAFE_DELETE(m_T0Buff[0]);
				m_T0Buff.erase(m_T0Buff.begin());
#else
				BUFF_DATA *p = m_T0Buff.front();
				m_T0Buff.pop_front();
				pResParam->dwSize = p->dwSize;
				pResParam->bData = p->pbBuff;
				p->pbBuff = NULL;	// ポインタをコピーしてるのでdelete pで削除されないようにする
				delete p;
#endif
				bSend = TRUE;
			}
			UnLock1();
			break;
		case 1:
			Lock2();
			if( m_T1Buff.size() > 0 ){
#ifndef USE_DEQUE
				pResParam->dwSize = m_T1Buff[0]->dwSize;
				pResParam->bData = new BYTE[pResParam->dwSize];
				memcpy(pResParam->bData, m_T1Buff[0]->pbBuff, pResParam->dwSize);
				SAFE_DELETE(m_T1Buff[0]);
				m_T1Buff.erase(m_T1Buff.begin());
#else
				BUFF_DATA *p = m_T1Buff.front();
				m_T1Buff.pop_front();
				pResParam->dwSize = p->dwSize;
				pResParam->bData = p->pbBuff;
				p->pbBuff = NULL;	// ポインタをコピーしてるのでdelete pで削除されないようにする
				delete p;
#endif
				bSend = TRUE;
			}
			UnLock2();
			break;
		case 2:
			Lock3();
			if( m_S0Buff.size() > 0 ){
#ifndef USE_DEQUE
				pResParam->dwSize = m_S0Buff[0]->dwSize;
				pResParam->bData = new BYTE[pResParam->dwSize];
				memcpy(pResParam->bData, m_S0Buff[0]->pbBuff, pResParam->dwSize);
				SAFE_DELETE(m_S0Buff[0]);
				m_S0Buff.erase(m_S0Buff.begin());
#else
				BUFF_DATA *p = m_S0Buff.front();
				m_S0Buff.pop_front();
				pResParam->dwSize = p->dwSize;
				pResParam->bData = p->pbBuff;
				p->pbBuff = NULL;	// ポインタをコピーしてるのでdelete pで削除されないようにする
				delete p;
#endif
				bSend = TRUE;
			}
			UnLock3();
			break;
		case 3:
			Lock4();
			if( m_S1Buff.size() > 0 ){
#ifndef USE_DEQUE
				pResParam->dwSize = m_S1Buff[0]->dwSize;
				pResParam->bData = new BYTE[pResParam->dwSize];
				memcpy(pResParam->bData, m_S1Buff[0]->pbBuff, pResParam->dwSize);
				SAFE_DELETE(m_S1Buff[0]);
				m_S1Buff.erase(m_S1Buff.begin());
#else
				BUFF_DATA *p = m_S1Buff.front();
				m_S1Buff.pop_front();
				pResParam->dwSize = p->dwSize;
				pResParam->bData = p->pbBuff;
				p->pbBuff = NULL;	// ポインタをコピーしてるのでdelete pで削除されないようにする
				delete p;
#endif
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

