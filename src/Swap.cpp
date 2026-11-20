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

		bool Loaded(const std::string& a_plugin)
		{
			auto* dh = RE::TESDataHandler::GetSingleton();
			return dh && (dh->LookupLoadedModByName(a_plugin) || dh->LookupLoadedLightModByName(a_plugin));
		}

		// "Plugin|0xID" or "@name" (a list made in the first pass).
		RE::TESLevItem* ResolveList(const std::string& a_ref)
		{
			if (a_ref.empty()) {
				return nullptr;
			}
			if (a_ref.starts_with('@')) {
				auto it = targets.find(a_ref.substr(1));
				return it != targets.end() ? it->second : nullptr;
			}
			auto* form = Forms::Resolve(a_ref);
			return form ? form->As<RE::TESLevItem>() : nullptr;
		}

		void Fill(const std::string& a_name, RE::TESLevItem* a_list, std::vector<RE::LEVELED_OBJECT>& a_entries, std::int8_t a_chanceNone,
			std::uint32_t a_missing)
		{
			std::ranges::stable_sort(a_entries, {}, &RE::LEVELED_OBJECT::level);  // the engine reads entries by rising level
			if (a_entries.size() > 255) {
				a_entries.resize(255);
			}
			a_list->entries.resize(static_cast<std::uint32_t>(a_entries.size()));
			for (std::uint32_t i = 0; i < a_entries.size(); ++i) {
				a_list->entries[i] = a_entries[i];
			}
			a_list->numEntries = static_cast<std::uint8_t>(a_entries.size());
			a_list->chanceNone = a_chanceNone;
			a_list->llFlags = RE::TESLeveledList::kCalculateFromAllLevelsLTOrEqPCLevel;
			a_list->chanceGlobal = nullptr;

			std::string text;
			for (const auto& e : a_entries) {
				text += std::format("{}{} (lvl {})", text.empty() ? "" : " ; ", Forms::Named(e.form), e.level);
			}
			logger::info("[Created] @{} 0x{:08X}: {} pieces, {} skipped: {}", a_name, a_list->GetFormID(), a_entries.size(), a_missing, text);
		}

		void BuildCreated(const Config& a_config)
		{
			targets.clear();
			auto* factory = RE::IFormFactory::GetConcreteFormFactoryByType<RE::TESLevItem>();

			// 1. The pieces of each list: missing plugin, or condition on the load order not met -> skipped.
			struct Pending
			{
				std::vector<RE::LEVELED_OBJECT> entries;
				std::uint32_t                   missing{ 0 };
			};
			std::map<std::string, Pending> pending;
			for (const auto& [name, spec] : a_config.created) {
				auto& p = pending[name];
				for (const auto& item : spec.items) {
					const bool wanted = (item.onlyIf.empty() || std::ranges::any_of(item.onlyIf, Loaded)) &&
					                    std::ranges::none_of(item.unless, Loaded);
					auto* form = wanted ? Forms::Resolve(item.form) : nullptr;
					if (!form || !(form->Is(RE::FormType::Armor) || form->Is(RE::FormType::Weapon) || form->Is(RE::FormType::LeveledItem))) {
						++p.missing;
						continue;
					}
					p.entries.push_back({ form, 1, item.level, 0, nullptr });
				}
				if (p.entries.empty()) {
					continue;
				}
				auto*& list = made[name];
				if (!list) {
					list = factory ? factory->Create() : nullptr;
				}
				if (!list) {
					logger::error("[Created] @{}: leveled list factory failed, its replacements are skipped", name);
					continue;
				}
				targets[name] = list;
			}

			// 2. Fallbacks, once every list exists ("@name" allowed): an empty list is replaced by its
			// fallback; a list with nothing at level 1 gets its fallback at level 1, otherwise it would
			// give nothing to a low-level player.
			for (const auto& [name, spec] : a_config.created) {
				auto& p = pending[name];
				auto* fallback = ResolveList(spec.fallback);
				auto  it = targets.find(name);
				if (it == targets.end()) {
					targets[name] = fallback;
					logger::info("[Created] @{}: no piece ({} skipped), fallback {}", name, p.missing,
						fallback ? Forms::Label(fallback) : "none (its replacements are skipped)"s);
					continue;
				}
				if (fallback && fallback != it->second && std::ranges::none_of(p.entries, [](const auto& e) { return e.level <= 1; })) {
					p.entries.push_back({ fallback, 1, 1, 0, nullptr });
				}
				Fill(name, it->second, p.entries, spec.chanceNone, p.missing);
			}
		}

		RE::TESLevItem* Target(const std::string& a_ref)
		{
			return ResolveList(a_ref);
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
		for (const auto& [name, family] : a_config.families) {
			ApplyFamily(name.c_str(), family);
		}
	}
}
