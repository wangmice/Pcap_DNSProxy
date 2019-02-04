﻿// This code is part of Pcap_DNSProxy
// Pcap_DNSProxy, a local DNS server based on WinPcap and LibPcap
// Copyright (C) 2012-2019 Chengr28
// 
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either
// version 2 of the License, or (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.


#include "TransportSecurity.h"

#if defined(ENABLE_TLS)
#if defined(PLATFORM_WIN)
//SSPI SChannel initializtion
bool SSPI_SChannelInitializtion(
	SSPI_HANDLE_TABLE &SSPI_Handle)
{
//Setup SChannel credentials
	SCHANNEL_CRED SChannelCredentials;
	memset(&SChannelCredentials, 0, sizeof(SChannelCredentials));
	SChannelCredentials.dwVersion = SCHANNEL_CRED_VERSION;

//TLS version selection
//Windows XP/2003 and Vista are not support TLS above 1.0.
#if !defined(PLATFORM_WIN_XP)
	if (Parameter.HTTP_CONNECT_TLS_Version == TLS_VERSION_SELECTION::VERSION_1_3)
		SChannelCredentials.grbitEnabledProtocols = SP_PROT_TLS1_3_CLIENT;
	else if (Parameter.HTTP_CONNECT_TLS_Version == TLS_VERSION_SELECTION::VERSION_1_2)
		SChannelCredentials.grbitEnabledProtocols = SP_PROT_TLS1_2_CLIENT;
	else if (Parameter.HTTP_CONNECT_TLS_Version == TLS_VERSION_SELECTION::VERSION_1_1)
		SChannelCredentials.grbitEnabledProtocols = SP_PROT_TLS1_1_CLIENT;
	else 
#endif
	if (Parameter.HTTP_CONNECT_TLS_Version == TLS_VERSION_SELECTION::VERSION_1_0)
		SChannelCredentials.grbitEnabledProtocols = SP_PROT_TLS1_0_CLIENT;

//TLS connection flags
	SChannelCredentials.dwFlags |= SCH_CRED_NO_DEFAULT_CREDS; //Attempting to automatically supply a certificate chain for client authentication.
#if !defined(PLATFORM_WIN_XP)
	SChannelCredentials.dwFlags |= SCH_USE_STRONG_CRYPTO; //Disable known weak cryptographic algorithms, cipher suites, and SSL/TLS protocol versions that may be otherwise enabled for better interoperability.
#endif
	if (Parameter.HTTP_CONNECT_TLS_Validation)
	{
		SChannelCredentials.dwFlags |= SCH_CRED_AUTO_CRED_VALIDATION; //Validate the received server certificate chain.
		SChannelCredentials.dwFlags |= SCH_CRED_REVOCATION_CHECK_CHAIN; //When validating a certificate chain, check all certificates for revocation.
	}
	else {
		SChannelCredentials.dwFlags |= SCH_CRED_IGNORE_NO_REVOCATION_CHECK; //When checking for revoked certificates, ignore CRYPT_E_NO_REVOCATION_CHECK errors.
		SChannelCredentials.dwFlags |= SCH_CRED_IGNORE_REVOCATION_OFFLINE; //When checking for revoked certificates, ignore CRYPT_E_REVOCATION_OFFLINE errors.
		SChannelCredentials.dwFlags |= SCH_CRED_MANUAL_CRED_VALIDATION; //Prevent Schannel from validating the received server certificate chain.
		SChannelCredentials.dwFlags |= SCH_CRED_NO_SERVERNAME_CHECK; //Prevent Schannel from comparing the supplied target name with the subject names in server certificates.
//		SChannelCredentials.dwFlags |= SCH_CRED_REVOCATION_CHECK_END_CERT; //When validating a certificate chain, check only the last certificate for revocation.
//		SChannelCredentials.dwFlags |= SCH_CRED_REVOCATION_CHECK_CHAIN_EXCLUDE_ROOT; //When validating a certificate chain, do not check the root for revocation.
	}

//Get client credentials handle.
	SSPI_Handle.LastReturnValue = AcquireCredentialsHandleW(
		nullptr, 
		const_cast<LPWSTR>(UNISP_NAME_W), 
		SECPKG_CRED_OUTBOUND, 
		nullptr, 
		&SChannelCredentials, 
		nullptr, 
		nullptr, 
		&SSPI_Handle.ClientCredentials, 
		nullptr);
	if (SSPI_Handle.LastReturnValue != SEC_E_OK)
	{
		PrintError(LOG_LEVEL_TYPE::LEVEL_3, LOG_ERROR_TYPE::TLS, L"SSPI get client credentials handle error", SSPI_Handle.LastReturnValue, nullptr, 0);
		return false;
	}

	return true;
}

