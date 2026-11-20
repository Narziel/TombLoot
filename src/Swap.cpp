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

		// Who references each leveled list: other lists, chests, and the rest of the world (NPC
		// inventories, death items, outfits). Built after Restore, so from the original entries.
		struct Users
		{
			std::vector<RE::TESLevItem*>    lists;
			std::vector<RE::TESObjectCONT*> chests;
			bool                            world{ false };
		};
		std::unordered_map<RE::TESLevItem*, Users> users;

		void BuildUsers()
		{
			users.clear();
			auto* dh = RE::TESDataHandler::GetSingleton();
			for (auto* lvli : dh->GetFormArray<RE::TESLevItem>()) {
				if (!lvli) {
					continue;
				}
				for (const auto& entry : lvli->entries) {
					if (auto* child = entry.form ? entry.form->As<RE::TESLevItem>() : nullptr) {
						users[child].lists.push_back(lvli);
					}
				}
			}
			for (auto* cont : dh->GetFormArray<RE::TESObjectCONT>()) {
				if (!cont) {
					continue;
				}
				cont->ForEachContainerObject([&](RE::ContainerObject& a_obj) {
					if (auto* child = a_obj.obj ? a_obj.obj->As<RE::TESLevItem>() : nullptr) {
						users[child].chests.push_back(cont);
					}
					return RE::BSContainer::ForEachResult::kContinue;
				});
			}
			for (auto* npc : dh->GetFormArray<RE::TESNPC>()) {
				if (!npc) {
					continue;
				}
				if (npc->deathItem) {
					users[npc->deathItem].world = true;
				}
				npc->ForEachContainerObject([&](RE::ContainerObject& a_obj) {
					if (auto* child = a_obj.obj ? a_obj.obj->As<RE::TESLevItem>() : nullptr) {
						users[child].world = true;
					}
					return RE::BSContainer::ForEachResult::kContinue;
				});
			}
			for (auto* outfit : dh->GetFormArray<RE::BGSOutfit>()) {
				if (!outfit) {
					continue;
				}
				for (auto* item : outfit->outfitItems) {
					if (auto* child = item ? item->As<RE::TESLevItem>() : nullptr) {
						users[child].world = true;
					}
				}
			}
		}

		std::vector<Report>                 reports;
		std::unordered_set<RE::TESLevItem*> allTombLists;  // every family's lists, configured and found

		std::uint32_t SwapList(const char* a_name, RE::TESLevItem* a_list, const std::unordered_map<RE::TESForm*, RE::TESLevItem*>& a_table,
			const char* a_how)
		{
			std::uint32_t changed = 0;
			for (std::uint32_t i = 0; i < a_list->entries.size(); ++i) {
				auto& entry = a_list->entries[i];
				auto  it = entry.form ? a_table.find(entry.form) : a_table.end();
				if (it == a_table.end()) {
					continue;
				}
				logger::debug("[Swap] {} {} #{} lvl {} x{}: {} -> {}", a_name, Forms::Label(a_list), i, entry.level, entry.count,
					Forms::Label(entry.form), Forms::Label(it->second));
				saved.push_back({ a_list, i, entry.form });
				entry.form = it->second;
				++changed;
			}
			logger::info("[Swap] {} {}list {} (winner {}): {} of {} entries replaced", a_name, a_how, Forms::Label(a_list), Forms::Winner(a_list),
				changed, a_list->entries.size());
			return changed;
		}

		std::uint32_t SwapChest(const char* a_name, RE::TESObjectCONT* a_chest, const std::unordered_map<RE::TESForm*, RE::TESLevItem*>& a_table)
		{
			std::uint32_t changed = 0;
			for (std::uint32_t i = 0; i < a_chest->numContainerObjects; ++i) {
				auto* obj = a_chest->containerObjects[i];
				auto  it = obj && obj->obj ? a_table.find(obj->obj) : a_table.end();
				if (it == a_table.end()) {
					continue;
				}
				logger::info("[Swap] {} chest {} #{} x{}: {} -> {}", a_name, Forms::Named(a_chest), i, obj->count, Forms::Label(obj->obj),
					Forms::Label(it->second));
				savedChests.push_back({ a_chest, i, obj->obj });
				obj->obj = it->second;
				++changed;
			}
			return changed;
		}

		void ApplyFamily(const std::string& a_name, const Config::Family& a_family)
		{
			const char* name = a_name.c_str();
			auto&       report = reports.emplace_back();
			report.family = a_name;

			// Generic list -> era list. A missing plugin only drops its own forms.
			std::unordered_map<RE::TESForm*, RE::TESLevItem*> table;
			std::unordered_set<RE::TESLevItem*>               targetsSet;
			for (const auto& rep : a_family.replace) {
				auto* toList = Target(rep.to);
				if (!toList) {
					logger::warn("[Swap] {}: target {} not found or not a leveled list, its {} sources are skipped", name, rep.to, rep.from.size());
					continue;
				}
				targetsSet.insert(toList);
				for (const auto& ref : rep.from) {
					if (auto* from = Forms::Resolve(ref)) {
						table[from] = toList;
					} else {
						logger::debug("[Swap] {}: source {} not loaded, skipped", name, ref);
					}
				}
			}

			// 1. The family's lists: the configured ones.
			std::vector<RE::TESLevItem*>        lists;
			std::unordered_set<RE::TESLevItem*> inFamily;
			for (const auto& ref : a_family.lists) {
				auto* form = Forms::Resolve(ref);
				if (auto* list = form ? form->As<RE::TESLevItem>() : nullptr; list && inFamily.insert(list).second) {
					lists.push_back(list);
				}
			}
			const auto configuredLists = lists.size();

			// 2. Its chests: the configured ones, and every chest that holds one of its lists directly
			// (other mods' crypts reuse these lists; boss chests such as Geirmund's).
			std::vector<RE::TESObjectCONT*>        chests;
			std::unordered_set<RE::TESObjectCONT*> chestSet;
			for (const auto& ref : a_family.containers) {
				auto* form = Forms::Resolve(ref);
				if (auto* chest = form ? form->As<RE::TESObjectCONT>() : nullptr; chest && chestSet.insert(chest).second) {
					chests.push_back(chest);
				}
			}
			const auto configuredChests = chests.size();
			if (a_family.autoChests) {
				for (std::size_t l = 0; l < configuredLists; ++l) {
					if (auto it = users.find(lists[l]); it != users.end()) {
						for (auto* chest : it->second.chests) {
							if (chestSet.insert(chest).second) {
								chests.push_back(chest);
							}
						}
					}
				}
			}

			// 3. Mod sub-lists used only by tomb lists and tomb chests, that hold generic lists
			// themselves (PI_SublistLootDragonCult1HWeapon…). A list shared with the rest of the world
			// is never touched.
			if (a_family.autoSublists) {
				for (bool grew = true; grew;) {
					grew = false;
					for (std::size_t l = 0; l < lists.size(); ++l) {
						for (std::uint32_t e = 0; e < lists[l]->entries.size(); ++e) {
							auto* form = lists[l]->entries[e].form;
							auto* sub = form ? form->As<RE::TESLevItem>() : nullptr;
							if (!sub || inFamily.contains(sub) || table.contains(sub) || targetsSet.contains(sub)) {
								continue;
							}
							const bool holdsGeneric = std::ranges::any_of(sub->entries, [&](const auto& a_e) { return a_e.form && table.contains(a_e.form); });
							if (!holdsGeneric) {
								continue;
							}
							const auto it = users.find(sub);
							const bool exclusive = it != users.end() && !it->second.world &&
							                       std::ranges::all_of(it->second.lists, [&](auto* p) { return inFamily.contains(p) || allTombLists.contains(p); }) &&
							                       std::ranges::all_of(it->second.chests, [&](auto* c) { return chestSet.contains(c); });
							if (!exclusive) {
								if (report.sharedSublists.insert(sub).second) {
									logger::info("[Swap] {}: sub-list {} holds generic lists but is shared with the rest of the world: kept", name,
										Forms::Label(sub));
								}
								continue;
							}
							inFamily.insert(sub);
							allTombLists.insert(sub);
							lists.push_back(sub);
							grew = true;
						}
					}
				}
			}

			// 4. Swap.
			for (std::size_t l = 0; l < lists.size(); ++l) {
				report.entries += SwapList(name, lists[l], table, l < configuredLists ? "" : "found ");
			}
			for (auto* chest : chests) {
				report.chestEntries += SwapChest(name, chest, table);
			}
			report.lists = static_cast<std::uint32_t>(lists.size());
			report.foundLists = static_cast<std::uint32_t>(lists.size() - configuredLists);
			report.chests = chests;
			report.foundChests = static_cast<std::uint32_t>(chests.size() - configuredChests);
			logger::info("[Swap] {}: {} entries replaced in {} lists ({} found), {} in {} chests ({} found)", name, report.entries, report.lists,
				report.foundLists, report.chestEntries, report.chests.size(), report.foundChests);
		}
	}

	const std::vector<Report>& Reports()
	{
		return reports;
	}

	void Apply(const Config& a_config)
	{
		Restore();
		BuildCreated(a_config);
		BuildUsers();
		reports.clear();
		allTombLists.clear();
		for (const auto& [name, family] : a_config.families) {
			for (const auto& ref : family.lists) {
				auto* form = Forms::Resolve(ref);
				if (auto* list = form ? form->As<RE::TESLevItem>() : nullptr) {
					allTombLists.insert(list);
				}
			}
		}
		for (const auto& [name, family] : a_config.families) {
			ApplyFamily(name, family);
		}
	}
}
