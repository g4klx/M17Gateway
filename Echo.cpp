/*
*   Copyright (C) 2016,2020,2021,2025 by Jonathan Naylor G4KLX
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

#include "Echo.h"
#include "M17Defines.h"

#include <cstdio>
#include <cassert>
#include <cstring>

CEcho::CEcho(unsigned int timeout) :
m_data(nullptr),
m_length(timeout * 25U * M17_NETWORK_FRAME_LENGTH),
m_used(0U),
m_sent(0U),
m_status(ECHO_STATUS::NONE),
m_stopWatch(),
m_timer(1000U, 2U)
{
	assert(timeout > 0U);

	m_data = new unsigned char[m_length];
}

CEcho::~CEcho()
{
	delete[] m_data;
}

bool CEcho::write(const unsigned char* data)
{
	assert(data != nullptr);

	if ((m_length - m_used) < M17_NETWORK_FRAME_LENGTH)
		return false;

	::memcpy(m_data + m_used, data, M17_NETWORK_FRAME_LENGTH);
	m_used += M17_NETWORK_FRAME_LENGTH;

	m_status = ECHO_STATUS::RECORDING;

	return true;
}

void CEcho::end()
{
	m_sent = 0U;

	m_status = ECHO_STATUS::WAITING;

	m_timer.start();
}

void CEcho::clear()
{
	m_sent = 0U;
	m_used = 0U;

	m_status = ECHO_STATUS::NONE;

	m_timer.stop();
}

ECHO_STATE CEcho::read(unsigned char* data)
{
	assert(data != nullptr);

	if (m_status != ECHO_STATUS::PLAYING)
		return ECHO_STATE::NONE;

	if (m_used == 0U) {
		m_status = ECHO_STATUS::NONE;
		return ECHO_STATE::END;
	}

	unsigned int wanted = m_stopWatch.elapsed() / 40U;
	if (m_sent >= wanted)
		return ECHO_STATE::NONE;

	unsigned int ptr = m_sent * M17_NETWORK_FRAME_LENGTH;

	if (ptr >= m_used) {
		m_used   = 0U;
		m_status = ECHO_STATUS::NONE;
		return ECHO_STATE::END;
	}

	::memcpy(data, m_data + ptr, M17_NETWORK_FRAME_LENGTH);

	m_sent++;

	return ECHO_STATE::DATA;
}

void CEcho::clock(unsigned int ms)
{
	m_timer.clock(ms);
	if (m_timer.isRunning() && m_timer.hasExpired()) {
		m_status = ECHO_STATUS::PLAYING;
		m_sent   = 0U;
		m_stopWatch.start();
		m_timer.stop();
	}
}
