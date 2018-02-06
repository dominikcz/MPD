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
#include "DatabaseCommands.hxx"
#include "db/DatabaseGlue.hxx"
#include "db/DatabaseQueue.hxx"
#include "db/DatabasePlaylist.hxx"
#include "db/DatabasePrint.hxx"
#include "db/Count.hxx"
#include "db/Selection.hxx"
#include "CommandError.hxx"
#include "client/Client.hxx"
#include "tag/Tag.hxx"
#include "util/ConstBuffer.hxx"
#include "util/Error.hxx"
#include "SongFilter.hxx"
#include "protocol/Result.hxx"
#include "protocol/ArgParser.hxx"
#include "BulkEdit.hxx"
#include "Partition.hxx"
#include "PlaylistError.hxx"
#include "PlaylistFile.hxx"
#include "fs/AllocatedPath.hxx"
#include "fs/FileSystem.hxx"
#include "db/plugins/upnp/UpnpDatabasePlugin.hxx"
#include "db/Interface.hxx"

#include <string.h>

CommandResult
handle_listfiles_db(Client &client, const char *uri)
{
	DatabaseSelection selection(uri, false);

	Error error;
	if (!db_selection_print(client, selection, false, true, error))
		return print_error(client, error);

	return CommandResult::OK;
}

CommandResult
handle_lsinfo2(Client &client, ConstBuffer<const char *> args)
{
	/* default is root directory */
	const char *const uri = args.IsEmpty() ? "" : args.front();

	unsigned start = 0, end = std::numeric_limits<unsigned>::max();
	if (args.size == 2 && !check_range(client, &start, &end, args.back()))
		return CommandResult::ERROR;

	DatabaseSelection selection(uri, false);
	selection.window_start = start;
	selection.window_end = end;

	Error error;
	if (!db_selection_print(client, selection, true, false, error))
		return print_error(client, error);

	return CommandResult::OK;
}

static CommandResult
handle_match(Client &client, ConstBuffer<const char *> args, bool fold_case)
{
	unsigned window_start = 0, window_end = std::numeric_limits<int>::max();
	const char *host = nullptr;
	if (args.size >= 2 && strcmp(args[args.size - 2], "window") == 0) {
		if (!check_range(client, &window_start, &window_end,
				 args.back()))
			return CommandResult::ERROR;

		args.pop_back();
		args.pop_back();
	}

	SongFilter filter;
	if (!filter.Parse(args, fold_case)) {
		command_error(client, ACK_ERROR_ARG, "incorrect arguments");
		return CommandResult::ERROR;
	}

	DatabaseSelection selection(host == nullptr?"":host, true, &filter);

	Error error;
	return db_selection_print(client, selection, true, false,
				  window_start, window_end, error)
		? CommandResult::OK
		: print_error(client, error);
}

CommandResult
handle_find(Client &client, ConstBuffer<const char *> args)
{
	return handle_match(client, args, false);
}

CommandResult
handle_search(Client &client, ConstBuffer<const char *> args)
{
	return handle_match(client, args, true);
}

static CommandResult
handle_match_add(Client &client, ConstBuffer<const char *> args, bool fold_case)
{
	SongFilter filter;
	if (!filter.Parse(args, fold_case)) {
		command_error(client, ACK_ERROR_ARG, "incorrect arguments");
		return CommandResult::ERROR;
	}

	const ScopeBulkEdit bulk_edit(client.partition);

	DmsConfig	&df = client.partition.df;
	DmsSource &source = df.source;
	std::string uri("");
	if (source.value == SOURCE_UPNP) {
		uri = source.getName();
	}
	Error error;
	const Database *db = client.GetDatabase(error);
	if (db == nullptr)
		return print_error(client, error);
	const DatabaseSelection selection(uri.c_str(), (db->IsPlugin(upnp_db_plugin) ? false : true), &filter);
	return AddFromDatabase(client, selection, error)
		? CommandResult::OK
		: print_error(client, error);
}

