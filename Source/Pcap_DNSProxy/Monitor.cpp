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


#include "Monitor.h"

//Monitor launcher process
void MonitorLauncher(
	void)
{
//Network monitor(Mark Local DNS address to PTR records)
	ParameterModificating.SetToMonitorItem();
	std::thread Thread_NetworkInformationMonitor(std::bind(NetworkInformationMonitor));
	Thread_NetworkInformationMonitor.detach();

//DNSCurve initialization(Encryption mode)
#if defined(ENABLE_LIBSODIUM)
	if (Parameter.IsDNSCurve)
	{
		DNSCurveParameterModificating.SetToMonitorItem();
		if (DNSCurveParameter.IsEncryption)
			DNSCurveInit();
	}
#endif

//Read parameter(Monitor mode)
	std::thread Thread_ReadParameter(std::bind(ReadParameter, false));
	Thread_ReadParameter.detach();

//Read Hosts monitor
	if (!GlobalRunningStatus.FileList_Hosts->empty())
	{
		std::thread Thread_ReadHosts(std::bind(ReadHosts));
		Thread_ReadHosts.detach();
	}

//Read IPFilter monitor
	if (!GlobalRunningStatus.FileList_IPFilter->empty() && 
		(Parameter.OperationMode == LISTEN_MODE::CUSTOM || Parameter.DataCheck_Blacklist || Parameter.IsLocalRouting))
	{
		std::thread Thread_ReadIPFilter(std::bind(ReadIPFilter));
		Thread_ReadIPFilter.detach();
	}

//Capture monitor
#if defined(ENABLE_PCAP)
	if (Parameter.IsPcapCapture && 
	//Force protocol(TCP).
		Parameter.RequestMode_Transport != REQUEST_MODE_TRANSPORT::FORCE_TCP && 
	//Direct Request mode
		Parameter.DirectRequest_Protocol != REQUEST_MODE_DIRECT::BOTH && 
		!(Parameter.DirectRequest_Protocol == REQUEST_MODE_DIRECT::IPV6 && Parameter.Target_Server_Main_IPv4.AddressData.Storage.ss_family == 0) && 
		!(Parameter.DirectRequest_Protocol == REQUEST_MODE_DIRECT::IPV4 && Parameter.Target_Server_Main_IPv6.AddressData.Storage.ss_family == 0) && 
	//SOCKS request only mode
		!(Parameter.SOCKS_Proxy && Parameter.SOCKS_Only) && 
	//HTTP CONNECT request only mode
		!(Parameter.HTTP_CONNECT_Proxy && Parameter.HTTP_CONNECT_Only)
	//DNSCurve request only mode
	#if defined(ENABLE_LIBSODIUM)
		&& !(Parameter.IsDNSCurve && DNSCurveParameter.IsEncryptionOnly)
	#endif
		)
	{
	#if defined(ENABLE_PCAP)
		std::thread Thread_CaptureInitialization(std::bind(CaptureInit));
		Thread_CaptureInitialization.detach();
	#endif

	//Get Hop Limits with normal DNS request.
	//IPv6
		if (Parameter.Target_Server_Main_IPv6.AddressData.Storage.ss_family != 0 && 
			(Parameter.RequestMode_Network == REQUEST_MODE_NETWORK::BOTH || Parameter.RequestMode_Network == REQUEST_MODE_NETWORK::IPV6)) //IPv6
		{
			std::thread Thread_TestDoamin_IPv6(std::bind(TestRequest_Domain, static_cast<const uint16_t>(AF_INET6)));
			Thread_TestDoamin_IPv6.detach();
		}
	//IPv4
		if (Parameter.Target_Server_Main_IPv4.AddressData.Storage.ss_family != 0 && 
			(Parameter.RequestMode_Network == REQUEST_MODE_NETWORK::BOTH || Parameter.RequestMode_Network == REQUEST_MODE_NETWORK::IPV4)) //IPv4
		{
			std::thread Thread_TestDoamin_IPv4(std::bind(TestRequest_Domain, static_cast<const uint16_t>(AF_INET)));
			Thread_TestDoamin_IPv4.detach();
		}

	//Get Hop Limits with ICMPv6 and ICMP echo.
	//IPv6
		if (Parameter.Target_Server_Main_IPv6.AddressData.Storage.ss_family != 0 && 
			(Parameter.RequestMode_Network == REQUEST_MODE_NETWORK::BOTH || Parameter.RequestMode_Network == REQUEST_MODE_NETWORK::IPV6)) //IPv6
		{
			std::thread Thread_ICMPv6(std::bind(TestRequest_ICMP, static_cast<const uint16_t>(AF_INET6)));
			Thread_ICMPv6.detach();
		}
	//IPv4
		if (Parameter.Target_Server_Main_IPv4.AddressData.Storage.ss_family != 0 && 
			(Parameter.RequestMode_Network == REQUEST_MODE_NETWORK::BOTH || Parameter.RequestMode_Network == REQUEST_MODE_NETWORK::IPV4)) //IPv4
		{
			std::thread Thread_ICMP(std::bind(TestRequest_ICMP, static_cast<const uint16_t>(AF_INET)));
			Thread_ICMP.detach();
		}
	}
#endif

//Alternate server monitor(Set Preferred DNS servers switcher)
	if ((!Parameter.AlternateMultipleRequest && 
		(Parameter.Target_Server_Alternate_IPv6.AddressData.Storage.ss_family != 0 || 
			Parameter.Target_Server_Alternate_IPv4.AddressData.Storage.ss_family != 0
	#if defined(ENABLE_LIBSODIUM)
		|| DNSCurveParameter.DNSCurve_Target_Server_Alternate_IPv6.AddressData.Storage.ss_family != 0
		|| DNSCurveParameter.DNSCurve_Target_Server_Alternate_IPv4.AddressData.Storage.ss_family != 0
	#endif
		)) || Parameter.Target_Server_Local_Alternate_IPv6.Storage.ss_family != 0 || 
		Parameter.Target_Server_Local_Alternate_IPv4.Storage.ss_family != 0)
	{
		std::thread Thread_AlternateServerSwitcher(std::bind(AlternateServerSwitcher));
		Thread_AlternateServerSwitcher.detach();
	}

//Mailslot and FIFO pipe listener
	if (Parameter.IsProcessUnique)
	{
	#if defined(PLATFORM_WIN)
		std::thread Thread_FlushDomainCache_MailslotListener(std::bind(FlushDomainCache_MailslotListener));
		Thread_FlushDomainCache_MailslotListener.detach();
	#elif (defined(PLATFORM_FREEBSD) || defined(PLATFORM_LINUX) || defined(PLATFORM_MACOS))
		std::thread Thread_FlushDomainCache_PipeListener(std::bind(FlushDomainCache_PipeListener));
		Thread_FlushDomainCache_PipeListener.detach();
	#endif
	}

	return;
}

