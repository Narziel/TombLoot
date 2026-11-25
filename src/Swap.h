// SPDX-License-Identifier: GPL-3.0-or-later
// TombLoot — Copyright (C) 2026 narziel. See COPYING.

#pragma once

#include "Config.h"

// In the tomb lists of the config, generic entries (LItemWeaponSword…) point to their era list
// instead. Only the entry's object changes; level, count and chance none stay as the load order set
// them. The original objects are kept and put back before each new application, so a changed config
// applies cleanly and removing the mod gives the old loot back.
//
// Besides the configured lists and chests, each family finds by itself:
// - the chests that hold one of its lists directly (other mods' crypts, unique boss chests);
// - the mod sub-lists used only by tomb lists and chests, that hold generic lists themselves.
namespace Swap
{
	struct Report
	{
		std::string                         family;
		std::uint32_t                       lists{ 0 };
		std::uint32_t                       foundLists{ 0 };
		std::uint32_t                       entries{ 0 };
		std::vector<RE::TESObjectCONT*>     chests;
		std::uint32_t                       foundChests{ 0 };
		std::uint32_t                       eraWeapons{ 0 };  // era weapons brought back from the generic lists
		std::uint32_t                       chestEntries{ 0 };
		std::unordered_set<RE::TESLevItem*> sharedSublists;  // hold generic lists, shared with the world: kept
	};

	void Apply(const Config& a_config);

	// Weapons and armor placed by hand in the base game get an era piece when they load (config "placed").
	void          InstallPlaced();
	std::uint32_t PlacedSwapped();

	// One report per family, from the last application.
	const std::vector<Report>& Reports();
}