CommandResult
handle_findadd(Client &client, ConstBuffer<const char *> args)
{
	return handle_match_add(client, args, false);
}

CommandResult
handle_findaddpl(Client &client, ConstBuffer<const char *> args)
{
	DmsConfig	&df = client.partition.df;
	std::string playlist = df.source.getPlaylistName(args.shift());

	SongFilter filter;
	if (!filter.Parse(args, false)) {
		command_error(client, ACK_ERROR_ARG, "incorrect arguments");
		return CommandResult::ERROR;
	}

	const DatabaseSelection selection("", true, &filter);

	Error error;
	const Database *db = client.GetDatabase(error);
	if (db == nullptr)
		return print_error(client, error);

	return search_add_to_playlist(*db, *client.GetStorage(),
				      playlist.c_str(), selection, error)
		? CommandResult::OK
		: print_error(client, error);
}

CommandResult
handle_findsavepl(Client &client, ConstBuffer<const char *> args)
{
	DmsConfig	&df = client.partition.df;
	std::string playlist = df.source.getPlaylistName(args.shift());
	Error error;

	SongFilter filter;
	if (!filter.Parse(args, false)) {
		command_error(client, ACK_ERROR_ARG, "incorrect arguments");
		return CommandResult::ERROR;
	}

	const DatabaseSelection selection("", true, &filter);

	const auto path_fs = spl_map_to_fs(playlist.c_str(), error);
	if (path_fs.IsNull())
		return print_error(client, error);

	if (FileExists(path_fs)) {
		error.Set(playlist_domain, int(PlaylistResult::LIST_EXISTS),
			  "Playlist already exists");
		return print_error(client, error);
	}

	const Database *db = client.GetDatabase(error);
	if (db == nullptr)
		return print_error(client, error);

	return search_add_to_playlist(*db, *client.GetStorage(),
				      playlist.c_str(), selection, error)
		? CommandResult::OK
		: print_error(client, error);
}

CommandResult
handle_searchadd(Client &client, ConstBuffer<const char *> args)
{
	return handle_match_add(client, args, true);
}

CommandResult
handle_searchaddpl(Client &client, ConstBuffer<const char *> args)
{
	DmsConfig	&df = client.partition.df;
	std::string playlist = df.source.getPlaylistName(args.shift());

	SongFilter filter;
	if (!filter.Parse(args, true)) {
		command_error(client, ACK_ERROR_ARG, "incorrect arguments");
		return CommandResult::ERROR;
	}

	const DatabaseSelection selection("", true, &filter);

	Error error;
	const Database *db = client.GetDatabase(error);
	if (db == nullptr)
		return print_error(client, error);

	return search_add_to_playlist(*db, *client.GetStorage(),
				      playlist.c_str(), selection, error)
		? CommandResult::OK
		: print_error(client, error);
}

CommandResult
handle_searchsavepl(Client &client, ConstBuffer<const char *> args)
{
	DmsConfig	&df = client.partition.df;
	std::string playlist = df.source.getPlaylistName(args.shift());
	Error error;

	const auto path_fs = spl_map_to_fs(playlist.c_str(), error);
	if (path_fs.IsNull())
		return print_error(client, error);

	if (FileExists(path_fs)) {
		error.Set(playlist_domain, int(PlaylistResult::LIST_EXISTS),
			  "Playlist already exists");
		return print_error(client, error);
	}

	SongFilter filter;
	if (!filter.Parse(args, true)) {
		command_error(client, ACK_ERROR_ARG, "incorrect arguments");
		return CommandResult::ERROR;
	}

	const DatabaseSelection selection("", true, &filter);

	const Database *db = client.GetDatabase(error);
	if (db == nullptr)
		return print_error(client, error);

	return search_add_to_playlist(*db, *client.GetStorage(),
				      playlist.c_str(), selection, error)
		? CommandResult::OK
		: print_error(client, error);
}

