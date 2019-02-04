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


#include "DNSCurveControl.h"

/* DNSCurve(DNSCrypt) Protocol version 2

Client -> Server:
*  8 bytes: Magic query bytes
* 32 bytes: The client's DNSCurve public key (crypto_box_PUBLICKEYBYTES)
* 12 bytes: A client-selected nonce for this packet (crypto_box_NONCEBYTES / 2)
* 16 bytes: Poly1305 MAC (crypto_box_ZEROBYTES - crypto_box_BOXZEROBYTES)
* Variable encryption data ..

Server -> Client:
*  8 bytes: The string "r6fnvWJ8" (DNSCRYPT_MAGIC_RESPONSE)
* 12 bytes: The client's nonce (crypto_box_NONCEBYTES / 2)
* 12 bytes: A server-selected nonce extension (crypto_box_NONCEBYTES / 2)
* 16 bytes: Poly1305 MAC (crypto_box_ZEROBYTES - crypto_box_BOXZEROBYTES)
* Variable encryption data ..

Using TCP protocol:
* 2 bytes: DNSCurve(DNSCrypt) data payload length
* Variable original DNSCurve(DNSCrypt) data ..

*/

#if defined(ENABLE_LIBSODIUM)
//DNSCurve check padding data length
size_t DNSCurve_PaddingData(
	const bool IsSetPadding, 
	uint8_t * const Buffer, 
	const size_t Length, 
	const size_t BufferSize)
{
//Length check
	if (BufferSize < Length)
	{
		return EXIT_FAILURE;
	}
//Set padding data sign.
	else if (IsSetPadding && BufferSize > Length)
	{
	//Padding starts with a byte valued 0x80
		Buffer[Length] = static_cast<const uint8_t>(DNSCRYPT_PADDING_SIGN_STRING);

	//Set NULL bytes in padding data.
		for (size_t Index = Length + 1U;Index < BufferSize;++Index)
			Buffer[Index] = 0;
	}
//Check padding data sign.
	else if (Length >= DNS_PACKET_MINSIZE)
	{
	//Prior to encryption, queries are padded using the ISO/IEC 7816-4 format.
	//The padding starts with a byte valued 0x80 followed by a variable number of NULL bytes.
		for (size_t Index = Length - 1U;Index >= DNS_PACKET_MINSIZE;--Index)
		{
			if (Buffer[Index] == DNSCRYPT_PADDING_SIGN_HEX)
				return Index;
		}
	}

	return EXIT_SUCCESS;
}

//DNSCurve verify keypair
bool DNSCurve_VerifyKeypair(
	const uint8_t * const PublicKey, 
	const uint8_t * const SecretKey)
{
//Initialization
	std::array<uint8_t, crypto_box_PUBLICKEYBYTES> TestPublicKey{};
	std::array<uint8_t, crypto_box_PUBLICKEYBYTES + crypto_box_SECRETKEYBYTES + crypto_box_ZEROBYTES> Validation{};
	DNSCURVE_HEAP_BUFFER_TABLE<uint8_t> TestSecretKey(crypto_box_PUBLICKEYBYTES);

//Keypair and validation data initialization
	if (crypto_box_keypair(
			TestPublicKey.data(), 
			TestSecretKey.Buffer) == 0)
				memcpy_s(Validation.data() + crypto_box_ZEROBYTES, crypto_box_PUBLICKEYBYTES + crypto_box_SECRETKEYBYTES, PublicKey, crypto_box_PUBLICKEYBYTES);
	else 
		return false;

//Make DNSCurve test nonce, 0x00 - 0x23(ASCII).
	DNSCURVE_HEAP_BUFFER_TABLE<uint8_t> Nonce(crypto_box_NONCEBYTES);
	for (size_t Index = 0;Index < crypto_box_NONCEBYTES;++Index)
		*(Nonce.Buffer + Index) = static_cast<const uint8_t>(Index);

//Verify keys
	if (crypto_box(
			Validation.data(), 
			Validation.data(), 
			crypto_box_PUBLICKEYBYTES + crypto_box_ZEROBYTES, 
			Nonce.Buffer, 
			TestPublicKey.data(), 
			SecretKey) != 0 || 
		crypto_box_open(
			Validation.data(), 
			Validation.data(), 
			crypto_box_PUBLICKEYBYTES + crypto_box_ZEROBYTES, 
			Nonce.Buffer, 
			PublicKey, 
			TestSecretKey.Buffer) != 0)
				return false;

	return true;
}

//DNSCurve select socket data of DNS target(Multiple threading)
uint16_t DNSCurve_SelectTargetSocket(
	const uint16_t Protocol, 
	const uint16_t QueryType, 
	const SOCKET_DATA &LocalSocketData, 
	bool ** const IsAlternate)
{
//Initialization
	const auto NetworkSpecific = SelectProtocol_Network(DNSCurveParameter.DNSCurveProtocol_Network, DNSCurveParameter.DNSCurve_Target_Server_Main_IPv6.AddressData.Storage.ss_family, DNSCurveParameter.DNSCurve_Target_Server_Main_IPv4.AddressData.Storage.ss_family, DNSCurveParameter.DNSCurveProtocol_IsAccordingType, QueryType, &LocalSocketData);

//IPv6
	if (NetworkSpecific == AF_INET6)
	{
		if (Protocol == IPPROTO_TCP)
		{
			*IsAlternate = &AlternateSwapList.IsSwapped.at(ALTERNATE_SWAP_TYPE_DNSCURVE_TCP_IPV6);
			return AF_INET6;
		}
		else if (Protocol == IPPROTO_UDP)
		{
			*IsAlternate = &AlternateSwapList.IsSwapped.at(ALTERNATE_SWAP_TYPE_DNSCURVE_UDP_IPV6);
			return AF_INET6;
		}
	}
//IPv4
	else if (NetworkSpecific == AF_INET)
	{
		if (Protocol == IPPROTO_TCP)
		{
			*IsAlternate = &AlternateSwapList.IsSwapped.at(ALTERNATE_SWAP_TYPE_DNSCURVE_TCP_IPV4);
			return AF_INET;
		}
		else if (Protocol == IPPROTO_UDP)
		{
			*IsAlternate = &AlternateSwapList.IsSwapped.at(ALTERNATE_SWAP_TYPE_DNSCURVE_UDP_IPV4);
			return AF_INET;
		}
	}

	return 0;
}

