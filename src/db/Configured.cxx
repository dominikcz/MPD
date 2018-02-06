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
#include "Configured.hxx"
#include "DatabaseGlue.hxx"
#include "config/ConfigGlobal.hxx"
#include "config/Param.hxx"
#include "config/Block.hxx"
#include "config/ConfigError.hxx"
#include "fs/AllocatedPath.hxx"
#include "fs/StandardDirectory.hxx"
#include "util/Error.hxx"
#include "Log.hxx"

Database *
CreateConfiguredDatabase(EventLoop &loop, DatabaseListener &listener,
			 Error &error)
{
	const auto *param = config_get_block(ConfigBlockOption::DATABASE);
	const auto *path = config_get_param(ConfigOption::DB_FILE);

	if (param != nullptr && path != nullptr) {
		error.Format(config_domain,
			     "Found both 'database' (line %d) and 'db_file' (line %d) setting",
			     param->line, path->line);
		return nullptr;
	}

	ConfigBlock *allocated = nullptr;

	if (param == nullptr && path != nullptr) {
		allocated = new ConfigBlock(path->line);
		allocated->AddBlockParam("path", path->value.c_str(),
					 path->line);
		param = allocated;
	}

	if (param == nullptr) {
		/* if there is no override, use the cache directory */

		const AllocatedPath cache_dir = GetUserCacheDir();
		if (cache_dir.IsNull())
			return nullptr;

		const auto db_file = AllocatedPath::Build(cache_dir,
							  PATH_LITERAL("mpd.db"));
		const auto db_file_utf8 = db_file.ToUTF8();
		if (db_file_utf8.empty())
			return nullptr;

		allocated = new ConfigBlock();
		allocated->AddBlockParam("path", db_file_utf8.c_str(), -1);
		param = allocated;
	}

	Database *db = DatabaseGlobalInit(loop, listener, *param,
					  error);
	delete allocated;
	return db;
}

Database * CreateUpnpDatabase(EventLoop &loop, DatabaseListener &listener,
			 Error &error)
{
	Database *db = UpnpDatabaseInit(loop, listener,error);
        return db;
}