//Local DNS server initialization
bool MonitorInit(
	void)
{
//Initialization
	std::vector<SOCKET_DATA> LocalSocketDataList;
	std::vector<uint16_t> LocalSocketProtocol;
	SOCKET_DATA LocalSocketData;
	memset(&LocalSocketData, 0, sizeof(LocalSocketData));
	LocalSocketData.Socket = INVALID_SOCKET;

//Set local machine Monitor sockets(IPv6/UDP).
	if (Parameter.ListenProtocol_Network == LISTEN_PROTOCOL_NETWORK::BOTH || Parameter.ListenProtocol_Network == LISTEN_PROTOCOL_NETWORK::IPV6)
	{
		if (Parameter.ListenProtocol_Transport == LISTEN_PROTOCOL_TRANSPORT::BOTH || Parameter.ListenProtocol_Transport == LISTEN_PROTOCOL_TRANSPORT::UDP)
		{
		//Socket check
			memset(&LocalSocketData, 0, sizeof(LocalSocketData));
			LocalSocketData.Socket = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
			if (!SocketSetting(LocalSocketData.Socket, SOCKET_SETTING_TYPE::INVALID_CHECK, true, nullptr))
			{
				SocketSetting(LocalSocketData.Socket, SOCKET_SETTING_TYPE::CLOSE, false, nullptr);
			}
		//Socket initialization
			else {
				GlobalRunningStatus.LocalListeningSocket->push_back(LocalSocketData.Socket);
				LocalSocketData.SockAddr.ss_family = AF_INET6;
				LocalSocketData.AddrLen = sizeof(sockaddr_in6);

			//Listen Address available
				if (Parameter.ListenAddress_IPv6 != nullptr)
				{
					for (const auto &ListenAddressItem:*Parameter.ListenAddress_IPv6)
					{
						if (!SocketSetting(LocalSocketData.Socket, SOCKET_SETTING_TYPE::INVALID_CHECK, false, nullptr))
						{
							LocalSocketData.Socket = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
							if (!SocketSetting(LocalSocketData.Socket, SOCKET_SETTING_TYPE::INVALID_CHECK, true, nullptr))
							{
								SocketSetting(LocalSocketData.Socket, SOCKET_SETTING_TYPE::CLOSE, false, nullptr);
								break;
							}
							else {
								GlobalRunningStatus.LocalListeningSocket->push_back(LocalSocketData.Socket);
							}
						}

						reinterpret_cast<sockaddr_in6 *>(&LocalSocketData.SockAddr)->sin6_addr = reinterpret_cast<const sockaddr_in6 *>(&ListenAddressItem)->sin6_addr;
						reinterpret_cast<sockaddr_in6 *>(&LocalSocketData.SockAddr)->sin6_port = reinterpret_cast<const sockaddr_in6 *>(&ListenAddressItem)->sin6_port;

					//Try to bind socket to system.
						if (!ListenMonitor_BindSocket(IPPROTO_UDP, LocalSocketData))
						{
						//Close all sockets.
							for (auto &SocketDataItem:LocalSocketDataList)
								SocketSetting(SocketDataItem.Socket, SOCKET_SETTING_TYPE::CLOSE, false, nullptr);

							return false;
						}
						else {
							LocalSocketDataList.push_back(LocalSocketData);
							LocalSocketProtocol.push_back(IPPROTO_UDP);
							LocalSocketData.Socket = INVALID_SOCKET;
						}
					}
				}
				else {
				//Normal mode
					if (Parameter.ListenPort != nullptr)
					{
					//Proxy Mode
						if (Parameter.OperationMode == LISTEN_MODE::PROXY)
							reinterpret_cast<sockaddr_in6 *>(&LocalSocketData.SockAddr)->sin6_addr = in6addr_loopback;
					//Server Mode, Priavte Mode and Custom Mode
						else 
							reinterpret_cast<sockaddr_in6 *>(&LocalSocketData.SockAddr)->sin6_addr = in6addr_any;

					//Set monitor port.
						for (const auto &ListenPortItem:*Parameter.ListenPort)
						{
							if (!SocketSetting(LocalSocketData.Socket, SOCKET_SETTING_TYPE::INVALID_CHECK, false, nullptr))
							{
								LocalSocketData.Socket = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
								if (!SocketSetting(LocalSocketData.Socket, SOCKET_SETTING_TYPE::INVALID_CHECK, true, nullptr))
								{
									SocketSetting(LocalSocketData.Socket, SOCKET_SETTING_TYPE::CLOSE, false, nullptr);
									break;
								}
								else {
									GlobalRunningStatus.LocalListeningSocket->push_back(LocalSocketData.Socket);
								}
							}

							reinterpret_cast<sockaddr_in6 *>(&LocalSocketData.SockAddr)->sin6_port = ListenPortItem;

						//Try to bind socket to system.
							if (!ListenMonitor_BindSocket(IPPROTO_UDP, LocalSocketData))
							{
							//Close all sockets.
								for (auto &SocketDataItem:LocalSocketDataList)
									SocketSetting(SocketDataItem.Socket, SOCKET_SETTING_TYPE::CLOSE, false, nullptr);

								return false;
							}
							else {
								LocalSocketDataList.push_back(LocalSocketData);
								LocalSocketProtocol.push_back(IPPROTO_UDP);
								LocalSocketData.Socket = INVALID_SOCKET;
							}
						}
					}
				}
			}
		}

	//Set local machine Monitor sockets(IPv6/TCP).
		if (Parameter.ListenProtocol_Transport == LISTEN_PROTOCOL_TRANSPORT::BOTH || Parameter.ListenProtocol_Transport == LISTEN_PROTOCOL_TRANSPORT::TCP)
		{
		//Socket check
			memset(&LocalSocketData, 0, sizeof(LocalSocketData));
			LocalSocketData.Socket = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
			if (!SocketSetting(LocalSocketData.Socket, SOCKET_SETTING_TYPE::INVALID_CHECK, true, nullptr))
			{
				SocketSetting(LocalSocketData.Socket, SOCKET_SETTING_TYPE::CLOSE, false, nullptr);
			}
		//Socket initialization
			else {
				GlobalRunningStatus.LocalListeningSocket->push_back(LocalSocketData.Socket);
				LocalSocketData.SockAddr.ss_family = AF_INET6;
				LocalSocketData.AddrLen = sizeof(sockaddr_in6);

			//Listen Address available
				if (Parameter.ListenAddress_IPv6 != nullptr)
				{
					for (const auto &ListenAddressItem:*Parameter.ListenAddress_IPv6)
					{
						if (!SocketSetting(LocalSocketData.Socket, SOCKET_SETTING_TYPE::INVALID_CHECK, false, nullptr))
						{
							LocalSocketData.Socket = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
							if (!SocketSetting(LocalSocketData.Socket, SOCKET_SETTING_TYPE::INVALID_CHECK, true, nullptr))
							{
								SocketSetting(LocalSocketData.Socket, SOCKET_SETTING_TYPE::CLOSE, false, nullptr);
								break;
							}
							else {
								GlobalRunningStatus.LocalListeningSocket->push_back(LocalSocketData.Socket);
							}
						}

						reinterpret_cast<sockaddr_in6 *>(&LocalSocketData.SockAddr)->sin6_addr = reinterpret_cast<const sockaddr_in6 *>(&ListenAddressItem)->sin6_addr;
						reinterpret_cast<sockaddr_in6 *>(&LocalSocketData.SockAddr)->sin6_port = reinterpret_cast<const sockaddr_in6 *>(&ListenAddressItem)->sin6_port;

					//Try to bind socket to system.
						if (!ListenMonitor_BindSocket(IPPROTO_TCP, LocalSocketData))
						{
						//Close all sockets.
							for (auto &SocketDataItem:LocalSocketDataList)
								SocketSetting(SocketDataItem.Socket, SOCKET_SETTING_TYPE::CLOSE, false, nullptr);

							return false;
						}
						else {
							LocalSocketDataList.push_back(LocalSocketData);
							LocalSocketProtocol.push_back(IPPROTO_TCP);
							LocalSocketData.Socket = INVALID_SOCKET;
						}
					}
				}
				else {
				//Mormal mode
					if (Parameter.ListenPort != nullptr)
					{
					//Proxy Mode
						if (Parameter.OperationMode == LISTEN_MODE::PROXY)
							reinterpret_cast<sockaddr_in6 *>(&LocalSocketData.SockAddr)->sin6_addr = in6addr_loopback;
					//Server Mode, Priavte Mode and Custom Mode
						else 
							reinterpret_cast<sockaddr_in6 *>(&LocalSocketData.SockAddr)->sin6_addr = in6addr_any;

					//Set monitor port.
						for (const auto &ListenPortItem:*Parameter.ListenPort)
						{
							if (!SocketSetting(LocalSocketData.Socket, SOCKET_SETTING_TYPE::INVALID_CHECK, false, nullptr))
							{
								LocalSocketData.Socket = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
								if (!SocketSetting(LocalSocketData.Socket, SOCKET_SETTING_TYPE::INVALID_CHECK, true, nullptr))
								{
									SocketSetting(LocalSocketData.Socket, SOCKET_SETTING_TYPE::CLOSE, false, nullptr);
									break;
								}
								else {
									GlobalRunningStatus.LocalListeningSocket->push_back(LocalSocketData.Socket);
								}
							}

							reinterpret_cast<sockaddr_in6 *>(&LocalSocketData.SockAddr)->sin6_port = ListenPortItem;

						//Try to bind socket to system.
							if (!ListenMonitor_BindSocket(IPPROTO_TCP, LocalSocketData))
							{
							//Close all sockets.
								for (auto &SocketDataItem:LocalSocketDataList)
									SocketSetting(SocketDataItem.Socket, SOCKET_SETTING_TYPE::CLOSE, false, nullptr);

								return false;
							}
							else {
								LocalSocketDataList.push_back(LocalSocketData);
								LocalSocketProtocol.push_back(IPPROTO_TCP);
								LocalSocketData.Socket = INVALID_SOCKET;
							}
						}
					}
				}
			}
		}
	}

//Set local machine Monitor sockets(IPv4/UDP).
	if (Parameter.ListenProtocol_Network == LISTEN_PROTOCOL_NETWORK::BOTH || Parameter.ListenProtocol_Network == LISTEN_PROTOCOL_NETWORK::IPV4)
	{
		if (Parameter.ListenProtocol_Transport == LISTEN_PROTOCOL_TRANSPORT::BOTH || Parameter.ListenProtocol_Transport == LISTEN_PROTOCOL_TRANSPORT::UDP)
		{
		//Socket check
			memset(&LocalSocketData, 0, sizeof(LocalSocketData));
			LocalSocketData.Socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
			if (!SocketSetting(LocalSocketData.Socket, SOCKET_SETTING_TYPE::INVALID_CHECK, true, nullptr))
			{
				SocketSetting(LocalSocketData.Socket, SOCKET_SETTING_TYPE::CLOSE, false, nullptr);
			}
		//Socket initialization
			else {
				GlobalRunningStatus.LocalListeningSocket->push_back(LocalSocketData.Socket);
				LocalSocketData.SockAddr.ss_family = AF_INET;
				LocalSocketData.AddrLen = sizeof(sockaddr_in);

			//Listen Address available
				if (Parameter.ListenAddress_IPv4 != nullptr)
				{
					for (const auto &ListenAddressItem:*Parameter.ListenAddress_IPv4)
					{
						if (!SocketSetting(LocalSocketData.Socket, SOCKET_SETTING_TYPE::INVALID_CHECK, false, nullptr))
						{
							LocalSocketData.Socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
							if (!SocketSetting(LocalSocketData.Socket, SOCKET_SETTING_TYPE::INVALID_CHECK, true, nullptr))
							{
								SocketSetting(LocalSocketData.Socket, SOCKET_SETTING_TYPE::CLOSE, false, nullptr);
								break;
							}
							else {
								GlobalRunningStatus.LocalListeningSocket->push_back(LocalSocketData.Socket);
							}
						}

						reinterpret_cast<sockaddr_in *>(&LocalSocketData.SockAddr)->sin_addr = reinterpret_cast<const sockaddr_in *>(&ListenAddressItem)->sin_addr;
						reinterpret_cast<sockaddr_in *>(&LocalSocketData.SockAddr)->sin_port = reinterpret_cast<const sockaddr_in *>(&ListenAddressItem)->sin_port;

					//Try to bind socket to system.
						if (!ListenMonitor_BindSocket(IPPROTO_UDP, LocalSocketData))
						{
						//Close all sockets.
							for (auto &SocketDataItem:LocalSocketDataList)
								SocketSetting(SocketDataItem.Socket, SOCKET_SETTING_TYPE::CLOSE, false, nullptr);

							return false;
						}
						else {
							LocalSocketDataList.push_back(LocalSocketData);
							LocalSocketProtocol.push_back(IPPROTO_UDP);
							LocalSocketData.Socket = INVALID_SOCKET;
						}
					}
				}
				else {
				//Normal mode
					if (Parameter.ListenPort != nullptr)
					{
					//Proxy Mode
						if (Parameter.OperationMode == LISTEN_MODE::PROXY)
							reinterpret_cast<sockaddr_in *>(&LocalSocketData.SockAddr)->sin_addr.s_addr = hton32(INADDR_LOOPBACK);
					//Server Mode, Priavte Mode and Custom Mode
						else 
							reinterpret_cast<sockaddr_in *>(&LocalSocketData.SockAddr)->sin_addr.s_addr = INADDR_ANY;

					//Set monitor port.
						for (const auto &ListenPortItem:*Parameter.ListenPort)
						{
							if (!SocketSetting(LocalSocketData.Socket, SOCKET_SETTING_TYPE::INVALID_CHECK, false, nullptr))
							{
								LocalSocketData.Socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
								if (!SocketSetting(LocalSocketData.Socket, SOCKET_SETTING_TYPE::INVALID_CHECK, true, nullptr))
								{
									SocketSetting(LocalSocketData.Socket, SOCKET_SETTING_TYPE::CLOSE, false, nullptr);
									break;
								}
								else {
									GlobalRunningStatus.LocalListeningSocket->push_back(LocalSocketData.Socket);
								}
							}

							reinterpret_cast<sockaddr_in *>(&LocalSocketData.SockAddr)->sin_port = ListenPortItem;

						//Try to bind socket to system.
							if (!ListenMonitor_BindSocket(IPPROTO_UDP, LocalSocketData))
							{
							//Close all sockets.
								for (auto &SocketDataItem:LocalSocketDataList)
									SocketSetting(SocketDataItem.Socket, SOCKET_SETTING_TYPE::CLOSE, false, nullptr);

								return false;
							}
							else {
								LocalSocketDataList.push_back(LocalSocketData);
								LocalSocketProtocol.push_back(IPPROTO_UDP);
								LocalSocketData.Socket = INVALID_SOCKET;
							}
						}
					}
				}
			}
		}

	//Set local machine Monitor sockets(IPv4/TCP).
		if (Parameter.ListenProtocol_Transport == LISTEN_PROTOCOL_TRANSPORT::BOTH || Parameter.ListenProtocol_Transport == LISTEN_PROTOCOL_TRANSPORT::TCP)
		{
		//Socket check
			memset(&LocalSocketData, 0, sizeof(LocalSocketData));
			LocalSocketData.Socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
			if (!SocketSetting(LocalSocketData.Socket, SOCKET_SETTING_TYPE::INVALID_CHECK, true, nullptr))
			{
				SocketSetting(LocalSocketData.Socket, SOCKET_SETTING_TYPE::CLOSE, false, nullptr);
			}
		//Socket initialization
			else {
				GlobalRunningStatus.LocalListeningSocket->push_back(LocalSocketData.Socket);
				LocalSocketData.SockAddr.ss_family = AF_INET;
				LocalSocketData.AddrLen = sizeof(sockaddr_in);

			//Listen Address available
				if (Parameter.ListenAddress_IPv4 != nullptr)
				{
					for (const auto &ListenAddressItem:*Parameter.ListenAddress_IPv4)
					{
						if (!SocketSetting(LocalSocketData.Socket, SOCKET_SETTING_TYPE::INVALID_CHECK, false, nullptr))
						{
							LocalSocketData.Socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
							if (!SocketSetting(LocalSocketData.Socket, SOCKET_SETTING_TYPE::INVALID_CHECK, true, nullptr))
							{
								SocketSetting(LocalSocketData.Socket, SOCKET_SETTING_TYPE::CLOSE, false, nullptr);
								break;
							}
							else {
								GlobalRunningStatus.LocalListeningSocket->push_back(LocalSocketData.Socket);
							}
						}

						reinterpret_cast<sockaddr_in *>(&LocalSocketData.SockAddr)->sin_addr = reinterpret_cast<const sockaddr_in *>(&ListenAddressItem)->sin_addr;
						reinterpret_cast<sockaddr_in *>(&LocalSocketData.SockAddr)->sin_port = reinterpret_cast<const sockaddr_in *>(&ListenAddressItem)->sin_port;

					//Try to bind socket to system.
						if (!ListenMonitor_BindSocket(IPPROTO_TCP, LocalSocketData))
						{
						//Close all sockets.
							for (auto &SocketDataItem:LocalSocketDataList)
								SocketSetting(SocketDataItem.Socket, SOCKET_SETTING_TYPE::CLOSE, false, nullptr);

							return false;
						}
						else {
							LocalSocketDataList.push_back(LocalSocketData);
							LocalSocketProtocol.push_back(IPPROTO_TCP);
							LocalSocketData.Socket = INVALID_SOCKET;
						}
					}
				}
				else {
				//Normal mode
					if (Parameter.ListenPort != nullptr)
					{
					//Proxy Mode
						if (Parameter.OperationMode == LISTEN_MODE::PROXY)
							reinterpret_cast<sockaddr_in *>(&LocalSocketData.SockAddr)->sin_addr.s_addr = hton32(INADDR_LOOPBACK);
					//Server Mode, Priavte Mode and Custom Mode
						else 
							reinterpret_cast<sockaddr_in *>(&LocalSocketData.SockAddr)->sin_addr.s_addr = INADDR_ANY;

					//Set monitor port.
						for (const auto &ListenPortItem:*Parameter.ListenPort)
						{
							if (!SocketSetting(LocalSocketData.Socket, SOCKET_SETTING_TYPE::INVALID_CHECK, false, nullptr))
							{
								LocalSocketData.Socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
								if (!SocketSetting(LocalSocketData.Socket, SOCKET_SETTING_TYPE::INVALID_CHECK, true, nullptr))
								{
									SocketSetting(LocalSocketData.Socket, SOCKET_SETTING_TYPE::CLOSE, false, nullptr);
									break;
								}
								else {
									GlobalRunningStatus.LocalListeningSocket->push_back(LocalSocketData.Socket);
								}

								GlobalRunningStatus.LocalListeningSocket->push_back(LocalSocketData.Socket);
							}

							reinterpret_cast<sockaddr_in *>(&LocalSocketData.SockAddr)->sin_port = ListenPortItem;

						//Try to bind socket to system.
							if (!ListenMonitor_BindSocket(IPPROTO_TCP, LocalSocketData))
							{
							//Close all sockets.
								for (auto &SocketDataItem:LocalSocketDataList)
									SocketSetting(SocketDataItem.Socket, SOCKET_SETTING_TYPE::CLOSE, false, nullptr);

								return false;
							}
							else {
								LocalSocketDataList.push_back(LocalSocketData);
								LocalSocketProtocol.push_back(IPPROTO_TCP);
								LocalSocketData.Socket = INVALID_SOCKET;
							}
						}
					}
				}
			}
		}
	}

	memset(&LocalSocketData, 0, sizeof(LocalSocketData));
	LocalSocketData.Socket = INVALID_SOCKET;

//Start monitor request consumer threads.
	if (Parameter.ThreadPoolBaseNum > 0)
	{
		for (size_t MonitorThreadIndex = 0;MonitorThreadIndex < Parameter.ThreadPoolBaseNum;++MonitorThreadIndex)
		{
		//Start monitor consumer thread.
			std::thread Thread_MonitorConsumer(std::bind(MonitorRequestConsumer));
			Thread_MonitorConsumer.detach();
		}

		*GlobalRunningStatus.ThreadRunningNum += Parameter.ThreadPoolBaseNum;
		*GlobalRunningStatus.ThreadRunningFreeNum += Parameter.ThreadPoolBaseNum;
	}

//Start all threads.
	std::vector<std::thread> MonitorThreadList;
	if (LocalSocketDataList.size() == LocalSocketProtocol.size())
	{
		for (size_t MonitorThreadIndex = 0;MonitorThreadIndex < LocalSocketDataList.size();++MonitorThreadIndex)
		{
		//UDP Monitor
			if (LocalSocketProtocol.at(MonitorThreadIndex) == IPPROTO_UDP)
			{
				std::thread ThreadTemp_Monitor(ListenMonitor_UDP, LocalSocketDataList.at(MonitorThreadIndex));
				MonitorThreadList.push_back(std::move(ThreadTemp_Monitor));
			}
		//TCP Monitor
			else if (LocalSocketProtocol.at(MonitorThreadIndex) == IPPROTO_TCP)
			{
				std::thread ThreadTemp_Monitor(ListenMonitor_TCP, LocalSocketDataList.at(MonitorThreadIndex));
				MonitorThreadList.push_back(std::move(ThreadTemp_Monitor));
			}
		//Not supported protocol
			else {
			//Stop all threads.
				MonitorThreadList.clear();

			//Close all sockets.
				for (auto &SocketDataItem:LocalSocketDataList)
					SocketSetting(SocketDataItem.Socket, SOCKET_SETTING_TYPE::CLOSE, false, nullptr);

				return false;
			}
		}
	}
	else {
	//Close all sockets.
		for (auto &SocketDataItem:LocalSocketDataList)
			SocketSetting(SocketDataItem.Socket, SOCKET_SETTING_TYPE::CLOSE, false, nullptr);

	//Print error messages(UDP).
		for (const auto &ProtocolItem:LocalSocketProtocol)
		{
			if (ProtocolItem == IPPROTO_UDP)
			{
				PrintError(LOG_LEVEL_TYPE::LEVEL_1, LOG_ERROR_TYPE::NETWORK, L"Bind UDP Monitor socket error", 0, nullptr, 0);
				break;
			}
		}

	//Print error messages(TCP).
		for (const auto &ProtocolItem:LocalSocketProtocol)
		{
			if (ProtocolItem == IPPROTO_TCP)
			{
				PrintError(LOG_LEVEL_TYPE::LEVEL_1, LOG_ERROR_TYPE::NETWORK, L"Bind TCP Monitor socket error", 0, nullptr, 0);
				break;
			}
		}

		return false;
	}

//Join all threads.
	for (auto &ThreadItem:MonitorThreadList)
	{
		if (ThreadItem.joinable())
			ThreadItem.join();
	}

//Close all sockets.
	for (auto &SocketDataItem:LocalSocketDataList)
		SocketSetting(SocketDataItem.Socket, SOCKET_SETTING_TYPE::CLOSE, false, nullptr);

	return true;
}