//SSPI TLS handshake
bool SSPI_Handshake(
	SSPI_HANDLE_TABLE &SSPI_Handle, 
	std::vector<SOCKET_DATA> &SocketDataList, 
	std::vector<SOCKET_SELECTING_SERIAL_DATA> &SocketSelectingDataList, 
	std::vector<ssize_t> &ErrorCodeList)
{
//Socket data check
	if (SocketDataList.empty() || SocketSelectingDataList.empty() || ErrorCodeList.empty())
		return false;

//Initializtion
	std::array<SecBuffer, 1U> InputBufferSec{}, OutputBufferSec{};
	SecBufferDesc InputBufferDesc, OutputBufferDesc;
	memset(&InputBufferDesc, 0, sizeof(InputBufferDesc));
	memset(&OutputBufferDesc, 0, sizeof(OutputBufferDesc));
	auto InputBufferDescPointer = &InputBufferDesc;

//TLS ALPN extension buffer initializtion
	std::unique_ptr<uint8_t[]> InputBufferPointer(nullptr);
#if !defined(PLATFORM_WIN_XP)
	if (Parameter.HTTP_CONNECT_TLS_ALPN)
	{
	//The first 4 bytes will be an indicating number of bytes of data in the rest of the the buffer.
	//The next 4 bytes are an indicator that this buffer will contain ALPN data.
	//The next 2 bytes will be indicating the number of bytes used to list the preferred protocols.
	//The next 1 byte will be indicating the number of bytes used to the ALPN string.
		if (Parameter.HTTP_CONNECT_Version == HTTP_VERSION_SELECTION::VERSION_1)
		{
			auto InputBufferPointerTemp = std::make_unique<uint8_t[]>(sizeof(uint32_t) * 2U + sizeof(uint16_t) + sizeof(uint8_t) + strlen(HTTP_1_TLS_ALPN_STRING) + NULL_TERMINATE_LENGTH);
			memset(InputBufferPointerTemp.get(), 0, sizeof(uint32_t) * 2U + sizeof(uint16_t) + sizeof(uint8_t) + strlen(HTTP_1_TLS_ALPN_STRING) + NULL_TERMINATE_LENGTH);
			std::swap(InputBufferPointer, InputBufferPointerTemp);

		//TLS ALPN extension buffer settings
			*reinterpret_cast<uint32_t *>(InputBufferPointer.get()) = static_cast<const uint32_t>(sizeof(uint32_t) + sizeof(uint16_t) + sizeof(uint8_t) + strlen(HTTP_1_TLS_ALPN_STRING));
			*reinterpret_cast<uint32_t *>(InputBufferPointer.get() + sizeof(uint32_t)) = SecApplicationProtocolNegotiationExt_ALPN;
			*reinterpret_cast<uint16_t *>(InputBufferPointer.get() + sizeof(uint32_t) * 2U) = static_cast<const uint16_t>(sizeof(uint8_t) + strlen(HTTP_1_TLS_ALPN_STRING));
			*reinterpret_cast<uint8_t *>(InputBufferPointer.get() + sizeof(uint32_t) * 2U + sizeof(uint16_t)) = static_cast<const uint8_t>(strlen(HTTP_1_TLS_ALPN_STRING));
			memcpy_s(InputBufferPointer.get() + sizeof(uint32_t) * 2U + sizeof(uint16_t) + sizeof(uint8_t), strlen(HTTP_1_TLS_ALPN_STRING), HTTP_1_TLS_ALPN_STRING, strlen(HTTP_1_TLS_ALPN_STRING));
			InputBufferSec.at(0).pvBuffer = InputBufferPointer.get();
			InputBufferSec.at(0).BufferType = SECBUFFER_APPLICATION_PROTOCOLS;
			InputBufferSec.at(0).cbBuffer = static_cast<const unsigned long>(sizeof(uint32_t) * 2U + sizeof(uint16_t) + sizeof(uint8_t) + strlen(HTTP_1_TLS_ALPN_STRING));
		}
		else if (Parameter.HTTP_CONNECT_Version == HTTP_VERSION_SELECTION::VERSION_2)
		{
			auto InputBufferPointerTemp = std::make_unique<uint8_t[]>(sizeof(uint32_t) * 2U + sizeof(uint16_t) + sizeof(uint8_t) + strlen(HTTP_2_TLS_ALPN_STRING) + NULL_TERMINATE_LENGTH);
			memset(InputBufferPointerTemp.get(), 0, sizeof(uint32_t) * 2U + sizeof(uint16_t) + sizeof(uint8_t) + strlen(HTTP_2_TLS_ALPN_STRING) + NULL_TERMINATE_LENGTH);
			std::swap(InputBufferPointer, InputBufferPointerTemp);

		//TLS ALPN extension buffer settings
			*reinterpret_cast<uint32_t *>(InputBufferPointer.get()) = static_cast<const uint32_t>(sizeof(uint32_t) + sizeof(uint16_t) + sizeof(uint8_t) + strlen(HTTP_2_TLS_ALPN_STRING));
			*reinterpret_cast<uint32_t *>(InputBufferPointer.get() + sizeof(uint32_t)) = SecApplicationProtocolNegotiationExt_ALPN;
			*reinterpret_cast<uint16_t *>(InputBufferPointer.get() + sizeof(uint32_t) * 2U) = static_cast<const uint16_t>(sizeof(uint8_t) + strlen(HTTP_2_TLS_ALPN_STRING));
			*reinterpret_cast<uint8_t *>(InputBufferPointer.get() + sizeof(uint32_t) * 2U + sizeof(uint16_t)) = static_cast<const uint8_t>(strlen(HTTP_2_TLS_ALPN_STRING));
			memcpy_s(InputBufferPointer.get() + sizeof(uint32_t) * 2U + sizeof(uint16_t) + sizeof(uint8_t), strlen(HTTP_2_TLS_ALPN_STRING), HTTP_2_TLS_ALPN_STRING, strlen(HTTP_2_TLS_ALPN_STRING));
			InputBufferSec.at(0).pvBuffer = InputBufferPointer.get();
			InputBufferSec.at(0).BufferType = SECBUFFER_APPLICATION_PROTOCOLS;
			InputBufferSec.at(0).cbBuffer = static_cast<const unsigned long>(sizeof(uint32_t) * 2U + sizeof(uint16_t) + sizeof(uint8_t) + strlen(HTTP_2_TLS_ALPN_STRING));
		}
		else {
			return false;
		}

	//BufferDesc initializtion
		InputBufferDesc.cBuffers = 1U;
		InputBufferDesc.pBuffers = InputBufferSec.data();
		InputBufferDesc.ulVersion = SECBUFFER_VERSION;
	}
	else {
#endif
		InputBufferDescPointer = nullptr;
#if !defined(PLATFORM_WIN_XP)
	}
#endif

//Buffer initializtion
	OutputBufferSec.at(0).pvBuffer = nullptr;
	OutputBufferSec.at(0).BufferType = SECBUFFER_TOKEN;
	OutputBufferSec.at(0).cbBuffer = 0;
	OutputBufferDesc.cBuffers = 1U;
	OutputBufferDesc.pBuffers = OutputBufferSec.data();
	OutputBufferDesc.ulVersion = SECBUFFER_VERSION;
	SEC_WCHAR *SSPI_SNI = nullptr;
	if (Parameter.HTTP_CONNECT_TLS_SNI != nullptr && !Parameter.HTTP_CONNECT_TLS_SNI->empty())
		SSPI_SNI = reinterpret_cast<SEC_WCHAR *>(const_cast<wchar_t *>(Parameter.HTTP_CONNECT_TLS_SNI->c_str()));
	DWORD OutputFlags = 0;

//First handshake
	SSPI_Handle.InputFlags |= ISC_REQ_SEQUENCE_DETECT;
	SSPI_Handle.InputFlags |= ISC_REQ_REPLAY_DETECT;
	SSPI_Handle.InputFlags |= ISC_REQ_CONFIDENTIALITY;
	SSPI_Handle.InputFlags |= ISC_RET_EXTENDED_ERROR;
	SSPI_Handle.InputFlags |= ISC_REQ_ALLOCATE_MEMORY;
	SSPI_Handle.InputFlags |= ISC_REQ_STREAM;
	SSPI_Handle.LastReturnValue = InitializeSecurityContextW(
		&SSPI_Handle.ClientCredentials, 
		nullptr, 
		SSPI_SNI, 
		SSPI_Handle.InputFlags, 
		0, 
		0, 
		InputBufferDescPointer, 
		0, 
		&SSPI_Handle.ContextHandle, 
		&OutputBufferDesc, 
		&OutputFlags, 
		nullptr);
	if (SSPI_Handle.LastReturnValue != SEC_I_CONTINUE_NEEDED || OutputBufferSec.at(0).pvBuffer == nullptr || OutputBufferSec.at(0).cbBuffer < sizeof(tls_base_record))
	{
		if (OutputBufferSec.at(0).pvBuffer != nullptr)
			FreeContextBuffer(OutputBufferSec.at(0).pvBuffer);
		PrintError(LOG_LEVEL_TYPE::LEVEL_3, LOG_ERROR_TYPE::TLS, L"SSPI initialize security context error", SSPI_Handle.LastReturnValue, nullptr, 0);

		return false;
	}
	else {
		InputBufferPointer.reset();
		InputBufferDescPointer = nullptr;

	//Connect to server.
		auto RecvLen = SocketConnecting(IPPROTO_TCP, SocketDataList.front().Socket, reinterpret_cast<const sockaddr *>(&SocketDataList.front().SockAddr), SocketDataList.front().AddrLen, reinterpret_cast<const uint8_t *>(OutputBufferSec.at(0).pvBuffer), OutputBufferSec.at(0).cbBuffer);
		if (RecvLen == EXIT_FAILURE)
		{
			if (OutputBufferSec.at(0).pvBuffer != nullptr)
				FreeContextBuffer(OutputBufferSec.at(0).pvBuffer);
			PrintError(LOG_LEVEL_TYPE::LEVEL_3, LOG_ERROR_TYPE::NETWORK, L"TLS connecting error", 0, nullptr, 0);

			return false;
		}
		else if (RecvLen >= DNS_PACKET_MINSIZE)
		{
			SocketSelectingDataList.front().SendBuffer.reset();
			SocketSelectingDataList.front().SendSize = 0;
			SocketSelectingDataList.front().SendLen = 0;
		}

	//Buffer initializtion
		auto SendBuffer = std::make_unique<uint8_t[]>(static_cast<const size_t>(OutputBufferSec.at(0).cbBuffer) + MEMORY_RESERVED_BYTES);
		memset(SendBuffer.get(), 0, static_cast<const size_t>(OutputBufferSec.at(0).cbBuffer) + MEMORY_RESERVED_BYTES);
		memcpy_s(SendBuffer.get(), OutputBufferSec.at(0).cbBuffer, OutputBufferSec.at(0).pvBuffer, OutputBufferSec.at(0).cbBuffer);
		std::swap(SocketSelectingDataList.front().SendBuffer, SendBuffer);
		SocketSelectingDataList.front().SendSize = OutputBufferSec.at(0).cbBuffer;
		SocketSelectingDataList.front().SendLen = OutputBufferSec.at(0).cbBuffer;
		if (OutputBufferSec.at(0).pvBuffer != nullptr)
			FreeContextBuffer(OutputBufferSec.at(0).pvBuffer);
		OutputBufferSec.at(0).pvBuffer = nullptr;
		OutputBufferSec.at(0).cbBuffer = 0;
		SendBuffer.reset();

	//TLS handshake exchange
		SocketSelectingDataList.front().RecvBuffer.reset();
		SocketSelectingDataList.front().RecvSize = 0;
		SocketSelectingDataList.front().RecvLen = 0;
		RecvLen = SocketSelectingSerial(REQUEST_PROCESS_TYPE::TLS_HANDSHAKE, IPPROTO_TCP, SocketDataList, SocketSelectingDataList, ErrorCodeList);
		SocketSelectingDataList.front().SendBuffer.reset();
		SocketSelectingDataList.front().SendSize = 0;
		SocketSelectingDataList.front().SendLen = 0;
		if (RecvLen == EXIT_FAILURE || SocketSelectingDataList.front().RecvLen < sizeof(tls_base_record))
		{
			PrintError(LOG_LEVEL_TYPE::LEVEL_3, LOG_ERROR_TYPE::NETWORK, L"TLS request error", ErrorCodeList.front(), nullptr, 0);
			return false;
		}
	}

//TLS handshake loop and get TLS stream sizes.
	if (!SSPI_HandshakeLoop(SSPI_Handle, SocketDataList, SocketSelectingDataList, ErrorCodeList) || !SSPI_GetStreamSize(SSPI_Handle))
		return false;

	return true;
}

