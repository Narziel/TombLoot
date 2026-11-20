// SPDX-License-Identifier: GPL-3.0-or-later
// TombLoot — Copyright (C) 2026 narziel. See COPYING.

#include "Survey.h"

#include "Forms.h"
#include "Swap.h"

namespace Survey
{
	namespace
	{
		constexpr std::uint32_t kMaxDepth = 16;  // safety net against a list that contains itself

		// Tomb chests relevés dans Skyrim.esm et Dragonborn.esm (CONCEPTION.md § 2).
		struct Seed
		{
			const char* family;
			const char* ref;
		};
		constexpr Seed kSeeds[] = {
			{ "draugr", "Skyrim.esm|0x020670" },     // TreasDraugrChest
			{ "draugr", "Skyrim.esm|0x020671" },     // TreasDraugrChestBoss
			{ "draugr", "Skyrim.esm|0x023969" },     // TreasDraugrAmbushCorpse01
			{ "draugr", "Skyrim.esm|0x08008F" },     // …02
			{ "draugr", "Skyrim.esm|0x080090" },     // …03
			{ "draugr", "Skyrim.esm|0x080091" },     // …04
			{ "draugr", "Skyrim.esm|0x080092" },     // …05
			{ "draugr", "Skyrim.esm|0x080093" },     // …06
			{ "draugr", "Skyrim.esm|0x080094" },     // …07
			{ "draugr", "Skyrim.esm|0x0DB047" },     // TreasDraugrAmbushCorpseSitting
			{ "draugr", "Skyrim.esm|0x08008D" },     // TreasDraugrAmbushCorpseWrapped01
			{ "draugr", "Skyrim.esm|0x08008E" },     // …Wrapped02
			{ "draugr", "Dragonborn.esm|0x02AAC1" },  // DLC2TreasDraugrChest
			{ "draugr", "Dragonborn.esm|0x02AAC2" },  // DLC2TreasDraugrChestBoss
			{ "dwemer", "Skyrim.esm|0x020652" },     // TreasDwarvenChestBoss
			{ "dwemer", "Skyrim.esm|0x02069A" },     // TreasDwarvenChestLarge
			{ "dwemer", "Skyrim.esm|0x020650" },     // TreasDwarvenChestSmall
			{ "dwemer", "Skyrim.esm|0x020653" },     // TreasDwarvenJewelryBox
			{ "falmer", "Skyrim.esm|0x020659" },     // TreasFalmerChest
			{ "falmer", "Skyrim.esm|0x02065B" },     // TreasFalmerChestBoss
			{ "falmer", "Skyrim.esm|0x0B1176" },     // TreasFalmerChestBossDwarven
		};

		// Playable Ancient Nord armor: in Skyrim.esm it carries ArmorMaterialDaedric, so the keyword
		// cannot tell its era (CONCEPTION.md § 2).
		constexpr const char* kAncientNordArmor[] = {
			"Skyrim.esm|0x018388",  // ArmorDraugrCuirass
			"Skyrim.esm|0x056A9D",  // ArmorDraugrBoots
			"Skyrim.esm|0x056B17",  // ArmorDraugrGauntlets
			"Skyrim.esm|0x056A9E",  // ArmorDraugrHelmet
		};

		// Probe of DynamicContainerLoot: Ancient Nord Armoury adds this list to TreasDraugrChestBoss and
		// the merge keeps it. Present = the merge ran before this survey.
		constexpr const char* kProbeChest = "Skyrim.esm|0x020671";
		constexpr const char* kProbeList = "Skyrim.esm|0x0B5562";  // LootDraugrEnchArmor25

		// Era of a piece of gear, from its material keyword (EditorID).
		enum class Era
		{
			kAncient,   // Ancient Nord / draugr
			kDwemer,
			kFalmer,
			kTimeless,  // ebony, daedric: option of CONCEPTION.md § 7
			kNordicDLC2,  // Dragonborn Nordic carved / stalhrim: modern, but used by some ancient armor mods
			kModern,
			kUnknown,  // gear with no recognized material keyword
			kStaff,    // staves: no era, never replaced
			kAmmo,
			kOther,  // not gear: gold, gems, potions, scrolls…
			kCount
		};
		constexpr const char* kEraNames[] = { "ancient", "dwemer", "falmer", "timeless", "nordic_dlc2", "modern", "unknown",
			"staff", "ammo", "other" };
		constexpr std::size_t kEras = static_cast<std::size_t>(Era::kCount);