//Bind and transfer socket to monitor process
bool ListenMonitor_BindSocket(
	const uint16_t Protocol, 
	SOCKET_DATA &LocalSocketData)
{
//TCP
	if (Protocol == IPPROTO_TCP)
	{
	//Socket attribute settings
		if (
		#if defined(PLATFORM_WIN)
			!SocketSetting(LocalSocketData.Socket, SOCKET_SETTING_TYPE::REUSE, true, nullptr) || 
		#elif (defined(PLATFORM_FREEBSD) || defined(PLATFORM_LINUX) || defined(PLATFORM_MACOS))
			(LocalSocketData.SockAddr.ss_family == AF_INET6 && !SocketSetting(LocalSocketData.Socket, SOCKET_SETTING_TYPE::REUSE, true, nullptr)) || 
		#endif
			!SocketSetting(LocalSocketData.Socket, SOCKET_SETTING_TYPE::TCP_FAST_OPEN_NORMAL, true, nullptr) || 
			!SocketSetting(LocalSocketData.Socket, SOCKET_SETTING_TYPE::NON_BLOCKING_MODE, true, nullptr))
				return false;

	//Bind socket to system.
		if (bind(LocalSocketData.Socket, reinterpret_cast<const sockaddr *>(&LocalSocketData.SockAddr), LocalSocketData.AddrLen) == SOCKET_ERROR)
		{
			PrintError(LOG_LEVEL_TYPE::LEVEL_1, LOG_ERROR_TYPE::NETWORK, L"Bind TCP Monitor socket error", WSAGetLastError(), nullptr, 0);
			SocketSetting(LocalSocketData.Socket, SOCKET_SETTING_TYPE::CLOSE, false, nullptr);

			return false;
		}

	//Listen request from socket.
		if (listen(LocalSocketData.Socket, SOMAXCONN) == SOCKET_ERROR)
		{
			PrintError(LOG_LEVEL_TYPE::LEVEL_1, LOG_ERROR_TYPE::NETWORK, L"TCP Monitor socket listening initialization error", WSAGetLastError(), nullptr, 0);
			SocketSetting(LocalSocketData.Socket, SOCKET_SETTING_TYPE::CLOSE, false, nullptr);

			return false;
		}
	}
//UDP
	else if (Protocol == IPPROTO_UDP)
	{
	//Socket attribute settings
	if (
	#if defined(PLATFORM_WIN)
		!SocketSetting(LocalSocketData.Socket, SOCKET_SETTING_TYPE::UDP_BLOCK_RESET, true, nullptr) || 
		!SocketSetting(LocalSocketData.Socket, SOCKET_SETTING_TYPE::REUSE, true, nullptr) || 
	#elif (defined(PLATFORM_FREEBSD) || defined(PLATFORM_LINUX) || defined(PLATFORM_MACOS))
		(LocalSocketData.SockAddr.ss_family == AF_INET6 && !SocketSetting(LocalSocketData.Socket, SOCKET_SETTING_TYPE::REUSE, true, nullptr)) || 
	#endif
		!SocketSetting(LocalSocketData.Socket, SOCKET_SETTING_TYPE::NON_BLOCKING_MODE, true, nullptr))
			return false;

	//Bind socket to system.
		if (bind(LocalSocketData.Socket, reinterpret_cast<const sockaddr *>(&LocalSocketData.SockAddr), LocalSocketData.AddrLen) == SOCKET_ERROR)
		{
			PrintError(LOG_LEVEL_TYPE::LEVEL_1, LOG_ERROR_TYPE::NETWORK, L"Bind UDP Monitor socket error", WSAGetLastError(), nullptr, 0);
			SocketSetting(LocalSocketData.Socket, SOCKET_SETTING_TYPE::CLOSE, false, nullptr);

			return false;
		}
	}
	else {
		return false;
	}

	return true;
}

//Listen UDP request
bool ListenMonitor_UDP(
	SOCKET_DATA LocalSocketData)
{
//Initialization
	const auto RecvBuffer = std::make_unique<uint8_t[]>((PACKET_NORMAL_MAXSIZE + MEMORY_RESERVED_BYTES) * Parameter.ThreadPoolMaxNum);
	const auto SendBuffer = std::make_unique<uint8_t[]>(PACKET_NORMAL_MAXSIZE + MEMORY_RESERVED_BYTES);
	memset(RecvBuffer.get(), 0, (PACKET_NORMAL_MAXSIZE + MEMORY_RESERVED_BYTES) * Parameter.ThreadPoolMaxNum);
	memset(SendBuffer.get(), 0, PACKET_NORMAL_MAXSIZE + MEMORY_RESERVED_BYTES);
	MONITOR_QUEUE_DATA MonitorQueryData;
	MonitorQueryData.MONITOR_QUEUE_DATA_DNS_PACKET.Buffer = nullptr;
	MonitorQueryData.MONITOR_QUEUE_DATA_DNS_PACKET.BufferSize = PACKET_NORMAL_MAXSIZE;
	MonitorQueryData.MONITOR_QUEUE_DATA_DNS_PACKET.Length = 0;
	memset(&MonitorQueryData.MONITOR_QUEUE_DATA_DNS_PACKET.LocalTarget, 0, sizeof(MonitorQueryData.MONITOR_QUEUE_DATA_DNS_PACKET.LocalTarget));
	MonitorQueryData.MONITOR_QUEUE_DATA_DNS_PACKET.Protocol = IPPROTO_UDP;
	MonitorQueryData.MONITOR_QUEUE_DATA_DNS_PACKET.QueryType = 0;
	MonitorQueryData.MONITOR_QUEUE_DATA_DNS_PACKET.IsLocalRequest = false;
	MonitorQueryData.MONITOR_QUEUE_DATA_DNS_PACKET.IsLocalInWhite = false;
	MonitorQueryData.MONITOR_QUEUE_DATA_DNS_PACKET.Records_QuestionLen = 0;
	MonitorQueryData.MONITOR_QUEUE_DATA_DNS_PACKET.Records_AnswerCount = 0;
	MonitorQueryData.MONITOR_QUEUE_DATA_DNS_PACKET.Records_AuthorityCount = 0;
	MonitorQueryData.MONITOR_QUEUE_DATA_DNS_PACKET.Records_AdditionalCount = 0;
	MonitorQueryData.MONITOR_QUEUE_DATA_DNS_PACKET.DomainString_Original.clear();
	MonitorQueryData.MONITOR_QUEUE_DATA_DNS_PACKET.DomainString_Request.clear();
	MonitorQueryData.MONITOR_QUEUE_DATA_DNS_PACKET.EDNS_Location = 0;
	MonitorQueryData.MONITOR_QUEUE_DATA_DNS_PACKET.EDNS_Length = 0;
	fd_set ReadFDS;
	memset(&ReadFDS, 0, sizeof(ReadFDS));
	const auto SocketDataPointer = &MonitorQueryData.MONITOR_QUEUE_DATA_SOCKET;
	uint64_t LastRegisterTime = 0, NowTime = 0;
	if (Parameter.QueueResetTime > 0)
		LastRegisterTime = GetCurrentSystemTime();
	ssize_t RecvLen = 0;
	size_t Index = 0;
	int OptionValue = 0;
	socklen_t OptionSize = sizeof(OptionValue);

//Listening module
	while (!GlobalRunningStatus.IsNeedExit)
	{
	//Interval time between receive
		if (Parameter.QueueResetTime > 0 && Index + 1U == Parameter.ThreadPoolMaxNum)
		{
			NowTime = GetCurrentSystemTime();
			if (LastRegisterTime + Parameter.QueueResetTime > NowTime)
				Sleep(LastRegisterTime + Parameter.QueueResetTime - NowTime);

			LastRegisterTime = GetCurrentSystemTime();
		}

	//Reset parameters(Part 1).
		MonitorQueryData.MONITOR_QUEUE_DATA_SOCKET = LocalSocketData;

	//Select file descriptor set size and maximum socket index check
	//Windows: The variable FD_SETSIZE determines the maximum number of descriptors in a set.
	//Windows: The default value of FD_SETSIZE is 64, which can be modified by defining FD_SETSIZE to another value before including Winsock2.h.
	//Windows: Internally, socket handles in an fd_set structure are not represented as bit flags as in Berkeley Unix.
	//Linux and macOS: Select nfds is the highest-numbered file descriptor in any of the three sets, plus 1.
	//Linux and macOS: An fd_set is a fixed size buffer.
	//Linux and macOS: Executing FD_CLR() or FD_SET() with a value of fd that is negative or is equal to or larger than FD_SETSIZE will result in undefined behavior.
		if (!SocketSetting(MonitorQueryData.MONITOR_QUEUE_DATA_SOCKET.Socket, SOCKET_SETTING_TYPE::INVALID_CHECK, false, nullptr)
		#if (defined(PLATFORM_FREEBSD) || defined(PLATFORM_LINUX) || defined(PLATFORM_MACOS))
			|| MonitorQueryData.MONITOR_QUEUE_DATA_SOCKET.Socket + 1U >= FD_SETSIZE
		#endif
			)
				break;

	//Reset parameters(Part 2).
		memset(RecvBuffer.get() + (PACKET_NORMAL_MAXSIZE + MEMORY_RESERVED_BYTES) * Index, 0, PACKET_NORMAL_MAXSIZE + MEMORY_RESERVED_BYTES);
		memset(SendBuffer.get(), 0, PACKET_NORMAL_MAXSIZE + MEMORY_RESERVED_BYTES);
		FD_ZERO(&ReadFDS);
		FD_SET(MonitorQueryData.MONITOR_QUEUE_DATA_SOCKET.Socket, &ReadFDS);
		OptionValue = 0;
		OptionSize = sizeof(OptionValue);

	//Wait for system calling.
	#if defined(PLATFORM_WIN)
		ssize_t SelectResult = select(0, &ReadFDS, nullptr, nullptr, nullptr);
	#elif (defined(PLATFORM_FREEBSD) || defined(PLATFORM_LINUX) || defined(PLATFORM_MACOS))
		ssize_t SelectResult = select(MonitorQueryData.MONITOR_QUEUE_DATA_SOCKET.Socket + 1U, &ReadFDS, nullptr, nullptr, nullptr);
	#endif
		if (SelectResult > 0)
		{
			if (FD_ISSET(MonitorQueryData.MONITOR_QUEUE_DATA_SOCKET.Socket, &ReadFDS) != 0)
			{
			//Socket option check
			//Select will set both reading and writing sets and set SO_ERROR to error code when connection was failed.
				if (getsockopt(MonitorQueryData.MONITOR_QUEUE_DATA_SOCKET.Socket, SOL_SOCKET, SO_ERROR, reinterpret_cast<char *>(&OptionValue), &OptionSize) == SOCKET_ERROR)
				{
					PrintError(LOG_LEVEL_TYPE::LEVEL_2, LOG_ERROR_TYPE::NETWORK, L"UDP socket connecting error", WSAGetLastError(), nullptr, 0);
					Sleep(LOOP_INTERVAL_TIME_DELAY);

					continue;
				}
				else if (OptionValue > 0)
				{
					PrintError(LOG_LEVEL_TYPE::LEVEL_2, LOG_ERROR_TYPE::NETWORK, L"UDP socket connecting error", OptionValue, nullptr, 0);
					Sleep(LOOP_INTERVAL_TIME_DELAY);

					continue;
				}

			//Receive response and check DNS query data.
				RecvLen = recvfrom(MonitorQueryData.MONITOR_QUEUE_DATA_SOCKET.Socket, reinterpret_cast<char *>(RecvBuffer.get() + (PACKET_NORMAL_MAXSIZE + MEMORY_RESERVED_BYTES) * Index), PACKET_NORMAL_MAXSIZE, 0, reinterpret_cast<sockaddr *>(&SocketDataPointer->SockAddr), reinterpret_cast<socklen_t *>(&SocketDataPointer->AddrLen));
				if (RecvLen < static_cast<const ssize_t>(DNS_PACKET_MINSIZE))
				{
					continue;
				}
				else {
					MonitorQueryData.MONITOR_QUEUE_DATA_DNS_PACKET.Buffer = RecvBuffer.get() + (PACKET_NORMAL_MAXSIZE + MEMORY_RESERVED_BYTES) * Index;
					MonitorQueryData.MONITOR_QUEUE_DATA_DNS_PACKET.Length = RecvLen;
					MonitorQueryData.MONITOR_QUEUE_DATA_DNS_PACKET.QueryType = 0;
					MonitorQueryData.MONITOR_QUEUE_DATA_DNS_PACKET.IsLocalRequest = false;
					MonitorQueryData.MONITOR_QUEUE_DATA_DNS_PACKET.IsLocalInWhite = false;
					memset(&MonitorQueryData.MONITOR_QUEUE_DATA_DNS_PACKET.LocalTarget, 0, sizeof(MonitorQueryData.MONITOR_QUEUE_DATA_DNS_PACKET.LocalTarget));
					MonitorQueryData.MONITOR_QUEUE_DATA_DNS_PACKET.Records_QuestionLen = 0;
					MonitorQueryData.MONITOR_QUEUE_DATA_DNS_PACKET.Records_AnswerCount = 0;
					MonitorQueryData.MONITOR_QUEUE_DATA_DNS_PACKET.Records_AuthorityCount = 0;
					MonitorQueryData.MONITOR_QUEUE_DATA_DNS_PACKET.Records_AdditionalCount = 0;
					MonitorQueryData.MONITOR_QUEUE_DATA_DNS_PACKET.Records_Location.clear();
					MonitorQueryData.MONITOR_QUEUE_DATA_DNS_PACKET.Records_Length.clear();
					MonitorQueryData.MONITOR_QUEUE_DATA_DNS_PACKET.DomainString_Original.clear();
					MonitorQueryData.MONITOR_QUEUE_DATA_DNS_PACKET.DomainString_Request.clear();
					MonitorQueryData.MONITOR_QUEUE_DATA_DNS_PACKET.EDNS_Location = 0;
					MonitorQueryData.MONITOR_QUEUE_DATA_DNS_PACKET.EDNS_Length = 0;

				//Check DNS query data.
					if (!CheckQueryData(&MonitorQueryData.MONITOR_QUEUE_DATA_DNS_PACKET, SendBuffer.get(), PACKET_NORMAL_MAXSIZE, MonitorQueryData.MONITOR_QUEUE_DATA_SOCKET))
						continue;
				}

			//Request process
				if (Parameter.ThreadPoolBaseNum > 0) //Thread pool mode
				{
					MonitorRequestProvider(MonitorQueryData);
				}
				else { //New thread mode
					std::thread Thread_RequestProcess(std::bind(EnterRequestProcess, MonitorQueryData, nullptr, 0));
					Thread_RequestProcess.detach();
				}

				Index = (Index + 1U) % Parameter.ThreadPoolMaxNum;
			}
			else {
				Sleep(LOOP_INTERVAL_TIME_DELAY);
			}
		}
	//Timeout
		else if (SelectResult == 0)
		{
			continue;
		}
	//SOCKET_ERROR
		else {
		//Block error messages when monitor is terminated.
		#if defined(PLATFORM_WIN)
			if (WSAGetLastError() != WSAENOTSOCK)
		#elif (defined(PLATFORM_FREEBSD) || defined(PLATFORM_LINUX) || defined(PLATFORM_MACOS))
			if (errno != ENOTSOCK && errno != EBADF)
		#endif
				PrintError(LOG_LEVEL_TYPE::LEVEL_2, LOG_ERROR_TYPE::NETWORK, L"UDP Monitor socket initialization error", WSAGetLastError(), nullptr, 0);

		//Refresh interval
			Sleep(Parameter.FileRefreshTime);
		}
	}

//Loop terminated
	SocketSetting(LocalSocketData.Socket, SOCKET_SETTING_TYPE::CLOSE, false, nullptr);
	if (!GlobalRunningStatus.IsNeedExit)
		PrintError(LOG_LEVEL_TYPE::LEVEL_2, LOG_ERROR_TYPE::SYSTEM, L"UDP listening module Monitor terminated", 0, nullptr, 0);
	return true;
}