CommandResult
handle_count(Client &client, ConstBuffer<const char *> args)
{
	TagType group = TAG_NUM_OF_ITEM_TYPES;
	if (args.size >= 2 && strcmp(args[args.size - 2], "group") == 0) {
		const char *s = args[args.size - 1];
		group = tag_name_parse_i(s);
		if (group == TAG_NUM_OF_ITEM_TYPES) {
			command_error(client, ACK_ERROR_ARG,
				      "Unknown tag type: %s", s);
			return CommandResult::ERROR;
		}

		args.pop_back();
		args.pop_back();
	}

	SongFilter filter;
	if (!args.IsEmpty() && !filter.Parse(args, false)) {
		command_error(client, ACK_ERROR_ARG, "incorrect arguments");
		return CommandResult::ERROR;
	}

	Error error;
	return PrintSongCount(client, "", &filter, group, error)
		? CommandResult::OK
		: print_error(client, error);
}

CommandResult
handle_listall(Client &client, ConstBuffer<const char *> args)
{
	/* default is root directory */
	const char *const uri = args.IsEmpty() ? "" : args.front();

	Error error;
	DatabaseSelection selection(uri, true);
	return db_selection_print(client, selection,
				  false, false, error)
		? CommandResult::OK
		: print_error(client, error);
}

CommandResult
handle_list(Client &client, ConstBuffer<const char *> args)
{
	const char *tag_name = args.shift();
	unsigned tagType = locate_parse_type(tag_name);

	if (tagType >= TAG_NUM_OF_ITEM_TYPES &&
	    tagType != LOCATE_TAG_FILE_TYPE) {
		command_error(client, ACK_ERROR_ARG,
			      "Unknown tag type: %s", tag_name);
		return CommandResult::ERROR;
	}

	SongFilter *filter = nullptr;
	uint32_t group_mask = 0;

	if (args.size == 1) {
		/* for compatibility with < 0.12.0 */
		if (tagType != TAG_ALBUM) {
			command_error(client, ACK_ERROR_ARG,
				      "should be \"%s\" for 3 arguments",
				      tag_item_names[TAG_ALBUM]);
			return CommandResult::ERROR;
		}

		filter = new SongFilter((unsigned)TAG_ARTIST, args.shift());
	}

	while (args.size >= 2 &&
	       strcmp(args[args.size - 2], "group") == 0) {
		const char *s = args[args.size - 1];
		TagType gt = tag_name_parse_i(s);
		if (gt == TAG_NUM_OF_ITEM_TYPES) {
			command_error(client, ACK_ERROR_ARG,
				      "Unknown tag type: %s", s);
			return CommandResult::ERROR;
		}

		group_mask |= 1u << unsigned(gt);

		args.pop_back();
		args.pop_back();
	}

	if (!args.IsEmpty()) {
		filter = new SongFilter();
		if (!filter->Parse(args, false)) {
			delete filter;
			command_error(client, ACK_ERROR_ARG,
				      "not able to parse args");
			return CommandResult::ERROR;
		}
	}

	if (tagType < TAG_NUM_OF_ITEM_TYPES &&
	    group_mask & (1u << tagType)) {
		delete filter;
		command_error(client, ACK_ERROR_ARG, "Conflicting group");
		return CommandResult::ERROR;
	}

	Error error;
	CommandResult ret =
		PrintUniqueTags(client, tagType, group_mask, filter, error)
		? CommandResult::OK
		: print_error(client, error);

	delete filter;

	return ret;
}

CommandResult
handle_listallinfo(Client &client, ConstBuffer<const char *> args)
{
	/* default is root directory */
	const char *const uri = args.IsEmpty() ? "" : args.front();

	Error error;
	DatabaseSelection selection(uri, true);
	return db_selection_print(client, selection,
				  true, false, error)
		? CommandResult::OK
		: print_error(client, error);
}
