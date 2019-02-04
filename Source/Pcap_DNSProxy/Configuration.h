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


#ifndef PCAP_DNSPROXY_CONFIGURATION_H
#define PCAP_DNSPROXY_CONFIGURATION_H

#include "Include.h"

//Size and length definitions
#define BOM_UTF_16_LENGTH                     2U 
#define BOM_UTF_32_LENGTH                     4U
#define BOM_UTF_8_LENGTH                      3U
#define READ_DATA_MINSIZE                     4U
#define READ_TEXT_MINSIZE                     2U
#define READ_PARAMETER_MINSIZE                8U
#define READ_HOSTS_MINSIZE                    3U
#define READ_HOSTS_ADDRESS_MINSIZE            5U
#define READ_IPFILTER_MINSIZE                 3U
#define READ_IPFILTER_MAIN_MINSIZE            5U
#define READ_IPFILTER_BLACKLIST_MINSIZE       3U
#define READ_IPFILTER_LOCAL_ROUTING_MINSIZE   4U

//Structure definitions
typedef enum class _label_ipfilter_type_
{
	NONE, 
	NORMAL, 
	BLACKLIST, 
	LOCAL_ROUTING
}LABEL_IPFILTER_TYPE;
typedef enum class _label_hosts_type_
{
	NONE, 
	WHITE, 
	BANNED, 
	WHITE_EXTENDED, 
	BANNED_EXTENDED, 
	NORMAL, 
	CNAME, 
	LOCAL, 
	ADDRESS, 
	SOURCE
}LABEL_HOSTS_TYPE;

//Global variables
extern CONFIGURATION_TABLE Parameter, ParameterModificating;
extern GLOBAL_STATUS GlobalRunningStatus;
#if defined(ENABLE_LIBSODIUM)
extern DNSCURVE_CONFIGURATION_TABLE DNSCurveParameter, DNSCurveParameterModificating;
#endif
extern std::vector<FILE_DATA> FileList_Config, FileList_IPFilter, FileList_Hosts;
#if defined(ENABLE_LIBSODIUM)
extern std::vector<FILE_DATA> FileList_DNSCurveDatabase;
#endif
extern std::vector<DIFFERNET_FILE_SET_IPFILTER> *IPFilterFileSetUsing, *IPFilterFileSetModificating;
extern std::vector<DIFFERNET_FILE_SET_HOSTS> *HostsFileSetUsing, *HostsFileSetModificating;
extern std::mutex IPFilterFileLock, HostsFileLock;

//Functions in Configuration.cpp
bool ReadSupport_ReadText(
	const FILE * const FileHandle, 
	const READ_TEXT_TYPE InputType, 
	const size_t FileIndex);
bool ReadSupport_FileAttributesLoop(
	const READ_TEXT_TYPE InputType, 
	const size_t FileIndex, 
	FILE_DATA &FileListItem, 
	bool &IsFileModified);
void ClearModificatingListData(
	const READ_TEXT_TYPE ClearType, 
	const size_t FileIndex);
void ReadSupport_GetParameterListData(
	std::vector<std::string> &ListData, 
	const std::string &Data, 
	const size_t DataOffset, 
	const size_t Length, 
	const uint8_t SeparatedSign, 
	const bool IsCaseConvert, 
	const bool IsKeepEmptyItem);

//Functions in ReadParameter.cpp
bool Parameter_CheckSetting(
	const bool IsFirstRead, 
	const size_t FileIndex);
uint16_t ServiceNameToBinary(
	const uint8_t * const OriginalBuffer);
uint16_t DNSTypeNameToBinary(
	const uint8_t * const OriginalBuffer);
bool ReadParameterData_Whole(
	std::string Data, 
	const size_t FileIndex, 
	const bool IsFirstRead, 
	const size_t Line);
bool ReadParameterData_Base(
	const std::string &Data, 
	const size_t FileIndex, 
	const bool IsFirstRead, 
	const size_t Line, 
	CONFIGURATION_TABLE * const ParameterPointer, 
	bool &IsFoundParameter);
bool ReadParameterData_Log(
	const std::string &Data, 
	const size_t FileIndex, 
//	const bool IsFirstRead, 
	const size_t Line, 
	CONFIGURATION_TABLE * const ParameterPointer, 
	bool &IsFoundParameter);