//DNSCurve select signature request socket data of DNS target
DNSCURVE_SERVER_DATA *DNSCurve_SelectSignatureTargetSocket(
	const uint16_t Protocol, 
	const bool IsAlternate, 
	DNSCURVE_SERVER_TYPE &ServerType, 
	std::vector<SOCKET_DATA> &SocketDataList)
{
//Socket data check
	if (SocketDataList.empty())
		return nullptr;

//Select target.
	DNSCURVE_SERVER_DATA *PacketTarget = nullptr;
	if (Protocol == AF_INET6)
	{
		if (IsAlternate)
		{
			reinterpret_cast<sockaddr_in6 *>(&SocketDataList.front().SockAddr)->sin6_addr = DNSCurveParameter.DNSCurve_Target_Server_Alternate_IPv6.AddressData.IPv6.sin6_addr;
			reinterpret_cast<sockaddr_in6 *>(&SocketDataList.front().SockAddr)->sin6_port = DNSCurveParameter.DNSCurve_Target_Server_Alternate_IPv6.AddressData.IPv6.sin6_port;
			PacketTarget = &DNSCurveParameter.DNSCurve_Target_Server_Alternate_IPv6;
			ServerType = DNSCURVE_SERVER_TYPE::ALTERNATE_IPV6;
		}
		else { //Main
			reinterpret_cast<sockaddr_in6 *>(&SocketDataList.front().SockAddr)->sin6_addr = DNSCurveParameter.DNSCurve_Target_Server_Main_IPv6.AddressData.IPv6.sin6_addr;
			reinterpret_cast<sockaddr_in6 *>(&SocketDataList.front().SockAddr)->sin6_port = DNSCurveParameter.DNSCurve_Target_Server_Main_IPv6.AddressData.IPv6.sin6_port;
			PacketTarget = &DNSCurveParameter.DNSCurve_Target_Server_Main_IPv6;
			ServerType = DNSCURVE_SERVER_TYPE::MAIN_IPV6;
		}

		SocketDataList.front().AddrLen = sizeof(sockaddr_in6);
		SocketDataList.front().SockAddr.ss_family = AF_INET6;
		return PacketTarget;
	}
	else if (Protocol == AF_INET)
	{
		if (IsAlternate)
		{
			reinterpret_cast<sockaddr_in *>(&SocketDataList.front().SockAddr)->sin_addr = DNSCurveParameter.DNSCurve_Target_Server_Alternate_IPv4.AddressData.IPv4.sin_addr;
			reinterpret_cast<sockaddr_in *>(&SocketDataList.front().SockAddr)->sin_port = DNSCurveParameter.DNSCurve_Target_Server_Alternate_IPv4.AddressData.IPv4.sin_port;
			PacketTarget = &DNSCurveParameter.DNSCurve_Target_Server_Alternate_IPv4;
			ServerType = DNSCURVE_SERVER_TYPE::ALTERNATE_IPV4;
		}
		else { //Main
			reinterpret_cast<sockaddr_in *>(&SocketDataList.front().SockAddr)->sin_addr = DNSCurveParameter.DNSCurve_Target_Server_Main_IPv4.AddressData.IPv4.sin_addr;
			reinterpret_cast<sockaddr_in *>(&SocketDataList.front().SockAddr)->sin_port = DNSCurveParameter.DNSCurve_Target_Server_Main_IPv4.AddressData.IPv4.sin_port;
			PacketTarget = &DNSCurveParameter.DNSCurve_Target_Server_Main_IPv4;
			ServerType = DNSCURVE_SERVER_TYPE::MAIN_IPV4;
		}

		SocketDataList.front().AddrLen = sizeof(sockaddr_in);
		SocketDataList.front().SockAddr.ss_family = AF_INET;
		return PacketTarget;
	}

	return nullptr;
}

//DNSCurve set packet target
bool DNSCurve_PacketTargetSetting(
	const DNSCURVE_SERVER_TYPE ServerType, 
	DNSCURVE_SERVER_DATA ** const PacketTarget)
{
	switch (ServerType)
	{
		case DNSCURVE_SERVER_TYPE::MAIN_IPV6:
		{
			*PacketTarget = &DNSCurveParameter.DNSCurve_Target_Server_Main_IPv6;
		}break;
		case DNSCURVE_SERVER_TYPE::MAIN_IPV4:
		{
			*PacketTarget = &DNSCurveParameter.DNSCurve_Target_Server_Main_IPv4;
		}break;
		case DNSCURVE_SERVER_TYPE::ALTERNATE_IPV6:
		{
			*PacketTarget = &DNSCurveParameter.DNSCurve_Target_Server_Alternate_IPv6;
		}break;
		case DNSCURVE_SERVER_TYPE::ALTERNATE_IPV4:
		{
			*PacketTarget = &DNSCurveParameter.DNSCurve_Target_Server_Alternate_IPv4;
		}break;
		default:
		{
			return false;
		}
	}

	return true;
}