		struct MaterialRule
		{
			std::string_view prefix;
			Era              era;
		};
		// First matching rule wins: exact names before broader prefixes.
		constexpr MaterialRule kMaterialRules[] = {
			{ "WeapMaterialDraugr", Era::kAncient },  // WeapMaterialDraugr, WeapMaterialDraugrHoned
			{ "WeapMaterialDwarven", Era::kDwemer },
			{ "ArmorMaterialDwarven", Era::kDwemer },
			{ "WeapMaterialFalmer", Era::kFalmer },
			{ "ArmorMaterialFalmer", Era::kFalmer },
			{ "DLC1ArmorMaterialFalmer", Era::kFalmer },
			{ "WeapMaterialEbony", Era::kTimeless },
			{ "ArmorMaterialEbony", Era::kTimeless },
			{ "WeapMaterialDaedric", Era::kTimeless },
			{ "ArmorMaterialDaedric", Era::kTimeless },
			{ "DLC2ArmorMaterialNordic", Era::kNordicDLC2 },
			{ "DLC2WeaponMaterialNordic", Era::kNordicDLC2 },
			{ "DLC2ArmorMaterialStalhrim", Era::kNordicDLC2 },
			{ "DLC2WeaponMaterialStalhrim", Era::kNordicDLC2 },
			{ "WeapMaterial", Era::kModern },
			{ "ArmorMaterial", Era::kModern },
			{ "DLC1WeapMaterial", Era::kModern },
			{ "DLC1ArmorMaterial", Era::kModern },
			{ "DLC2WeaponMaterial", Era::kModern },
			{ "DLC2ArmorMaterial", Era::kModern },
		};

		std::unordered_set<RE::FormID>  ancientArmor;
		std::unordered_set<std::string> ancientPlugins;  // every weapon and armor of these files is ancient

		std::string Keywords(const RE::BGSKeywordForm* a_form)
		{
			std::string out;
			if (!a_form) {
				return out;
			}
			for (std::uint32_t i = 0; i < a_form->numKeywords; ++i) {
				const auto* kw = a_form->keywords[i];
				const char* edid = kw ? kw->GetFormEditorID() : nullptr;
				if (edid && *edid) {
					out += out.empty() ? edid : ", "s + edid;
				}
			}
			return out;
		}

		Era Classify(RE::TESForm* a_form)
		{
			if (!a_form) {
				return Era::kOther;
			}
			if (a_form->Is(RE::FormType::Ammo)) {
				return Era::kAmmo;
			}
			const bool weapon = a_form->Is(RE::FormType::Weapon);
			const bool armor = a_form->Is(RE::FormType::Armor);
			if (!weapon && !armor) {
				return Era::kOther;
			}
			if (ancientArmor.contains(a_form->GetFormID())) {
				return Era::kAncient;
			}
			if (const auto* file = a_form->GetFile(0); file && ancientPlugins.contains(std::string(file->GetFilename()))) {
				return Era::kAncient;
			}
			if (weapon) {
				if (auto* weap = a_form->As<RE::TESObjectWEAP>(); weap && weap->IsStaff()) {
					return Era::kStaff;
				}
			}
			auto* kwForm = a_form->As<RE::BGSKeywordForm>();
			if (armor) {
				auto* arm = a_form->As<RE::TESObjectARMO>();
				// Jewelry and clothing are not "gear" for this mod.
				if (arm && !arm->IsHeavyArmor() && !arm->IsLightArmor() && !arm->IsShield()) {
					return Era::kOther;
				}
			}
			if (kwForm) {
				for (const auto& rule : kMaterialRules) {
					for (std::uint32_t i = 0; i < kwForm->numKeywords; ++i) {
						const auto* kw = kwForm->keywords[i];
						const char* edid = kw ? kw->GetFormEditorID() : nullptr;
						if (edid && std::string_view{ edid }.starts_with(rule.prefix)) {
							return rule.era;
						}
					}
				}
			}
			return Era::kUnknown;
		}

		struct Leaves
		{
			std::array<std::uint32_t, kEras>            counts{};
			std::array<std::vector<std::string>, kEras> examples;
			std::unordered_set<RE::FormID>              seen;

			void Add(RE::TESForm* a_item)
			{
				if (!seen.insert(a_item->GetFormID()).second) {
					return;
				}
				const auto e = static_cast<std::size_t>(Classify(a_item));
				++counts[e];
				if (examples[e].size() < 6) {
					auto text = std::format("{} [{}]", Forms::Named(a_item), a_item->GetFile(0) ? a_item->GetFile(0)->GetFilename() : "-"sv);
					if (e == static_cast<std::size_t>(Era::kUnknown)) {
						text += std::format(" {{{}}}", Keywords(a_item->As<RE::BGSKeywordForm>()));
					}
					examples[e].push_back(std::move(text));
				}
			}