//SSPI TLS handshake loop
bool SSPI_HandshakeLoop(
	SSPI_HANDLE_TABLE &SSPI_Handle, 
	std::vector<SOCKET_DATA> &SocketDataList, 
	std::vector<SOCKET_SELECTING_SERIAL_DATA> &SocketSelectingDataList, 
	std::vector<ssize_t> &ErrorCodeList)
{
//Socket data check
	if (SocketDataList.empty() || SocketSelectingDataList.empty() || ErrorCodeList.empty())
		return false;

//Initializtion
	std::array<SecBuffer, 2U> InputBufferSec{};
	std::array<SecBuffer, 1U> OutputBufferSec{};
	SecBufferDesc InputBufferDesc, OutputBufferDesc;
	memset(&InputBufferDesc, 0, sizeof(InputBufferDesc));
	memset(&OutputBufferDesc, 0, sizeof(OutputBufferDesc));
	SSPI_Handle.LastReturnValue = SEC_I_CONTINUE_NEEDED;
	SEC_WCHAR *SSPI_SNI = nullptr;
	if (Parameter.HTTP_CONNECT_TLS_SNI != nullptr && !Parameter.HTTP_CONNECT_TLS_SNI->empty())
		SSPI_SNI = reinterpret_cast<SEC_WCHAR *>(const_cast<wchar_t *>(Parameter.HTTP_CONNECT_TLS_SNI->c_str()));
	DWORD OutputFlags = 0;
	size_t RecvLen = 0;

//Handshake loop exchange
	while (!GlobalRunningStatus.IsNeedExit)
	{
	//Reset parameters.
		SSPI_Handle.InputFlags |= ISC_REQ_SEQUENCE_DETECT;
		SSPI_Handle.InputFlags |= ISC_REQ_REPLAY_DETECT;
		SSPI_Handle.InputFlags |= ISC_REQ_CONFIDENTIALITY;
		SSPI_Handle.InputFlags |= ISC_RET_EXTENDED_ERROR;
		SSPI_Handle.InputFlags |= ISC_RET_ALLOCATED_MEMORY;
		SSPI_Handle.InputFlags |= ISC_REQ_STREAM;
		InputBufferSec.at(0).BufferType = SECBUFFER_TOKEN;
		InputBufferSec.at(0).pvBuffer = SocketSelectingDataList.front().RecvBuffer.get();
		InputBufferSec.at(0).cbBuffer = static_cast<const DWORD>(SocketSelectingDataList.front().RecvLen);
		InputBufferSec.at(1U).BufferType = SECBUFFER_EMPTY;
		InputBufferSec.at(1U).pvBuffer = nullptr;
		InputBufferSec.at(1U).cbBuffer = 0;
		OutputBufferSec.at(0).BufferType = SECBUFFER_TOKEN;
		OutputBufferSec.at(0).pvBuffer = nullptr;
		OutputBufferSec.at(0).cbBuffer = 0;
		InputBufferDesc.ulVersion = SECBUFFER_VERSION;
		InputBufferDesc.pBuffers = InputBufferSec.data();
		InputBufferDesc.cBuffers = 2U;
		OutputBufferDesc.ulVersion = SECBUFFER_VERSION;
		OutputBufferDesc.pBuffers = OutputBufferSec.data();
		OutputBufferDesc.cBuffers = 1U;

	//Initialize security context.
		SSPI_Handle.LastReturnValue = InitializeSecurityContextW(
			&SSPI_Handle.ClientCredentials, 
			&SSPI_Handle.ContextHandle, 
			SSPI_SNI, 
			SSPI_Handle.InputFlags, 
			0, 
			0, 
			&InputBufferDesc, 
			0, 
			nullptr, 
			&OutputBufferDesc, 
			&OutputFlags, 
			nullptr);
		if (SSPI_Handle.LastReturnValue == SEC_E_OK)
		{
			if (OutputBufferSec.at(0).pvBuffer != nullptr)
				FreeContextBuffer(OutputBufferSec.at(0).pvBuffer);

			break;
		}
		else if (SSPI_Handle.LastReturnValue == SEC_I_COMPLETE_NEEDED || SSPI_Handle.LastReturnValue == SEC_I_COMPLETE_AND_CONTINUE)
		{
		//Complete authentication token.
			SSPI_Handle.LastReturnValue = CompleteAuthToken(&SSPI_Handle.ContextHandle, &OutputBufferDesc);
			if (SSPI_Handle.LastReturnValue != SEC_E_OK)
			{
				if (OutputBufferSec.at(0).pvBuffer != nullptr)
					FreeContextBuffer(OutputBufferSec.at(0).pvBuffer);
				PrintError(LOG_LEVEL_TYPE::LEVEL_3, LOG_ERROR_TYPE::TLS, L"SSPI complete authentication token error", SSPI_Handle.LastReturnValue, nullptr, 0);

				return false;
			}

			if (OutputBufferSec.at(0).pvBuffer != nullptr)
				FreeContextBuffer(OutputBufferSec.at(0).pvBuffer);

			break;
		}
		else if (SSPI_Handle.LastReturnValue == SEC_I_CONTINUE_NEEDED)
		{
		//Buffer initializtion
			if (OutputBufferSec.at(0).pvBuffer != nullptr && OutputBufferSec.at(0).cbBuffer >= sizeof(tls_base_record))
			{
				auto SendBuffer = std::make_unique<uint8_t[]>(static_cast<const size_t>(OutputBufferSec.at(0).cbBuffer) + MEMORY_RESERVED_BYTES);
				memset(SendBuffer.get(), 0, static_cast<const size_t>(OutputBufferSec.at(0).cbBuffer) + MEMORY_RESERVED_BYTES);
				memcpy_s(SendBuffer.get(), OutputBufferSec.at(0).cbBuffer, OutputBufferSec.at(0).pvBuffer, OutputBufferSec.at(0).cbBuffer);
				std::swap(SocketSelectingDataList.front().SendBuffer, SendBuffer);
				SocketSelectingDataList.front().SendSize = OutputBufferSec.at(0).cbBuffer;
				SocketSelectingDataList.front().SendLen = OutputBufferSec.at(0).cbBuffer;
				if (OutputBufferSec.at(0).pvBuffer != nullptr)
					FreeContextBuffer(OutputBufferSec.at(0).pvBuffer);
				OutputBufferSec.at(0).pvBuffer = nullptr;
				OutputBufferSec.at(0).cbBuffer = 0;
			}
			else {
				continue;
			}
		}
		else {
			if (OutputBufferSec.at(0).pvBuffer != nullptr)
				FreeContextBuffer(OutputBufferSec.at(0).pvBuffer);
			PrintError(LOG_LEVEL_TYPE::LEVEL_3, LOG_ERROR_TYPE::TLS, L"SSPI initialize security context error", SSPI_Handle.LastReturnValue, nullptr, 0);

			return false;
		}

	//TLS handshake exchange
		SocketSelectingDataList.front().RecvBuffer.reset();
		SocketSelectingDataList.front().RecvSize = 0;
		SocketSelectingDataList.front().RecvLen = 0;
		RecvLen = SocketSelectingSerial(REQUEST_PROCESS_TYPE::TLS_HANDSHAKE, IPPROTO_TCP, SocketDataList, SocketSelectingDataList, ErrorCodeList);
		SocketSelectingDataList.front().SendBuffer.reset();
		SocketSelectingDataList.front().SendSize = 0;
		SocketSelectingDataList.front().SendLen = 0;
		if (RecvLen == EXIT_FAILURE || SocketSelectingDataList.front().RecvLen < sizeof(tls_base_record))
		{
			PrintError(LOG_LEVEL_TYPE::LEVEL_3, LOG_ERROR_TYPE::NETWORK, L"TLS request error", ErrorCodeList.front(), nullptr, 0);
			return false;
		}
	}

	return true;
}

//SSPI get the stream encryption sizes
bool SSPI_GetStreamSize(
	SSPI_HANDLE_TABLE &SSPI_Handle)
{
//Get the stream encryption sizes, this needs to be done once per connection.
	SSPI_Handle.LastReturnValue = QueryContextAttributesW(
		&SSPI_Handle.ContextHandle, 
		SECPKG_ATTR_STREAM_SIZES, 
		&SSPI_Handle.StreamSizes);
	if (FAILED(SSPI_Handle.LastReturnValue))
	{
		PrintError(LOG_LEVEL_TYPE::LEVEL_3, LOG_ERROR_TYPE::TLS, L"SSPI get stream encryption sizes error", SSPI_Handle.LastReturnValue, nullptr, 0);
		return false;
	}

	return true;
}

