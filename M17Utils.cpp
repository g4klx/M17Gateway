/*
 *   Copyright (C) 2020,2021,2024,2025 by Jonathan Naylor G4KLX
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

#include "M17Utils.h"
#include "M17Defines.h"

#include <cassert>
#include <cstdint>

const std::string M17_CHARS = " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-/.";

void CM17Utils::encodeCallsign(const std::string& callsign, unsigned char* encoded)
{
	assert(encoded != nullptr);

	if (callsign == "ALL      ") {
		encoded[0U] = 0xFFU;
		encoded[1U] = 0xFFU;
		encoded[2U] = 0xFFU;
		encoded[3U] = 0xFFU;
		encoded[4U] = 0xFFU;
		encoded[5U] = 0xFFU;
		return;
	}

	unsigned int len = (unsigned int)callsign.size();
	if (len > 9U)
		len = 9U;

	uint64_t enc = 0ULL;
	for (int i = len - 1; i >= 0; i--) {
		if ((i == 0) && (callsign[i] == '#')) {
			enc += 262144000000000ULL;
		} else {
			size_t pos = M17_CHARS.find(callsign[i]);
			if (pos == std::string::npos)
				pos = 0ULL;

			enc *= 40ULL;
			enc += pos;
		}
	}

	encoded[0U] = (enc >> 40) & 0xFFU;
	encoded[1U] = (enc >> 32) & 0xFFU;
	encoded[2U] = (enc >> 24) & 0xFFU;
	encoded[3U] = (enc >> 16) & 0xFFU;
	encoded[4U] = (enc >> 8)  & 0xFFU;
	encoded[5U] = (enc >> 0)  & 0xFFU;
}

std::string CM17Utils::decodeCallsign(const unsigned char* encoded)
{
	assert(encoded != nullptr);

	std::string callsign;

	uint64_t enc = (uint64_t(encoded[0U]) << 40) +
		           (uint64_t(encoded[1U]) << 32) +
		           (uint64_t(encoded[2U]) << 24) +
		           (uint64_t(encoded[3U]) << 16) +
		           (uint64_t(encoded[4U]) << 8)  +
		           (uint64_t(encoded[5U]) << 0);

	if (enc == 281474976710655ULL)
		return "ALL      ";

	if (enc >= 268697600000000ULL)
		return "Invalid";

	if (enc >= 262144000000000ULL) {
		callsign = "#";
		enc -= 262144000000000ULL;
	}

	while (enc > 0ULL) {
		callsign += " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-/."[enc % 40ULL];
		enc /= 40ULL;
	}

	return callsign;
}
