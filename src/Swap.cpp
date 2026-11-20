// SPDX-License-Identifier: GPL-3.0-or-later
// TombLoot — Copyright (C) 2026 narziel. See COPYING.

#include "Swap.h"

#include "Forms.h"

namespace Swap
{
	namespace
	{
		struct Saved
		{
			RE::TESLevItem* list;
			std::uint32_t   index;
			RE::TESForm*    original;
		};
		std::vector<Saved> saved;  // entries changed by the last application

		void Restore()
		{
			for (auto it = saved.rbegin(); it != saved.rend(); ++it) {
				if (it->index < it->list->entries.size()) {
					it->list->entries[it->index].form = it->original;
				}
			}
			if (!saved.empty()) {
				logger::info("[Swap] {} entries restored before applying again", saved.size());
			}
			saved.clear();
		}

		void ApplyFamily(const char* a_name, const Config::Family& a_family)
		{
			// Generic list -> era list. A missing plugin only drops its own forms.
			std::unordered_map<RE::TESForm*, RE::TESLevItem*> table;
			for (const auto& rep : a_family.replace) {
				auto* to = Forms::Resolve(rep.to);
				auto* toList = to ? to->As<RE::TESLevItem>() : nullptr;
				if (!toList) {
					logger::warn("[Swap] {}: target {} not found or not a leveled list, its {} sources are skipped", a_name, rep.to, rep.from.size());
					continue;
				}
				for (const auto& ref : rep.from) {
					if (auto* from = Forms::Resolve(ref)) {
						table[from] = toList;
					} else {
						logger::info("[Swap] {}: source {} not loaded, skipped", a_name, ref);
					}
				}
			}

			std::uint32_t total = 0;
			for (const auto& ref : a_family.lists) {
				auto* form = Forms::Resolve(ref);
				auto* list = form ? form->As<RE::TESLevItem>() : nullptr;
				if (!list) {
					logger::info("[Swap] {}: list {} not loaded, skipped", a_name, ref);
					continue;
				}
				std::uint32_t changed = 0;
				std::uint32_t kept = 0;
				for (std::uint32_t i = 0; i < list->entries.size(); ++i) {
					auto& entry = list->entries[i];
					auto  it = entry.form ? table.find(entry.form) : table.end();
					if (it == table.end()) {
						++kept;
						continue;
					}
					logger::info("[Swap] {} {} #{} lvl {} x{}: {} -> {}", a_name, Forms::Label(list), i, entry.level, entry.count,
						Forms::Label(entry.form), Forms::Label(it->second));
					saved.push_back({ list, i, entry.form });
					entry.form = it->second;
					++changed;
				}
				logger::info("[Swap] {} {} (winner {}): {} entries replaced, {} kept", a_name, Forms::Label(list), Forms::Winner(list), changed, kept);
				total += changed;
			}
			logger::info("[Swap] {}: {} entries replaced in {} lists", a_name, total, a_family.lists.size());
		}
	}

	void Apply(const Config& a_config)
	{
		Restore();
		ApplyFamily("nordic", a_config.nordic);
	}
}
