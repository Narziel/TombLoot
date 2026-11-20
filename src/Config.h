// SPDX-License-Identifier: GPL-3.0-or-later
// TombLoot — Copyright (C) 2026 narziel. See COPYING.

#pragma once

// Data/SKSE/Plugins/TombLoot/TombLoot.json, read at data load and at each game load.
struct Config
{
	struct Replacement
	{
		std::string              to;
		std::vector<std::string> from;
	};

	struct Family
	{
		std::vector<std::string> lists;    // lists whose generic entries are swapped
		std::vector<Replacement> replace;  // generic list -> era list
	};

	// A leveled list made at launch (no vanilla list gives Ancient Nord armor), named "@name" in a
	// replacement. A piece whose plugin is missing is skipped; an empty list falls back to `fallback`.
	struct CreatedList
	{
		struct Item
		{
			std::string   form;
			std::uint16_t level{ 1 };
		};
		std::vector<Item> items;
		std::string       fallback;
		std::string       extends;  // name of another created list whose pieces come first
	};

	bool                               survey{ true };
	std::vector<std::string>           ancientPlugins;
	std::map<std::string, CreatedList> created;
	Family                             nordic;     // crypts of Skyrim
	Family                             solstheim;  // crypts of Solstheim (Dragonborn lists)

	// Reads the file; on a missing or invalid file, keeps the defaults (nothing replaced) and says so.
	static Config Load();
};