//DNSCurve set Precomputation Key between client and server
bool DNSCurve_PrecomputationKeySetting(
	uint8_t * const PrecomputationKey, 
	uint8_t * const Client_PublicKey, 
	const uint8_t * const ServerFingerprint)
{
//Server fingerprint check
	if (CheckEmptyBuffer(ServerFingerprint, crypto_box_PUBLICKEYBYTES))
	{
		return false;
	}
	else {
		sodium_memzero(PrecomputationKey, crypto_box_BEFORENMBYTES);
		memset(Client_PublicKey, 0, crypto_box_PUBLICKEYBYTES);
	}

//Make a client ephemeral key pair and a precomputation key.
	DNSCURVE_HEAP_BUFFER_TABLE<uint8_t> Client_SecretKey(crypto_box_SECRETKEYBYTES);
	if (crypto_box_keypair(
			Client_PublicKey, 
			Client_SecretKey.Buffer) != 0 || 
		crypto_box_beforenm(
			PrecomputationKey, 
			ServerFingerprint, 
			Client_SecretKey.Buffer) != 0)
				return false;

	return true;
}

//DNSCurve packet precomputation
void DNSCurve_SocketPrecomputation(
	const uint16_t Protocol, 
	const uint8_t * const OriginalSend, 
	const size_t SendSize, 
	const size_t RecvSize, 
	uint8_t ** const PrecomputationKey, 
	uint8_t ** const Alternate_PrecomputationKey, 
	DNSCURVE_SERVER_DATA ** const PacketTarget, 
	std::vector<SOCKET_DATA> &SocketDataList, 
	std::vector<DNSCURVE_SOCKET_SELECTING_TABLE> &SocketSelectingDataList, 
	const uint16_t QueryType, 
	const SOCKET_DATA &LocalSocketData, 
	std::unique_ptr<uint8_t[]> &SendBuffer, 
	size_t &DataLength, 
	std::unique_ptr<uint8_t[]> &Alternate_SendBuffer, 
	size_t &Alternate_DataLength)
{
//Selecting check
	bool *IsAlternate = nullptr;
	const auto NetworkSpecific = DNSCurve_SelectTargetSocket(Protocol, QueryType, LocalSocketData, &IsAlternate);
	if (NetworkSpecific == 0)
		return;

//Initialization
	SOCKET_DATA SocketDataTemp;
	DNSCURVE_SOCKET_SELECTING_TABLE SocketSelectingDataTemp;
	memset(&SocketDataTemp, 0, sizeof(SocketDataTemp));
	SocketDataTemp.Socket = INVALID_SOCKET;
	std::vector<SOCKET_DATA> Alternate_SocketDataList;
	std::vector<DNSCURVE_SOCKET_SELECTING_TABLE> Alternate_SocketSelectingDataList;
	std::array<uint8_t, crypto_box_PUBLICKEYBYTES> Client_PublicKey_Buffer{};
	auto Client_PublicKey = Client_PublicKey_Buffer.data();
	size_t Index = 0, LoopLimits = 0;
	uint16_t TransportSpecific = 0;
	if (Protocol == IPPROTO_TCP)
		TransportSpecific = SOCK_STREAM;
	else if (Protocol == IPPROTO_UDP)
		TransportSpecific = SOCK_DGRAM;
	else 
		return;

//Main
	if (!*IsAlternate)
	{
	//Set target.
		if (NetworkSpecific == AF_INET6)
			*PacketTarget = &DNSCurveParameter.DNSCurve_Target_Server_Main_IPv6;
		else if (NetworkSpecific == AF_INET)
			*PacketTarget = &DNSCurveParameter.DNSCurve_Target_Server_Main_IPv4;
		else 
			return;

	//Encryption mode check
		if (DNSCurveParameter.IsEncryption && 
			((!DNSCurveParameter.IsClientEphemeralKey && sodium_is_zero((*PacketTarget)->PrecomputationKey, crypto_box_BEFORENMBYTES) != 0) || 
			(DNSCurveParameter.IsClientEphemeralKey && CheckEmptyBuffer((*PacketTarget)->ServerFingerprint, crypto_box_PUBLICKEYBYTES)) || 
			CheckEmptyBuffer((*PacketTarget)->SendMagicNumber, DNSCURVE_MAGIC_QUERY_LEN)))
				goto SkipProcess_Main;

	//Set loop limit.
		if (Protocol == IPPROTO_TCP)
			LoopLimits = Parameter.MultipleRequestTimes;
		else if (Protocol == IPPROTO_UDP)
			LoopLimits = 1U;
		else 
			goto SkipProcess_Main;

	//Socket initialization
		for (Index = 0;Index < LoopLimits;++Index)
		{
			SocketDataTemp.SockAddr = (*PacketTarget)->AddressData.Storage;
			if (NetworkSpecific == AF_INET6)
			{
				SocketDataTemp.Socket = socket(AF_INET6, TransportSpecific, Protocol);
			}
			else if (NetworkSpecific == AF_INET)
			{
				SocketDataTemp.Socket = socket(AF_INET, TransportSpecific, Protocol);
			}
			else {
				for (auto &SocketDataItem:SocketDataList)
					SocketSetting(SocketDataItem.Socket, SOCKET_SETTING_TYPE::CLOSE, false, nullptr);
				SocketDataList.clear();
				SocketSelectingDataList.clear();

				goto SkipProcess_Main;
			}

		//Socket attribute settings
			if (!SocketSetting(SocketDataTemp.Socket, SOCKET_SETTING_TYPE::INVALID_CHECK, true, nullptr) || 
				(TransportSpecific == IPPROTO_TCP && !SocketSetting(SocketDataTemp.Socket, SOCKET_SETTING_TYPE::TCP_FAST_OPEN_NORMAL, true, nullptr)) || 
				!SocketSetting(SocketDataTemp.Socket, SOCKET_SETTING_TYPE::NON_BLOCKING_MODE, true, nullptr) || 
				(NetworkSpecific == AF_INET6 && !SocketSetting(SocketDataTemp.Socket, SOCKET_SETTING_TYPE::HOP_LIMITS_IPV6, true, nullptr)) || 
				(NetworkSpecific == AF_INET && (!SocketSetting(SocketDataTemp.Socket, SOCKET_SETTING_TYPE::HOP_LIMITS_IPV4, true, nullptr) || 
				(TransportSpecific == IPPROTO_UDP && !SocketSetting(SocketDataTemp.Socket, SOCKET_SETTING_TYPE::DO_NOT_FRAGMENT, true, nullptr)))))
			{
				for (auto &SocketDataItem:SocketDataList)
					SocketSetting(SocketDataItem.Socket, SOCKET_SETTING_TYPE::CLOSE, false, nullptr);
				SocketDataList.clear();
				SocketSelectingDataList.clear();

				goto SkipProcess_Main;
			}

		//IPv6
			if (NetworkSpecific == AF_INET6)
			{
				SocketDataTemp.AddrLen = sizeof(sockaddr_in6);
				SocketSelectingDataTemp.ServerType = DNSCURVE_SERVER_TYPE::MAIN_IPV6;
			}
		//IPv4
			else if (NetworkSpecific == AF_INET)
			{
				SocketDataTemp.AddrLen = sizeof(sockaddr_in);
				SocketSelectingDataTemp.ServerType = DNSCURVE_SERVER_TYPE::MAIN_IPV4;
			}
			else {
				for (auto &SocketDataItem:SocketDataList)
					SocketSetting(SocketDataItem.Socket, SOCKET_SETTING_TYPE::CLOSE, false, nullptr);
				SocketDataList.clear();
				SocketSelectingDataList.clear();

				goto SkipProcess_Main;
			}

			SocketDataList.push_back(SocketDataTemp);
			SocketSelectingDataList.push_back(std::move(SocketSelectingDataTemp));
			memset(&SocketDataTemp, 0, sizeof(SocketDataTemp));
		}

	//Make Precomputation Key between client and server.
		if (DNSCurveParameter.IsEncryption && DNSCurveParameter.IsClientEphemeralKey)
		{
			if (!DNSCurve_PrecomputationKeySetting(*PrecomputationKey, Client_PublicKey, (*PacketTarget)->ServerFingerprint))
			{
				for (auto &SocketDataItem:SocketDataList)
					SocketSetting(SocketDataItem.Socket, SOCKET_SETTING_TYPE::CLOSE, false, nullptr);
				SocketDataList.clear();
				SocketSelectingDataList.clear();

				goto SkipProcess_Main;
			}
		}
		else {
			Client_PublicKey = DNSCurveParameter.Client_PublicKey;
			*PrecomputationKey = (*PacketTarget)->PrecomputationKey;
		}

	//Make encryption or normal packet of Main server.
		if (DNSCurveParameter.IsEncryption || Protocol == IPPROTO_TCP)
		{
			auto SendBufferTemp = std::make_unique<uint8_t[]>(RecvSize + MEMORY_RESERVED_BYTES);
			memset(SendBufferTemp.get(), 0, RecvSize + MEMORY_RESERVED_BYTES);
			std::swap(SendBuffer, SendBufferTemp);
			DataLength = DNSCurve_PacketEncryption(Protocol, (*PacketTarget)->SendMagicNumber, Client_PublicKey, *PrecomputationKey, OriginalSend, SendSize, SendBuffer.get(), RecvSize);
			if (DataLength < DNS_PACKET_MINSIZE)
			{
				for (auto &SocketDataItem:SocketDataList)
					SocketSetting(SocketDataItem.Socket, SOCKET_SETTING_TYPE::CLOSE, false, nullptr);
				SocketDataList.clear();
				SocketSelectingDataList.clear();
				DataLength = 0;

				goto SkipProcess_Main;
			}
		}
	}

//Jump here to skip Main process
SkipProcess_Main:
	memset(&SocketDataTemp, 0, sizeof(SocketDataTemp));

//Set target.
	if (NetworkSpecific == AF_INET6)
	{
		*PacketTarget = &DNSCurveParameter.DNSCurve_Target_Server_Alternate_IPv6;
	}
	else if (NetworkSpecific == AF_INET)
	{
		*PacketTarget = &DNSCurveParameter.DNSCurve_Target_Server_Alternate_IPv4;
	}
	else {
		for (auto &SocketDataItem:SocketDataList)
			SocketSetting(SocketDataItem.Socket, SOCKET_SETTING_TYPE::CLOSE, false, nullptr);
		SocketDataList.clear();
		SocketSelectingDataList.clear();
		DataLength = 0;
	}

//Alternate
	if ((*PacketTarget)->AddressData.Storage.ss_family != 0 && (*IsAlternate || Parameter.AlternateMultipleRequest))
	{
	//Encryption mode check
		if (DNSCurveParameter.IsEncryption && 
			((!DNSCurveParameter.IsClientEphemeralKey && sodium_is_zero((*PacketTarget)->PrecomputationKey, crypto_box_BEFORENMBYTES) != 0) || 
			(DNSCurveParameter.IsClientEphemeralKey && CheckEmptyBuffer((*PacketTarget)->ServerFingerprint, crypto_box_PUBLICKEYBYTES)) || 
			CheckEmptyBuffer((*PacketTarget)->SendMagicNumber, DNSCURVE_MAGIC_QUERY_LEN)))
		{
			for (auto &SocketDataItem:SocketDataList)
				SocketSetting(SocketDataItem.Socket, SOCKET_SETTING_TYPE::CLOSE, false, nullptr);
			SocketDataList.clear();
			SocketSelectingDataList.clear();
			DataLength = 0;

			return;
		}

	//Set loop limit.
		if (Protocol == IPPROTO_TCP)
		{
			LoopLimits = Parameter.MultipleRequestTimes;
		}
		else if (Protocol == IPPROTO_UDP)
		{
			LoopLimits = 1U;
		}
		else {
			for (auto &SocketDataItem:SocketDataList)
				SocketSetting(SocketDataItem.Socket, SOCKET_SETTING_TYPE::CLOSE, false, nullptr);
			SocketDataList.clear();
			SocketSelectingDataList.clear();
			DataLength = 0;

			return;
		}

	//Socket initialization
		for (Index = 0;Index < LoopLimits;++Index)
		{
			SocketDataTemp.SockAddr = (*PacketTarget)->AddressData.Storage;
			if (NetworkSpecific == AF_INET6)
			{
				SocketDataTemp.Socket = socket(AF_INET6, TransportSpecific, Protocol);
			}
			else if (NetworkSpecific == AF_INET)
			{
				SocketDataTemp.Socket = socket(AF_INET, TransportSpecific, Protocol);
			}
			else {
				for (auto &SocketDataItem:SocketDataList)
					SocketSetting(SocketDataItem.Socket, SOCKET_SETTING_TYPE::CLOSE, false, nullptr);
				SocketDataList.clear();
				SocketSelectingDataList.clear();
				DataLength = 0;
				for (auto &SocketDataItem:Alternate_SocketDataList)
					SocketSetting(SocketDataItem.Socket, SOCKET_SETTING_TYPE::CLOSE, false, nullptr);
				Alternate_SocketDataList.clear();
				Alternate_SocketSelectingDataList.clear();

				return;
			}

		//Socket attribute settings
			if (!SocketSetting(SocketDataTemp.Socket, SOCKET_SETTING_TYPE::INVALID_CHECK, true, nullptr) || 
				(Protocol == IPPROTO_TCP && !SocketSetting(SocketDataTemp.Socket, SOCKET_SETTING_TYPE::TCP_FAST_OPEN_NORMAL, true, nullptr)) || 
				!SocketSetting(SocketDataTemp.Socket, SOCKET_SETTING_TYPE::NON_BLOCKING_MODE, true, nullptr) || 
				(NetworkSpecific == AF_INET6 && !SocketSetting(SocketDataTemp.Socket, SOCKET_SETTING_TYPE::HOP_LIMITS_IPV6, true, nullptr)) || 
				(NetworkSpecific == AF_INET && (!SocketSetting(SocketDataTemp.Socket, SOCKET_SETTING_TYPE::HOP_LIMITS_IPV4, true, nullptr) || 
				(Protocol == IPPROTO_UDP && !SocketSetting(SocketDataTemp.Socket, SOCKET_SETTING_TYPE::DO_NOT_FRAGMENT, true, nullptr)))))
			{
				for (auto &SocketDataItem:SocketDataList)
					SocketSetting(SocketDataItem.Socket, SOCKET_SETTING_TYPE::CLOSE, false, nullptr);
				SocketDataList.clear();
				SocketSelectingDataList.clear();
				DataLength = 0;
				for (auto &SocketDataItem:Alternate_SocketDataList)
					SocketSetting(SocketDataItem.Socket, SOCKET_SETTING_TYPE::CLOSE, false, nullptr);
				Alternate_SocketDataList.clear();
				Alternate_SocketSelectingDataList.clear();

				return;
			}

		//IPv6
			if (NetworkSpecific == AF_INET6)
			{
				SocketDataTemp.AddrLen = sizeof(sockaddr_in6);
				SocketSelectingDataTemp.ServerType = DNSCURVE_SERVER_TYPE::ALTERNATE_IPV6;
			}
		//IPv4
			else if (NetworkSpecific == AF_INET)
			{
				SocketDataTemp.AddrLen = sizeof(sockaddr_in);
				SocketSelectingDataTemp.ServerType = DNSCURVE_SERVER_TYPE::ALTERNATE_IPV4;
			}
			else {
				for (auto &SocketDataItem:SocketDataList)
					SocketSetting(SocketDataItem.Socket, SOCKET_SETTING_TYPE::CLOSE, false, nullptr);
				SocketDataList.clear();
				SocketSelectingDataList.clear();
				DataLength = 0;
				for (auto &SocketDataItem:Alternate_SocketDataList)
					SocketSetting(SocketDataItem.Socket, SOCKET_SETTING_TYPE::CLOSE, false, nullptr);
				Alternate_SocketDataList.clear();
				Alternate_SocketSelectingDataList.clear();

				return;
			}

			Alternate_SocketDataList.push_back(SocketDataTemp);
			Alternate_SocketSelectingDataList.push_back(std::move(SocketSelectingDataTemp));
			memset(&SocketDataTemp, 0, sizeof(SocketDataTemp));
		}

	//Make Precomputation Key between client and server.
		if (DNSCurveParameter.IsEncryption && DNSCurveParameter.IsClientEphemeralKey)
		{
			if (!DNSCurve_PrecomputationKeySetting(*Alternate_PrecomputationKey, Client_PublicKey, (*PacketTarget)->ServerFingerprint))
			{
				for (auto &SocketDataItem:SocketDataList)
					SocketSetting(SocketDataItem.Socket, SOCKET_SETTING_TYPE::CLOSE, false, nullptr);
				SocketDataList.clear();
				SocketSelectingDataList.clear();
				DataLength = 0;
				for (auto &SocketDataItem:Alternate_SocketDataList)
					SocketSetting(SocketDataItem.Socket, SOCKET_SETTING_TYPE::CLOSE, false, nullptr);
				Alternate_SocketDataList.clear();
				Alternate_SocketSelectingDataList.clear();

				return;
			}
		}
		else {
			Client_PublicKey = DNSCurveParameter.Client_PublicKey;
			*Alternate_PrecomputationKey = (*PacketTarget)->PrecomputationKey;
		}

	//Make encryption or normal packet of Alternate server.
		if (DNSCurveParameter.IsEncryption)
		{
			auto SendBufferTemp = std::make_unique<uint8_t[]>(RecvSize + MEMORY_RESERVED_BYTES);
			memset(SendBufferTemp.get(), 0, RecvSize + MEMORY_RESERVED_BYTES);
			std::swap(Alternate_SendBuffer, SendBufferTemp);
			SendBufferTemp.reset();
			Alternate_DataLength = DNSCurve_PacketEncryption(Protocol, (*PacketTarget)->SendMagicNumber, Client_PublicKey, *Alternate_PrecomputationKey, OriginalSend, SendSize, Alternate_SendBuffer.get(), RecvSize);
			if (Alternate_DataLength < DNS_PACKET_MINSIZE)
			{
				for (auto &SocketDataItem:SocketDataList)
					SocketSetting(SocketDataItem.Socket, SOCKET_SETTING_TYPE::CLOSE, false, nullptr);
				SocketDataList.clear();
				SocketSelectingDataList.clear();
				DataLength = 0;
				for (auto &SocketDataItem:Alternate_SocketDataList)
					SocketSetting(SocketDataItem.Socket, SOCKET_SETTING_TYPE::CLOSE, false, nullptr);
				Alternate_SocketDataList.clear();
				Alternate_SocketSelectingDataList.clear();
				Alternate_DataLength = 0;

				return;
			}
		}

	//Register to global list.
		if (!Alternate_SocketDataList.empty() && !Alternate_SocketSelectingDataList.empty())
		{
			for (auto &SocketDataItem:Alternate_SocketDataList)
				SocketDataList.push_back(SocketDataItem);
			for (auto &SocketSelectingItem:Alternate_SocketSelectingDataList)
				SocketSelectingDataList.push_back(std::move(SocketSelectingItem));
		}
	}

	return;
}

