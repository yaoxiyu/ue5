// Copyright Epic Games, Inc. All Rights Reserved.

#include "Pch.h"
#include "AsioSocket.h"
#include "Foundation.h"

////////////////////////////////////////////////////////////////////////////////
FAsioSocket::FAsioSocket(asio::ip::tcp::socket& InSocket)
: Socket(MoveTemp(InSocket))
{
}

////////////////////////////////////////////////////////////////////////////////
FAsioSocket::~FAsioSocket()
{
	check(!IsOpen());
}

////////////////////////////////////////////////////////////////////////////////
asio::io_context& FAsioSocket::GetIoContext()
{
	return Socket.get_executor().context();
}

////////////////////////////////////////////////////////////////////////////////
bool FAsioSocket::IsOpen() const
{
	return Socket.is_open();
}

////////////////////////////////////////////////////////////////////////////////
bool FAsioSocket::HasDataAvailable() const
{
	return (Socket.available() > 0);
}

////////////////////////////////////////////////////////////////////////////////
uint32 FAsioSocket::GetRemoteAddress() const
{
	if (!IsOpen())
	{
		return 0;
	}

	const asio::ip::tcp::endpoint Endpoint = Socket.remote_endpoint();
	asio::ip::address Address = Endpoint.address();
	return Address.is_v4() ? Address.to_v4().to_uint() : 0;
}

////////////////////////////////////////////////////////////////////////////////
uint32 FAsioSocket::GetRemotePort() const
{
	if (!IsOpen())
	{
		return 0;
	}

	const asio::ip::tcp::endpoint Endpoint = Socket.remote_endpoint();
	return Endpoint.port();
}

////////////////////////////////////////////////////////////////////////////////
uint32 FAsioSocket::GetLocalPort() const
{
	if (!IsOpen())
	{
		return 0;
	}

	const asio::ip::tcp::endpoint Endpoint = Socket.local_endpoint();
	return Endpoint.port();
}

////////////////////////////////////////////////////////////////////////////////
void FAsioSocket::Close()
{
	Socket.close();
}

////////////////////////////////////////////////////////////////////////////////
bool FAsioSocket::IsLocalConnection() const
{
	return Socket.local_endpoint().address() == Socket.remote_endpoint().address();
}

////////////////////////////////////////////////////////////////////////////////
bool FAsioSocket::Read(void* Dest, uint32 Size, FAsioIoSink* Sink, uint32 Id)
{
	if (!SetSink(Sink, Id))
	{
		return false;
	}

	asio::async_read(
		Socket,
		asio::buffer(Dest, Size),
		[this] (const asio::error_code& ErrorCode, size_t BytesReceived)
		{
			OnIoComplete(ErrorCode, uint32(BytesReceived));
		}
	);

	return true;
}

////////////////////////////////////////////////////////////////////////////////
bool FAsioSocket::ReadSome(void* Dest, uint32 BufferSize, FAsioIoSink* Sink, uint32 Id)
{
	if (!SetSink(Sink, Id))
	{
		return false;
	}

	Socket.async_receive(
		asio::buffer(Dest, BufferSize),
		[this] (const asio::error_code& ErrorCode, size_t BytesReceived)
		{
			return OnIoComplete(ErrorCode, uint32(BytesReceived));
		}
	);

	return true;
}

////////////////////////////////////////////////////////////////////////////////
bool FAsioSocket::Write(const void* Src, uint32 Size, FAsioIoSink* Sink, uint32 Id)
{
	if (!SetSink(Sink, Id))
	{
		return false;
	}

	asio::async_write(
		Socket,
		asio::buffer(Src, Size),
		[this] (const asio::error_code& ErrorCode, size_t BytesSent)
		{
			return OnIoComplete(ErrorCode, uint32(BytesSent));
		}
	);

	return true;
}

/* vim: set noexpandtab : */
