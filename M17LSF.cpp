/*
 *   Copyright (C) 2020,2021,2025 by Jonathan Naylor G4KLX
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

#include "M17LSF.h"
#include "M17Utils.h"
#include "M17Defines.h"

#include <cassert>
#include <cstring>

CM17LSF::CM17LSF() :
m_lsf(nullptr)
{
	m_lsf = new unsigned char[M17_LSF_LENGTH_BYTES];
}

CM17LSF::~CM17LSF()
{
	delete[] m_lsf;
}

void CM17LSF::getNetwork(unsigned char* data) const
{
	assert(data != nullptr);

	::memcpy(data, m_lsf, M17_LSF_LENGTH_BYTES);
}

void CM17LSF::setNetwork(const unsigned char* data)
{
	assert(data != nullptr);

	::memcpy(m_lsf, data, M17_LSF_LENGTH_BYTES);
}

std::string CM17LSF::getSource() const
{
	return CM17Utils::decodeCallsign(m_lsf + 6U);
}

void CM17LSF::setSource(const std::string& callsign)
{
	CM17Utils::encodeCallsign(callsign, m_lsf + 6U);
}

std::string CM17LSF::getDest() const
{
	return CM17Utils::decodeCallsign(m_lsf + 0U);
}

void CM17LSF::setDest(const std::string& callsign)
{
	CM17Utils::encodeCallsign(callsign, m_lsf + 0U);
}

unsigned char CM17LSF::getPacketStream() const
{
	return m_lsf[13U] & 0x01U;
}

void CM17LSF::setPacketStream(unsigned char ps)
{
	m_lsf[13U] &= 0xF7U;
	m_lsf[13U] |= ps & 0x01U;
}

unsigned char CM17LSF::getDataType() const
{
	return (m_lsf[13U] >> 1) & 0x03U;
}

void CM17LSF::setDataType(unsigned char type)
{
	m_lsf[13U] &= 0xF9U;
	m_lsf[13U] |= (type << 1) & 0x06U;
}

unsigned char CM17LSF::getEncryptionType() const
{
	return (m_lsf[13U] >> 3) & 0x03U;
}

void CM17LSF::setEncryptionType(unsigned char type)
{
	m_lsf[13U] &= 0xE7U;
	m_lsf[13U] |= (type << 3) & 0x18U;
}

unsigned char CM17LSF::getEncryptionSubType() const
{
	return (m_lsf[13U] >> 5) & 0x03U;
}

void CM17LSF::setEncryptionSubType(unsigned char type)
{
	m_lsf[13U] &= 0x9FU;
	m_lsf[13U] |= (type << 5) & 0x60U;
}

unsigned char CM17LSF::getCAN() const
{
	return ((m_lsf[12U] << 1) & 0x0EU) | ((m_lsf[13U] >> 7) & 0x01U);
}

void CM17LSF::setCAN(unsigned char can)
{
	m_lsf[13U] &= 0x7FU;
	m_lsf[13U] |= (can << 7) & 0x80U;

	m_lsf[12U] &= 0xF8U;
	m_lsf[12U] |= (can >> 1) & 0x07U;
}

void CM17LSF::getMeta(unsigned char* data) const
{
	assert(data != nullptr);

	::memcpy(data, m_lsf + 14U, M17_META_LENGTH_BYTES);
}

void CM17LSF::setMeta(const unsigned char* data)
{
	assert(data != nullptr);

	::memcpy(m_lsf + 14U, data, M17_META_LENGTH_BYTES);
}