//DNSCurve packet encryption
size_t DNSCurve_PacketEncryption(
	const uint16_t Protocol, 
	const uint8_t * const SendMagicNumber, 
	const uint8_t * const Client_PublicKey, 
	const uint8_t * const PrecomputationKey, 
	const uint8_t * const OriginalSend, 
	const size_t Length, 
	uint8_t * const SendBuffer, 
	const size_t SendSize)
{
//Encryption mode
	if (DNSCurveParameter.IsEncryption)
	{
	//Make nonce.
		DNSCURVE_HEAP_BUFFER_TABLE<uint8_t> Nonce(crypto_box_NONCEBYTES);
		GenerateRandomBuffer(Nonce.Buffer, crypto_box_HALF_NONCEBYTES, nullptr, 0, 0);

	//Buffer initialization
		std::unique_ptr<uint8_t[]> Buffer(nullptr);
		if (Protocol == IPPROTO_TCP || Protocol == IPPROTO_UDP)
		{
			auto BufferTemp = std::make_unique<uint8_t[]>(DNSCurveParameter.DNSCurvePayloadSize - DNSCRYPT_BUFFER_RESERVED_LEN);
			memset(BufferTemp.get(), 0, DNSCurveParameter.DNSCurvePayloadSize - DNSCRYPT_BUFFER_RESERVED_LEN);
			std::swap(Buffer, BufferTemp);
		}
		else {
			return EXIT_FAILURE;
		}

	//Make a crypto box.
		memcpy_s(Buffer.get() + crypto_box_ZEROBYTES, DNSCurveParameter.DNSCurvePayloadSize - DNSCRYPT_BUFFER_RESERVED_LEN - crypto_box_ZEROBYTES, OriginalSend, Length);
		DNSCurve_PaddingData(true, Buffer.get(), crypto_box_ZEROBYTES + Length, DNSCurveParameter.DNSCurvePayloadSize - DNSCRYPT_BUFFER_RESERVED_LEN);

	//Encrypt data.
		if (Protocol == IPPROTO_TCP)
		{
			if (crypto_box_afternm(
					SendBuffer + DNSCRYPT_BUFFER_RESERVED_TCP_LEN, 
					Buffer.get(), 
					DNSCurveParameter.DNSCurvePayloadSize - DNSCRYPT_BUFFER_RESERVED_TCP_LEN, 
					Nonce.Buffer, 
					PrecomputationKey) != 0)
						return EXIT_FAILURE;
		}
		else if (Protocol == IPPROTO_UDP)
		{
			if (crypto_box_afternm(
					SendBuffer + DNSCRYPT_BUFFER_RESERVED_LEN, 
					Buffer.get(), 
					DNSCurveParameter.DNSCurvePayloadSize - DNSCRYPT_BUFFER_RESERVED_LEN, 
					Nonce.Buffer, 
					PrecomputationKey) != 0)
						return EXIT_FAILURE;
		}
		else {
			return EXIT_FAILURE;
		}

	//Make DNSCurve encryption packet.
		Buffer.reset();
		if (Protocol == IPPROTO_TCP)
		{
			memcpy_s(SendBuffer + sizeof(uint16_t), SendSize - sizeof(uint16_t), SendMagicNumber, DNSCURVE_MAGIC_QUERY_LEN);
			memcpy_s(SendBuffer + sizeof(uint16_t) + DNSCURVE_MAGIC_QUERY_LEN, SendSize - sizeof(uint16_t) - DNSCURVE_MAGIC_QUERY_LEN, Client_PublicKey, crypto_box_PUBLICKEYBYTES);
			memcpy_s(SendBuffer + sizeof(uint16_t) + DNSCURVE_MAGIC_QUERY_LEN + crypto_box_PUBLICKEYBYTES, SendSize - sizeof(uint16_t) - DNSCURVE_MAGIC_QUERY_LEN - crypto_box_PUBLICKEYBYTES, Nonce.Buffer, crypto_box_HALF_NONCEBYTES);

		//Add length of request packet.
			*reinterpret_cast<uint16_t *>(SendBuffer) = hton16(static_cast<const uint16_t>(DNSCurveParameter.DNSCurvePayloadSize - sizeof(uint16_t)));
		}
		else if (Protocol == IPPROTO_UDP)
		{
			memcpy_s(SendBuffer, SendSize, SendMagicNumber, DNSCURVE_MAGIC_QUERY_LEN);
			memcpy_s(SendBuffer + DNSCURVE_MAGIC_QUERY_LEN, SendSize - DNSCURVE_MAGIC_QUERY_LEN, Client_PublicKey, crypto_box_PUBLICKEYBYTES);
			memcpy_s(SendBuffer + DNSCURVE_MAGIC_QUERY_LEN + crypto_box_PUBLICKEYBYTES, SendSize - DNSCURVE_MAGIC_QUERY_LEN - crypto_box_PUBLICKEYBYTES, Nonce.Buffer, crypto_box_HALF_NONCEBYTES);
		}
		else {
			return EXIT_FAILURE;
		}

		return DNSCurveParameter.DNSCurvePayloadSize;
	}
//Normal mode
	else {
		memcpy_s(SendBuffer, SendSize, OriginalSend, Length);

	//Add length of request packet.
		if (Protocol == IPPROTO_TCP)
			return AddLengthDataToHeader(SendBuffer, Length, SendSize);
		else if (Protocol == IPPROTO_UDP)
			return Length;
	}

	return EXIT_FAILURE;
}