//SSPI encryption process
bool SSPI_EncryptPacket(
	SSPI_HANDLE_TABLE &SSPI_Handle, 
	std::vector<SOCKET_SELECTING_SERIAL_DATA> &SocketSelectingDataList)
{
//Socket data check
	if (SocketSelectingDataList.empty())
		return false;

//Send length check
	if (SocketSelectingDataList.front().SendLen >= SSPI_Handle.StreamSizes.cbMaximumMessage)
	{
		PrintError(LOG_LEVEL_TYPE::LEVEL_3, LOG_ERROR_TYPE::TLS, L"SSPI plaintext input length is too long", 0, nullptr, 0);
		return false;
	}

//Initializtion
	SecBufferDesc BufferDesc;
	memset(&BufferDesc, 0, sizeof(BufferDesc));
	std::array<SecBuffer, SSPI_SECURE_BUFFER_NUM> BufferSec{};

//Allocate a working buffer.
//The plaintext sent to EncryptMessage can never be more than 'Sizes.cbMaximumMessage', so a buffer size of Sizes.cbMaximumMessage plus the header and trailer sizes is sufficient for the longest message.
	auto SendBuffer = std::make_unique<uint8_t[]>(static_cast<const size_t>(SSPI_Handle.StreamSizes.cbHeader) + static_cast<const size_t>(SSPI_Handle.StreamSizes.cbMaximumMessage) + static_cast<const size_t>(SSPI_Handle.StreamSizes.cbTrailer) + MEMORY_RESERVED_BYTES);
	memset(SendBuffer.get(), 0, static_cast<const size_t>(SSPI_Handle.StreamSizes.cbHeader) + static_cast<const size_t>(SSPI_Handle.StreamSizes.cbMaximumMessage) + static_cast<const size_t>(SSPI_Handle.StreamSizes.cbTrailer) + MEMORY_RESERVED_BYTES);
	memcpy_s(SendBuffer.get() + static_cast<const size_t>(SSPI_Handle.StreamSizes.cbHeader), static_cast<const size_t>(SSPI_Handle.StreamSizes.cbMaximumMessage) + static_cast<const size_t>(SSPI_Handle.StreamSizes.cbTrailer), SocketSelectingDataList.front().SendBuffer.get(), SocketSelectingDataList.front().SendLen);
	BufferSec.at(0).BufferType = SECBUFFER_STREAM_HEADER;
	BufferSec.at(0).pvBuffer = SendBuffer.get();
	BufferSec.at(0).cbBuffer = SSPI_Handle.StreamSizes.cbHeader;
	BufferSec.at(1U).BufferType = SECBUFFER_DATA;
	BufferSec.at(1U).pvBuffer = SendBuffer.get() + SSPI_Handle.StreamSizes.cbHeader;
	BufferSec.at(1U).cbBuffer = static_cast<const DWORD>(SocketSelectingDataList.front().SendLen);
	BufferSec.at(2U).BufferType = SECBUFFER_STREAM_TRAILER;
	BufferSec.at(2U).pvBuffer = SendBuffer.get() + SSPI_Handle.StreamSizes.cbHeader + SocketSelectingDataList.front().SendLen;
	BufferSec.at(2U).cbBuffer = SSPI_Handle.StreamSizes.cbTrailer;
	BufferSec.at(3U).BufferType = SECBUFFER_EMPTY;
	BufferSec.at(3U).pvBuffer = nullptr;
	BufferSec.at(3U).cbBuffer = 0;
	BufferDesc.ulVersion = SECBUFFER_VERSION;
	BufferDesc.pBuffers = BufferSec.data();
	BufferDesc.cBuffers = SSPI_SECURE_BUFFER_NUM;

//Encrypt data.
	SSPI_Handle.LastReturnValue = EncryptMessage(
		&SSPI_Handle.ContextHandle, 
		0, 
		&BufferDesc, 
		0);
	if (FAILED(SSPI_Handle.LastReturnValue))
	{
		PrintError(LOG_LEVEL_TYPE::LEVEL_3, LOG_ERROR_TYPE::TLS, L"SSPI encrypt data error", SSPI_Handle.LastReturnValue, nullptr, 0);
		return false;
	}
	else {
		std::swap(SocketSelectingDataList.front().SendBuffer, SendBuffer);
		SocketSelectingDataList.front().SendLen += static_cast<const size_t>(SSPI_Handle.StreamSizes.cbHeader) + static_cast<const size_t>(SSPI_Handle.StreamSizes.cbTrailer);
		SocketSelectingDataList.front().SendSize = static_cast<const size_t>(SSPI_Handle.StreamSizes.cbHeader) + static_cast<const size_t>(SSPI_Handle.StreamSizes.cbMaximumMessage) + static_cast<const size_t>(SSPI_Handle.StreamSizes.cbTrailer);
	}

	return true;
}

//SSPI decryption process
bool SSPI_DecryptPacket(
	SSPI_HANDLE_TABLE &SSPI_Handle, 
	std::vector<SOCKET_SELECTING_SERIAL_DATA> &SocketSelectingDataList)
{
//Socket data check
	if (SocketSelectingDataList.empty())
		return false;

//Initializtion
	std::array<SecBuffer, SSPI_SECURE_BUFFER_NUM> BufferSec{};
	SecBufferDesc BufferDesc;
	memset(&BufferDesc, 0, sizeof(BufferDesc));
	BufferSec.at(0).pvBuffer = SocketSelectingDataList.front().RecvBuffer.get();
	BufferSec.at(0).cbBuffer = static_cast<const DWORD>(SocketSelectingDataList.front().RecvLen);
	BufferSec.at(0).BufferType = SECBUFFER_DATA;
	BufferSec.at(1U).BufferType = SECBUFFER_EMPTY;
	BufferSec.at(1U).pvBuffer = nullptr;
	BufferSec.at(1U).cbBuffer = 0;
	BufferSec.at(2U).BufferType = SECBUFFER_EMPTY;
	BufferSec.at(2U).pvBuffer = nullptr;
	BufferSec.at(2U).cbBuffer = 0;
	BufferSec.at(3U).BufferType = SECBUFFER_EMPTY;
	BufferSec.at(3U).pvBuffer = nullptr;
	BufferSec.at(3U).cbBuffer = 0;
	BufferDesc.ulVersion = SECBUFFER_VERSION;
	BufferDesc.pBuffers = BufferSec.data();
	BufferDesc.cBuffers = SSPI_SECURE_BUFFER_NUM;

//Decrypt data.
	SSPI_Handle.LastReturnValue = DecryptMessage(
		&SSPI_Handle.ContextHandle, 
		&BufferDesc, 
		0, 
		nullptr);
	if (FAILED(SSPI_Handle.LastReturnValue))
	{
		PrintError(LOG_LEVEL_TYPE::LEVEL_3, LOG_ERROR_TYPE::TLS, L"SSPI decrypt data error", SSPI_Handle.LastReturnValue, nullptr, 0);
		return false;
	}
	else {
	//Scan all security buffers.
		for (size_t Index = 0;Index < SSPI_SECURE_BUFFER_NUM;++Index)
		{
			if (BufferSec.at(Index).BufferType == SECBUFFER_DATA && BufferSec.at(Index).pvBuffer != nullptr && BufferSec.at(Index).cbBuffer >= sizeof(tls_base_record))
			{
			//Buffer initializtion
				auto RecvBuffer = std::make_unique<uint8_t[]>(static_cast<const size_t>(BufferSec.at(Index).cbBuffer) + MEMORY_RESERVED_BYTES);
				memset(RecvBuffer.get(), 0, static_cast<const size_t>(BufferSec.at(Index).cbBuffer) + MEMORY_RESERVED_BYTES);
				memcpy_s(RecvBuffer.get(), BufferSec.at(Index).cbBuffer, BufferSec.at(Index).pvBuffer, BufferSec.at(Index).cbBuffer);
				std::swap(SocketSelectingDataList.front().RecvBuffer, RecvBuffer);
				SocketSelectingDataList.front().RecvSize = BufferSec.at(Index).cbBuffer;
				SocketSelectingDataList.front().RecvLen = BufferSec.at(Index).cbBuffer;
				break;
			}
			else if (Index + 1U == SSPI_SECURE_BUFFER_NUM)
			{
				PrintError(LOG_LEVEL_TYPE::LEVEL_3, LOG_ERROR_TYPE::TLS, L"SSPI decrypt data error", 0, nullptr, 0);
				return false;
			}
		}
	}

	return true;
}

//Transport with TLS security connection
bool TLS_TransportSerial(
	const REQUEST_PROCESS_TYPE RequestType, 
	const size_t PacketMinSize, 
	SSPI_HANDLE_TABLE &SSPI_Handle, 
	std::vector<SOCKET_DATA> &SocketDataList, 
	std::vector<SOCKET_SELECTING_SERIAL_DATA> &SocketSelectingDataList, 
	std::vector<ssize_t> &ErrorCodeList)
{
//Socket data check
	if (SocketDataList.empty() || SocketSelectingDataList.empty() || ErrorCodeList.empty())
		return false;

//TLS encrypt packet.
	if (SocketSelectingDataList.front().SendBuffer && SocketSelectingDataList.front().SendLen > 0 && 
		(!SSPI_EncryptPacket(SSPI_Handle, SocketSelectingDataList) || SocketSelectingDataList.front().SendLen < sizeof(tls_base_record)))
	{
		SocketSelectingDataList.front().SendBuffer.reset();
		SocketSelectingDataList.front().SendSize = 0;
		SocketSelectingDataList.front().SendLen = 0;

		return false;
	}

//Request exchange 
	SocketSelectingDataList.front().RecvBuffer.reset();
	SocketSelectingDataList.front().RecvSize = 0;
	SocketSelectingDataList.front().RecvLen = 0;
	const auto RecvLen = SocketSelectingSerial(REQUEST_PROCESS_TYPE::TLS_TRANSPORT, IPPROTO_TCP, SocketDataList, SocketSelectingDataList, ErrorCodeList);
	SocketSelectingDataList.front().SendBuffer.reset();
	SocketSelectingDataList.front().SendSize = 0;
	SocketSelectingDataList.front().SendLen = 0;
	if (RecvLen == EXIT_FAILURE || SSPI_Handle.LastReturnValue == SEC_I_CONTEXT_EXPIRED || 
		SocketSelectingDataList.front().RecvLen < sizeof(tls_base_record))
	{
		SocketSelectingDataList.front().RecvBuffer.reset();
		SocketSelectingDataList.front().RecvSize = 0;
		SocketSelectingDataList.front().RecvLen = 0;

		return false;
	}
	else {
	//TLS decrypt packet.
		if (SSPI_DecryptPacket(SSPI_Handle, SocketSelectingDataList) && SocketSelectingDataList.front().RecvLen >= PacketMinSize)
			return true;
	}

	return false;
}