			[[nodiscard]] std::string Summary() const
			{
				std::string out;
				for (std::size_t e = 0; e < kEras; ++e) {
					if (counts[e]) {
						out += std::format("{}{} {}", out.empty() ? "" : ", ", counts[e], kEraNames[e]);
					}
				}
				return out.empty() ? "nothing"s : out;
			}

			[[nodiscard]] bool HasGear() const
			{
				for (std::size_t e = 0; e < static_cast<std::size_t>(Era::kStaff); ++e) {
					if (counts[e]) {
						return true;
					}
				}
				return false;
			}
		};

		void Collect(RE::TESForm* a_form, Leaves& a_out, std::unordered_set<RE::FormID>& a_visited, std::uint32_t a_depth = 0)
		{
			if (!a_form || a_depth > kMaxDepth) {
				return;
			}
			if (auto* list = a_form->As<RE::TESLevItem>()) {
				if (!a_visited.insert(list->GetFormID()).second) {
					return;
				}
				for (const auto& entry : list->entries) {
					Collect(entry.form, a_out, a_visited, a_depth + 1);
				}
				return;
			}
			a_out.Add(a_form);
		}

		Leaves LeavesOf(RE::TESForm* a_form)
		{
			Leaves                         leaves;
			std::unordered_set<RE::FormID> visited;
			Collect(a_form, leaves, visited);
			return leaves;
		}

		const char* TypeName(const RE::TESForm* a_form)
		{
			if (!a_form) {
				return "-";
			}
			switch (a_form->GetFormType()) {
			case RE::FormType::LeveledItem:
				return "LVLI";
			case RE::FormType::Weapon:
				return "WEAP";
			case RE::FormType::Armor:
				return "ARMO";
			case RE::FormType::Ammo:
				return "AMMO";
			default:
				return "item";
			}
		}

		// Who references a list directly: containers, other lists, NPC death items.
		struct Users
		{
			std::vector<RE::TESObjectCONT*> containers;
			std::vector<RE::TESLevItem*>    lists;
			std::uint32_t                   deathItems{ 0 };
		};

		std::unordered_map<RE::TESLevItem*, Users> FindUsers(const std::unordered_set<RE::TESLevItem*>& a_lists)
		{
			std::unordered_map<RE::TESLevItem*, Users> users;
			auto* dh = RE::TESDataHandler::GetSingleton();
			for (auto* cont : dh->GetFormArray<RE::TESObjectCONT>()) {
				if (!cont) {
					continue;
				}
				cont->ForEachContainerObject([&](RE::ContainerObject& a_obj) {
					if (auto* list = a_obj.obj ? a_obj.obj->As<RE::TESLevItem>() : nullptr; list && a_lists.contains(list)) {
						users[list].containers.push_back(cont);
					}
					return RE::BSContainer::ForEachResult::kContinue;
				});
			}
			for (auto* lvli : dh->GetFormArray<RE::TESLevItem>()) {
				if (!lvli) {
					continue;
				}
				for (const auto& entry : lvli->entries) {
					if (auto* list = entry.form ? entry.form->As<RE::TESLevItem>() : nullptr; list && a_lists.contains(list)) {
						users[list].lists.push_back(lvli);
					}
				}
			}
			for (auto* npc : dh->GetFormArray<RE::TESNPC>()) {
				if (npc && npc->deathItem && a_lists.contains(npc->deathItem)) {
					++users[npc->deathItem].deathItems;
				}
			}
			return users;
		}

		template <class T>
		std::string Sample(const std::vector<T*>& a_forms, std::size_t a_max = 5)
		{
			std::string out;
			for (std::size_t i = 0; i < a_forms.size() && i < a_max; ++i) {
				out += std::format("{}{}", out.empty() ? "" : " ; ", Forms::Named(a_forms[i]));
			}
			if (a_forms.size() > a_max) {
				out += std::format(" ; … ({} in all)", a_forms.size());
			}
			return out;
		}
	}

	namespace
	{
		void Prepare(const Config& a_config)
		{
			ancientPlugins = { a_config.ancientPlugins.begin(), a_config.ancientPlugins.end() };
			ancientArmor.clear();
			for (const auto* ref : kAncientNordArmor) {
				if (auto* form = Forms::Resolve(ref)) {
					ancientArmor.insert(form->GetFormID());
				}
			}
			for (const auto& ref : a_config.ancientItems) {
				if (auto* form = Forms::Resolve(ref)) {
					ancientArmor.insert(form->GetFormID());
				}
			}
		}