//Listen TCP request
bool ListenMonitor_TCP(
	SOCKET_DATA LocalSocketData)
{
//Initialization
	MONITOR_QUEUE_DATA MonitorQueryData;
	MonitorQueryData.MONITOR_QUEUE_DATA_DNS_PACKET.Buffer = nullptr;
	MonitorQueryData.MONITOR_QUEUE_DATA_DNS_PACKET.BufferSize = Parameter.LargeBufferSize;
	MonitorQueryData.MONITOR_QUEUE_DATA_DNS_PACKET.Length = 0;
	memset(&MonitorQueryData.MONITOR_QUEUE_DATA_DNS_PACKET.LocalTarget, 0, sizeof(MonitorQueryData.MONITOR_QUEUE_DATA_DNS_PACKET.LocalTarget));
	MonitorQueryData.MONITOR_QUEUE_DATA_DNS_PACKET.Protocol = IPPROTO_TCP;
	MonitorQueryData.MONITOR_QUEUE_DATA_DNS_PACKET.QueryType = 0;
	MonitorQueryData.MONITOR_QUEUE_DATA_DNS_PACKET.IsLocalRequest = false;
	MonitorQueryData.MONITOR_QUEUE_DATA_DNS_PACKET.IsLocalInWhite = false;
	MonitorQueryData.MONITOR_QUEUE_DATA_DNS_PACKET.Records_QuestionLen = 0;
	MonitorQueryData.MONITOR_QUEUE_DATA_DNS_PACKET.Records_AnswerCount = 0;
	MonitorQueryData.MONITOR_QUEUE_DATA_DNS_PACKET.Records_AuthorityCount = 0;
	MonitorQueryData.MONITOR_QUEUE_DATA_DNS_PACKET.Records_AdditionalCount = 0;
	MonitorQueryData.MONITOR_QUEUE_DATA_DNS_PACKET.DomainString_Original.clear();
	MonitorQueryData.MONITOR_QUEUE_DATA_DNS_PACKET.DomainString_Request.clear();
	MonitorQueryData.MONITOR_QUEUE_DATA_DNS_PACKET.EDNS_Location = 0;
	MonitorQueryData.MONITOR_QUEUE_DATA_DNS_PACKET.EDNS_Length = 0;
	fd_set ReadFDS;
	memset(&ReadFDS, 0, sizeof(ReadFDS));
	const auto SocketDataPointer = &MonitorQueryData.MONITOR_QUEUE_DATA_SOCKET;
	uint64_t LastRegisterTime = 0, NowTime = 0;
	if (Parameter.QueueResetTime > 0)
		LastRegisterTime = GetCurrentSystemTime();
	size_t Index = 0;
	int OptionValue = 0;
	socklen_t OptionSize = sizeof(OptionValue);

//Start listening Monitor.
	while (!GlobalRunningStatus.IsNeedExit)
	{
	//Interval time between receive
		if (Parameter.QueueResetTime > 0 && Index + 1U == Parameter.ThreadPoolMaxNum)
		{
			NowTime = GetCurrentSystemTime();
			if (LastRegisterTime + Parameter.QueueResetTime > NowTime)
				Sleep(LastRegisterTime + Parameter.QueueResetTime - NowTime);

			LastRegisterTime = GetCurrentSystemTime();
		}

	//Select file descriptor set size and maximum socket index check
	//Windows: The variable FD_SETSIZE determines the maximum number of descriptors in a set.
	//Windows: The default value of FD_SETSIZE is 64, which can be modified by defining FD_SETSIZE to another value before including Winsock2.h.
	//Windows: Internally, socket handles in an fd_set structure are not represented as bit flags as in Berkeley Unix.
	//Linux and macOS: Select nfds is the highest-numbered file descriptor in any of the three sets, plus 1.
	//Linux and macOS: An fd_set is a fixed size buffer.
	//Linux and macOS: Executing FD_CLR() or FD_SET() with a value of fd that is negative or is equal to or larger than FD_SETSIZE will result in undefined behavior.
		if (!SocketSetting(LocalSocketData.Socket, SOCKET_SETTING_TYPE::INVALID_CHECK, false, nullptr)
		#if (defined(PLATFORM_FREEBSD) || defined(PLATFORM_LINUX) || defined(PLATFORM_MACOS))
			|| LocalSocketData.Socket + 1U >= FD_SETSIZE
		#endif
			)
				break;

	//Reset parameters.
		memset(&MonitorQueryData.MONITOR_QUEUE_DATA_SOCKET.SockAddr, 0, sizeof(MonitorQueryData.MONITOR_QUEUE_DATA_SOCKET.SockAddr));
		MonitorQueryData.MONITOR_QUEUE_DATA_SOCKET.AddrLen = LocalSocketData.AddrLen;
		MonitorQueryData.MONITOR_QUEUE_DATA_SOCKET.SockAddr.ss_family = LocalSocketData.SockAddr.ss_family;
		FD_ZERO(&ReadFDS);
		FD_SET(LocalSocketData.Socket, &ReadFDS);
		OptionValue = 0;
		OptionSize = sizeof(OptionValue);

	//Wait for system calling.
	#if defined(PLATFORM_WIN)
		ssize_t SelectResult = select(0, &ReadFDS, nullptr, nullptr, nullptr);
	#elif (defined(PLATFORM_FREEBSD) || defined(PLATFORM_LINUX) || defined(PLATFORM_MACOS))
		ssize_t SelectResult = select(LocalSocketData.Socket + 1U, &ReadFDS, nullptr, nullptr, nullptr);
	#endif
		if (SelectResult > 0)
		{
			if (FD_ISSET(LocalSocketData.Socket, &ReadFDS) != 0)
			{
			//Socket option check
			//Select will set both reading and writing sets and set SO_ERROR to error code when connection was failed.
				if (getsockopt(LocalSocketData.Socket, SOL_SOCKET, SO_ERROR, reinterpret_cast<char *>(&OptionValue), &OptionSize) == SOCKET_ERROR)
				{
					PrintError(LOG_LEVEL_TYPE::LEVEL_2, LOG_ERROR_TYPE::NETWORK, L"TCP socket connecting error", WSAGetLastError(), nullptr, 0);
					Sleep(LOOP_INTERVAL_TIME_DELAY);

					continue;
				}
				else if (OptionValue > 0)
				{
					PrintError(LOG_LEVEL_TYPE::LEVEL_2, LOG_ERROR_TYPE::NETWORK, L"TCP socket connecting error", OptionValue, nullptr, 0);
					Sleep(LOOP_INTERVAL_TIME_DELAY);

					continue;
				}

			//Accept connection.
				MonitorQueryData.MONITOR_QUEUE_DATA_SOCKET.Socket = accept(LocalSocketData.Socket, reinterpret_cast<sockaddr *>(&SocketDataPointer->SockAddr), &SocketDataPointer->AddrLen);
				if (!SocketSetting(MonitorQueryData.MONITOR_QUEUE_DATA_SOCKET.Socket, SOCKET_SETTING_TYPE::INVALID_CHECK, false, nullptr))
				{
					continue;
				}
			//Check request address.
				else if (!CheckQueryData(nullptr, nullptr, 0, MonitorQueryData.MONITOR_QUEUE_DATA_SOCKET))
				{
					SocketSetting(MonitorQueryData.MONITOR_QUEUE_DATA_SOCKET.Socket, SOCKET_SETTING_TYPE::CLOSE, false, nullptr);
					continue;
				}

			//Accept process.
				if (Parameter.ThreadPoolBaseNum > 0) //Thread pool mode
				{
					MonitorRequestProvider(MonitorQueryData);
				}
				else { //New thread mode
					std::thread Thread_TCP_AcceptProcess(std::bind(TCP_AcceptProcess, MonitorQueryData, nullptr, 0));
					Thread_TCP_AcceptProcess.detach();
				}

				Index = (Index + 1U) % Parameter.ThreadPoolMaxNum;
			}
			else {
				Sleep(LOOP_INTERVAL_TIME_DELAY);
			}
		}
	//Timeout
		else if (SelectResult == 0)
		{
			continue;
		}
	//SOCKET_ERROR
		else {
		//Block error messages when monitor is terminated.
		#if defined(PLATFORM_WIN)
			if (WSAGetLastError() != WSAENOTSOCK)
		#elif (defined(PLATFORM_FREEBSD) || defined(PLATFORM_LINUX) || defined(PLATFORM_MACOS))
			if (errno != ENOTSOCK && errno != EBADF)
		#endif
				PrintError(LOG_LEVEL_TYPE::LEVEL_2, LOG_ERROR_TYPE::NETWORK, L"TCP Monitor socket initialization error", WSAGetLastError(), nullptr, 0);

		//Refresh interval
			Sleep(Parameter.FileRefreshTime);
		}
	}

//Loop terminated
	SocketSetting(LocalSocketData.Socket, SOCKET_SETTING_TYPE::CLOSE, false, nullptr);
	if (!GlobalRunningStatus.IsNeedExit)
		PrintError(LOG_LEVEL_TYPE::LEVEL_2, LOG_ERROR_TYPE::SYSTEM, L"TCP listening module Monitor terminated", 0, nullptr, 0);
	return true;
}