//DNSCurve packet decryption
ssize_t DNSCurve_PacketDecryption(
	const uint8_t * const ReceiveMagicNumber, 
	const uint8_t * const PrecomputationKey, 
	uint8_t * const OriginalRecv, 
	const size_t RecvSize, 
	const ssize_t Length)
{
	auto DataLength = Length;

//Encryption mode
	if (DNSCurveParameter.IsEncryption)
	{
	//Receive Magic number check
		memset(OriginalRecv + Length, 0, RecvSize - Length);
		if (memcmp(OriginalRecv, ReceiveMagicNumber, DNSCURVE_MAGIC_QUERY_LEN) != 0)
			return EXIT_FAILURE;

	//Nonce initialization
		DNSCURVE_HEAP_BUFFER_TABLE<uint8_t> WholeNonce(crypto_box_NONCEBYTES);
		memcpy_s(WholeNonce.Buffer, crypto_box_NONCEBYTES, OriginalRecv + DNSCURVE_MAGIC_QUERY_LEN, crypto_box_NONCEBYTES);

	//Open crypto box.
		memset(OriginalRecv, 0, DNSCURVE_MAGIC_QUERY_LEN + crypto_box_NONCEBYTES);
		memmove_s(OriginalRecv + crypto_box_BOXZEROBYTES, RecvSize - crypto_box_BOXZEROBYTES, OriginalRecv + DNSCURVE_MAGIC_QUERY_LEN + crypto_box_NONCEBYTES, Length - (DNSCURVE_MAGIC_QUERY_LEN + crypto_box_NONCEBYTES));
		if (crypto_box_open_afternm(
				reinterpret_cast<unsigned char *>(OriginalRecv), 
				reinterpret_cast<unsigned char *>(OriginalRecv), 
				Length + static_cast<const ssize_t>(crypto_box_BOXZEROBYTES) - static_cast<const ssize_t>(DNSCURVE_MAGIC_QUERY_LEN + crypto_box_NONCEBYTES), 
				WholeNonce.Buffer, 
				PrecomputationKey) != 0)
					return EXIT_FAILURE;
		memmove_s(OriginalRecv, RecvSize, OriginalRecv + crypto_box_ZEROBYTES, Length - (DNSCURVE_MAGIC_QUERY_LEN + crypto_box_NONCEBYTES));
		memset(OriginalRecv + Length - (DNSCURVE_MAGIC_QUERY_LEN + crypto_box_NONCEBYTES), 0, RecvSize - (Length - (DNSCURVE_MAGIC_QUERY_LEN + crypto_box_NONCEBYTES)));

	//Check padding data and responses check.
		DataLength = DNSCurve_PaddingData(false, OriginalRecv, Length, RecvSize);
		if (DataLength < static_cast<const ssize_t>(DNS_PACKET_MINSIZE))
			return EXIT_FAILURE;
	}

//Response check
	DataLength = CheckResponseData(
		REQUEST_PROCESS_TYPE::DNSCURVE_MAIN, 
		OriginalRecv, 
		DataLength, 
		RecvSize, 
		nullptr, 
		nullptr);
	if (DataLength < static_cast<const ssize_t>(DNS_PACKET_MINSIZE))
		return EXIT_FAILURE;

	return DataLength;
}

