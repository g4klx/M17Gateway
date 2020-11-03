/*
 *   Copyright (C) 2016,2017,2018,2020 by Jonathan Naylor G4KLX
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

#if !defined(M17DEFINES_H)
#define  M17DEFINES_H

const unsigned int M17_CALLSIGN_LENGTH = 9U;

const unsigned int M17_NETWORK_FRAME_LENGTH = 54U;

const unsigned int M17_LICH_LENGTH_BITS  = 240U;
const unsigned int M17_LICH_LENGTH_BYTES = M17_LICH_LENGTH_BITS / 8U;

const unsigned int M17_LICH_FRAGMENT_LENGTH_BITS  = M17_LICH_LENGTH_BITS / 5U;
const unsigned int M17_LICH_FRAGMENT_LENGTH_BYTES = M17_LICH_FRAGMENT_LENGTH_BITS / 8U;

const unsigned int M17_LICH_FRAGMENT_FEC_LENGTH_BITS  = M17_LICH_FRAGMENT_LENGTH_BITS * 2U;
const unsigned int M17_LICH_FRAGMENT_FEC_LENGTH_BYTES = M17_LICH_FRAGMENT_FEC_LENGTH_BITS / 8U;

const unsigned int M17_PAYLOAD_LENGTH_BITS  = 128U;
const unsigned int M17_PAYLOAD_LENGTH_BYTES = M17_PAYLOAD_LENGTH_BITS / 8U;

const unsigned int  M17_NONCE_LENGTH_BITS  = 112U;
const unsigned int  M17_NONCE_LENGTH_BYTES = M17_NONCE_LENGTH_BITS / 8U;

const unsigned int M17_FN_LENGTH_BITS  = 16U;
const unsigned int M17_FN_LENGTH_BYTES = M17_FN_LENGTH_BITS / 8U;

const unsigned int M17_CRC_LENGTH_BITS  = 16U;
const unsigned int M17_CRC_LENGTH_BYTES = M17_CRC_LENGTH_BITS / 8U;

#endif
