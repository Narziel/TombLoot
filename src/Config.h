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
		std::vector<std::string> lists;       // lists whose generic entries are swapped
		std::vector<std::string> containers;  // chests whose own entries are swapped (a shared list stays intact)
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
		std::int8_t       chanceNone{ 0 };  // percent; keeps the chance of a list it replaces (10 % special loot)
	};

	bool                               survey{ true };
	std::vector<std::string>           ancientPlugins;
	std::map<std::string, CreatedList> created;
	// Families applied in this order: nordic (crypts of Skyrim), solstheim (Dragonborn lists), dwemer…
	std::vector<std::pair<std::string, Family>> families;

	// Reads the file; on a missing or invalid file, keeps the defaults (nothing replaced) and says so.
	static Config Load();
};
