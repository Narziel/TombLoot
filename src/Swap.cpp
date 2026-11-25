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
			bool a_highest, std::uint32_t a_missing)
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
			// "Highest": one piece among those of the highest level reached, as Bethesda's boss gold.
			a_list->llFlags = a_highest ? RE::TESLeveledList::kCalculateForEachItemInCount : RE::TESLeveledList::kCalculateFromAllLevelsLTOrEqPCLevel;
			a_list->chanceGlobal = nullptr;

			std::string text;
			for (const auto& e : a_entries) {
				text += std::format("{}{}{} (lvl {})", text.empty() ? "" : " ; ", Forms::Named(e.form), e.count > 1 ? std::format(" x{}", e.count) : ""s, e.level);
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
					// Gear, leveled lists, and treasure (ingots, ore, gems are misc items).
					if (!form || !(form->Is(RE::FormType::Armor) || form->Is(RE::FormType::Weapon) || form->Is(RE::FormType::LeveledItem) ||
									  form->Is(RE::FormType::Misc))) {
						++p.missing;
						continue;
					}
					p.entries.push_back({ form, item.count, item.level, 0, nullptr });
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
				Fill(name, it->second, p.entries, spec.chanceNone, spec.highest, p.missing);
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

		// The keywords of "era_keywords", by EditorID. An unknown one is noted and skipped.
		std::vector<RE::BGSKeyword*> EraKeywords(const std::vector<std::string>& a_edids)
		{
			std::vector<RE::BGSKeyword*> out;
			if (a_edids.empty()) {
				return out;
			}
			auto* dh = RE::TESDataHandler::GetSingleton();
			for (const auto& edid : a_edids) {
				RE::BGSKeyword* found = nullptr;
				for (auto* kw : dh->GetFormArray<RE::BGSKeyword>()) {
					const char* name = kw ? kw->GetFormEditorID() : nullptr;
					if (name && edid == name) {
						found = kw;
						break;
					}
				}
				if (found) {
					out.push_back(found);
				} else {
					logger::warn("[Era] keyword {} not found, skipped", edid);
				}
			}
			return out;
		}

		bool IsEraWeapon(RE::TESForm* a_form, const std::vector<RE::BGSKeyword*>& a_keywords)
		{
			// Plain weapons only: enchanted lists keep their own pieces and odds.
			auto* weap = a_form ? a_form->As<RE::TESObjectWEAP>() : nullptr;
			return weap && !weap->formEnchanting && weap->GetPlayable() && std::ranges::any_of(a_keywords, [&](auto* kw) { return weap->HasKeyword(kw); });
		}

		// Every era weapon a list can give, with the lowest level it can come at.
		void CollectEraWeapons(RE::TESForm* a_form, std::uint16_t a_level, const std::vector<RE::BGSKeyword*>& a_keywords,
			std::unordered_set<RE::TESLevItem*>& a_visited, std::map<RE::TESForm*, std::uint16_t>& a_found)
		{
			if (IsEraWeapon(a_form, a_keywords)) {
				auto [it, added] = a_found.try_emplace(a_form, a_level);
				if (!added) {
					it->second = (std::min)(it->second, a_level);
				}
				return;
			}
			auto* list = a_form ? a_form->As<RE::TESLevItem>() : nullptr;
			if (!list || !a_visited.insert(list).second) {
				return;
			}
			for (const auto& entry : list->entries) {
				CollectEraWeapons(entry.form, (std::max)(a_level, entry.level), a_keywords, a_visited, a_found);
			}
		}

		// Everything a list can give, sub-lists included.
		void CollectAll(RE::TESForm* a_form, std::unordered_set<RE::TESForm*>& a_out)
		{
			if (!a_form || !a_out.insert(a_form).second) {
				return;
			}
			if (auto* list = a_form->As<RE::TESLevItem>()) {
				for (const auto& entry : list->entries) {
					CollectAll(entry.form, a_out);
				}
			}
		}

		// Copies of era lists with the era weapons of their sources, by "family|list". Made once per
		// session, refilled at each application.
		std::map<std::string, RE::TESLevItem*> eraCopies;

		// The era list, or a copy of it that also holds the era weapons found in its generic sources
		// and not already in it. The era list itself is never changed: draugr carry it too.
		RE::TESLevItem* WithEraWeapons(const std::string& a_family, RE::TESLevItem* a_era, const std::vector<RE::TESForm*>& a_sources,
			const std::vector<RE::BGSKeyword*>& a_keywords, std::uint32_t& a_count)
		{
			std::map<RE::TESForm*, std::uint16_t> found;
			std::unordered_set<RE::TESLevItem*>   visited;
			for (auto* source : a_sources) {
				CollectEraWeapons(source, 1, a_keywords, visited, found);
			}
			std::unordered_set<RE::TESForm*> already;
			CollectAll(a_era, already);
			// Only an era list of plain weapons takes them (not the enchanted or special loot lists).
			const bool plain = std::ranges::any_of(already, [](RE::TESForm* a_form) {
				auto* weap = a_form->As<RE::TESObjectWEAP>();
				return weap && !weap->formEnchanting;
			});
			if (!plain) {
				return a_era;
			}
			std::erase_if(found, [&](const auto& a_pair) { return already.contains(a_pair.first); });
			if (found.empty()) {
				return a_era;
			}

			auto*& copy = eraCopies[std::format("{}|{}", a_family, Forms::Label(a_era))];
			if (!copy) {
				auto* factory = RE::IFormFactory::GetConcreteFormFactoryByType<RE::TESLevItem>();
				copy = factory ? factory->Create() : nullptr;
			}
			if (!copy) {
				logger::error("[Era] {}: leveled list factory failed, {} keeps its pieces only", a_family, Forms::Label(a_era));
				return a_era;
			}

			std::vector<RE::LEVELED_OBJECT> entries(a_era->entries.begin(), a_era->entries.end());
			std::string                     text;
			for (const auto& [form, level] : found) {
				if (entries.size() >= 255) {
					logger::warn("[Era] {}: {} is full (255 entries), some era weapons are left out", a_family, Forms::Label(a_era));
					break;
				}
				entries.push_back({ form, 1, level, 0, nullptr });
				text += std::format("{}{} (lvl {})", text.empty() ? "" : " ; ", Forms::Named(form), level);
				++a_count;
			}
			std::ranges::stable_sort(entries, {}, &RE::LEVELED_OBJECT::level);  // the engine reads entries by rising level
			copy->entries.resize(static_cast<std::uint32_t>(entries.size()));
			for (std::uint32_t i = 0; i < entries.size(); ++i) {
				copy->entries[i] = entries[i];
			}
			copy->numEntries = static_cast<std::uint8_t>(entries.size());
			copy->chanceNone = a_era->chanceNone;
			copy->llFlags = a_era->llFlags;
			copy->chanceGlobal = a_era->chanceGlobal;
			logger::info("[Era] {}: {} + {} era weapons from its generic sources, as 0x{:08X}: {}", a_family, Forms::Label(a_era),
				entries.size() - a_era->entries.size(), copy->GetFormID(), text);
			return copy;
		}

		// --- Pieces placed by hand (altars, tables, floors of the base game) ---

		// One family's table for placed pieces: generic piece -> the era lists of its replacements.
		struct PlacedFamily
		{
			std::string                                                     name;
			std::vector<std::string>                                        locTypes;
			std::vector<std::string>                                        rootPlugins;
			std::unordered_map<RE::TESForm*, std::vector<RE::TESLevItem*>> targets;
		};
		std::vector<PlacedFamily> placedFamilies;  // families with a root plugin come first (Solstheim before Skyrim)
		std::shared_mutex         placedLock;      // the hook reads on the loading threads; Apply writes
		std::atomic<std::uint32_t> placedSwapped{ 0 };

		bool SameText(std::string_view a, std::string_view b)
		{
			return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(), [](char x, char y) {
				return std::tolower(static_cast<unsigned char>(x)) == std::tolower(static_cast<unsigned char>(y));
			});
		}

		// Placed by Bethesda: the reference comes from a base game master.
		bool OfficialFile(const RE::TESFile* a_file)
		{
			if (!a_file) {
				return false;
			}
			const std::string_view name = a_file->GetFilename();
			return std::ranges::any_of(std::array{ "Skyrim.esm"sv, "Update.esm"sv, "Dawnguard.esm"sv, "HearthFires.esm"sv, "Dragonborn.esm"sv },
				[&](std::string_view a_official) { return SameText(name, a_official); });
		}

		// What a piece is, for an era piece of the same kind: 100 + weapon type, or the armor's main slot
		// (1 head, 2 body, 3 hands, 4 feet, 5 shield). 0: not a piece to replace (clothes, jewelry, staves…).
		int KindOf(const RE::TESForm* a_form)
		{
			if (const auto* weap = a_form ? a_form->As<RE::TESObjectWEAP>() : nullptr) {
				const auto type = weap->GetWeaponType();
				return type == RE::WEAPON_TYPE::kHandToHandMelee || type == RE::WEAPON_TYPE::kStaff ? 0 : 100 + static_cast<int>(type);
			}
			const auto* armor = a_form ? a_form->As<RE::TESObjectARMO>() : nullptr;
			if (!armor || armor->GetArmorType() == RE::BGSBipedObjectForm::ArmorType::kClothing) {
				return 0;
			}
			using Slot = RE::BGSBipedObjectForm::BipedObjectSlot;
			const auto slots = armor->GetSlotMask();
			return armor->IsShield()          ? 5 :
			       slots.any(Slot::kBody)     ? 2 :
			       slots.any(Slot::kHands)    ? 3 :
			       slots.any(Slot::kFeet)     ? 4 :
			       slots.any(Slot::kHead, Slot::kHair) ? 1 :
			                                    0;
		}

		bool Enchanted(const RE::TESForm* a_form)
		{
			if (const auto* weap = a_form->As<RE::TESObjectWEAP>()) {
				return weap->formEnchanting != nullptr;
			}
			const auto* armor = a_form->As<RE::TESObjectARMO>();
			return armor && armor->formEnchanting != nullptr;
		}

		// Every weapon and armor piece a list can give, with the lowest level it comes at.
		void CollectPieces(RE::TESForm* a_form, std::uint16_t a_level, std::unordered_set<RE::TESLevItem*>& a_visited,
			std::map<RE::TESBoundObject*, std::uint16_t>& a_out)
		{
			if (auto* list = a_form ? a_form->As<RE::TESLevItem>() : nullptr) {
				if (a_visited.insert(list).second) {
					for (const auto& entry : list->entries) {
						CollectPieces(entry.form, (std::max)(a_level, entry.level), a_visited, a_out);
					}
				}
				return;
			}
			if (a_form && (a_form->Is(RE::FormType::Weapon) || a_form->Is(RE::FormType::Armor))) {
				auto [it, added] = a_out.try_emplace(static_cast<RE::TESBoundObject*>(a_form), a_level);
				if (!added) {
					it->second = (std::min)(it->second, a_level);
				}
			}
		}

		bool PlaceMatches(const PlacedFamily& a_family, const RE::BGSLocation* a_location)
		{
			bool typed = false;
			const RE::BGSLocation* root = a_location;
			for (const auto* loc = a_location; loc; loc = loc->parentLoc) {
				typed = typed || std::ranges::any_of(a_family.locTypes, [&](const auto& a_type) { return loc->HasKeywordString(a_type); });
				root = loc;
			}
			if (!typed) {
				return false;
			}
			if (a_family.rootPlugins.empty()) {
				return true;
			}
			const auto* file = root->GetFile(0);
			return file && std::ranges::any_of(a_family.rootPlugins, [&](const auto& a_plugin) { return SameText(file->GetFilename(), a_plugin); });
		}

		// Before the piece loads: a generic piece placed by Bethesda in a tomb becomes an era piece of the same
		// kind, chosen by its reference (the same each time, nothing in the save; without TombLoot it comes back).
		void SwapPlaced(RE::TESObjectREFR* a_ref)
		{
			auto* base = a_ref ? a_ref->GetBaseObject() : nullptr;
			if (!base || !(base->Is(RE::FormType::Weapon) || base->Is(RE::FormType::Armor)) || a_ref->IsPersistent() || !OfficialFile(a_ref->GetFile(0))) {
				return;
			}
			std::shared_lock lock{ placedLock };
			const auto       found = std::ranges::find_if(placedFamilies, [&](const auto& a_f) { return a_f.targets.contains(base); });
			if (found == placedFamilies.end()) {
				return;
			}
			const int kind = KindOf(base);
			if (kind == 0 || a_ref->GetOwner() || a_ref->extraList.HasType(RE::ExtraDataType::kEnchantment)) {
				return;
			}
			const auto* cell = a_ref->GetParentCell();
			const auto* location = cell ? cell->GetLocation() : nullptr;
			if (!location) {
				location = a_ref->GetEditorLocation();
			}
			if (!location) {
				return;
			}
			for (const auto& family : placedFamilies) {
				const auto it = family.targets.find(base);
				if (it == family.targets.end() || !PlaceMatches(family, location)) {
					continue;
				}
				std::map<RE::TESBoundObject*, std::uint16_t> pieces;
				std::unordered_set<RE::TESLevItem*>          visited;
				for (auto* list : it->second) {
					CollectPieces(list, 1, visited, pieces);
				}
				const bool enchanted = Enchanted(base);
				std::erase_if(pieces, [&](const auto& a_p) { return KindOf(a_p.first) != kind || Enchanted(a_p.first) != enchanted; });
				if (pieces.empty()) {
					return;  // no era piece of this kind (a mace…): it stays
				}
				// Among the pieces of the player's level; the lowest ones when none is reached yet.
				const auto* player = RE::PlayerCharacter::GetSingleton();
				const auto  level = static_cast<std::uint16_t>((std::max)(player ? static_cast<int>(player->GetLevel()) : 1, 1));
				std::vector<RE::TESBoundObject*> choice;
				for (const auto& [piece, min] : pieces) {
					if (min <= level) {
						choice.push_back(piece);
					}
				}
				if (choice.empty()) {
					const auto lowest = std::ranges::min_element(pieces, {}, [](const auto& a_p) { return a_p.second; })->second;
					for (const auto& [piece, min] : pieces) {
						if (min == lowest) {
							choice.push_back(piece);
						}
					}
				}
				std::uint64_t seed = a_ref->GetFormID() * 0x9E3779B97F4A7C15ULL;
				seed ^= seed >> 31;
				auto* pick = choice[static_cast<std::size_t>(seed % choice.size())];
				a_ref->SetObjectReference(pick);
				placedSwapped.fetch_add(1);
				const char* place = location->GetName();
				logger::info("[Placed] {}: {} -> {} | ref 0x{:08X} | {}", family.name, Forms::Named(base), Forms::Named(pick), a_ref->GetFormID(),
					place && *place ? place : Forms::Label(location));
				return;
			}
		}

		struct PlacedHook
		{
			static void thunk(RE::TESObjectREFR* a_ref)
			{
				SwapPlaced(a_ref);
				func(a_ref);
			}
			static inline REL::Relocation<decltype(thunk)> func;
		};

		// The family's generic pieces and the era lists they go to (gear only: gold and treasure are not placed).
		void RecordPlaced(const std::string& a_name, const Config::Family& a_family,
			const std::vector<std::pair<RE::TESLevItem*, std::vector<RE::TESForm*>>>& a_bySource,
			const std::unordered_map<RE::TESForm*, RE::TESLevItem*>& a_table, const std::vector<RE::BGSKeyword*>& a_eraKeywords)
		{
			if (a_family.placedLocTypes.empty()) {
				return;
			}
			auto& family = placedFamilies.emplace_back();
			family.name = a_name;
			family.locTypes = a_family.placedLocTypes;
			family.rootPlugins = a_family.placedRootPlugins;
			for (const auto& [toList, sources] : a_bySource) {
				for (auto* source : sources) {
					auto* target = a_table.at(source);
					std::unordered_set<RE::TESForm*> leaves;
					CollectAll(source, leaves);
					for (auto* leaf : leaves) {
						const auto* kwForm = leaf->As<RE::BGSKeywordForm>();
						const bool  era = kwForm && std::ranges::any_of(a_eraKeywords, [&](auto* kw) { return kwForm->HasKeyword(kw); });
						if (era || KindOf(leaf) == 0) {
							continue;
						}
						auto& lists = family.targets[leaf];
						if (std::ranges::find(lists, target) == lists.end()) {
							lists.push_back(target);
						}
					}
				}
			}
			logger::info("[Placed] {}: {} generic pieces placed by hand in {} get an era piece{}", a_name, family.targets.size(),
				[&] {
					std::string text;
					for (const auto& t : family.locTypes) {
						text += (text.empty() ? "" : ", ") + t;
					}
					return text;
				}(),
				family.rootPlugins.empty() ? "" : " (root location from " + family.rootPlugins.front() + ")");
		}

		void ApplyFamily(const std::string& a_name, const Config::Family& a_family, bool a_treasure)
		{
			const char* name = a_name.c_str();
			auto&       report = reports.emplace_back();
			report.family = a_name;

			// Generic list -> era list. A missing plugin only drops its own forms.
			std::vector<std::pair<RE::TESLevItem*, std::vector<RE::TESForm*>>> bySource;  // era list -> its sources, in config order
			for (const auto& rep : a_family.replace) {
				if (rep.treasure && !a_treasure) {
					continue;
				}
				auto* toList = Target(rep.to);
				if (!toList) {
					logger::warn("[Swap] {}: target {} not found or not a leveled list, its {} sources are skipped", name, rep.to, rep.from.size());
					continue;
				}
				auto it = std::ranges::find(bySource, toList, &decltype(bySource)::value_type::first);
				auto& sources = it != bySource.end() ? it->second : bySource.emplace_back(toList, std::vector<RE::TESForm*>{}).second;
				for (const auto& ref : rep.from) {
					if (auto* from = Forms::Resolve(ref)) {
						sources.push_back(from);
					} else {
						logger::debug("[Swap] {}: source {} not loaded, skipped", name, ref);
					}
				}
			}

			// Era weapons that mods put in the generic lists: the era list is copied with them.
			const auto keywords = EraKeywords(a_family.eraKeywords);
			std::unordered_map<RE::TESForm*, RE::TESLevItem*> table;
			std::unordered_set<RE::TESLevItem*>               targetsSet;
			for (auto& [toList, sources] : bySource) {
				targetsSet.insert(toList);
				auto* target = keywords.empty() ? toList : WithEraWeapons(a_name, toList, sources, keywords, report.eraWeapons);
				targetsSet.insert(target);
				for (auto* from : sources) {
					table[from] = target;
				}
			}

			RecordPlaced(a_name, a_family, bySource, table, keywords);

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

	void InstallPlaced()
	{
		REL::Relocation<std::uintptr_t> vtable{ RE::VTABLE_TESObjectREFR[0] };
		PlacedHook::func = vtable.write_vfunc(0x13, PlacedHook::thunk);  // InitItemImpl: before the piece loads
		logger::info("[Placed] hook installed (InitItemImpl of TESObjectREFR)");
	}

	std::uint32_t PlacedSwapped()
	{
		return placedSwapped.load();
	}

	void Apply(const Config& a_config)
	{
		std::unique_lock lock{ placedLock };  // the placed pieces' hook waits while lists change
		placedFamilies.clear();
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
		// Treasure instead of septims: off in the config, or left to a coin mod that is loaded.
		bool treasure = a_config.treasure;
		if (!treasure) {
			logger::info("[Treasure] off in the config: tomb gold stays as the load order set it");
		} else if (auto it = std::ranges::find_if(a_config.treasureUnless, Loaded); it != a_config.treasureUnless.end()) {
			treasure = false;
			logger::info("[Treasure] {} is loaded: tomb gold is left to it", *it);
		}
		for (const auto& [name, family] : a_config.families) {
			ApplyFamily(name, family, treasure);
		}
		// A family bound to a root (Solstheim) is tried before the general one (Skyrim's barrows).
		std::ranges::stable_partition(placedFamilies, [](const auto& a_f) { return !a_f.rootPlugins.empty(); });
	}
}