//SSPI shutdown SChannel security connection
bool SSPI_ShutdownConnection(
	SSPI_HANDLE_TABLE &SSPI_Handle, 
	std::vector<SOCKET_DATA> &SocketDataList, 
	std::vector<ssize_t> &ErrorCodeList)
{
//Socket data check
	if (SocketDataList.empty() || ErrorCodeList.empty() || !SocketSetting(SocketDataList.front().Socket, SOCKET_SETTING_TYPE::INVALID_CHECK, false, nullptr))
		return false;

//Buffer initializtion(Part 1)
	std::vector<SOCKET_SELECTING_SERIAL_DATA> SocketSelectingDataList(1U);
	SecBufferDesc BufferDesc;
	memset(&BufferDesc, 0, sizeof(BufferDesc));
	std::array<SecBuffer, 1U> BufferSec{};
	SSPI_Handle.InputFlags = SCHANNEL_SHUTDOWN;
	BufferSec.at(0).pvBuffer = &SSPI_Handle.InputFlags;
	BufferSec.at(0).BufferType = SECBUFFER_TOKEN;
	BufferSec.at(0).cbBuffer = sizeof(SSPI_Handle.InputFlags);
	BufferDesc.cBuffers = 1U;
	BufferDesc.pBuffers = BufferSec.data();
	BufferDesc.ulVersion = SECBUFFER_VERSION;

//Apply control token.
	SSPI_Handle.LastReturnValue = ApplyControlToken(
		&SSPI_Handle.ContextHandle, 
		&BufferDesc);
	if (FAILED(SSPI_Handle.LastReturnValue))
	{
		PrintError(LOG_LEVEL_TYPE::LEVEL_3, LOG_ERROR_TYPE::TLS, L"SSPI initialize security context error", SSPI_Handle.LastReturnValue, nullptr, 0);
		return false;
	}

//Buffer initializtion(Part 2)
	SSPI_Handle.InputFlags |= ISC_REQ_SEQUENCE_DETECT;
	SSPI_Handle.InputFlags |= ISC_REQ_REPLAY_DETECT;
	SSPI_Handle.InputFlags |= ISC_REQ_CONFIDENTIALITY;
	SSPI_Handle.InputFlags |= ISC_RET_EXTENDED_ERROR;
	SSPI_Handle.InputFlags |= ISC_REQ_ALLOCATE_MEMORY;
	SSPI_Handle.InputFlags |= ISC_REQ_STREAM;
	BufferSec.at(0).BufferType = SECBUFFER_TOKEN;
	BufferSec.at(0).pvBuffer = nullptr;
	BufferSec.at(0).cbBuffer = 0;
	BufferDesc.cBuffers = 1U;
	BufferDesc.pBuffers = BufferSec.data();
	BufferDesc.ulVersion = SECBUFFER_VERSION;
	SEC_WCHAR *SSPI_SNI = nullptr;
	if (Parameter.HTTP_CONNECT_TLS_SNI != nullptr && !Parameter.HTTP_CONNECT_TLS_SNI->empty())
		SSPI_SNI = reinterpret_cast<SEC_WCHAR *>(const_cast<wchar_t *>(Parameter.HTTP_CONNECT_TLS_SNI->c_str()));
	DWORD OutputFlags = 0;

//Send "Close Notify" to server to notify shutdown connection.
	SSPI_Handle.LastReturnValue = InitializeSecurityContextW(
		&SSPI_Handle.ClientCredentials, 
		&SSPI_Handle.ContextHandle, 
		SSPI_SNI, 
		SSPI_Handle.InputFlags, 
		0, 
		0, 
		nullptr, 
		0, 
		nullptr, 
		&BufferDesc, 
		&OutputFlags, 
		nullptr);
	if (FAILED(SSPI_Handle.LastReturnValue) || BufferSec.at(0).pvBuffer == nullptr || BufferSec.at(0).cbBuffer < sizeof(tls_base_record))
	{
		if (BufferSec.at(0).pvBuffer != nullptr)
			FreeContextBuffer(BufferSec.at(0).pvBuffer);
		PrintError(LOG_LEVEL_TYPE::LEVEL_3, LOG_ERROR_TYPE::TLS, L"SSPI initialize security context error", SSPI_Handle.LastReturnValue, nullptr, 0);

		return false;
	}
	else {
	//Buffer initializtion
		auto SendBuffer = std::make_unique<uint8_t[]>(static_cast<const size_t>(BufferSec.at(0).cbBuffer) + MEMORY_RESERVED_BYTES);
		memset(SendBuffer.get(), 0, static_cast<const size_t>(BufferSec.at(0).cbBuffer) + MEMORY_RESERVED_BYTES);
		memcpy_s(SendBuffer.get(), BufferSec.at(0).cbBuffer, BufferSec.at(0).pvBuffer, BufferSec.at(0).cbBuffer);
		std::swap(SocketSelectingDataList.front().SendBuffer, SendBuffer);
		SocketSelectingDataList.front().SendSize = BufferSec.at(0).cbBuffer;
		SocketSelectingDataList.front().SendLen = BufferSec.at(0).cbBuffer;
		if (BufferSec.at(0).pvBuffer != nullptr)
			FreeContextBuffer(BufferSec.at(0).pvBuffer);
		SendBuffer.reset();

	//TLS handshake exchange
		SocketSelectingDataList.front().RecvBuffer.reset();
		SocketSelectingDataList.front().RecvSize = 0;
		SocketSelectingDataList.front().RecvLen = 0;
		const auto RecvLen = SocketSelectingSerial(REQUEST_PROCESS_TYPE::TLS_SHUTDOWN, IPPROTO_TCP, SocketDataList, SocketSelectingDataList, ErrorCodeList);
		SocketSelectingDataList.front().SendBuffer.reset();
		SocketSelectingDataList.front().SendSize = 0;
		SocketSelectingDataList.front().SendLen = 0;
		if (RecvLen == EXIT_FAILURE)
		{
			PrintError(LOG_LEVEL_TYPE::LEVEL_3, LOG_ERROR_TYPE::NETWORK, L"TLS request error", ErrorCodeList.front(), nullptr, 0);
			return false;
		}
	}

	return true;
}
#elif (defined(PLATFORM_FREEBSD) || defined(PLATFORM_LINUX) || defined(PLATFORM_MACOS))
//OpenSSL print error messages process
bool OpenSSL_PrintError(
	const uint8_t *OpenSSL_ErrorMessage, 
	const wchar_t *ErrorMessage)
{
//Message check
	if (OpenSSL_ErrorMessage == nullptr || ErrorMessage == nullptr || 
		strnlen(reinterpret_cast<const char *>(OpenSSL_ErrorMessage), OPENSSL_STATIC_BUFFER_SIZE) == 0 || 
		wcsnlen(ErrorMessage, OPENSSL_STATIC_BUFFER_SIZE) == 0)
	{
		PrintError(LOG_LEVEL_TYPE::LEVEL_2, LOG_ERROR_TYPE::SYSTEM, L"Convert multiple byte or wide char string error", 0, nullptr, 0);
		return false;
	}

//Convert message.
	std::wstring Message;
	if (MBS_To_WCS_String(OpenSSL_ErrorMessage, OPENSSL_STATIC_BUFFER_SIZE, Message))
	{
		std::wstring InnerMessage(ErrorMessage); //OpenSSL will return message like "error:.."
		InnerMessage.append(Message);
		PrintError(LOG_LEVEL_TYPE::LEVEL_3, LOG_ERROR_TYPE::TLS, InnerMessage.c_str(), 0, nullptr, 0);
	}
	else {
		PrintError(LOG_LEVEL_TYPE::LEVEL_2, LOG_ERROR_TYPE::SYSTEM, L"Convert multiple byte or wide char string error", 0, nullptr, 0);
		return false;
	}

	return true;
}

//OpenSSL initializtion
void OpenSSL_LibraryInit(
	bool IsLoad)
{
//Load all OpenSSL libraries, algorithms and strings.
	if (IsLoad)
	{
	#if OPENSSL_VERSION_NUMBER >= OPENSSL_VERSION_1_1_0 //OpenSSL version 1.1.0 and above
		OPENSSL_init_ssl(0, nullptr);
	#else //OpenSSL version below 1.1.0
		SSL_library_init();
		OpenSSL_add_all_algorithms();
		SSL_load_error_strings();
		ERR_load_crypto_strings();
		OPENSSL_config(nullptr);
	#endif
	}
#if OPENSSL_VERSION_NUMBER < OPENSSL_VERSION_1_1_0 //OpenSSL version below 1.1.0
//Unoad all OpenSSL libraries, algorithms and strings.
	else {
		CONF_modules_unload(1); //All modules
		ERR_free_strings();
		EVP_cleanup();
	}
#endif

	return;
}

