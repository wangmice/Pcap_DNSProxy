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


#ifndef PCAP_DNSPROXY_PACKETDATA_H
#define PCAP_DNSPROXY_PACKETDATA_H

#include "Include.h"

//Structure definitions
typedef enum _cpm_pointer_type_
{
	CPM_POINTER_TYPE_HEADER, 
	CPM_POINTER_TYPE_RR, 
	CPM_POINTER_TYPE_ADDITIONAL
}CPM_POINTER_TYPE;

//Global variables
extern CONFIGURATION_TABLE Parameter;
extern GLOBAL_STATUS GlobalRunningStatus;
extern std::list<DNS_CACHE_DATA> DNSCacheList;
extern std::unordered_multimap<std::string, std::list<DNS_CACHE_DATA>::iterator> DNSCacheIndexList;
extern std::mutex DNSCacheListLock;

//Functions
void RemoveExpiredDomainCache(
	void);
#endif