		// What a chest entry can give, remembering for each item of the wrong era the path that
		// brings it: "chest entry > its entry". That path is what a player adds to the config.
		struct Traced
		{
			Leaves                                     leaves;
			std::map<std::string, std::vector<std::string>> wrongVia;  // path -> items
		};

		bool WrongEra(Era a_era)
		{
			return a_era == Era::kModern || a_era == Era::kUnknown;
		}

		void Trace(RE::TESForm* a_form, Traced& a_out, std::unordered_set<RE::FormID>& a_visited, const std::string& a_via, std::uint32_t a_depth)
		{
			if (!a_form || a_depth > kMaxDepth) {
				return;
			}
			if (auto* list = a_form->As<RE::TESLevItem>()) {
				if (!a_visited.insert(list->GetFormID()).second) {
					return;
				}
				for (const auto& entry : list->entries) {
					// The path keeps the first two levels: the chest entry, then its entry.
					const auto via = a_depth == 0 ? std::format("{} > {}", a_via, Forms::Label(entry.form)) : a_via;
					Trace(entry.form, a_out, a_visited, via, a_depth + 1);
				}
				return;
			}
			if (a_out.leaves.seen.contains(a_form->GetFormID())) {
				return;
			}
			a_out.leaves.Add(a_form);
			if (WrongEra(Classify(a_form))) {
				auto& items = a_out.wrongVia[a_via];
				if (items.size() < 3) {
					items.push_back(std::format("{} [{}]", Forms::Named(a_form), a_form->GetFile(0) ? a_form->GetFile(0)->GetFilename() : "-"sv));
				} else if (items.size() == 3) {
					items.push_back("…");
				}
			}
		}

		void Summary()
		{
			for (const auto& report : Swap::Reports()) {
				Traced                         traced;
				std::unordered_set<RE::FormID> visited;  // shared by the family: each list walked once
				for (auto* chest : report.chests) {
					chest->ForEachContainerObject([&](RE::ContainerObject& a_obj) {
						Trace(a_obj.obj, traced, visited, Forms::Label(a_obj.obj), 0);
						return RE::BSContainer::ForEachResult::kContinue;
					});
				}
				logger::info("[Summary] {}: {} lists ({} found by themselves), {} chests ({} found by themselves); {} list entries and {} chest entries replaced",
					report.family, report.lists, report.foundLists, report.chests.size(), report.foundChests, report.entries, report.chestEntries);
				logger::info("[Summary] {}: its chests can give {}", report.family, traced.leaves.Summary());
				if (traced.wrongVia.empty()) {
					logger::info("[Summary] {}: no gear of the wrong era left", report.family);
				}
				for (const auto& [via, items] : traced.wrongVia) {
					std::string text;
					for (const auto& item : items) {
						text += text.empty() ? item : " ; " + item;
					}
					logger::info("[Summary] {}: wrong era via {}: {}", report.family, via, text);
				}
				for (auto* sub : report.sharedSublists) {
					logger::info("[Summary] {}: sub-list {} holds generic lists but is shared with the rest of the world, kept as is", report.family,
						Forms::Label(sub));
				}
			}
		}

