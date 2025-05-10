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

#if !defined(Reflectors_H)
#define	Reflectors_H

#include "UDPSocket.h"
#include "Timer.h"

#include <vector>
#include <string>

class CM17Reflector {
public:
	CM17Reflector() :
	m_name()
	{
		IPv4.m_addrLen = 0U;
		IPv6.m_addrLen = 0U;
	}

	CM17Reflector(const CM17Reflector& in)
	{
		m_name = in.m_name;

		IPv4.m_addrLen = in.IPv4.m_addrLen;
		IPv6.m_addrLen = in.IPv6.m_addrLen;

		::memcpy(&IPv4.m_addr, &in.IPv4.m_addr, sizeof(sockaddr_storage));
		::memcpy(&IPv6.m_addr, &in.IPv6.m_addr, sizeof(sockaddr_storage));
	}

	bool isEmpty() const
	{
		return m_name.empty();
	}

	bool isUsed() const
	{
		return !m_name.empty();
	}

	void reset()
	{
		m_name.clear();
	}

	bool hasIPv4() const
	{
		return IPv4.m_addrLen > 0U;
	}

	bool hasIPv6() const
	{
		return IPv6.m_addrLen > 0U;
	}

	std::string          m_name;
	struct {
		sockaddr_storage m_addr;
		unsigned int     m_addrLen;
	} IPv4;
	struct {
		sockaddr_storage m_addr;
		unsigned int     m_addrLen;
	} IPv6;

	CM17Reflector& operator=(const CM17Reflector& in)
	{
		if (&in != this) {
			m_name = in.m_name;

			IPv4.m_addrLen = in.IPv4.m_addrLen;
			IPv6.m_addrLen = in.IPv6.m_addrLen;

			::memcpy(&IPv4.m_addr, &in.IPv4.m_addr, sizeof(sockaddr_storage));
			::memcpy(&IPv6.m_addr, &in.IPv6.m_addr, sizeof(sockaddr_storage));
		}

		return *this;
	}
};

class CReflectors {
public:
	CReflectors(const std::string& hostsFile1, const std::string& hostsFile2, unsigned int reloadTime);
	~CReflectors();

	bool load();

	CM17Reflector* find(const std::string& name);

	void clock(unsigned int ms);

private:
	std::string  m_hostsFile1;
	std::string  m_hostsFile2;
	std::vector<CM17Reflector*> m_reflectors;
	CTimer       m_timer;

	void remove();
	bool parseJSON(const std::string& fileName);
	bool parseHosts(const std::string& fileName);
};

#endif