//OpenSSL TLS CTX initializtion
bool OpenSSL_CTX_Initializtion(
	OPENSSL_CONTEXT_TABLE &OpenSSL_CTX)
{
//TLS version selection(Part 1)
#if OPENSSL_VERSION_NUMBER < OPENSSL_VERSION_1_1_0 //OpenSSL version between 1.0.2 and 1.1.0
	if (OpenSSL_CTX.Protocol_Transport == IPPROTO_TCP)
	{
	//No TLS 1.3 and above support below 1.1.1
		if (Parameter.HTTP_CONNECT_TLS_Version == TLS_VERSION_SELECTION::VERSION_1_2)
			OpenSSL_CTX.MethodContext = SSL_CTX_new(TLSv1_2_method());
		else if (Parameter.HTTP_CONNECT_TLS_Version == TLS_VERSION_SELECTION::VERSION_1_1)
			OpenSSL_CTX.MethodContext = SSL_CTX_new(TLSv1_1_method());
		else if (Parameter.HTTP_CONNECT_TLS_Version == TLS_VERSION_SELECTION::VERSION_1_0)
			OpenSSL_CTX.MethodContext = SSL_CTX_new(TLSv1_method());
		else //Auto select
			OpenSSL_CTX.MethodContext = SSL_CTX_new(SSLv23_method());
	}
	else if (OpenSSL_CTX.Protocol_Transport == IPPROTO_UDP)
	{
	//No DTLS 1.3 and above support below 1.1.1
		if (Parameter.HTTP_CONNECT_TLS_Version == TLS_VERSION_SELECTION::VERSION_1_2)
			OpenSSL_CTX.MethodContext = SSL_CTX_new(DTLSv1_2_method());
		else if (Parameter.HTTP_CONNECT_TLS_Version == TLS_VERSION_SELECTION::VERSION_1_1 || //DTLS has no any version 1.1.
			Parameter.HTTP_CONNECT_TLS_Version == TLS_VERSION_SELECTION::VERSION_1_0)
				OpenSSL_CTX.MethodContext = SSL_CTX_new(DTLSv1_method());
		else //Auto select
			OpenSSL_CTX.MethodContext = SSL_CTX_new(DTLS_method());
	}
	else {
		return false;
	}
#else //OpenSSL version 1.1.0 and above
	if (OpenSSL_CTX.Protocol_Transport == IPPROTO_TCP)
		OpenSSL_CTX.MethodContext = SSL_CTX_new(TLS_method()); //TLS selection in OpenSSL version 1.1.0 and above must set flags of method context.
	else if (OpenSSL_CTX.Protocol_Transport == IPPROTO_UDP)
		OpenSSL_CTX.MethodContext = SSL_CTX_new(DTLS_method()); //TLS selection in OpenSSL version 1.1.0 and above must set flags of method context.
	else 
		return false;
#endif

//Create new client-method instance.
	if (OpenSSL_CTX.MethodContext == nullptr)
	{
		OpenSSL_PrintError(reinterpret_cast<const uint8_t *>(ERR_error_string(ERR_get_error(), nullptr)), L"OpenSSL create new client-method instance ");
		return false;
	}

//TLS version selection(Part 2)
	ssize_t ResultValue = 0;
#if OPENSSL_VERSION_NUMBER >= OPENSSL_VERSION_1_1_0 //OpenSSL version 1.1.0 and above
	ssize_t InnerResultValue = OPENSSL_RETURN_SUCCESS;
#if OPENSSL_VERSION_NUMBER >= OPENSSL_VERSION_1_1_1 //OpenSSL version 1.1.1 and above
	if (Parameter.HTTP_CONNECT_TLS_Version == TLS_VERSION_SELECTION::VERSION_1_3)
	{
		if (OpenSSL_CTX.Protocol_Transport == IPPROTO_TCP)
		{
			ResultValue = SSL_CTX_set_min_proto_version(OpenSSL_CTX.MethodContext, TLS1_3_VERSION);
			InnerResultValue = SSL_CTX_set_max_proto_version(OpenSSL_CTX.MethodContext, TLS1_3_VERSION);
		}
//		else if (OpenSSL_CTX.Protocol_Transport == IPPROTO_UDP) //No DTLS 1.3 and above support below 1.1.1
//		{
//			ResultValue = SSL_CTX_set_min_proto_version(OpenSSL_CTX.MethodContext, DTLS1_3_VERSION);
//			InnerResultValue = SSL_CTX_set_max_proto_version(OpenSSL_CTX.MethodContext, DTLS1_3_VERSION);
//		}
		else {
			return false;
		}
	}
	else 
#endif
	if (Parameter.HTTP_CONNECT_TLS_Version == TLS_VERSION_SELECTION::VERSION_1_2)
	{
		if (OpenSSL_CTX.Protocol_Transport == IPPROTO_TCP)
		{
			ResultValue = SSL_CTX_set_min_proto_version(OpenSSL_CTX.MethodContext, TLS1_2_VERSION);
			InnerResultValue = SSL_CTX_set_max_proto_version(OpenSSL_CTX.MethodContext, TLS1_2_VERSION);
		}
		else if (OpenSSL_CTX.Protocol_Transport == IPPROTO_UDP)
		{
			ResultValue = SSL_CTX_set_min_proto_version(OpenSSL_CTX.MethodContext, DTLS1_2_VERSION);
			InnerResultValue = SSL_CTX_set_max_proto_version(OpenSSL_CTX.MethodContext, DTLS1_2_VERSION);
		}
		else {
			return false;
		}
	}
	else if (Parameter.HTTP_CONNECT_TLS_Version == TLS_VERSION_SELECTION::VERSION_1_1)
	{
		if (OpenSSL_CTX.Protocol_Transport == IPPROTO_TCP)
		{
			ResultValue = SSL_CTX_set_min_proto_version(OpenSSL_CTX.MethodContext, TLS1_1_VERSION);
			InnerResultValue = SSL_CTX_set_max_proto_version(OpenSSL_CTX.MethodContext, TLS1_1_VERSION);
		}
		else if (OpenSSL_CTX.Protocol_Transport == IPPROTO_UDP)
		{
			ResultValue = SSL_CTX_set_min_proto_version(OpenSSL_CTX.MethodContext, DTLS1_VERSION);
			InnerResultValue = SSL_CTX_set_max_proto_version(OpenSSL_CTX.MethodContext, DTLS1_VERSION);
		}
		else {
			return false;
		}
	}
	else if (Parameter.HTTP_CONNECT_TLS_Version == TLS_VERSION_SELECTION::VERSION_1_0)
	{
		if (OpenSSL_CTX.Protocol_Transport == IPPROTO_TCP)
		{
			ResultValue = SSL_CTX_set_min_proto_version(OpenSSL_CTX.MethodContext, TLS1_VERSION);
			InnerResultValue = SSL_CTX_set_max_proto_version(OpenSSL_CTX.MethodContext, TLS1_VERSION);
		}
		else if (OpenSSL_CTX.Protocol_Transport == IPPROTO_UDP)
		{
			ResultValue = SSL_CTX_set_min_proto_version(OpenSSL_CTX.MethodContext, DTLS1_VERSION);
			InnerResultValue = SSL_CTX_set_max_proto_version(OpenSSL_CTX.MethodContext, DTLS1_VERSION);
		}
		else {
			return false;
		}
	}
	else { //Setting the minimum or maximum version to 0 will enable protocol versions down to the lowest version, or up to the highest version supported by the library, respectively.
		ResultValue = SSL_CTX_set_max_proto_version(OpenSSL_CTX.MethodContext, 0);
	}

//TLS selection check
	if (ResultValue != OPENSSL_RETURN_SUCCESS || InnerResultValue != OPENSSL_RETURN_SUCCESS)
	{
		OpenSSL_PrintError(reinterpret_cast<const uint8_t *>(ERR_error_string(ERR_get_error(), nullptr)), L"OpenSSL TLS version selection ");
		return false;
	}
#endif

//Set TLS CTX mode.
	SSL_CTX_set_mode(OpenSSL_CTX.MethodContext, SSL_MODE_AUTO_RETRY);
#if defined(SSL_MODE_RELEASE_BUFFERS)
	SSL_CTX_set_mode(OpenSSL_CTX.MethodContext, SSL_MODE_RELEASE_BUFFERS);
#endif

//TLS connection flags
#if OPENSSL_VERSION_NUMBER < OPENSSL_VERSION_1_1_1 //OpenSSL version below 1.1.1
	SSL_CTX_set_options(OpenSSL_CTX.MethodContext, SSL_OP_NO_SSLv2); //Disable SSLv2 protocol
#endif
	SSL_CTX_set_options(OpenSSL_CTX.MethodContext, SSL_OP_NO_SSLv3); //Disable SSLv3 protocol
	SSL_CTX_set_options(OpenSSL_CTX.MethodContext, SSL_OP_NO_COMPRESSION); //Disable TLS compression
	SSL_CTX_set_options(OpenSSL_CTX.MethodContext, SSL_OP_SINGLE_DH_USE); //Always create a new key when using temporary/ephemeral DH parameters.

//Set ciphers suites(TLS/DTLS 1.2 and below).
	if (Parameter.HTTP_CONNECT_TLS_Version == TLS_VERSION_SELECTION::VERSION_1_0 || Parameter.HTTP_CONNECT_TLS_Version == TLS_VERSION_SELECTION::VERSION_1_1)
		ResultValue = SSL_CTX_set_cipher_list(OpenSSL_CTX.MethodContext, OPENSSL_CIPHER_LIST_COMPATIBILITY);
	else //Auto select and new TLS version
		ResultValue = SSL_CTX_set_cipher_list(OpenSSL_CTX.MethodContext, OPENSSL_CIPHER_LIST_STRONG);
	if (ResultValue == OPENSSL_RETURN_FAILURE)
	{
		OpenSSL_PrintError(reinterpret_cast<const uint8_t *>(ERR_error_string(ERR_get_error(), nullptr)), L"OpenSSL set strong ciphers ");
		return false;
	}

//Set ciphers suites(TLS/DTLS 1.3 and above).
#if OPENSSL_VERSION_NUMBER >= OPENSSL_VERSION_1_1_1 //OpenSSL version 1.1.1 and above
	ResultValue = SSL_CTX_set_ciphersuites(OpenSSL_CTX.MethodContext, OPENSSL_CIPHER_LIST_STRONG);
	if (ResultValue == OPENSSL_RETURN_FAILURE)
	{
		OpenSSL_PrintError(reinterpret_cast<const uint8_t *>(ERR_error_string(ERR_get_error(), nullptr)), L"OpenSSL set strong ciphers ");
		return false;
	}
#endif

//TLS ALPN extension settings
	if (Parameter.HTTP_CONNECT_TLS_ALPN)
	{
		if (Parameter.HTTP_CONNECT_Version == HTTP_VERSION_SELECTION::VERSION_1)
			ResultValue = SSL_CTX_set_alpn_protos(OpenSSL_CTX.MethodContext, HTTP_1_ALPN_List, sizeof(HTTP_1_ALPN_List));
		else if (Parameter.HTTP_CONNECT_Version == HTTP_VERSION_SELECTION::VERSION_2)
			ResultValue = SSL_CTX_set_alpn_protos(OpenSSL_CTX.MethodContext, HTTP_2_ALPN_List, sizeof(HTTP_2_ALPN_List));
		else 
			return false;

	//ResultValue check
	//OpenSSL ALPN functions return 0 on success, do not use OPENSSL_RETURN_SUCCESS(1).
		if (ResultValue != 0)
		{
			OpenSSL_PrintError(reinterpret_cast<const uint8_t *>(ERR_error_string(ERR_get_error(), nullptr)), L"OpenSSL set ALPN extension ");
			return false;
		}
	}

//TLS certificate store location and verification settings
	if (Parameter.HTTP_CONNECT_TLS_Validation)
	{
	//Locate default certificate store.
		ResultValue = SSL_CTX_set_default_verify_paths(OpenSSL_CTX.MethodContext);
		if (ResultValue != OPENSSL_RETURN_SUCCESS)
		{
			OpenSSL_PrintError(reinterpret_cast<const uint8_t *>(ERR_error_string(ERR_get_error(), nullptr)), L"OpenSSL locate default certificate store ");
			return false;
		}
	//Set certificate verification.
		else {
			SSL_CTX_set_verify(OpenSSL_CTX.MethodContext, SSL_VERIFY_PEER, nullptr);
		}
	}

	return true;
}

