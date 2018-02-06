/*
 * Copyright (C) 2003-2015 The Music Player Daemon Project
 * http://www.musicpd.org
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef MPD_ACK_H
#define MPD_ACK_H

#include <stdexcept>

#include <stdio.h>

class Domain;

enum ack {
	ACK_ERROR_NOT_LIST = 1,
	ACK_ERROR_ARG = 2,
	ACK_ERROR_PASSWORD = 3,
	ACK_ERROR_PERMISSION = 4,
	ACK_ERROR_UNKNOWN = 5,

	ACK_ERROR_DMS_IO = 10,
	
	ACK_ERROR_NO_EXIST = 50,
	ACK_ERROR_PLAYLIST_MAX = 51,
	ACK_ERROR_SYSTEM = 52,
	ACK_ERROR_PLAYLIST_LOAD = 53,
	ACK_ERROR_UPDATE_ALREADY = 54,
	ACK_ERROR_PLAYER_SYNC = 55,
	ACK_ERROR_EXIST = 56,

	ACK_ERROR_CONNECTION_REFUSED = 111,
};

extern const Domain ack_domain;

class ProtocolError : public std::runtime_error {
	enum ack code;

public:
	ProtocolError(enum ack _code, const char *msg)
		:std::runtime_error(msg), code(_code) {}

	enum ack GetCode() const {
		return code;
	}
};

template<typename... Args>
static inline ProtocolError
FormatProtocolError(enum ack code, const char *fmt, Args&&... args) noexcept
{
	char buffer[256];
	snprintf(buffer, sizeof(buffer), fmt, std::forward<Args>(args)...);
	return ProtocolError(code, buffer);
}

#endif