//Get Signature Data of server from packets
bool DNSCruve_GetSignatureData(
	const uint8_t * const Buffer, 
	const DNSCURVE_SERVER_TYPE ServerType)
{
	if (ntoh16(reinterpret_cast<const dns_record_txt *>(Buffer)->Name) == DNS_POINTER_QUERY && 
		ntoh16(reinterpret_cast<const dns_record_txt *>(Buffer)->Length) == reinterpret_cast<const dns_record_txt *>(Buffer)->TXT_Length + NULL_TERMINATE_LENGTH && 
		reinterpret_cast<const dns_record_txt *>(Buffer)->TXT_Length == DNSCRYPT_RECORD_TXT_LEN && 
		memcmp(&reinterpret_cast<const dnscurve_txt_hdr *>(Buffer + sizeof(dns_record_txt))->CertMagicNumber, DNSCRYPT_CERT_MAGIC, sizeof(uint16_t)) == 0 && 
		ntoh16(reinterpret_cast<const dnscurve_txt_hdr *>(Buffer + sizeof(dns_record_txt))->MinorVersion) == DNSCURVE_VERSION_MINOR)
	{
		if (ntoh16(reinterpret_cast<const dnscurve_txt_hdr *>(Buffer + sizeof(dns_record_txt))->MajorVersion) == DNSCURVE_ES_X25519_XSALSA20_POLY1305) //DNSCurve X25519-XSalsa20Poly1305
		{
		//Get Send Magic Number, Server Fingerprint and Precomputation Key.
			DNSCURVE_SERVER_DATA *PacketTarget = nullptr;
			if (!DNSCurve_PacketTargetSetting(ServerType, &PacketTarget))
				return false;

		//Check signature.
			DNSCURVE_HEAP_BUFFER_TABLE<uint8_t> DecryptBuffer(PACKET_NORMAL_MAXSIZE + MEMORY_RESERVED_BYTES);
			unsigned long long SignatureLength = 0;
			if (PacketTarget == nullptr || 
				crypto_sign_open(
					reinterpret_cast<unsigned char *>(DecryptBuffer.Buffer), 
					&SignatureLength, 
					reinterpret_cast<const unsigned char *>(Buffer + sizeof(dns_record_txt) + sizeof(dnscurve_txt_hdr)), 
					reinterpret_cast<const dns_record_txt *>(Buffer)->TXT_Length - sizeof(dnscurve_txt_hdr), 
					PacketTarget->ServerPublicKey) != 0)
			{
				std::wstring Message;
				PrintLog_DNSCurve(ServerType, Message);
				if (!Message.empty())
				{
					Message.append(L"Fingerprint signature validation error");
					PrintError(LOG_LEVEL_TYPE::LEVEL_3, LOG_ERROR_TYPE::DNSCURVE, Message.c_str(), 0, nullptr, 0);
				}
				else {
					PrintError(LOG_LEVEL_TYPE::LEVEL_3, LOG_ERROR_TYPE::DNSCURVE, L"Fingerprint signature validation error", 0, nullptr, 0);
				}

				return false;
			}

		//Signature available time check
			const auto TimeValues = time(nullptr);
			if (TimeValues > 0 && PacketTarget->ServerFingerprint != nullptr && 
				TimeValues >= static_cast<const time_t>(ntoh32(reinterpret_cast<const dnscurve_txt_signature *>(DecryptBuffer.Buffer)->CertTime_Begin)) && 
				TimeValues <= static_cast<const time_t>(ntoh32(reinterpret_cast<const dnscurve_txt_signature *>(DecryptBuffer.Buffer)->CertTime_End)))
			{
				memcpy_s(PacketTarget->SendMagicNumber, DNSCURVE_MAGIC_QUERY_LEN, reinterpret_cast<dnscurve_txt_signature *>(DecryptBuffer.Buffer)->MagicNumber, DNSCURVE_MAGIC_QUERY_LEN);
				memcpy_s(PacketTarget->ServerFingerprint, crypto_box_PUBLICKEYBYTES, reinterpret_cast<dnscurve_txt_signature *>(DecryptBuffer.Buffer)->PublicKey, crypto_box_PUBLICKEYBYTES);
				if (!DNSCurveParameter.IsClientEphemeralKey)
				{
					if (crypto_box_beforenm(
							PacketTarget->PrecomputationKey, 
							PacketTarget->ServerFingerprint, 
							DNSCurveParameter.Client_SecretKey) != 0)
					{
						std::wstring Message;
						PrintLog_DNSCurve(ServerType, Message);
						if (!Message.empty())
						{
							Message.append(L"Key calculating error");
							PrintError(LOG_LEVEL_TYPE::LEVEL_3, LOG_ERROR_TYPE::DNSCURVE, Message.c_str(), 0, nullptr, 0);
						}
						else {
							PrintError(LOG_LEVEL_TYPE::LEVEL_3, LOG_ERROR_TYPE::DNSCURVE, L"Key calculating error", 0, nullptr, 0);
						}

						return false;
					}
				}

				return true;
			}
			else {
				std::wstring Message;
				PrintLog_DNSCurve(ServerType, Message);
				if (!Message.empty())
				{
					Message.append(L"Fingerprint signature is not available in this time");
					PrintError(LOG_LEVEL_TYPE::LEVEL_3, LOG_ERROR_TYPE::DNSCURVE, Message.c_str(), 0, nullptr, 0);
				}
				else {
					PrintError(LOG_LEVEL_TYPE::LEVEL_3, LOG_ERROR_TYPE::DNSCURVE, L"Fingerprint signature is not available in this time", 0, nullptr, 0);
				}
			}
		}
		else {
			PrintError(LOG_LEVEL_TYPE::LEVEL_3, LOG_ERROR_TYPE::DNSCURVE, L"DNSCurve version is not supported", 0, nullptr, 0);
		}
	}

	return false;
}
#endif
