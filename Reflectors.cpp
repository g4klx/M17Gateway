/*
*   Copyright (C) 2016,2018,2020,2025 by Jonathan Naylor G4KLX
*
*   This program is free software; you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation; either version 2 of the License, or
*   (at your option) any later version.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program; if not, write to the Free Software
*   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include "Reflectors.h"
#include "M17Defines.h"
#include "Log.h"

#include <fstream>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <functional>
#include <cstdio>
#include <cassert>
#include <cstring>
#include <cctype>

CReflectors::CReflectors(const std::string& hostsFile1, const std::string& hostsFile2, unsigned int reloadTime) :
m_hostsFile1(hostsFile1),
m_hostsFile2(hostsFile2),
m_reflectors(),
m_timer(1000U, reloadTime * 60U)
{
	if (reloadTime > 0U)
		m_timer.start();
}

CReflectors::~CReflectors()
{
	remove();
}

bool CReflectors::load()
{
	remove();

	bool ret = parseJSON(m_hostsFile1);
	if (!ret)
		return false;

	parseHosts(m_hostsFile2);

	size_t size = m_reflectors.size();
	LogInfo("Loaded %u M17 reflectors", size);

	if (size == 0U)
		return false;

	return true;
}

CM17Reflector* CReflectors::find(const std::string& name)
{
	std::string nm = name;
	nm.resize(7U);

	for (const auto& it : m_reflectors) {
		if (nm == it->m_name)
			return it;
	}

	return nullptr;
}

void CReflectors::clock(unsigned int ms)
{
	m_timer.clock(ms);

	if (m_timer.isRunning() && m_timer.hasExpired()) {
		load();
		m_timer.start();
	}
}

void CReflectors::remove()
{
	// Clear out the old reflector list
	for (std::vector<CM17Reflector*>::iterator it = m_reflectors.begin(); it != m_reflectors.end(); ++it)
		delete* it;

	m_reflectors.clear();
}

bool CReflectors::parseJSON(const std::string& fileName)
{
	try {
		std::fstream file(fileName);

		nlohmann::json data = nlohmann::json::parse(file);

		bool hasData = data["reflectors"].is_array();
		if (!hasData)
			throw;

		nlohmann::json::array_t hosts = data["reflectors"];
		for (const auto& it : hosts) {
			std::string name = it["designator"];

			unsigned short port = it["port"];

			sockaddr_storage addr_v4 = sockaddr_storage();
			unsigned int     addrLen_v4 = 0U;

			bool isNull = it["ipv4"].is_null();
			if (!isNull) {
				std::string ipv4 = it["ipv4"];
				if (!CUDPSocket::lookup(ipv4, port, addr_v4, addrLen_v4) == 0) {
					LogWarning("Unable to resolve the address of %s", ipv4.c_str());
					addrLen_v4 = 0U;
				}
			}

			sockaddr_storage addr_v6 = sockaddr_storage();
			unsigned int     addrLen_v6 = 0U;

			isNull = it["ipv6"].is_null();
			if (!isNull) {
				std::string ipv6 = it["ipv6"];
				if (!CUDPSocket::lookup(ipv6, port, addr_v6, addrLen_v6) == 0) {
					LogWarning("Unable to resolve the address of %s", ipv6.c_str());
					addrLen_v6 = 0U;
				}
			}

			if ((addrLen_v4 > 0U) || (addrLen_v6 > 0U)) {
				CM17Reflector* refl = new CM17Reflector;
				refl->m_name         = "M17-" + name;
				refl->IPv4.m_addr    = addr_v4;
				refl->IPv4.m_addrLen = addrLen_v4;
				refl->IPv6.m_addr    = addr_v6;
				refl->IPv6.m_addrLen = addrLen_v6;
				m_reflectors.push_back(refl);
			}
		}
	}
	catch (...) {
		LogError("Unable to load/parse JSON file %s", fileName.c_str());
		return false;
	}

	return true;
}

bool CReflectors::parseHosts(const std::string& fileName)
{
	FILE* fp = ::fopen(fileName.c_str(), "rt");
	if (fp == nullptr) {
		LogWarning("Unable to open the Hosts file %s", fileName.c_str());
		return false;
	}

	char buffer[100U];
	while (::fgets(buffer, 100U, fp) != nullptr) {
		if (buffer[0U] == '#')
			continue;

		char* p1 = ::strtok(buffer,  " \t\r\n");
		char* p2 = ::strtok(nullptr, " \t\r\n");
		char* p3 = ::strtok(nullptr, " \t\r\n");

		if (p1 == nullptr || p2 == nullptr || p3 == nullptr)
			continue;

		std::string name = std::string(p1);
		name.resize(M17_CALLSIGN_LENGTH - 2U, ' ');

		std::string host = std::string(p2);

		unsigned short port = (unsigned short)::atoi(p3);

		sockaddr_storage addr;
		unsigned int addrLen;
		if (CUDPSocket::lookup(host, port, addr, addrLen) == 0) {
			CM17Reflector* refl = nullptr;
			switch (addr.ss_family) {
			case AF_INET:
				refl = new CM17Reflector;
				refl->m_name         = name;
				refl->IPv4.m_addr    = addr;
				refl->IPv4.m_addrLen = addrLen;
				m_reflectors.push_back(refl);
				break;
			case AF_INET6:
				refl = new CM17Reflector;
				refl->m_name         = name;
				refl->IPv6.m_addr    = addr;
				refl->IPv6.m_addrLen = addrLen;
				m_reflectors.push_back(refl);
				break;
			default:
				LogWarning("Unknown address family for %s", host.c_str());
				break;
			}
		} else {
			LogWarning("Unable to resolve the address of %s", host.c_str());
		}
	}

	::fclose(fp);

	return true;
}
