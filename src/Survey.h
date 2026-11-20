// SPDX-License-Identifier: GPL-3.0-or-later
// TombLoot — Copyright (C) 2026 narziel. See COPYING.

#pragma once

#include "Config.h"

// Read-only survey of the tomb chests and their leveled lists, once all plugins are loaded and
// after the swap: shows what a tomb chest can give now. Writes the log, changes nothing.
namespace Survey
{
	void Run(const Config& a_config);
}
