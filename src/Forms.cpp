// SPDX-License-Identifier: GPL-3.0-or-later
// TombLoot — Copyright (C) 2026 narziel. See COPYING.

#include "Forms.h"

namespace Forms
{
	RE::TESForm* Resolve(std::string_view a_ref)
	{
		const auto sep = a_ref.find('|');
		if (sep == std::string_view::npos) {
			return nullptr;
		}
		const std::string plugin{ a_ref.substr(0, sep) };
		RE::FormID        localID = 0;
		try {
			localID = static_cast<RE::FormID>(std::stoul(std::string{ a_ref.substr(sep + 1) }, nullptr, 16));
		} catch (const std::exception&) {
			return nullptr;
		}
		auto* dh = RE::TESDataHandler::GetSingleton();
		return dh ? dh->LookupForm(localID, plugin) : nullptr;
	}

	std::string Label(const RE::TESForm* a_form)
	{
		if (!a_form) {
			return "none";
		}
		const auto  id = a_form->GetFormID();
		const auto* file = a_form->GetFile(0);
		if (!file) {
			return std::format("0x{:08X}", id);  // dynamic form (created in game)
		}
		const auto local = file->IsLight() ? id & 0xFFF : id & 0xFFFFFF;
		return std::format("{}|0x{:06X}", file->GetFilename(), local);
	}

	std::string Named(const RE::TESForm* a_form)
	{
		if (!a_form) {
			return "none";
		}
		const char* name = a_form->GetName();
		return name && *name ? std::format("{} {}", name, Label(a_form)) : Label(a_form);
	}

	std::string Winner(const RE::TESForm* a_form)
	{
		const auto* file = a_form ? a_form->GetFile(-1) : nullptr;
		return file ? std::string(file->GetFilename()) : "-"s;
	}
}
