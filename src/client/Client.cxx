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

#include "config.h"
#include "ClientInternal.hxx"
#include "util/Domain.hxx"
#include "Partition.hxx"
#include "Instance.hxx"
#include "IOThread.hxx"
#include "dms/Context.hxx"

extern Instance *instance;

const Domain client_domain("client");

IgnoreClient::IgnoreClient()
	: client(io_thread_get(), *instance->partition, -1, -1, -1)
{
}

#ifdef ENABLE_DATABASE

const Database *
Client::GetDatabase(Error &error) const
{
	return partition.instance.GetDatabase(error);
}

const Storage *
Client::GetStorage() const
{
	return partition.instance.storage;
}

#endif

Dms::Context &
Client::GetContext()
{
	assert(partition.instance.dc != nullptr);

	return *partition.instance.dc;
}