//TCP Monitor accept process
bool TCP_AcceptProcess(
	MONITOR_QUEUE_DATA MonitorQueryData, 
	uint8_t * const OriginalRecv, 
	size_t RecvSize)
{
//Select file descriptor set size and maximum socket index check(Part 1)
//Windows: The variable FD_SETSIZE determines the maximum number of descriptors in a set.
//Windows: The default value of FD_SETSIZE is 64, which can be modified by defining FD_SETSIZE to another value before including Winsock2.h.
//Windows: Internally, socket handles in an fd_set structure are not represented as bit flags as in Berkeley Unix.
//Linux and macOS: Select nfds is the highest-numbered file descriptor in any of the three sets, plus 1.
//Linux and macOS: An fd_set is a fixed size buffer.
//Linux and macOS: Executing FD_CLR() or FD_SET() with a value of fd that is negative or is equal to or larger than FD_SETSIZE will result in undefined behavior.
	if (!SocketSetting(MonitorQueryData.MONITOR_QUEUE_DATA_SOCKET.Socket, SOCKET_SETTING_TYPE::INVALID_CHECK, false, nullptr)
	#if (defined(PLATFORM_FREEBSD) || defined(PLATFORM_LINUX) || defined(PLATFORM_MACOS))
		|| MonitorQueryData.MONITOR_QUEUE_DATA_SOCKET.Socket + 1U >= FD_SETSIZE
	#endif
		)
	{
		SocketSetting(MonitorQueryData.MONITOR_QUEUE_DATA_SOCKET.Socket, SOCKET_SETTING_TYPE::CLOSE, false, nullptr);
		return false;
	}

//Initialization(Part 1)
	const auto RecvBuffer = std::make_unique<uint8_t[]>(Parameter.LargeBufferSize + MEMORY_RESERVED_BYTES);
	memset(RecvBuffer.get(), 0, Parameter.LargeBufferSize + MEMORY_RESERVED_BYTES);
	fd_set ReadFDS;
	timeval Timeout;
	memset(&ReadFDS, 0, sizeof(ReadFDS));
	memset(&Timeout, 0, sizeof(Timeout));
	ssize_t RecvLenFirst = 0, RecvLenSecond = 0;
	int OptionValue = 0;
	socklen_t OptionSize = sizeof(OptionValue);

//Socket selecting structure initialization(Part 1)
#if defined(PLATFORM_WIN)
	Timeout.tv_sec = Parameter.SocketTimeout_Reliable_Once / SECOND_TO_MILLISECOND;
	Timeout.tv_usec = Parameter.SocketTimeout_Reliable_Once % SECOND_TO_MILLISECOND * MICROSECOND_TO_MILLISECOND;
#elif (defined(PLATFORM_FREEBSD) || defined(PLATFORM_LINUX) || defined(PLATFORM_MACOS))
	Timeout = Parameter.SocketTimeout_Reliable_Once;
#endif
	FD_ZERO(&ReadFDS);
	FD_SET(MonitorQueryData.MONITOR_QUEUE_DATA_SOCKET.Socket, &ReadFDS);

//Receive process
//Only receive 2 times data sending operations from sender when accepting, reject all connections which have more than 2 times sending operations.
#if defined(PLATFORM_WIN)
	RecvLenFirst = select(0, &ReadFDS, nullptr, nullptr, &Timeout);
#elif (defined(PLATFORM_FREEBSD) || defined(PLATFORM_LINUX) || defined(PLATFORM_MACOS))
	RecvLenFirst = select(MonitorQueryData.MONITOR_QUEUE_DATA_SOCKET.Socket + 1U, &ReadFDS, nullptr, nullptr, &Timeout);
#endif
	if (RecvLenFirst > 0 && 
		FD_ISSET(MonitorQueryData.MONITOR_QUEUE_DATA_SOCKET.Socket, &ReadFDS) != 0)
	{
	//Socket option check
	//Select will set both reading and writing sets and set SO_ERROR to error code when connection was failed.
		if (getsockopt(MonitorQueryData.MONITOR_QUEUE_DATA_SOCKET.Socket, SOL_SOCKET, SO_ERROR, reinterpret_cast<char *>(&OptionValue), &OptionSize) == SOCKET_ERROR)
		{
//			PrintError(LOG_LEVEL_TYPE::LEVEL_2, LOG_ERROR_TYPE::NETWORK, L"TCP socket connecting error", WSAGetLastError(), nullptr, 0);
			SocketSetting(MonitorQueryData.MONITOR_QUEUE_DATA_SOCKET.Socket, SOCKET_SETTING_TYPE::CLOSE, false, nullptr);

			return false;
		}
		else if (OptionValue > 0)
		{
//			PrintError(LOG_LEVEL_TYPE::LEVEL_2, LOG_ERROR_TYPE::NETWORK, L"TCP socket connecting error", OptionValue, nullptr, 0);
			SocketSetting(MonitorQueryData.MONITOR_QUEUE_DATA_SOCKET.Socket, SOCKET_SETTING_TYPE::CLOSE, false, nullptr);

			return false;
		}

	//Receive data.
		RecvLenFirst = recv(MonitorQueryData.MONITOR_QUEUE_DATA_SOCKET.Socket, reinterpret_cast<char *>(RecvBuffer.get()), static_cast<const int>(Parameter.LargeBufferSize), 0);

	//Connection closed or SOCKET_ERROR
		if (RecvLenFirst < static_cast<const ssize_t>(sizeof(uint16_t))) //Sender must send packet length value(16 bits) at first.
		{
			SocketSetting(MonitorQueryData.MONITOR_QUEUE_DATA_SOCKET.Socket, SOCKET_SETTING_TYPE::CLOSE, false, nullptr);
			return false;
		}
	}
//Timeout or SOCKET_ERROR
	else {
		SocketSetting(MonitorQueryData.MONITOR_QUEUE_DATA_SOCKET.Socket, SOCKET_SETTING_TYPE::CLOSE, false, nullptr);
		return false;
	}

//Connection closed or SOCKET_ERROR
	if (RecvLenFirst < static_cast<const ssize_t>(DNS_PACKET_MINSIZE))
	{
	//Select file descriptor set size and maximum socket index check(Part 2)
		if (!SocketSetting(MonitorQueryData.MONITOR_QUEUE_DATA_SOCKET.Socket, SOCKET_SETTING_TYPE::INVALID_CHECK, false, nullptr)
		#if (defined(PLATFORM_FREEBSD) || defined(PLATFORM_LINUX) || defined(PLATFORM_MACOS))
			|| MonitorQueryData.MONITOR_QUEUE_DATA_SOCKET.Socket + 1U >= FD_SETSIZE
		#endif
			)
		{
			SocketSetting(MonitorQueryData.MONITOR_QUEUE_DATA_SOCKET.Socket, SOCKET_SETTING_TYPE::CLOSE, false, nullptr);
			return false;
		}

	//Socket selecting structure initialization(Part 2)
		memset(&ReadFDS, 0, sizeof(ReadFDS));
		memset(&Timeout, 0, sizeof(Timeout));
	#if defined(PLATFORM_WIN)
		Timeout.tv_sec = Parameter.SocketTimeout_Reliable_Once / SECOND_TO_MILLISECOND;
		Timeout.tv_usec = Parameter.SocketTimeout_Reliable_Once % SECOND_TO_MILLISECOND * MICROSECOND_TO_MILLISECOND;
	#elif (defined(PLATFORM_FREEBSD) || defined(PLATFORM_LINUX) || defined(PLATFORM_MACOS))
		Timeout = Parameter.SocketTimeout_Reliable_Once;
	#endif
		FD_ZERO(&ReadFDS);
		FD_SET(MonitorQueryData.MONITOR_QUEUE_DATA_SOCKET.Socket, &ReadFDS);
		OptionValue = 0;
		OptionSize = sizeof(OptionValue);

	//Wait for system calling.
	#if defined(PLATFORM_WIN)
		RecvLenSecond = select(0, &ReadFDS, nullptr, nullptr, &Timeout);
	#elif (defined(PLATFORM_FREEBSD) || defined(PLATFORM_LINUX) || defined(PLATFORM_MACOS))
		RecvLenSecond = select(MonitorQueryData.MONITOR_QUEUE_DATA_SOCKET.Socket + 1U, &ReadFDS, nullptr, nullptr, &Timeout);
	#endif
		if (RecvLenSecond > 0 && 
			FD_ISSET(MonitorQueryData.MONITOR_QUEUE_DATA_SOCKET.Socket, &ReadFDS) != 0)
		{
		//Socket option check
		//Select will set both reading and writing sets and set SO_ERROR to error code when connection was failed.
			if (getsockopt(MonitorQueryData.MONITOR_QUEUE_DATA_SOCKET.Socket, SOL_SOCKET, SO_ERROR, reinterpret_cast<char *>(&OptionValue), &OptionSize) == SOCKET_ERROR)
			{
//				PrintError(LOG_LEVEL_TYPE::LEVEL_2, LOG_ERROR_TYPE::NETWORK, L"TCP socket connecting error", WSAGetLastError(), nullptr, 0);
				SocketSetting(MonitorQueryData.MONITOR_QUEUE_DATA_SOCKET.Socket, SOCKET_SETTING_TYPE::CLOSE, false, nullptr);

				return false;
			}
			else if (OptionValue > 0)
			{
//				PrintError(LOG_LEVEL_TYPE::LEVEL_2, LOG_ERROR_TYPE::NETWORK, L"TCP socket connecting error", OptionValue, nullptr, 0);
				SocketSetting(MonitorQueryData.MONITOR_QUEUE_DATA_SOCKET.Socket, SOCKET_SETTING_TYPE::CLOSE, false, nullptr);

				return false;
			}

		//Receive data.
			RecvLenSecond = recv(MonitorQueryData.MONITOR_QUEUE_DATA_SOCKET.Socket, reinterpret_cast<char *>(RecvBuffer.get() + RecvLenFirst), static_cast<const int>(Parameter.LargeBufferSize - RecvLenFirst), 0);

		//Connection closed or SOCKET_ERROR
			if (RecvLenSecond <= 0)
			{
				SocketSetting(MonitorQueryData.MONITOR_QUEUE_DATA_SOCKET.Socket, SOCKET_SETTING_TYPE::CLOSE, false, nullptr);
				return false;
			}
		}
	//Timeout or SOCKET_ERROR
		else {
			SocketSetting(MonitorQueryData.MONITOR_QUEUE_DATA_SOCKET.Socket, SOCKET_SETTING_TYPE::CLOSE, false, nullptr);
			return false;
		}
	}

//Length check
	const auto LengthValue = ntoh16(reinterpret_cast<const uint16_t *>(RecvBuffer.get())[0]);
	if (LengthValue >= DNS_PACKET_MINSIZE && LengthValue + sizeof(uint16_t) < Parameter.LargeBufferSize && 
		((RecvLenFirst < static_cast<const ssize_t>(DNS_PACKET_MINSIZE) && RecvLenFirst + RecvLenSecond >= static_cast<const ssize_t>(LengthValue + sizeof(uint16_t))) || 
		(RecvLenFirst >= static_cast<const ssize_t>(DNS_PACKET_MINSIZE) && RecvLenFirst >= static_cast<const ssize_t>(LengthValue + sizeof(uint16_t)))))
	{
		MonitorQueryData.MONITOR_QUEUE_DATA_DNS_PACKET.Buffer = RecvBuffer.get() + sizeof(uint16_t);
		MonitorQueryData.MONITOR_QUEUE_DATA_DNS_PACKET.Length = LengthValue;
		MonitorQueryData.MONITOR_QUEUE_DATA_DNS_PACKET.QueryType = 0;
		MonitorQueryData.MONITOR_QUEUE_DATA_DNS_PACKET.IsLocalRequest = false;
		MonitorQueryData.MONITOR_QUEUE_DATA_DNS_PACKET.IsLocalInWhite = false;
		memset(&MonitorQueryData.MONITOR_QUEUE_DATA_DNS_PACKET.LocalTarget, 0, sizeof(MonitorQueryData.MONITOR_QUEUE_DATA_DNS_PACKET.LocalTarget));
		MonitorQueryData.MONITOR_QUEUE_DATA_DNS_PACKET.Records_QuestionLen = 0;
		MonitorQueryData.MONITOR_QUEUE_DATA_DNS_PACKET.Records_AnswerCount = 0;
		MonitorQueryData.MONITOR_QUEUE_DATA_DNS_PACKET.Records_AuthorityCount = 0;
		MonitorQueryData.MONITOR_QUEUE_DATA_DNS_PACKET.Records_AdditionalCount = 0;
		MonitorQueryData.MONITOR_QUEUE_DATA_DNS_PACKET.Records_Location.clear();
		MonitorQueryData.MONITOR_QUEUE_DATA_DNS_PACKET.Records_Length.clear();
		MonitorQueryData.MONITOR_QUEUE_DATA_DNS_PACKET.DomainString_Original.clear();
		MonitorQueryData.MONITOR_QUEUE_DATA_DNS_PACKET.DomainString_Request.clear();
		MonitorQueryData.MONITOR_QUEUE_DATA_DNS_PACKET.EDNS_Location = 0;
		MonitorQueryData.MONITOR_QUEUE_DATA_DNS_PACKET.EDNS_Length = 0;

	//Check DNS query data.
		auto SendBuffer = std::make_unique<uint8_t[]>(Parameter.LargeBufferSize + MEMORY_RESERVED_BYTES);
		memset(SendBuffer.get(), 0, Parameter.LargeBufferSize + MEMORY_RESERVED_BYTES);
		if (!CheckQueryData(&MonitorQueryData.MONITOR_QUEUE_DATA_DNS_PACKET, SendBuffer.get(), Parameter.LargeBufferSize, MonitorQueryData.MONITOR_QUEUE_DATA_SOCKET))
		{
			SocketSetting(MonitorQueryData.MONITOR_QUEUE_DATA_SOCKET.Socket, SOCKET_SETTING_TYPE::CLOSE, false, nullptr);
			return false;
		}
		else {
			SendBuffer.reset();
		}

	//Main request process
		EnterRequestProcess(MonitorQueryData, OriginalRecv, RecvSize);
	}
	else {
		SocketSetting(MonitorQueryData.MONITOR_QUEUE_DATA_SOCKET.Socket, SOCKET_SETTING_TYPE::CLOSE, false, nullptr);
		return false;
	}

//Block Port Unreachable messages of system.
	shutdown(MonitorQueryData.MONITOR_QUEUE_DATA_SOCKET.Socket, SD_SEND);
#if defined(PLATFORM_WIN)
	Sleep(Parameter.SocketTimeout_Reliable_Once);
#elif (defined(PLATFORM_FREEBSD) || defined(PLATFORM_LINUX) || defined(PLATFORM_MACOS))
	usleep(Parameter.SocketTimeout_Reliable_Once.tv_sec * SECOND_TO_MILLISECOND * MICROSECOND_TO_MILLISECOND + Parameter.SocketTimeout_Reliable_Once.tv_usec);
#endif
	SocketSetting(MonitorQueryData.MONITOR_QUEUE_DATA_SOCKET.Socket, SOCKET_SETTING_TYPE::CLOSE, false, nullptr);

	return true;
}

//Alternate DNS servers switcher
void AlternateServerSwitcher(
	void)
{
//Initialization
	size_t Index = 0;
	std::array<uint64_t, ALTERNATE_SERVER_NUM> RangeTimer{}, SwapTimer{};

//Start Switcher.
	while (!GlobalRunningStatus.IsNeedExit)
	{
	//Complete request process check
		for (Index = 0;Index < ALTERNATE_SERVER_NUM;++Index)
		{
		//Reset TimeoutTimes out of alternate time range.
			if (RangeTimer.at(Index) <= GetCurrentSystemTime())
			{
				RangeTimer.at(Index) = GetCurrentSystemTime() + Parameter.AlternateTimeRange;
				AlternateSwapList.TimeoutTimes.at(Index) = 0;

				continue;
			}

		//Reset alternate switching.
			if (AlternateSwapList.IsSwapped.at(Index))
			{
				if (SwapTimer.at(Index) <= GetCurrentSystemTime())
				{
					AlternateSwapList.IsSwapped.at(Index) = false;
					AlternateSwapList.TimeoutTimes.at(Index) = 0;
					SwapTimer.at(Index) = 0;
				}
			}
			else {
			//Mark alternate switching.
				if (AlternateSwapList.TimeoutTimes.at(Index) >= Parameter.AlternateTimes)
				{
					AlternateSwapList.IsSwapped.at(Index) = true;
					AlternateSwapList.TimeoutTimes.at(Index) = 0;
					SwapTimer.at(Index) = GetCurrentSystemTime() + Parameter.AlternateResetTime;
				}
			}
		}

		Sleep(Parameter.FileRefreshTime);
	}

//Loop terminated
	if (!GlobalRunningStatus.IsNeedExit)
		PrintError(LOG_LEVEL_TYPE::LEVEL_2, LOG_ERROR_TYPE::SYSTEM, L"Alternate Server module Monitor terminated", 0, nullptr, 0);
	return;
}

//Get local address list
#if defined(PLATFORM_WIN)
addrinfo *GetLocalAddressList(
	const uint16_t Protocol, 
	uint8_t * const HostName)
{
	memset(HostName, 0, DOMAIN_MAXSIZE);

//Get local machine name.
	if (gethostname(reinterpret_cast<char *>(HostName), DOMAIN_MAXSIZE) == SOCKET_ERROR || CheckEmptyBuffer(HostName, DOMAIN_MAXSIZE))
	{
		PrintError(LOG_LEVEL_TYPE::LEVEL_3, LOG_ERROR_TYPE::NETWORK, L"Get local machine name error", WSAGetLastError(), nullptr, 0);
		return nullptr;
	}

//Initialization
	addrinfo Hints;
	memset(&Hints, 0, sizeof(Hints));
	addrinfo *AddrResult = nullptr;
	Hints.ai_family = Protocol;
	Hints.ai_socktype = SOCK_DGRAM;
	Hints.ai_protocol = IPPROTO_UDP;

//Get local machine name data.
	const auto UnsignedResult = getaddrinfo(reinterpret_cast<char *>(HostName), nullptr, &Hints, &AddrResult);
	if (UnsignedResult != 0 || AddrResult == nullptr)
	{
		PrintError(LOG_LEVEL_TYPE::LEVEL_3, LOG_ERROR_TYPE::NETWORK, L"Get local machine address error", UnsignedResult, nullptr, 0);
		return nullptr;
	}

	return AddrResult;
}