		void Full(const Config& a_config)
		{
		auto* dh = RE::TESDataHandler::GetSingleton();
		if (!dh) {
			logger::error("[Survey] TESDataHandler missing");
			return;
		}

		Prepare(a_config);

		// DynamicContainerLoot: loaded, and did its merge run before us?
		const bool dclLoaded = REX::W32::GetModuleHandleW(L"DynamicContainerLoot.dll") != nullptr;
		bool       probe = false;
		if (auto* chest = Forms::Resolve(kProbeChest); chest && chest->As<RE::TESObjectCONT>()) {
			auto* wanted = Forms::Resolve(kProbeList);
			chest->As<RE::TESObjectCONT>()->ForEachContainerObject([&](RE::ContainerObject& a_obj) {
				probe = probe || (wanted && a_obj.obj == wanted);
				return RE::BSContainer::ForEachResult::kContinue;
			});
		}
		logger::info("[Survey] DynamicContainerLoot.dll {}; TreasDraugrChestBoss {} LootDraugrEnchArmor25{}", dclLoaded ? "loaded" : "not loaded",
			probe ? "contains" : "does not contain", dclLoaded ? (probe ? " (merge already done)" : " (merge not done yet, or not needed)") : "");

		// 1. Tomb chests and the lists they reference directly ("wrappers").
		std::map<std::string, std::vector<RE::TESLevItem*>> wrappersByFamily;
		std::unordered_set<RE::TESLevItem*>                  wrappers;
		std::map<std::string, Leaves>                        byFamily;
		for (const auto& seed : kSeeds) {
			auto* cont = Forms::Resolve(seed.ref);
			auto* chest = cont ? cont->As<RE::TESObjectCONT>() : nullptr;
			if (!chest) {
				logger::info("[Chest] {} {}: not loaded", seed.family, seed.ref);
				continue;
			}
			std::string entries;
			chest->ForEachContainerObject([&](RE::ContainerObject& a_obj) {
				entries += std::format("{}{} x{}", entries.empty() ? "" : ", ", Forms::Label(a_obj.obj), a_obj.count);
				if (auto* list = a_obj.obj ? a_obj.obj->As<RE::TESLevItem>() : nullptr) {
					if (wrappers.insert(list).second) {
						wrappersByFamily[seed.family].push_back(list);
					}
				}
				return RE::BSContainer::ForEachResult::kContinue;
			});
			Leaves chestLeaves;  // a container is not a list: walk its entries one by one
			chest->ForEachContainerObject([&](RE::ContainerObject& a_obj) {
				std::unordered_set<RE::FormID> visited;
				Collect(a_obj.obj, chestLeaves, visited);
				return RE::BSContainer::ForEachResult::kContinue;
			});
			auto& family = byFamily[seed.family];
			for (const auto id : chestLeaves.seen) {
				family.Add(RE::TESForm::LookupByID(id));
			}
			logger::info("[Chest] {} {} (winner {}): {}", seed.family, Forms::Named(chest), Forms::Winner(chest), chestLeaves.Summary());
			logger::info("[Chest]   entries: {}", entries);
		}

		// 2. Each wrapper: its entries, and what each one can give.
		const auto users = FindUsers(wrappers);
		for (const auto& [family, lists] : wrappersByFamily) {
			for (auto* list : lists) {
				const auto leaves = LeavesOf(list);
				if (!leaves.HasGear()) {
					logger::info("[List] {} {} (winner {}): no gear ({})", family, Forms::Label(list), Forms::Winner(list), leaves.Summary());
					continue;
				}
				logger::info("[List] {} {} (winner {}, chance none {} %): {}", family, Forms::Label(list), Forms::Winner(list),
					list->chanceNone, leaves.Summary());
				for (const auto& entry : list->entries) {
					const auto sub = LeavesOf(entry.form);
					logger::info("[List]     lvl {:>3} x{} {} {} (from {}, winner {}): {}", entry.level, entry.count, TypeName(entry.form),
						Forms::Named(entry.form), entry.form && entry.form->GetFile(0) ? entry.form->GetFile(0)->GetFilename() : "-"sv,
						Forms::Winner(entry.form), sub.Summary());
				}
				for (const auto e : { Era::kModern, Era::kUnknown, Era::kNordicDLC2, Era::kTimeless }) {
					const auto i = static_cast<std::size_t>(e);
					if (!leaves.examples[i].empty()) {
						logger::info("[List]   {} e.g.: {}", kEraNames[i], [&] {
							std::string out;
							for (const auto& text : leaves.examples[i]) {
								out += out.empty() ? text : " ; " + text;
							}
							return out;
						}());
					}
				}
				if (auto it = users.find(list); it != users.end()) {
					logger::info("[List]   used by {} containers ({}), {} lists ({}), {} NPC death items", it->second.containers.size(),
						Sample(it->second.containers), it->second.lists.size(), Sample(it->second.lists), it->second.deathItems);
				}
			}
		}

		// 3. Totals per family of the seed chests.
		for (const auto& [family, leaves] : byFamily) {
			logger::info("[Totals] {}: {}", family, leaves.Summary());
			for (const auto e : { Era::kModern, Era::kUnknown }) {
				const auto i = static_cast<std::size_t>(e);
				for (const auto& text : leaves.examples[i]) {
					logger::info("[Totals]   {} {}", kEraNames[i], text);
				}
			}
		}
		}
	}

	void Run(const Config& a_config)
	{
		if (a_config.survey == Config::Survey::kOff) {
			return;
		}
		Prepare(a_config);
		Summary();
		if (a_config.survey == Config::Survey::kFull) {
			Full(a_config);
		}
	}
}
