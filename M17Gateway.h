/*
*   Copyright (C) 2016,2018,2020,2021,2025 by Jonathan Naylor G4KLX
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

#if !defined(M17Gateway_H)
#define	M17Gateway_H

#include "M17Network.h"
#include "APRSWriter.h"
#include "GPSHandler.h"
#include "Reflectors.h"
#include "Voice.h"
#include "Conf.h"

#include <cstdio>
#include <string>
#include <vector>

#if !defined(_WIN32) && !defined(_WIN64)
#include <netdb.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#else
#include <winsock.h>
#endif

enum class M17_STATUS {
	NOTLINKED,
	LINKED,
	LINKING,
	UNLINKING,
	ECHO
};

class CM17Gateway
{
public:
	CM17Gateway(const std::string& file);
	~CM17Gateway();

	int run();

private:
	CConf            m_conf;
	M17_STATUS       m_status;
	M17_STATUS       m_oldStatus;
	CM17Network*     m_network;
	CM17Reflector    m_reflector;
	char             m_module;
	CAPRSWriter*     m_writer;
	CVoice*          m_voice;
	CGPSHandler*     m_gps;

	void createGPS();
	bool isVoiceBusy() const;
};

#endif
