// SPDX-License-Identifier: GPL-3.0-or-later
// TombLoot — Copyright (C) 2026 narziel. See COPYING.

#include "Config.h"

namespace
{
	const std::filesystem::path kFile{ "Data/SKSE/Plugins/TombLoot/TombLoot.json" };

	std::vector<std::string> Strings(const nlohmann::json& a_json, const char* a_key)
	{
		std::vector<std::string> out;
		if (auto it = a_json.find(a_key); it != a_json.end() && it->is_array()) {
			for (const auto& v : *it) {
				if (v.is_string()) {
					out.push_back(v.get<std::string>());
				}
			}
		}
		return out;
	}

	Config::Family ReadFamily(const nlohmann::json& a_json, const char* a_key)
	{
		Config::Family family;
		auto           it = a_json.find(a_key);
		if (it == a_json.end() || !it->is_object()) {
			return family;
		}
		family.lists = Strings(*it, "lists");
		family.containers = Strings(*it, "containers");
		family.autoChests = it->value("auto_chests", true);
		family.autoSublists = it->value("auto_sublists", true);
		family.eraKeywords = Strings(*it, "era_keywords");
		if (auto rep = it->find("replace"); rep != it->end() && rep->is_array()) {
			for (const auto& r : *rep) {
				if (!r.is_object() || !r.contains("to") || !r["to"].is_string()) {
					continue;
				}
				family.replace.push_back({ r["to"].get<std::string>(), Strings(r, "from") });
			}
		}
		return family;
	}
}

Config Config::Load()
{
	Config cfg;
	std::ifstream file(kFile);
	if (!file) {
		logger::warn("[Config] {} not found: nothing will be replaced", kFile.string());
		return cfg;
	}
	try {
		const auto json = nlohmann::json::parse(file, nullptr, true, true);  // comments allowed
		// "off", "summary" or "full" (true / false accepted: full / off).
		if (auto it = json.find("survey"); it != json.end()) {
			if (it->is_boolean()) {
				cfg.survey = it->get<bool>() ? Config::Survey::kFull : Config::Survey::kOff;
			} else if (it->is_string()) {
				const auto mode = it->get<std::string>();
				cfg.survey = mode == "off" ? Config::Survey::kOff : mode == "full" ? Config::Survey::kFull : Config::Survey::kSummary;
			}
		}
		cfg.ancientPlugins = Strings(json, "ancient_plugins");
		cfg.ancientItems = Strings(json, "ancient_items");
		if (auto it = json.find("created_lists"); it != json.end() && it->is_object()) {
			for (const auto& [name, value] : it->items()) {
				if (!value.is_object()) {
					continue;
				}
				Config::CreatedList list;
				if (auto items = value.find("items"); items != value.end() && items->is_array()) {
					for (const auto& item : *items) {
						if (item.is_string()) {
							list.items.push_back({ item.get<std::string>(), 1 });
						} else if (item.is_object() && item.contains("form") && item["form"].is_string()) {
							const auto level = item.value("level", 1);
							list.items.push_back({ item["form"].get<std::string>(), static_cast<std::uint16_t>(std::clamp(level, 1, 255)),
								Strings(item, "only_if"), Strings(item, "unless") });
						}
					}
				}
				if (auto fb = value.find("fallback"); fb != value.end() && fb->is_string()) {
					list.fallback = fb->get<std::string>();
				}
				list.chanceNone = static_cast<std::int8_t>(std::clamp(value.value("chance_none", 0), 0, 100));
				if (auto ex = value.find("extends"); ex != value.end() && ex->is_string()) {
					list.extends = ex->get<std::string>();
				}
				cfg.created.emplace(name, std::move(list));
			}
			// "extends": the parent's pieces come first, the parent resolved first (chains allowed, a
			// loop or an unknown parent is ignored with a warning).
			std::set<std::string>                                  done;
			std::function<void(const std::string&, std::uint32_t)> resolve = [&](const std::string& a_name, std::uint32_t a_depth) {
				auto& list = cfg.created.at(a_name);
				if (done.contains(a_name) || list.extends.empty()) {
					done.insert(a_name);
					return;
				}
				auto parent = cfg.created.find(list.extends);
				if (parent == cfg.created.end() || a_depth > 8) {
					logger::warn("[Config] @{} extends @{}: unknown parent or a loop; ignored", a_name, list.extends);
					done.insert(a_name);
					return;
				}
				resolve(parent->first, a_depth + 1);
				list.items.insert(list.items.begin(), parent->second.items.begin(), parent->second.items.end());
				if (list.fallback.empty()) {
					list.fallback = parent->second.fallback;
				}
				done.insert(a_name);
			};
			for (const auto& [name, list] : cfg.created) {
				resolve(name, 0);
			}
		}
		for (const char* name : { "nordic", "solstheim", "dwemer" }) {
			if (json.contains(name)) {
				cfg.families.emplace_back(name, ReadFamily(json, name));
			}
		}
	} catch (const std::exception& e) {
		logger::error("[Config] {} is invalid ({}): nothing will be replaced", kFile.string(), e.what());
		return Config{};
	}
	std::string families;
	for (const auto& [name, family] : cfg.families) {
		families += std::format("{}{} {} lists / {} chests / {} replacements", families.empty() ? "" : ", ", name, family.lists.size(),
			family.containers.size(), family.replace.size());
	}
	constexpr const char* kSurveyNames[] = { "off", "summary", "full" };
	logger::info("[Config] read: survey {}, {} ancient plugins, {} created lists; {}", kSurveyNames[static_cast<int>(cfg.survey)], cfg.ancientPlugins.size(),
		cfg.created.size(), families.empty() ? "no family"s : families);
	return cfg;
}