#elif (defined(PLATFORM_FREEBSD) || defined(PLATFORM_LINUX) || defined(PLATFORM_MACOS))
//Get address from best network interface
bool GetBestInterfaceAddress(
	const uint16_t Protocol, 
	const sockaddr_storage * const OriginalSockAddr)
{
//Initialization
	sockaddr_storage SockAddr;
	memset(&SockAddr, 0, sizeof(SockAddr));
	SockAddr.ss_family = Protocol;
	SYSTEM_SOCKET InterfaceSocket = socket(Protocol, SOCK_DGRAM, IPPROTO_UDP);
	socklen_t AddrLen = 0;

//Socket check
	if (!SocketSetting(InterfaceSocket, SOCKET_SETTING_TYPE::INVALID_CHECK, true, nullptr))
	{
		SocketSetting(InterfaceSocket, SOCKET_SETTING_TYPE::CLOSE, false, nullptr);
		if (Protocol == AF_INET6)
			GlobalRunningStatus.GatewayAvailable_IPv6 = false;
		else if (Protocol == AF_INET)
			GlobalRunningStatus.GatewayAvailable_IPv4 = false;

		return false;
	}

//Get socket information from system.
	if (Protocol == AF_INET6)
	{
		reinterpret_cast<sockaddr_in6 *>(&SockAddr)->sin6_addr = reinterpret_cast<const sockaddr_in6 *>(OriginalSockAddr)->sin6_addr;
		reinterpret_cast<sockaddr_in6 *>(&SockAddr)->sin6_port = reinterpret_cast<const sockaddr_in6 *>(OriginalSockAddr)->sin6_port;
		AddrLen = sizeof(sockaddr_in6);

	//UDP connecting
		if (connect(InterfaceSocket, reinterpret_cast<const sockaddr *>(&SockAddr), sizeof(sockaddr_in6)) == SOCKET_ERROR || 
			getsockname(InterfaceSocket, reinterpret_cast<sockaddr *>(&SockAddr), &AddrLen) == SOCKET_ERROR || 
			SockAddr.ss_family != AF_INET6 || AddrLen != sizeof(sockaddr_in6) || 
			CheckEmptyBuffer(&reinterpret_cast<const sockaddr_in6 *>(&SockAddr)->sin6_addr, sizeof(reinterpret_cast<const sockaddr_in6 *>(&SockAddr)->sin6_addr)))
		{
			SocketSetting(InterfaceSocket, SOCKET_SETTING_TYPE::CLOSE, false, nullptr);
			GlobalRunningStatus.GatewayAvailable_IPv6 = false;

			return false;
		}
	}
	else if (Protocol == AF_INET)
	{
		reinterpret_cast<sockaddr_in *>(&SockAddr)->sin_addr = reinterpret_cast<const sockaddr_in *>(OriginalSockAddr)->sin_addr;
		reinterpret_cast<sockaddr_in *>(&SockAddr)->sin_port = reinterpret_cast<const sockaddr_in *>(OriginalSockAddr)->sin_port;
		AddrLen = sizeof(sockaddr_in);

	//UDP connecting
		if (connect(InterfaceSocket, reinterpret_cast<const sockaddr *>(&SockAddr), sizeof(sockaddr_in)) == SOCKET_ERROR || 
			getsockname(InterfaceSocket, reinterpret_cast<sockaddr *>(&SockAddr), &AddrLen) == SOCKET_ERROR || 
			SockAddr.ss_family != AF_INET || AddrLen != sizeof(sockaddr_in) || 
			CheckEmptyBuffer(&reinterpret_cast<const sockaddr_in *>(&SockAddr)->sin_addr, sizeof(reinterpret_cast<const sockaddr_in *>(&SockAddr)->sin_addr)))
		{
			SocketSetting(InterfaceSocket, SOCKET_SETTING_TYPE::CLOSE, false, nullptr);
			GlobalRunningStatus.GatewayAvailable_IPv4 = false;

			return false;
		}
	}
	else {
		SocketSetting(InterfaceSocket, SOCKET_SETTING_TYPE::CLOSE, false, nullptr);
		return false;
	}

	SocketSetting(InterfaceSocket, SOCKET_SETTING_TYPE::CLOSE, false, nullptr);
	return true;
}
#endif

//Get gateway information
void GetGatewayInformation(
	const uint16_t Protocol)
{
//IPv6
	if (Protocol == AF_INET6)
	{
	//Gateway status from configure.
		if (Parameter.Target_Server_Main_IPv6.AddressData.Storage.ss_family == 0 && 
			Parameter.Target_Server_Alternate_IPv6.AddressData.Storage.ss_family == 0 && 
			Parameter.Target_Server_Local_Main_IPv6.Storage.ss_family == 0 && 
			Parameter.Target_Server_Local_Alternate_IPv6.Storage.ss_family == 0
		#if defined(ENABLE_LIBSODIUM)
			&& DNSCurveParameter.DNSCurve_Target_Server_Main_IPv6.AddressData.Storage.ss_family == 0
			&& DNSCurveParameter.DNSCurve_Target_Server_Alternate_IPv6.AddressData.Storage.ss_family == 0
		#endif
			)
		{
			GlobalRunningStatus.GatewayAvailable_IPv6 = false;
			return;
		}

	#if defined(PLATFORM_WIN)
	//Gateway status from system network stack.
		DWORD AdaptersIndex = 0;
		if ((Parameter.Target_Server_Main_IPv6.AddressData.Storage.ss_family != 0 && 
			GetBestInterfaceEx(
				reinterpret_cast<sockaddr *>(&Parameter.Target_Server_Main_IPv6.AddressData.IPv6), 
				&AdaptersIndex) != NO_ERROR) || 
			(Parameter.Target_Server_Alternate_IPv6.AddressData.Storage.ss_family != 0 && 
			GetBestInterfaceEx(
				reinterpret_cast<sockaddr *>(&Parameter.Target_Server_Alternate_IPv6.AddressData.IPv6), &
				AdaptersIndex) != NO_ERROR) || 
			(Parameter.Target_Server_Local_Main_IPv6.Storage.ss_family != 0 && 
			GetBestInterfaceEx(
				reinterpret_cast<sockaddr *>(&Parameter.Target_Server_Local_Main_IPv6.IPv6), 
				&AdaptersIndex) != NO_ERROR) || 
			(Parameter.Target_Server_Local_Alternate_IPv6.Storage.ss_family != 0 && 
			GetBestInterfaceEx(
				reinterpret_cast<sockaddr *>(&Parameter.Target_Server_Local_Alternate_IPv6.IPv6), 
				&AdaptersIndex) != NO_ERROR)
		#if defined(ENABLE_LIBSODIUM)
			|| (DNSCurveParameter.DNSCurve_Target_Server_Main_IPv6.AddressData.Storage.ss_family != 0 && 
			GetBestInterfaceEx(
				reinterpret_cast<sockaddr *>(&DNSCurveParameter.DNSCurve_Target_Server_Main_IPv6.AddressData.IPv6), 
				&AdaptersIndex) != NO_ERROR) || 
			(DNSCurveParameter.DNSCurve_Target_Server_Alternate_IPv6.AddressData.Storage.ss_family != 0 && 
			GetBestInterfaceEx(
				reinterpret_cast<sockaddr *>(&DNSCurveParameter.DNSCurve_Target_Server_Alternate_IPv6.AddressData.IPv6), 
				&AdaptersIndex) != NO_ERROR)
		#endif
			)
		{
			GlobalRunningStatus.GatewayAvailable_IPv6 = false;
			return;
		}

	//Multiple list
		if (Parameter.Target_Server_IPv6_Multiple != nullptr)
		{
			for (const auto &DNS_ServerDataItem:*Parameter.Target_Server_IPv6_Multiple)
			{
				if (GetBestInterfaceEx(
						reinterpret_cast<sockaddr *>(const_cast<sockaddr_in6 *>(&DNS_ServerDataItem.AddressData.IPv6)), 
						&AdaptersIndex) != NO_ERROR)
				{
					GlobalRunningStatus.GatewayAvailable_IPv6 = false;
					return;
				}
			}
		}
	#elif (defined(PLATFORM_FREEBSD) || defined(PLATFORM_LINUX) || defined(PLATFORM_MACOS))
	//Gateway status from system network stack.
		if ((Parameter.Target_Server_Main_IPv6.AddressData.Storage.ss_family != 0 && 
			!GetBestInterfaceAddress(AF_INET6, &Parameter.Target_Server_Main_IPv6.AddressData.Storage)) || 
			(Parameter.Target_Server_Alternate_IPv6.AddressData.Storage.ss_family != 0 && 
			!GetBestInterfaceAddress(AF_INET6, &Parameter.Target_Server_Alternate_IPv6.AddressData.Storage)) || 
			(Parameter.Target_Server_Local_Main_IPv6.Storage.ss_family != 0 && 
			!GetBestInterfaceAddress(AF_INET6, &Parameter.Target_Server_Local_Main_IPv6.Storage)) || 
			(Parameter.Target_Server_Local_Alternate_IPv6.Storage.ss_family != 0 && 
			!GetBestInterfaceAddress(AF_INET6, &Parameter.Target_Server_Local_Alternate_IPv6.Storage))
		#if defined(ENABLE_LIBSODIUM)
			|| (DNSCurveParameter.DNSCurve_Target_Server_Main_IPv6.AddressData.Storage.ss_family != 0 && 
			!GetBestInterfaceAddress(AF_INET6, &DNSCurveParameter.DNSCurve_Target_Server_Main_IPv6.AddressData.Storage)) || 
			(DNSCurveParameter.DNSCurve_Target_Server_Alternate_IPv6.AddressData.Storage.ss_family != 0 && 
			!GetBestInterfaceAddress(AF_INET6, &DNSCurveParameter.DNSCurve_Target_Server_Alternate_IPv6.AddressData.Storage))
		#endif
			)
		{
			GlobalRunningStatus.GatewayAvailable_IPv6 = false;
			return;
		}

	//Multiple list
		if (Parameter.Target_Server_IPv6_Multiple != nullptr)
		{
			for (const auto &DNS_ServerDataItem:*Parameter.Target_Server_IPv6_Multiple)
			{
				if (!GetBestInterfaceAddress(AF_INET6, &DNS_ServerDataItem.AddressData.Storage))
				{
					GlobalRunningStatus.GatewayAvailable_IPv6 = false;
					return;
				}
			}
		}
	#endif

		GlobalRunningStatus.GatewayAvailable_IPv6 = true;
	}
//IPv4
	else if (Protocol == AF_INET)
	{
	//Gateway status from configure.
		if (Parameter.Target_Server_Main_IPv4.AddressData.Storage.ss_family == 0 && 
			Parameter.Target_Server_Alternate_IPv4.AddressData.Storage.ss_family == 0 && 
			Parameter.Target_Server_Local_Main_IPv4.Storage.ss_family == 0 && 
			Parameter.Target_Server_Local_Alternate_IPv4.Storage.ss_family == 0
		#if defined(ENABLE_LIBSODIUM)
			&& DNSCurveParameter.DNSCurve_Target_Server_Main_IPv4.AddressData.Storage.ss_family == 0
			&& DNSCurveParameter.DNSCurve_Target_Server_Alternate_IPv4.AddressData.Storage.ss_family == 0
		#endif
			)
		{
			GlobalRunningStatus.GatewayAvailable_IPv4 = false;
			return;
		}

	#if defined(PLATFORM_WIN)
	//Gateway status from system network stack.
		DWORD AdaptersIndex = 0;
		if ((Parameter.Target_Server_Main_IPv4.AddressData.Storage.ss_family != 0 && 
			GetBestInterfaceEx(
				reinterpret_cast<sockaddr *>(&Parameter.Target_Server_Main_IPv4.AddressData.IPv4), 
				&AdaptersIndex) != NO_ERROR) || 
			(Parameter.Target_Server_Alternate_IPv4.AddressData.Storage.ss_family != 0 && 
			GetBestInterfaceEx(
				reinterpret_cast<sockaddr *>(&Parameter.Target_Server_Alternate_IPv4.AddressData.IPv4), 
				&AdaptersIndex) != NO_ERROR) || 
			(Parameter.Target_Server_Local_Main_IPv4.Storage.ss_family != 0 && 
			GetBestInterfaceEx(
				reinterpret_cast<sockaddr *>(&Parameter.Target_Server_Local_Main_IPv4.IPv4), 
				&AdaptersIndex) != NO_ERROR) || 
			(Parameter.Target_Server_Local_Alternate_IPv4.Storage.ss_family != 0 && 
			GetBestInterfaceEx(
				reinterpret_cast<sockaddr *>(&Parameter.Target_Server_Local_Alternate_IPv4.IPv4), 
				&AdaptersIndex) != NO_ERROR)
		#if defined(ENABLE_LIBSODIUM)
			|| DNSCurveParameter.DNSCurve_Target_Server_Main_IPv4.AddressData.Storage.ss_family != 0 && 
			(GetBestInterfaceEx(
				reinterpret_cast<sockaddr *>(&DNSCurveParameter.DNSCurve_Target_Server_Main_IPv4.AddressData.IPv4), 
				&AdaptersIndex) != NO_ERROR) || 
			DNSCurveParameter.DNSCurve_Target_Server_Alternate_IPv4.AddressData.Storage.ss_family != 0 && 
			(GetBestInterfaceEx(
				reinterpret_cast<sockaddr *>(&DNSCurveParameter.DNSCurve_Target_Server_Alternate_IPv4.AddressData.IPv4), 
				&AdaptersIndex) != NO_ERROR)
		#endif
			)
		{
			GlobalRunningStatus.GatewayAvailable_IPv4 = false;
			return;
		}

	//Multiple list
		if (Parameter.Target_Server_IPv4_Multiple != nullptr)
		{
			for (const auto &DNS_ServerDataItem:*Parameter.Target_Server_IPv4_Multiple)
			{
				if (GetBestInterfaceEx(
						reinterpret_cast<sockaddr *>(const_cast<sockaddr_in *>(&DNS_ServerDataItem.AddressData.IPv4)), 
						&AdaptersIndex) != NO_ERROR)
				{
					GlobalRunningStatus.GatewayAvailable_IPv4 = false;
					return;
				}
			}
		}
	#elif (defined(PLATFORM_FREEBSD) || defined(PLATFORM_LINUX) || defined(PLATFORM_MACOS))
	//Gateway status from system network stack.
		if ((Parameter.Target_Server_Main_IPv4.AddressData.Storage.ss_family != 0 && 
			!GetBestInterfaceAddress(AF_INET, &Parameter.Target_Server_Main_IPv4.AddressData.Storage)) || 
			(Parameter.Target_Server_Alternate_IPv4.AddressData.Storage.ss_family != 0 && 
			!GetBestInterfaceAddress(AF_INET, &Parameter.Target_Server_Alternate_IPv4.AddressData.Storage)) || 
			(Parameter.Target_Server_Local_Main_IPv4.Storage.ss_family != 0 && 
			!GetBestInterfaceAddress(AF_INET, &Parameter.Target_Server_Local_Main_IPv4.Storage)) || 
			(Parameter.Target_Server_Local_Alternate_IPv4.Storage.ss_family != 0 && 
			!GetBestInterfaceAddress(AF_INET, &Parameter.Target_Server_Local_Alternate_IPv4.Storage))
		#if defined(ENABLE_LIBSODIUM)
			|| (DNSCurveParameter.DNSCurve_Target_Server_Main_IPv4.AddressData.Storage.ss_family != 0 && 
			!GetBestInterfaceAddress(AF_INET, &DNSCurveParameter.DNSCurve_Target_Server_Main_IPv4.AddressData.Storage)) || 
			(DNSCurveParameter.DNSCurve_Target_Server_Alternate_IPv4.AddressData.Storage.ss_family != 0 && 
			!GetBestInterfaceAddress(AF_INET, &DNSCurveParameter.DNSCurve_Target_Server_Alternate_IPv4.AddressData.Storage))
		#endif
			)
		{
			GlobalRunningStatus.GatewayAvailable_IPv4 = false;
			return;
		}

	//Multiple list
		if (Parameter.Target_Server_IPv4_Multiple != nullptr)
		{
			for (const auto &DNS_ServerDataItem:*Parameter.Target_Server_IPv4_Multiple)
			{
				if (!GetBestInterfaceAddress(AF_INET, &DNS_ServerDataItem.AddressData.Storage))
				{
					GlobalRunningStatus.GatewayAvailable_IPv4 = false;
					return;
				}
			}
		}
	#endif

		GlobalRunningStatus.GatewayAvailable_IPv4 = true;
	}

	return;
}

