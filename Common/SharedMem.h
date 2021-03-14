//===========================================================================
#pragma once
#ifndef _SHAREDMEM_20210313200203597_H_INCLUDED_
#define _SHAREDMEM_20210313200203597_H_INCLUDED_
//---------------------------------------------------------------------------

#include <string>
//===========================================================================
namespace PRY8EAlByw {
//---------------------------------------------------------------------------

  // CSharedMemory (プロセス間メモリ共有)

class CSharedMemory
{
private:
	std::wstring BaseName;
	HANDLE HMutex;
	HANDLE HMapping;
	LPVOID PMapView;
	DWORD SzMapView;
protected:
	LPVOID Memory() const { return PMapView; }
	bool Lock(DWORD timeout = INFINITE) const ;
	bool Unlock() const ;
protected:
	CSharedMemory(std::wstring name, DWORD size);
	virtual ~CSharedMemory();
public:
	std::wstring Name() const { return BaseName; }
	virtual bool IsValid() const ;
	virtual DWORD Read(LPVOID *dst, DWORD sz, DWORD pos, DWORD timeout=INFINITE) const ;
	virtual DWORD Write(const LPVOID *src, DWORD sz, DWORD pos, DWORD timeout=INFINITE);
	DWORD Size() const { return SzMapView; }
};

  // CSharedCmdOperator ( Bidirectional command operator )

class CSharedCmdOperator : public CSharedMemory
{
private:
	DWORD SzCmd; // Maximum command size
	LPVOID PCmdRecv, PCmdSend; // Pointer position of command RX/TX
	std::wstring NamSent ; // Sent event name
	HANDLE HEvSent ;  // Command sent event handle
	std::wstring NamListen ; // Listening event name
public:
	CSharedCmdOperator(std::wstring name, BOOL server, DWORD cmd_size,  DWORD extra_sz=0);
	virtual ~CSharedCmdOperator();
	bool IsValid() const { return CSharedMemory::IsValid() && HEvSent && HEvSent!=INVALID_HANDLE_VALUE ; }
	virtual BOOL Send(const LPVOID cmd, DWORD timeout=INFINITE) ;
	virtual BOOL Recv(LPVOID cmd, DWORD timeout=INFINITE) ;
	virtual BOOL Listen(DWORD timeout=INFINITE) { return WaitForCmd(timeout)==WAIT_OBJECT_0 ? TRUE : FALSE ; }
	virtual DWORD WaitForCmd(DWORD timeout=INFINITE) ;
	DWORD CmdSize() { return SzCmd ; }
};

  // CSharedPacketStreamer ( Unidirectional packet streamer )

class CSharedPacketStreamer : public CSharedCmdOperator
{
protected:
	DWORD SzPacket;  // Packet size
	DWORD NumPacket; // Packet count (Packet size x Packet count = Whole size)
	DWORD CurPacketSend, CurPacketRecv; // FIFO streaming cursor
	BOOL FReceiver; // TRUE: Receiver, FALSE: Transimitter
	DWORD PosPackets;  // Position of begining packets
	HANDLE *PacketMutex; // Mutexes for locking packets
	bool LockPacket(DWORD index, DWORD timeout = INFINITE) const ;
	bool UnlockPacket(DWORD index) const ;
public:
	CSharedPacketStreamer(std::wstring name, BOOL receiver, DWORD packet_sz,
		DWORD packet_num, DWORD extra_sz=0);
	virtual ~CSharedPacketStreamer();
	virtual BOOL Send(const LPVOID data, DWORD timeout=INFINITE) ;
	virtual BOOL Recv(LPVOID data, DWORD timeout=INFINITE) ;
	DWORD PacketSize() { return SzPacket ; }
	DWORD PacketCount() { return NumPacket ; }
	BOOL IsReceiver() { return FReceiver; }
	DWORD PacketRemain(DWORD timeout=INFINITE) ;
};

  // CSharedTransportStreamer ( Unidirectional packet streamer for async )

class CSharedTransportStreamer : public CSharedPacketStreamer
{
public:
	typedef BOOL (__stdcall *TXDIRECTFUNC)(LPVOID dst, DWORD &sz, PVOID arg);
private:
	CSharedPacketStreamer::Send ;
	CSharedPacketStreamer::Recv ;
	DWORD LastLockedRecvCur;
protected:
	DWORD PosSzPackets ; // Position of the begining packets' size lists
	DWORD TickLastTx ; // Tick count when the last Tx was done
public:
	CSharedTransportStreamer(std::wstring name, BOOL receiver, DWORD packet_sz,
		DWORD packet_num, DWORD extra_sz=0);
	virtual ~CSharedTransportStreamer();
	BOOL Tx(const LPVOID data, DWORD size, DWORD timeout=INFINITE) ;
	BOOL TxDirect(TXDIRECTFUNC Func, PVOID arg, DWORD timeout=INFINITE);
	BOOL Rx(LPVOID data, DWORD &size, DWORD timeout=INFINITE) ;
	DWORD LastTxAlive() { return TickLastTx; }
};

//---------------------------------------------------------------------------
} // End of namespace PRY8EAlByw
//===========================================================================
using namespace PRY8EAlByw ;
//===========================================================================
#endif // _SHAREDMEM_20210313200203597_H_INCLUDED_