bool ReadParameterData_Listen(
	const std::string &Data, 
	const size_t FileIndex, 
	const bool IsFirstRead, 
	const size_t Line, 
	CONFIGURATION_TABLE * const ParameterPointer, 
	bool &IsFoundParameter);
bool ReadParameterData_DNS(
	const std::string &Data, 
	const size_t FileIndex, 
	const bool IsFirstRead, 
	const size_t Line, 
	CONFIGURATION_TABLE * const ParameterPointer, 
	bool &IsFoundParameter);
bool ReadParameterData_Local_DNS(
	const std::string &Data, 
//	const size_t FileIndex, 
	const bool IsFirstRead, 
//	const size_t Line, 
	CONFIGURATION_TABLE * const ParameterPointer, 
	bool &IsFoundParameter);
bool ReadParameterData_Addresses(
	const std::string &Data, 
	const size_t FileIndex, 
	const bool IsFirstRead, 
	const size_t Line, 
//	CONFIGURATION_TABLE * const ParameterPointer, 
	bool &IsFoundParameter);
bool ReadParameterData_Values(
	const std::string &Data, 
	const size_t FileIndex, 
	const bool IsFirstRead, 
	const size_t Line, 
	CONFIGURATION_TABLE * const ParameterPointer, 
	bool &IsFoundParameter);
bool ReadParameterData_Switches(
	const std::string &Data, 
	const size_t FileIndex, 
	const bool IsFirstRead, 
	const size_t Line, 
	CONFIGURATION_TABLE * const ParameterPointer, 
	bool &IsFoundParameter);
bool ReadParameterData_Data(
	const std::string &Data, 
	const size_t FileIndex, 
	const bool IsFirstRead, 
	const size_t Line, 
	CONFIGURATION_TABLE * const ParameterPointer, 
	bool &IsFoundParameter);
bool ReadParameterData_Proxy(
	const std::string &Data, 
	const size_t FileIndex, 
	const bool IsFirstRead, 
	const size_t Line, 
	CONFIGURATION_TABLE * const ParameterPointer, 
	bool &IsFoundParameter);
#if defined(ENABLE_LIBSODIUM)
bool ReadParameterData_DNSCurve_Main(
	const std::string &Data, 
	const size_t FileIndex, 
	const bool IsFirstRead, 
	const size_t Line, 
//	CONFIGURATION_TABLE * const ParameterPointer, 
	DNSCURVE_CONFIGURATION_TABLE * const DNSCurveParameterPointer, 
	bool &IsFoundParameter);
bool ReadParameterData_DNSCurve_Database(
	const std::string &Data, 
	const size_t FileIndex, 
	const bool IsFirstRead, 
	const size_t Line, 
//	CONFIGURATION_TABLE * const ParameterPointer, 
//	DNSCURVE_CONFIGURATION_TABLE * const DNSCurveParameterPointer, 
	bool &IsFoundParameter);
bool ReadParameterData_DNSCurve_Addresses(
	const std::string &Data, 
	const size_t FileIndex, 
	const bool IsFirstRead, 
	const size_t Line, 
//	CONFIGURATION_TABLE * const ParameterPointer, 
//	DNSCURVE_CONFIGURATION_TABLE * const DNSCurveParameterPointer, 
	bool &IsFoundParameter);
bool ReadParameterData_DNSCurve_Keys(
	const std::string &Data, 
	const size_t FileIndex, 
//	const bool IsFirstRead, 
	const size_t Line, 
//	CONFIGURATION_TABLE * const ParameterPointer, 
	DNSCURVE_CONFIGURATION_TABLE * const DNSCurveParameterPointer, 
	bool &IsFoundParameter);
bool ReadParameterData_DNSCurve_MagicNumber(
	const std::string &Data, 
	const size_t FileIndex, 
//	const bool IsFirstRead, 
	const size_t Line, 
//	CONFIGURATION_TABLE * const ParameterPointer, 
	DNSCURVE_CONFIGURATION_TABLE * const DNSCurveParameterPointer, 
	bool &IsFoundParameter);
