/*
*   Copyright (C) 2016,2020 by Jonathan Naylor G4KLX
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

#include <cstdio>
#include <cassert>
#include <cstring>

CEcho::CEcho(unsigned int timeout) :
m_data(NULL),
m_length(timeout * 25U * 46U),
m_used(0U),
m_ptr(0U)
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
	assert(data != NULL);

	if ((m_length - m_used) < 46U)
		return false;

	::memcpy(m_data + m_used, data, 46U);
	m_used += 46U;

	return true;
}

void CEcho::end()
{
	m_ptr = 0U;
}

void CEcho::clear()
{
	m_used = 0U;
	m_ptr = 0U;
}

bool CEcho::read(unsigned char* data)
{
	assert(data != NULL);

	if (m_used == 0U)
		return false;

	::memcpy(data, m_data + m_ptr, 46U);
	m_ptr += 46U;

	if (m_ptr >= m_used)
		m_used = 0U;

	return true;
}
