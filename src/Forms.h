// SPDX-License-Identifier: GPL-3.0-or-later
// TombLoot — Copyright (C) 2026 narziel. See COPYING.

#pragma once

// Form references written "Plugin.esp|0xLOCALID" (local ID, without the load order index), as in
// Every Crumb Counts.
namespace Forms
{
	// Returns nullptr if the text is invalid or the form is missing (plugin not loaded).
	RE::TESForm* Resolve(std::string_view a_ref);

	// "Plugin.esp|0xLOCALID" of the file that created the form.
	std::string Label(const RE::TESForm* a_form);

	// Label plus the in-game name when the form has one: "Iron Sword Skyrim.esm|0x012EB7".
	std::string Named(const RE::TESForm* a_form);

	// File that last modified the form (the winning override), or "-".
	std::string Winner(const RE::TESForm* a_form);
}
