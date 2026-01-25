/*
 * Copyright 2011, Axel Dörfler, axeld@pinc-software.de.
 * Distributed under the terms of the MIT License.
 */


#include <DatagramSocket.h>

#include <errno.h>


//#define TRACE_SOCKET
#ifdef TRACE_SOCKET
#	define TRACE(x...) printf(x)
#else
#	define TRACE(x...) ;
#endif


BDatagramSocket::BDatagramSocket()
{
}


BDatagramSocket::BDatagramSocket(const BNetworkAddress& peer, bigtime_t timeout)
{
	Connect(peer, timeout);
}


BDatagramSocket::BDatagramSocket(const BDatagramSocket& other)
	:
	BAbstractSocket(other)
{
}


BDatagramSocket::~BDatagramSocket()
{
}


status_t
BDatagramSocket::Bind(const BNetworkAddress& local, bool reuseAddr)
{
	return BAbstractSocket::Bind(local, reuseAddr, SOCK_DGRAM);
}


status_t
BDatagramSocket::Accept(BAbstractSocket*& _socket)
{
	return B_NOT_SUPPORTED;
}


status_t
BDatagramSocket::Connect(const BNetworkAddress& peer, bigtime_t timeout)
{
	return BAbstractSocket::Connect(peer, SOCK_DGRAM, timeout);
}


status_t
BDatagramSocket::SetBroadcast(bool broadcast)
{
	int value = broadcast ? 1 : 0;
#ifdef _WIN32
	if (setsockopt(fSocket, SOL_SOCKET, SO_BROADCAST, (const char*)&value, sizeof(value))
#else
	if (setsockopt(fSocket, SOL_SOCKET, SO_BROADCAST, &value, sizeof(value))
#endif
			!= 0)
		return errno;

	return B_OK;
}


void
BDatagramSocket::SetPeer(const BNetworkAddress& peer)
{
	fPeer = peer;
}


size_t
BDatagramSocket::MaxTransmissionSize() const
{
	// TODO: might vary on family!
	return 32768;
}


ssize_t
BDatagramSocket::SendTo(const BNetworkAddress& address, const void* buffer,
	size_t size)
{
#ifdef _WIN32
	ssize_t bytesSent = sendto(fSocket, (const char*)buffer, size, 0, address,
		address.Length());
#else
	ssize_t bytesSent = sendto(fSocket, buffer, size, 0, address,
		address.Length());
#endif
	if (bytesSent < 0)
		return errno;

	return bytesSent;
}


ssize_t
BDatagramSocket::ReceiveFrom(void* buffer, size_t bufferSize,
	BNetworkAddress& from)
{
	socklen_t fromLength = sizeof(sockaddr_storage);
#ifdef _WIN32
	ssize_t bytesReceived = recvfrom(fSocket, (char*)buffer, bufferSize, 0,
		from, &fromLength);
#else
	ssize_t bytesReceived = recvfrom(fSocket, buffer, bufferSize, 0,
		from, &fromLength);
#endif
	if (bytesReceived < 0)
		return errno;

	return bytesReceived;
}


//	#pragma mark - BDataIO implementation


ssize_t
BDatagramSocket::Read(void* buffer, size_t size)
{
#ifdef _WIN32
	ssize_t bytesReceived = recv(Socket(), (char*)buffer, size, 0);
#else
	ssize_t bytesReceived = recv(Socket(), buffer, size, 0);
#endif
	if (bytesReceived < 0) {
		TRACE("%p: BSocket::Read() error: %s\n", this, strerror(errno));
		return errno;
	}

	return bytesReceived;
}


ssize_t
BDatagramSocket::Write(const void* buffer, size_t size)
{
	ssize_t bytesSent;

	if (!fIsConnected) {
#ifdef _WIN32
		bytesSent = sendto(Socket(), (const char*)buffer, size, 0, fPeer, fPeer.Length());
#else
		bytesSent = sendto(Socket(), buffer, size, 0, fPeer, fPeer.Length());
#endif
	} else {
#ifdef _WIN32
		bytesSent = send(Socket(), (const char*)buffer, size, 0);
#else
		bytesSent = send(Socket(), buffer, size, 0);
#endif
	}

	if (bytesSent < 0) {
		TRACE("%p: BDatagramSocket::Write() error: %s\n", this,
			strerror(errno));
		return errno;
	}

	return bytesSent;
}
