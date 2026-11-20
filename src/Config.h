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

	bool                     survey{ true };
	std::vector<std::string> ancientPlugins;
	Family                   nordic;

	// Reads the file; on a missing or invalid file, keeps the defaults (nothing replaced) and says so.
	static Config Load();
};