//OpenSSL BIO initializtion
bool OpenSSL_BIO_Initializtion(
	OPENSSL_CONTEXT_TABLE &OpenSSL_CTX)
{
//Create a new BIO method.
	OpenSSL_CTX.SessionBIO = BIO_new_ssl_connect(OpenSSL_CTX.MethodContext);
	if (OpenSSL_CTX.SessionBIO == nullptr)
	{
		OpenSSL_PrintError(reinterpret_cast<const uint8_t *>(ERR_error_string(ERR_get_error(), nullptr)), L"OpenSSL create new BIO method ");
		return false;
	}

//Create a new socket.
	if (OpenSSL_CTX.Protocol_Transport == IPPROTO_TCP)
		OpenSSL_CTX.Socket = socket(OpenSSL_CTX.Protocol_Network, SOCK_STREAM, OpenSSL_CTX.Protocol_Transport);
	else if (OpenSSL_CTX.Protocol_Transport == IPPROTO_UDP)
		OpenSSL_CTX.Socket = socket(OpenSSL_CTX.Protocol_Network, SOCK_DGRAM, OpenSSL_CTX.Protocol_Transport);
	else 
		return false;

//Socket attribute settings
	if (!SocketSetting(OpenSSL_CTX.Socket, SOCKET_SETTING_TYPE::INVALID_CHECK, true, nullptr) || 
		!SocketSetting(OpenSSL_CTX.Socket, SOCKET_SETTING_TYPE::NON_BLOCKING_MODE, true, nullptr) || 
		(OpenSSL_CTX.Protocol_Transport == IPPROTO_TCP && !SocketSetting(OpenSSL_CTX.Socket, SOCKET_SETTING_TYPE::TCP_FAST_OPEN_NORMAL, true, nullptr)) || 
		(OpenSSL_CTX.Protocol_Network == AF_INET6 && !SocketSetting(OpenSSL_CTX.Socket, SOCKET_SETTING_TYPE::HOP_LIMITS_IPV6, true, nullptr)) || 
		(OpenSSL_CTX.Protocol_Network == AF_INET && (!SocketSetting(OpenSSL_CTX.Socket, SOCKET_SETTING_TYPE::HOP_LIMITS_IPV4, true, nullptr) || 
		(OpenSSL_CTX.Protocol_Transport == IPPROTO_UDP && !SocketSetting(OpenSSL_CTX.Socket, SOCKET_SETTING_TYPE::DO_NOT_FRAGMENT, true, nullptr)))))
			return false;

//BIO attribute settings
	BIO_set_fd(OpenSSL_CTX.SessionBIO, OpenSSL_CTX.Socket, BIO_NOCLOSE); //Set the socket.
	BIO_set_nbio(OpenSSL_CTX.SessionBIO, OPENSSL_SET_NON_BLOCKING); //Socket non-blocking mode
	BIO_set_conn_hostname(OpenSSL_CTX.SessionBIO, OpenSSL_CTX.AddressString.c_str()); //Set connect target.

//Get SSL method data.
	BIO_get_ssl(OpenSSL_CTX.SessionBIO, &OpenSSL_CTX.SessionData);
	if (OpenSSL_CTX.SessionData == nullptr)
	{
		OpenSSL_PrintError(reinterpret_cast<const uint8_t *>(ERR_error_string(ERR_get_error(), nullptr)), L"OpenSSL BIO and SSL data attribute settings ");
		return false;
	}

//TLS Server Name Indication/SNI settings
	if (Parameter.HTTP_CONNECT_TLS_SNI_MBS != nullptr && !Parameter.HTTP_CONNECT_TLS_SNI_MBS->empty())
		SSL_set_tlsext_host_name(OpenSSL_CTX.SessionData, Parameter.HTTP_CONNECT_TLS_SNI_MBS->c_str());

//Built-in functionality for hostname checking and validation
	if (Parameter.HTTP_CONNECT_TLS_Validation && Parameter.HTTP_CONNECT_TLS_SNI_MBS != nullptr && !Parameter.HTTP_CONNECT_TLS_SNI_MBS->empty())
	{
	//Get certificate paremeter.
		auto X509_Param = SSL_get0_param(OpenSSL_CTX.SessionData);
		if (X509_Param == nullptr)
		{
			OpenSSL_PrintError(reinterpret_cast<const uint8_t *>(ERR_error_string(ERR_get_error(), nullptr)), L"OpenSSL hostname checking and validation ");
			return false;
		}

	//Set certificate paremeter flags.
		X509_VERIFY_PARAM_set_hostflags(X509_Param, X509_CHECK_FLAG_NO_PARTIAL_WILDCARDS);
//		X509_VERIFY_PARAM_set_hostflags(X509_Param, X509_CHECK_FLAG_MULTI_LABEL_WILDCARDS);
		X509_VERIFY_PARAM_set_hostflags(X509_Param, X509_CHECK_FLAG_SINGLE_LABEL_SUBDOMAINS);
		if (X509_VERIFY_PARAM_set1_host(X509_Param, Parameter.HTTP_CONNECT_TLS_SNI_MBS->c_str(), 0) == OPENSSL_RETURN_FAILURE)
		{
			OpenSSL_PrintError(reinterpret_cast<const uint8_t *>(ERR_error_string(ERR_get_error(), nullptr)), L"OpenSSL hostname checking and validation ");
			return false;
		}
	}

	return true;
}

