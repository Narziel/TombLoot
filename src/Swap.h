// SPDX-License-Identifier: GPL-3.0-or-later
// TombLoot — Copyright (C) 2026 narziel. See COPYING.

#pragma once

#include "Config.h"

// Step 2 of CONCEPTION.md: in the tomb lists of the config, generic entries (LItemWeaponSword…)
// point to their era list instead. Only the entry's object changes; level, count and chance none
// stay as the load order set them. The original objects are kept and put back before each new
// application, so a changed config applies cleanly and removing the mod gives the old loot back.
namespace Swap
{
	void Apply(const Config& a_config);
}