#endif
bool ReadSupport_PathFileName(
	std::string Data, 
	const size_t DataOffset, 
	const bool IsPath, 
	std::vector<std::wstring> * const ListData, 
#if (defined(PLATFORM_FREEBSD) || defined(PLATFORM_LINUX) || defined(PLATFORM_MACOS))
	std::vector<std::string> * const ListData_MBS, 
#endif
	const size_t FileIndex, 
	const size_t Line);
bool ReadSupport_MultipleAddresses(
	const uint16_t Protocol, 
	std::string Data, 
	const size_t DataOffset, 
	std::vector<DNS_SERVER_DATA> * const DNSServerDataList, 
	const READ_TEXT_TYPE InputType, 
	const size_t FileIndex, 
	const size_t Line);
bool ReadSupport_SOCKS_AddressDomain(
	std::string Data, 
	const size_t DataOffset, 
	CONFIGURATION_TABLE * const ParameterPointer, 
	const size_t FileIndex, 
	const size_t Line);
#if defined(ENABLE_PCAP)
bool ReadSupport_HopLimitsData(
	const uint16_t Protocol, 
	std::string Data, 
	const size_t DataOffset, 
	std::vector<DNS_SERVER_DATA> * const DNSServerDataList, 
	const bool IsFirstRead, 
	const size_t FileIndex, 
	const size_t Line);
#endif
bool ReadSupport_AddressPrefixData(
	const uint16_t Protocol, 
	std::string OriginalData, 
	const size_t DataOffset, 
	ADDRESS_PREFIX_BLOCK * const AddressPrefix, 
	const std::vector<FILE_DATA> &FileList, 
	const size_t FileIndex, 
	const size_t Line);
#if defined(ENABLE_LIBSODIUM)
void ReadSupport_DNSCurveDatabaseData(
	std::string Data, 
	const READ_TEXT_TYPE InputType, 
	const size_t FileIndex, 
	const size_t Line);
bool ReadSupport_DNSCurveDatabaseItem(
	const READ_TEXT_TYPE InputType);
bool ReadSupport_DNSCurveProviderName(
	std::string Data, 
	const size_t DataOffset, 
	uint8_t * const ProviderNameData, 
	const READ_TEXT_TYPE InputType, 
	const size_t FileIndex, 
	const size_t Line);
bool ReadSupport_DNSCurveKey(
	std::string Data, 
	const size_t DataOffset, 
	uint8_t * const KeyData, 
	const READ_TEXT_TYPE InputType, 
	const size_t FileIndex, 
	const size_t Line);
#endif
bool ReadSupport_DNSCurveMagicNumber(
	std::string Data, 
	const size_t DataOffset, 
	uint8_t * const MagicNumber, 
	const size_t FileIndex, 
	const size_t Line);

//Functions in ReadIPFilter.cpp
bool ReadIPFilterData(
	std::string Data, 
	const size_t FileIndex, 
	const size_t Line, 
	LABEL_IPFILTER_TYPE &LabelType, 
	bool &IsStopLabel);
bool ReadIPFilter_BlacklistData(
	std::string Data, 
	const size_t FileIndex, 
	const size_t Line);
bool ReadIPFilter_LocalRoutingData(
	std::string Data, 
	const size_t FileIndex, 
	const size_t Line);
bool ReadIPFilter_MainData(
	std::string Data, 
	const size_t FileIndex, 
	const size_t Line);

//Functions in ReadHosts.cpp
bool ReadHostsData(
	std::string Data, 
	const size_t FileIndex, 
	const size_t Line, 
	LABEL_HOSTS_TYPE &LabelType, 
	bool &IsStopLabel);
bool ReadHosts_OtherData(
	std::string Data, 
	const size_t FileIndex, 
	const size_t Line, 
	const LABEL_HOSTS_TYPE LabelType, 
	const LABEL_HOSTS_TYPE ItemType);
bool ReadHosts_LocalData(
	std::string Data, 
	const size_t FileIndex, 
	const size_t Line);
bool ReadHosts_AddressData(
	std::string Data, 
	const size_t FileIndex, 
	const size_t Line);
bool ReadHosts_MainData(
	std::string Data, 
	const HOSTS_TYPE HostsType, 
	const size_t FileIndex, 
	const size_t Line);
#endif