//OpenSSL TLS handshake
bool OpenSSL_Handshake(
	OPENSSL_CONTEXT_TABLE &OpenSSL_CTX)
{
//Initializtion
	ssize_t RecvLen = 0;
	size_t Timeout = 0;

//OpenSSL BIO connecting
	while (RecvLen <= 0)
	{
		RecvLen = BIO_do_connect(OpenSSL_CTX.SessionBIO);
		if (RecvLen == OPENSSL_RETURN_SUCCESS) //Connection was established successfully.
		{
			break;
		}
		else if (Timeout <= static_cast<const uint64_t>(Parameter.SocketTimeout_Reliable_Serial.tv_sec) * SECOND_TO_MILLISECOND * MICROSECOND_TO_MILLISECOND + static_cast<const uint64_t>(Parameter.SocketTimeout_Reliable_Serial.tv_usec) && 
			BIO_should_retry(OpenSSL_CTX.SessionBIO))
		{
			usleep(LOOP_INTERVAL_TIME_NO_DELAY);
			Timeout += LOOP_INTERVAL_TIME_NO_DELAY;
		}
		else {
			OpenSSL_PrintError(reinterpret_cast<const uint8_t *>(ERR_error_string(ERR_get_error(), nullptr)), L"OpenSSL connecting ");
			return false;
		}
	}

//OpenSSL TLS handshake
	RecvLen = 0;
	Timeout = 0;
	while (RecvLen <= 0)
	{
		RecvLen = BIO_do_handshake(OpenSSL_CTX.SessionBIO);
		if (RecvLen == OPENSSL_RETURN_SUCCESS) //Connection was established successfully.
		{
			break;
		}
		else if (Timeout <= static_cast<const uint64_t>(Parameter.SocketTimeout_Reliable_Serial.tv_sec) * SECOND_TO_MILLISECOND * MICROSECOND_TO_MILLISECOND + static_cast<const uint64_t>(Parameter.SocketTimeout_Reliable_Serial.tv_usec) && 
			BIO_should_retry(OpenSSL_CTX.SessionBIO))
		{
			usleep(LOOP_INTERVAL_TIME_NO_DELAY);
			Timeout += LOOP_INTERVAL_TIME_NO_DELAY;
		}
		else {
			OpenSSL_PrintError(reinterpret_cast<const uint8_t *>(ERR_error_string(ERR_get_error(), nullptr)), L"OpenSSL handshake ");
			return false;
		}
	}

//Verify a server certificate was presented during the negotiation.
	auto Certificate = SSL_get_peer_certificate(OpenSSL_CTX.SessionData);
	if (Certificate == nullptr)
	{
		OpenSSL_PrintError(reinterpret_cast<const uint8_t *>(ERR_error_string(ERR_get_error(), nullptr)), L"OpenSSL verify server certificate ");
		return false;
	}
	else {
		X509_free(Certificate);
		Certificate = nullptr;
	}

//Verify the result of chain verification, verification performed according to RFC 4158.
	if (Parameter.HTTP_CONNECT_TLS_Validation && SSL_get_verify_result(OpenSSL_CTX.SessionData) != X509_V_OK)
	{
		OpenSSL_PrintError(reinterpret_cast<const uint8_t *>(ERR_error_string(ERR_get_error(), nullptr)), L"OpenSSL verify result of chain verification ");
		return false;
	}

	return true;
}

//Transport with TLS security connection
bool TLS_TransportSerial(
	const REQUEST_PROCESS_TYPE RequestType, 
	const size_t PacketMinSize, 
	OPENSSL_CONTEXT_TABLE &OpenSSL_CTX, 
	std::vector<SOCKET_SELECTING_SERIAL_DATA> &SocketSelectingDataList)
{
//Socket data check
	if (SocketSelectingDataList.empty())
		return false;

//Initializtion
	ssize_t RecvLen = 0;
	size_t Timeout = 0;

//OpenSSL transport(Send process)
	if (SocketSelectingDataList.front().SendBuffer && SocketSelectingDataList.front().SendLen > 0)
	{
		while (RecvLen <= 0)
		{
			RecvLen = BIO_write(OpenSSL_CTX.SessionBIO, SocketSelectingDataList.front().SendBuffer.get(), static_cast<const int>(SocketSelectingDataList.front().SendLen));
			if (RecvLen >= static_cast<const ssize_t>(SocketSelectingDataList.front().SendLen))
			{
				break;
			}
			else if (Timeout <= static_cast<const uint64_t>(Parameter.SocketTimeout_Reliable_Serial.tv_sec) * SECOND_TO_MILLISECOND * MICROSECOND_TO_MILLISECOND + static_cast<const uint64_t>(Parameter.SocketTimeout_Reliable_Serial.tv_usec) && 
				BIO_should_retry(OpenSSL_CTX.SessionBIO))
			{
				usleep(LOOP_INTERVAL_TIME_NO_DELAY);
				Timeout += LOOP_INTERVAL_TIME_NO_DELAY;
			}
			else {
			//Buffer initializtion
				SocketSelectingDataList.front().SendBuffer.reset();
				SocketSelectingDataList.front().SendSize = 0;
				SocketSelectingDataList.front().SendLen = 0;

			//Print error messages.
				OpenSSL_PrintError(reinterpret_cast<const uint8_t *>(ERR_error_string(ERR_get_error(), nullptr)), L"OpenSSL send data ");
				return false;
			}
		}
	}

//Buffer initializtion
	SocketSelectingDataList.front().SendBuffer.reset();
	SocketSelectingDataList.front().SendSize = 0;
	SocketSelectingDataList.front().SendLen = 0;
	SocketSelectingDataList.front().RecvBuffer.reset();
	SocketSelectingDataList.front().RecvSize = 0;
	SocketSelectingDataList.front().RecvLen = 0;
	RecvLen = 0;
	Timeout = 0;

//OpenSSL transpot(Receive process)
	while (!GlobalRunningStatus.IsNeedExit)
	{
	//Prepare buffer.
		if (!SocketSelectingDataList.front().RecvBuffer)
		{
			auto RecvBuffer = std::make_unique<uint8_t[]>(Parameter.LargeBufferSize + MEMORY_RESERVED_BYTES);
			memset(RecvBuffer.get(), 0, Parameter.LargeBufferSize + MEMORY_RESERVED_BYTES);
			std::swap(SocketSelectingDataList.front().RecvBuffer, RecvBuffer);
			SocketSelectingDataList.front().RecvSize = Parameter.LargeBufferSize;
			SocketSelectingDataList.front().RecvLen = 0;
		}
		else if (SocketSelectingDataList.front().RecvSize < SocketSelectingDataList.front().RecvLen + Parameter.LargeBufferSize)
		{
			auto RecvBuffer = std::make_unique<uint8_t[]>(SocketSelectingDataList.front().RecvSize + Parameter.LargeBufferSize);
			memset(RecvBuffer.get(), 0, SocketSelectingDataList.front().RecvSize + Parameter.LargeBufferSize);
			memcpy_s(RecvBuffer.get(), SocketSelectingDataList.front().RecvSize + Parameter.LargeBufferSize, SocketSelectingDataList.front().RecvBuffer.get(), SocketSelectingDataList.front().RecvLen);
			std::swap(SocketSelectingDataList.front().RecvBuffer, RecvBuffer);
			SocketSelectingDataList.front().RecvSize += Parameter.LargeBufferSize;
		}

	//Receive process
		RecvLen = BIO_read(OpenSSL_CTX.SessionBIO, SocketSelectingDataList.front().RecvBuffer.get() + SocketSelectingDataList.front().RecvLen, static_cast<const int>(Parameter.LargeBufferSize));
		if (RecvLen <= 0)
		{
			if (Timeout <= static_cast<const uint64_t>(Parameter.SocketTimeout_Reliable_Serial.tv_sec) * SECOND_TO_MILLISECOND * MICROSECOND_TO_MILLISECOND + static_cast<const uint64_t>(Parameter.SocketTimeout_Reliable_Serial.tv_usec) && 
				BIO_should_retry(OpenSSL_CTX.SessionBIO))
			{
				usleep(LOOP_INTERVAL_TIME_NO_DELAY);
				Timeout += LOOP_INTERVAL_TIME_NO_DELAY;
			}
			else if (RequestType == REQUEST_PROCESS_TYPE::TLS_SHUTDOWN) //Do not print any error messages when connecting is shutting down.
			{
				return false;
			}
			else {
			//Buffer initializtion
				SocketSelectingDataList.front().RecvBuffer.reset();
				SocketSelectingDataList.front().RecvSize = 0;
				SocketSelectingDataList.front().RecvLen = 0;

			//Print error messages.
				OpenSSL_PrintError(reinterpret_cast<const uint8_t *>(ERR_error_string(ERR_get_error(), nullptr)), L"OpenSSL receive data ");
				return false;
			}
		}
		else {
			SocketSelectingDataList.front().RecvLen += RecvLen;
			if (RecvLen < static_cast<const ssize_t>(Parameter.LargeBufferSize) && SocketSelectingDataList.front().RecvLen >= PacketMinSize && 
				((RequestType != REQUEST_PROCESS_TYPE::TCP_NORMAL && //Only TCP DNS response should be check.
				RequestType != REQUEST_PROCESS_TYPE::HTTP_CONNECT_MAIN && RequestType != REQUEST_PROCESS_TYPE::HTTP_CONNECT_1 && RequestType != REQUEST_PROCESS_TYPE::HTTP_CONNECT_2) || //Only HTTP CONNECT response should be check.
				CheckConnectionStreamFin(RequestType, SocketSelectingDataList.front().RecvBuffer.get(), SocketSelectingDataList.front().RecvLen)))
					return true;
		}
	}

	return false;
}

//OpenSSL shutdown security connection
bool OpenSSL_ShutdownConnection(
	OPENSSL_CONTEXT_TABLE &OpenSSL_CTX)
{
//Initializtion
	std::vector<SOCKET_SELECTING_SERIAL_DATA> SocketSelectingDataList(1U);
	ssize_t ResultValue = 0;

//Send "Close Notify" to server to notify shutdown connection.
	while (ResultValue == 0)
	{
	//Shutdown security connection.
		ResultValue = SSL_shutdown(OpenSSL_CTX.SessionData);
		if (ResultValue < OPENSSL_RETURN_FAILURE)
			return false;

	//Receive rest of data.
		SocketSelectingDataList.front().RecvBuffer.reset();
		SocketSelectingDataList.front().RecvSize = 0;
		SocketSelectingDataList.front().RecvLen = 0;
		TLS_TransportSerial(REQUEST_PROCESS_TYPE::TLS_SHUTDOWN, sizeof(tls_base_record), OpenSSL_CTX, SocketSelectingDataList);
	}

	return true;
}
#endif
#endif
