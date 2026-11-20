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

		struct SavedChest
		{
			RE::TESObjectCONT*    chest;
			std::uint32_t         index;
			RE::TESBoundObject*   original;
		};
		std::vector<SavedChest> savedChests;

		void Restore()
		{
			for (auto it = saved.rbegin(); it != saved.rend(); ++it) {
				if (it->index < it->list->entries.size()) {
					it->list->entries[it->index].form = it->original;
				}
			}
			for (auto it = savedChests.rbegin(); it != savedChests.rend(); ++it) {
				if (it->index < it->chest->numContainerObjects && it->chest->containerObjects[it->index]) {
					it->chest->containerObjects[it->index]->obj = it->original;
				}
			}
			if (!saved.empty() || !savedChests.empty()) {
				logger::info("[Swap] {} list entries and {} chest entries restored before applying again", saved.size(), savedChests.size());
			}
			saved.clear();
			savedChests.clear();
		}

		// Lists made at launch, by name. Made once per session; refilled at each application.
		std::map<std::string, RE::TESLevItem*> made;
		// What "@name" points to for this application: the made list, or its fallback when empty.
		std::map<std::string, RE::TESLevItem*> targets;

		void BuildCreated(const Config& a_config)
		{
			targets.clear();
			auto* factory = RE::IFormFactory::GetConcreteFormFactoryByType<RE::TESLevItem>();
			for (const auto& [name, spec] : a_config.created) {
				std::vector<RE::LEVELED_OBJECT> entries;
				std::uint32_t                   missing = 0;
				for (const auto& item : spec.items) {
					auto* form = Forms::Resolve(item.form);
					if (!form || !(form->Is(RE::FormType::Armor) || form->Is(RE::FormType::Weapon) || form->Is(RE::FormType::LeveledItem))) {
						++missing;
						continue;
					}
					entries.push_back({ form, 1, item.level, 0, nullptr });
				}
				// Nothing at level 1 (the low-level pieces are missing): the fallback fills the low levels,
				// otherwise the list would give nothing to a low-level player.
				if (!entries.empty() && std::ranges::none_of(entries, [](const auto& e) { return e.level <= 1; })) {
					if (auto* fb = Forms::Resolve(spec.fallback); fb && fb->As<RE::TESLevItem>()) {
						entries.push_back({ fb, 1, 1, 0, nullptr });
					}
				}
				std::ranges::stable_sort(entries, {}, &RE::LEVELED_OBJECT::level);  // the engine reads entries by rising level
				if (entries.size() > 255) {
					entries.resize(255);
				}

				if (entries.empty()) {
					auto* fb = Forms::Resolve(spec.fallback);
					targets[name] = fb ? fb->As<RE::TESLevItem>() : nullptr;
					logger::info("[Created] @{}: no piece loaded ({} missing), fallback {}", name, missing,
						targets[name] ? Forms::Label(targets[name]) : "none (its replacements are skipped)"s);
					continue;
				}

				auto*& list = made[name];
				if (!list) {
					list = factory ? factory->Create() : nullptr;
					if (!list) {
						logger::error("[Created] @{}: leveled list factory failed, its replacements are skipped", name);
						continue;
					}
				}
				list->entries.resize(static_cast<std::uint32_t>(entries.size()));
				for (std::uint32_t i = 0; i < entries.size(); ++i) {
					list->entries[i] = entries[i];
				}
				list->numEntries = static_cast<std::uint8_t>(entries.size());
				list->chanceNone = 0;
				list->llFlags = RE::TESLeveledList::kCalculateFromAllLevelsLTOrEqPCLevel;
				list->chanceGlobal = nullptr;
				targets[name] = list;

				std::string text;
				for (const auto& e : entries) {
					text += std::format("{}{} (lvl {})", text.empty() ? "" : " ; ", Forms::Named(e.form), e.level);
				}
				logger::info("[Created] @{} 0x{:08X}: {} pieces, {} missing: {}", name, list->GetFormID(), entries.size(), missing, text);
			}
		}

		RE::TESLevItem* Target(const std::string& a_ref)
		{
			if (a_ref.starts_with('@')) {
				auto it = targets.find(a_ref.substr(1));
				return it != targets.end() ? it->second : nullptr;
			}
			auto* form = Forms::Resolve(a_ref);
			return form ? form->As<RE::TESLevItem>() : nullptr;
		}

		void ApplyFamily(const char* a_name, const Config::Family& a_family)
		{
			// Generic list -> era list. A missing plugin only drops its own forms.
			std::unordered_map<RE::TESForm*, RE::TESLevItem*> table;
			for (const auto& rep : a_family.replace) {
				auto* toList = Target(rep.to);
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

			// Chests: only their own entries change, so a list shared with other chests stays intact.
			std::uint32_t chestTotal = 0;
			for (const auto& ref : a_family.containers) {
				auto* form = Forms::Resolve(ref);
				auto* chest = form ? form->As<RE::TESObjectCONT>() : nullptr;
				if (!chest) {
					logger::info("[Swap] {}: chest {} not loaded, skipped", a_name, ref);
					continue;
				}
				std::uint32_t changed = 0;
				for (std::uint32_t i = 0; i < chest->numContainerObjects; ++i) {
					auto* obj = chest->containerObjects[i];
					auto  it = obj && obj->obj ? table.find(obj->obj) : table.end();
					if (it == table.end()) {
						continue;
					}
					logger::info("[Swap] {} chest {} #{} x{}: {} -> {}", a_name, Forms::Label(chest), i, obj->count, Forms::Label(obj->obj),
						Forms::Label(it->second));
					savedChests.push_back({ chest, i, obj->obj });
					obj->obj = it->second;
					++changed;
				}
				logger::info("[Swap] {} chest {} (winner {}): {} entries replaced", a_name, Forms::Named(chest), Forms::Winner(chest), changed);
				chestTotal += changed;
			}
			if (!a_family.containers.empty()) {
				logger::info("[Swap] {}: {} entries replaced in {} chests", a_name, chestTotal, a_family.containers.size());
			}
		}
	}

	void Apply(const Config& a_config)
	{
		Restore();
		BuildCreated(a_config);
		ApplyFamily("nordic", a_config.nordic);
		ApplyFamily("solstheim", a_config.solstheim);
	}
}