//Local network information monitor
void NetworkInformationMonitor(
	void)
{
//Initialization
#if (defined(PLATFORM_FREEBSD) || defined(PLATFORM_LINUX) || defined(PLATFORM_WIN))
	std::array<uint8_t, ADDRESS_STRING_MAXSIZE + MEMORY_RESERVED_BYTES> AddrBuffer{};
	std::string DomainString;
#if defined(PLATFORM_WIN)
	std::array<uint8_t, DOMAIN_MAXSIZE + MEMORY_RESERVED_BYTES> HostName{};
	addrinfo *LocalAddressList = nullptr, *LocalAddressItem = nullptr;
#elif (defined(PLATFORM_FREEBSD) || defined(PLATFORM_LINUX))
	ifaddrs *InterfaceAddressList = nullptr, *InterfaceAddressItem = nullptr;
	auto IsErrorFirstPrint = true;
#endif
#elif defined(PLATFORM_MACOS)
	ifaddrs *InterfaceAddressList = nullptr, *InterfaceAddressItem = nullptr;
	auto IsErrorFirstPrint = true;
#endif
	dns_hdr *DNS_Header = nullptr;
	dns_qry *DNS_Query = nullptr;
	void *DNS_Record = nullptr;
	std::unique_lock<std::mutex> LocalAddressMutexIPv6(LocalAddressLock.at(NETWORK_LAYER_TYPE_IPV6), std::defer_lock), LocalAddressMutexIPv4(LocalAddressLock.at(NETWORK_LAYER_TYPE_IPV4), std::defer_lock), SocketRegisterMutex(SocketRegisterLock, std::defer_lock);

//Start listening Monitor.
	while (!GlobalRunningStatus.IsNeedExit)
	{
	//Get local machine addresses.
	//IPv6
		if (Parameter.ListenProtocol_Network == LISTEN_PROTOCOL_NETWORK::BOTH || Parameter.ListenProtocol_Network == LISTEN_PROTOCOL_NETWORK::IPV6)
		{
		#if defined(PLATFORM_WIN)
			HostName.fill(0);
			LocalAddressList = GetLocalAddressList(AF_INET6, HostName.data());
			if (LocalAddressList == nullptr)
		#elif (defined(PLATFORM_FREEBSD) || defined(PLATFORM_LINUX) || defined(PLATFORM_MACOS))
			errno = 0;
			if (getifaddrs(&InterfaceAddressList) != 0 || InterfaceAddressList == nullptr)
		#endif
			{
			#if (defined(PLATFORM_FREEBSD) || defined(PLATFORM_LINUX) || defined(PLATFORM_MACOS))
				PrintError(LOG_LEVEL_TYPE::LEVEL_3, LOG_ERROR_TYPE::NETWORK, L"Get local machine address error", errno, nullptr, 0);
				if (InterfaceAddressList != nullptr)
				{
					freeifaddrs(InterfaceAddressList);
					InterfaceAddressList = nullptr;
				}
			#endif

				goto JumpTo_Restart;
			}
			else {
				LocalAddressMutexIPv6.lock();
				memset(GlobalRunningStatus.LocalAddress_Response[NETWORK_LAYER_TYPE_IPV6], 0, PACKET_NORMAL_MAXSIZE + MEMORY_RESERVED_BYTES);
				GlobalRunningStatus.LocalAddress_Length[NETWORK_LAYER_TYPE_IPV6] = 0;
			#if (defined(PLATFORM_FREEBSD) || defined(PLATFORM_LINUX) || defined(PLATFORM_WIN))
				GlobalRunningStatus.LocalAddress_PointerResponse[NETWORK_LAYER_TYPE_IPV6]->clear();
				GlobalRunningStatus.LocalAddress_PointerResponse[NETWORK_LAYER_TYPE_IPV6]->shrink_to_fit();
			#endif

			//Mark local addresses(A part).
				DNS_Header = reinterpret_cast<dns_hdr *>(GlobalRunningStatus.LocalAddress_Response[NETWORK_LAYER_TYPE_IPV6]);
				DNS_Header->Flags = hton16(DNS_FLAG_SQR_NEA);
				DNS_Header->Question = hton16(UINT16_NUM_ONE);
				GlobalRunningStatus.LocalAddress_Length[NETWORK_LAYER_TYPE_IPV6] += sizeof(dns_hdr);
				memcpy_s(GlobalRunningStatus.LocalAddress_Response[NETWORK_LAYER_TYPE_IPV6] + GlobalRunningStatus.LocalAddress_Length[NETWORK_LAYER_TYPE_IPV6], PACKET_NORMAL_MAXSIZE - GlobalRunningStatus.LocalAddress_Length[NETWORK_LAYER_TYPE_IPV6], Parameter.Local_FQDN_Response, Parameter.Local_FQDN_Length);
				GlobalRunningStatus.LocalAddress_Length[NETWORK_LAYER_TYPE_IPV6] += Parameter.Local_FQDN_Length;
				DNS_Query = reinterpret_cast<dns_qry *>(GlobalRunningStatus.LocalAddress_Response[NETWORK_LAYER_TYPE_IPV6] + GlobalRunningStatus.LocalAddress_Length[NETWORK_LAYER_TYPE_IPV6]);
				DNS_Query->Type = hton16(DNS_TYPE_AAAA);
				DNS_Query->Classes = hton16(DNS_CLASS_INTERNET);
				GlobalRunningStatus.LocalAddress_Length[NETWORK_LAYER_TYPE_IPV6] += sizeof(dns_qry);

			//Read addresses list and convert to Fully Qualified Domain Name/FQDN PTR record.
			#if defined(PLATFORM_WIN)
				for (LocalAddressItem = LocalAddressList;LocalAddressItem != nullptr;LocalAddressItem = LocalAddressItem->ai_next)
				{
					if (LocalAddressItem->ai_family == AF_INET6 && LocalAddressItem->ai_addrlen == sizeof(sockaddr_in6) && 
						LocalAddressItem->ai_addr->sa_family == AF_INET6)
			#elif (defined(PLATFORM_FREEBSD) || defined(PLATFORM_LINUX) || defined(PLATFORM_MACOS))
				for (InterfaceAddressItem = InterfaceAddressList;InterfaceAddressItem != nullptr;InterfaceAddressItem = InterfaceAddressItem->ifa_next)
				{
					if (InterfaceAddressItem->ifa_addr != nullptr && InterfaceAddressItem->ifa_addr->sa_family == AF_INET6)
			#endif
					{
					//Mark local addresses(B part).
						if (GlobalRunningStatus.LocalAddress_Length[NETWORK_LAYER_TYPE_IPV6] <= PACKET_NORMAL_MAXSIZE - sizeof(dns_record_aaaa))
						{
							DNS_Record = reinterpret_cast<dns_record_aaaa *>(GlobalRunningStatus.LocalAddress_Response[NETWORK_LAYER_TYPE_IPV6] + GlobalRunningStatus.LocalAddress_Length[NETWORK_LAYER_TYPE_IPV6]);
							reinterpret_cast<dns_record_aaaa *>(DNS_Record)->Name = hton16(DNS_POINTER_QUERY);
							reinterpret_cast<dns_record_aaaa *>(DNS_Record)->Classes = hton16(DNS_CLASS_INTERNET);
							if (Parameter.HostsDefaultTTL > 0)
								reinterpret_cast<dns_record_aaaa *>(DNS_Record)->TTL = hton32(Parameter.HostsDefaultTTL);
							else 
								reinterpret_cast<dns_record_aaaa *>(DNS_Record)->TTL = hton32(DEFAULT_HOSTS_TTL);
							reinterpret_cast<dns_record_aaaa *>(DNS_Record)->Type = hton16(DNS_TYPE_AAAA);
							reinterpret_cast<dns_record_aaaa *>(DNS_Record)->Length = hton16(sizeof(reinterpret_cast<const dns_record_aaaa *>(DNS_Record)->Address));
						#if defined(PLATFORM_WIN)
							reinterpret_cast<dns_record_aaaa *>(DNS_Record)->Address = reinterpret_cast<const sockaddr_in6 *>(LocalAddressItem->ai_addr)->sin6_addr;
						#elif (defined(PLATFORM_FREEBSD) || defined(PLATFORM_LINUX) || defined(PLATFORM_MACOS))
							reinterpret_cast<dns_record_aaaa *>(DNS_Record)->Address = reinterpret_cast<const sockaddr_in6 *>(InterfaceAddressItem->ifa_addr)->sin6_addr;
						#endif
							GlobalRunningStatus.LocalAddress_Length[NETWORK_LAYER_TYPE_IPV6] += sizeof(dns_record_aaaa);
							++DNS_Header->Answer;
						}

					#if (defined(PLATFORM_FREEBSD) || defined(PLATFORM_LINUX) || defined(PLATFORM_WIN))
					//Convert from binary to string.
						DomainString.clear();
						for (ssize_t Index = sizeof(in6_addr) / sizeof(uint8_t) - 1U;Index >= 0;--Index)
						{
							AddrBuffer.fill(0);

						#if defined(PLATFORM_WIN)
							if (reinterpret_cast<const sockaddr_in6 *>(LocalAddressItem->ai_addr)->sin6_addr.s6_addr[Index] == 0)
						#elif (defined(PLATFORM_FREEBSD) || defined(PLATFORM_LINUX))
							if (reinterpret_cast<const sockaddr_in6 *>(InterfaceAddressItem->ifa_addr)->sin6_addr.s6_addr[Index] == 0)
						#endif
							{
								DomainString.append("0.0.");
							}
						#if defined(PLATFORM_WIN)
							else if (reinterpret_cast<const sockaddr_in6 *>(LocalAddressItem->ai_addr)->sin6_addr.s6_addr[Index] < 0x10)
						#elif (defined(PLATFORM_FREEBSD) || defined(PLATFORM_LINUX))
							else if (reinterpret_cast<const sockaddr_in6 *>(InterfaceAddressItem->ifa_addr)->sin6_addr.s6_addr[Index] < 0x10)
						#endif
							{
							#if defined(PLATFORM_WIN)
								if (snprintf(reinterpret_cast<char *>(AddrBuffer.data()), ADDRESS_STRING_MAXSIZE, "%x", reinterpret_cast<const sockaddr_in6 *>(LocalAddressItem->ai_addr)->sin6_addr.s6_addr[Index]) < 0 || 
							#elif (defined(PLATFORM_FREEBSD) || defined(PLATFORM_LINUX))
								if (snprintf(reinterpret_cast<char *>(AddrBuffer.data()), ADDRESS_STRING_MAXSIZE, "%x", reinterpret_cast<const sockaddr_in6 *>(InterfaceAddressItem->ifa_addr)->sin6_addr.s6_addr[Index]) < 0 || 
							#endif
									strnlen_s(reinterpret_cast<const char *>(AddrBuffer.data()), ADDRESS_STRING_MAXSIZE) != 1U)
								{
									goto StopLoop;
								}
								else {
									DomainString.append(reinterpret_cast<const char *>(AddrBuffer.data()));
									DomainString.append(".0.");
								}
							}
							else {
							#if defined(PLATFORM_WIN)
								if (snprintf(reinterpret_cast<char *>(AddrBuffer.data()), ADDRESS_STRING_MAXSIZE, "%x", reinterpret_cast<const sockaddr_in6 *>(LocalAddressItem->ai_addr)->sin6_addr.s6_addr[Index]) < 0 || 
							#elif (defined(PLATFORM_FREEBSD) || defined(PLATFORM_LINUX))
								if (snprintf(reinterpret_cast<char *>(AddrBuffer.data()), ADDRESS_STRING_MAXSIZE, "%x", reinterpret_cast<const sockaddr_in6 *>(InterfaceAddressItem->ifa_addr)->sin6_addr.s6_addr[Index]) < 0 || 
							#endif
									strnlen_s(reinterpret_cast<const char *>(AddrBuffer.data()), ADDRESS_STRING_MAXSIZE) != 2U)
								{
									goto StopLoop;
								}
								else {
									DomainString.append(reinterpret_cast<const char *>(AddrBuffer.data() + 1U), 1U);
									DomainString.append(".");
									DomainString.append(reinterpret_cast<const char *>(AddrBuffer.data()), 1U);
									DomainString.append(".");
								}
							}
						}

					//Register to global list.
						DomainString.append("ip6.arpa");
						GlobalRunningStatus.LocalAddress_PointerResponse[NETWORK_LAYER_TYPE_IPV6]->push_back(DomainString);
					#endif
					}
				}

			//Jump here to stop loop.
			StopLoop:

			//Mark local addresses(C part).
				if (DNS_Header->Answer == 0)
				{
					memset(GlobalRunningStatus.LocalAddress_Response[NETWORK_LAYER_TYPE_IPV6], 0, PACKET_NORMAL_MAXSIZE + MEMORY_RESERVED_BYTES);
					GlobalRunningStatus.LocalAddress_Length[NETWORK_LAYER_TYPE_IPV6] = 0;
				}
				else {
					DNS_Header->Answer = hton16(DNS_Header->Answer);
				}

			//Free all lists.
				LocalAddressMutexIPv6.unlock();
			#if defined(PLATFORM_WIN)
				freeaddrinfo(LocalAddressList);
				LocalAddressList = nullptr;
			#elif (defined(PLATFORM_FREEBSD) || defined(PLATFORM_LINUX) || defined(PLATFORM_MACOS))
				freeifaddrs(InterfaceAddressList);
				InterfaceAddressList = nullptr;
			#endif
			}
		}
	//IPv4
		if (Parameter.ListenProtocol_Network == LISTEN_PROTOCOL_NETWORK::BOTH || Parameter.ListenProtocol_Network == LISTEN_PROTOCOL_NETWORK::IPV4)
		{
		#if defined(PLATFORM_WIN)
			HostName.fill(0);
			LocalAddressList = GetLocalAddressList(AF_INET, HostName.data());
			if (LocalAddressList == nullptr)
		#elif (defined(PLATFORM_FREEBSD) || defined(PLATFORM_LINUX) || defined(PLATFORM_MACOS))
			errno = 0;
			if (getifaddrs(&InterfaceAddressList) != 0 || InterfaceAddressList == nullptr)
		#endif
			{
			#if (defined(PLATFORM_FREEBSD) || defined(PLATFORM_LINUX) || defined(PLATFORM_MACOS))
				PrintError(LOG_LEVEL_TYPE::LEVEL_3, LOG_ERROR_TYPE::NETWORK, L"Get local machine address error", errno, nullptr, 0);
				if (InterfaceAddressList != nullptr)
				{
					freeifaddrs(InterfaceAddressList);
					InterfaceAddressList = nullptr;
				}
			#endif

				goto JumpTo_Restart;
			}
			else {
				LocalAddressMutexIPv4.lock();
				memset(GlobalRunningStatus.LocalAddress_Response[NETWORK_LAYER_TYPE_IPV4], 0, PACKET_NORMAL_MAXSIZE + MEMORY_RESERVED_BYTES);
				GlobalRunningStatus.LocalAddress_Length[NETWORK_LAYER_TYPE_IPV4] = 0;
			#if (defined(PLATFORM_FREEBSD) || defined(PLATFORM_LINUX) || defined(PLATFORM_WIN))
				GlobalRunningStatus.LocalAddress_PointerResponse[NETWORK_LAYER_TYPE_IPV4]->clear();
				GlobalRunningStatus.LocalAddress_PointerResponse[NETWORK_LAYER_TYPE_IPV4]->shrink_to_fit();
			#endif

			//Mark local addresses(A part).
				DNS_Header = reinterpret_cast<dns_hdr *>(GlobalRunningStatus.LocalAddress_Response[NETWORK_LAYER_TYPE_IPV4]);
				DNS_Header->Flags = hton16(DNS_FLAG_SQR_NEA);
				DNS_Header->Question = hton16(UINT16_NUM_ONE);
				GlobalRunningStatus.LocalAddress_Length[NETWORK_LAYER_TYPE_IPV4] += sizeof(dns_hdr);
				memcpy_s(GlobalRunningStatus.LocalAddress_Response[NETWORK_LAYER_TYPE_IPV4] + GlobalRunningStatus.LocalAddress_Length[NETWORK_LAYER_TYPE_IPV4], PACKET_NORMAL_MAXSIZE - GlobalRunningStatus.LocalAddress_Length[NETWORK_LAYER_TYPE_IPV4], Parameter.Local_FQDN_Response, Parameter.Local_FQDN_Length);
				GlobalRunningStatus.LocalAddress_Length[NETWORK_LAYER_TYPE_IPV4] += Parameter.Local_FQDN_Length;
				DNS_Query = reinterpret_cast<dns_qry *>(GlobalRunningStatus.LocalAddress_Response[NETWORK_LAYER_TYPE_IPV4] + GlobalRunningStatus.LocalAddress_Length[NETWORK_LAYER_TYPE_IPV4]);
				DNS_Query->Type = hton16(DNS_TYPE_AAAA);
				DNS_Query->Classes = hton16(DNS_CLASS_INTERNET);
				GlobalRunningStatus.LocalAddress_Length[NETWORK_LAYER_TYPE_IPV4] += sizeof(dns_qry);

			//Read addresses list and convert to Fully Qualified Domain Name/FQDN PTR record.
			#if defined(PLATFORM_WIN)
				for (LocalAddressItem = LocalAddressList;LocalAddressItem != nullptr;LocalAddressItem = LocalAddressItem->ai_next)
				{
					if (LocalAddressItem->ai_family == AF_INET && LocalAddressItem->ai_addrlen == sizeof(sockaddr_in) && 
						LocalAddressItem->ai_addr->sa_family == AF_INET)
			#elif (defined(PLATFORM_FREEBSD) || defined(PLATFORM_LINUX) || defined(PLATFORM_MACOS))
				for (InterfaceAddressItem = InterfaceAddressList;InterfaceAddressItem != nullptr;InterfaceAddressItem = InterfaceAddressItem->ifa_next)
				{
					if (InterfaceAddressItem->ifa_addr != nullptr && InterfaceAddressItem->ifa_addr->sa_family == AF_INET)
			#endif
					{
					//Mark local addresses(B part).
						if (GlobalRunningStatus.LocalAddress_Length[NETWORK_LAYER_TYPE_IPV4] <= PACKET_NORMAL_MAXSIZE - sizeof(dns_record_a))
						{
							DNS_Record = reinterpret_cast<dns_record_a *>(GlobalRunningStatus.LocalAddress_Response[NETWORK_LAYER_TYPE_IPV4] + GlobalRunningStatus.LocalAddress_Length[NETWORK_LAYER_TYPE_IPV4]);
							reinterpret_cast<dns_record_a *>(DNS_Record)->Name = hton16(DNS_POINTER_QUERY);
							reinterpret_cast<dns_record_a *>(DNS_Record)->Classes = hton16(DNS_CLASS_INTERNET);
							if (Parameter.HostsDefaultTTL > 0)
								reinterpret_cast<dns_record_a *>(DNS_Record)->TTL = hton32(Parameter.HostsDefaultTTL);
							else 
								reinterpret_cast<dns_record_a *>(DNS_Record)->TTL = hton32(DEFAULT_HOSTS_TTL);
							reinterpret_cast<dns_record_a *>(DNS_Record)->Type = hton16(DNS_TYPE_A);
							reinterpret_cast<dns_record_a *>(DNS_Record)->Length = hton16(sizeof(reinterpret_cast<const dns_record_a *>(DNS_Record)->Address));
						#if defined(PLATFORM_WIN)
							reinterpret_cast<dns_record_a *>(DNS_Record)->Address = reinterpret_cast<const sockaddr_in *>(LocalAddressItem->ai_addr)->sin_addr;
						#elif (defined(PLATFORM_FREEBSD) || defined(PLATFORM_LINUX) || defined(PLATFORM_MACOS))
							reinterpret_cast<dns_record_a *>(DNS_Record)->Address = reinterpret_cast<const sockaddr_in *>(InterfaceAddressItem->ifa_addr)->sin_addr;
						#endif
							GlobalRunningStatus.LocalAddress_Length[NETWORK_LAYER_TYPE_IPV4] += sizeof(dns_record_a);
							++DNS_Header->Answer;
						}

					#if (defined(PLATFORM_FREEBSD) || defined(PLATFORM_LINUX) || defined(PLATFORM_WIN))
					//Convert from binary to DNS PTR response.
						AddrBuffer.fill(0);
						DomainString.clear();
					#if defined(PLATFORM_WIN)
						snprintf(reinterpret_cast<char *>(AddrBuffer.data()), ADDRESS_STRING_MAXSIZE, "%u", *((reinterpret_cast<const uint8_t *>(&reinterpret_cast<const sockaddr_in *>(LocalAddressItem->ai_addr)->sin_addr)) + sizeof(uint8_t) * 3U));
					#elif (defined(PLATFORM_FREEBSD) || defined(PLATFORM_LINUX))
						snprintf(reinterpret_cast<char *>(AddrBuffer.data()), ADDRESS_STRING_MAXSIZE, "%u", *((reinterpret_cast<const uint8_t *>(&reinterpret_cast<const sockaddr_in *>(InterfaceAddressItem->ifa_addr)->sin_addr)) + sizeof(uint8_t) * 3U));
					#endif
						DomainString.append(reinterpret_cast<const char *>(AddrBuffer.data()));
						AddrBuffer.fill(0);
						DomainString.append(".");
					#if defined(PLATFORM_WIN)
						snprintf(reinterpret_cast<char *>(AddrBuffer.data()), ADDRESS_STRING_MAXSIZE, "%u", *((reinterpret_cast<const uint8_t *>(&reinterpret_cast<const sockaddr_in *>(LocalAddressItem->ai_addr)->sin_addr)) + sizeof(uint8_t) * 2U));
					#elif (defined(PLATFORM_FREEBSD) || defined(PLATFORM_LINUX))
						snprintf(reinterpret_cast<char *>(AddrBuffer.data()), ADDRESS_STRING_MAXSIZE, "%u", *((reinterpret_cast<const uint8_t *>(&reinterpret_cast<const sockaddr_in *>(InterfaceAddressItem->ifa_addr)->sin_addr)) + sizeof(uint8_t) * 2U));
					#endif
						DomainString.append(reinterpret_cast<const char *>(AddrBuffer.data()));
						AddrBuffer.fill(0);
						DomainString.append(".");
					#if defined(PLATFORM_WIN)
						snprintf(reinterpret_cast<char *>(AddrBuffer.data()), ADDRESS_STRING_MAXSIZE, "%u", *((reinterpret_cast<const uint8_t *>(&reinterpret_cast<const sockaddr_in *>(LocalAddressItem->ai_addr)->sin_addr)) + sizeof(uint8_t)));
					#elif (defined(PLATFORM_FREEBSD) || defined(PLATFORM_LINUX))
						snprintf(reinterpret_cast<char *>(AddrBuffer.data()), ADDRESS_STRING_MAXSIZE, "%u", *((reinterpret_cast<const uint8_t *>(&reinterpret_cast<const sockaddr_in *>(InterfaceAddressItem->ifa_addr)->sin_addr)) + sizeof(uint8_t)));
					#endif
						DomainString.append(reinterpret_cast<const char *>(AddrBuffer.data()));
						AddrBuffer.fill(0);
						DomainString.append(".");
					#if defined(PLATFORM_WIN)
						snprintf(reinterpret_cast<char *>(AddrBuffer.data()), ADDRESS_STRING_MAXSIZE, "%u", *(reinterpret_cast<const uint8_t *>(&reinterpret_cast<const sockaddr_in *>(LocalAddressItem->ai_addr)->sin_addr)));
					#elif (defined(PLATFORM_FREEBSD) || defined(PLATFORM_LINUX))
						snprintf(reinterpret_cast<char *>(AddrBuffer.data()), ADDRESS_STRING_MAXSIZE, "%u", *(reinterpret_cast<const uint8_t *>(&reinterpret_cast<const sockaddr_in *>(InterfaceAddressItem->ifa_addr)->sin_addr)));
					#endif
						DomainString.append(reinterpret_cast<const char *>(AddrBuffer.data()));
						AddrBuffer.fill(0);

					//Register to global list.
						DomainString.append(".in-addr.arpa");
						GlobalRunningStatus.LocalAddress_PointerResponse[NETWORK_LAYER_TYPE_IPV4]->push_back(DomainString);
					#endif
					}
				}

			//Mark local addresses(C part).
				if (DNS_Header->Answer == 0)
				{
					memset(GlobalRunningStatus.LocalAddress_Response[NETWORK_LAYER_TYPE_IPV4], 0, PACKET_NORMAL_MAXSIZE + MEMORY_RESERVED_BYTES);
					GlobalRunningStatus.LocalAddress_Length[NETWORK_LAYER_TYPE_IPV4] = 0;
				}
				else {
					DNS_Header->Answer = hton16(DNS_Header->Answer);
				}

			//Free all lists.
				LocalAddressMutexIPv4.unlock();
			#if defined(PLATFORM_WIN)
				freeaddrinfo(LocalAddressList);
				LocalAddressList = nullptr;
			#elif (defined(PLATFORM_FREEBSD) || defined(PLATFORM_LINUX) || defined(PLATFORM_MACOS))
				freeifaddrs(InterfaceAddressList);
				InterfaceAddressList = nullptr;
			#endif
			}
		}

	//Get gateway information and check.
		GetGatewayInformation(AF_INET6);
		GetGatewayInformation(AF_INET);
		if (!GlobalRunningStatus.GatewayAvailable_IPv4)
		{
		#if defined(PLATFORM_WIN)
			if (!GlobalRunningStatus.GatewayAvailable_IPv6)
		#elif (defined(PLATFORM_FREEBSD) || defined(PLATFORM_LINUX) || defined(PLATFORM_MACOS))
			if (!(IsErrorFirstPrint || GlobalRunningStatus.GatewayAvailable_IPv6))
		#endif
				PrintError(LOG_LEVEL_TYPE::LEVEL_3, LOG_ERROR_TYPE::NETWORK, L"No any available gateways to external network", 0, nullptr, 0);

		#if (defined(PLATFORM_FREEBSD) || defined(PLATFORM_LINUX) || defined(PLATFORM_MACOS))
			IsErrorFirstPrint = false;
		#endif
		}

	//Jump here to restart.
	JumpTo_Restart:

	//Free all lists.
	#if defined(PLATFORM_WIN)
		if (LocalAddressList != nullptr)
		{
			freeaddrinfo(LocalAddressList);
			LocalAddressList = nullptr;
		}
	#elif (defined(PLATFORM_FREEBSD) || defined(PLATFORM_LINUX) || defined(PLATFORM_MACOS))
		if (InterfaceAddressList != nullptr)
		{
			freeifaddrs(InterfaceAddressList);
			InterfaceAddressList = nullptr;
		}
	#endif

	//Close all socket registers.
		SocketRegisterMutex.lock();
		while (!SocketRegisterList.empty() && SocketRegisterList.front().SOCKET_REGISTER_DATA_TIME <= GetCurrentSystemTime())
		{
			SocketSetting(SocketRegisterList.front().SOCKET_REGISTER_DATA_SOCKET, SOCKET_SETTING_TYPE::CLOSE, false, nullptr);
			SocketRegisterList.pop_front();
		}
		SocketRegisterMutex.unlock();

	//Wait for interval time.
		Sleep(Parameter.FileRefreshTime);
	}

//Loop terminated
	if (!GlobalRunningStatus.IsNeedExit)
		PrintError(LOG_LEVEL_TYPE::LEVEL_2, LOG_ERROR_TYPE::SYSTEM, L"Get Local Address Information module Monitor terminated", 0, nullptr, 0);
	return;
}
