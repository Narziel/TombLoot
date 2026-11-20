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
		if (auto it = json.find("survey"); it != json.end() && it->is_boolean()) {
			cfg.survey = it->get<bool>();
		}
		cfg.ancientPlugins = Strings(json, "ancient_plugins");
		cfg.nordic = ReadFamily(json, "nordic");
	} catch (const std::exception& e) {
		logger::error("[Config] {} is invalid ({}): nothing will be replaced", kFile.string(), e.what());
		return Config{};
	}
	logger::info("[Config] read: survey {}, {} ancient plugins, nordic {} lists / {} replacements", cfg.survey, cfg.ancientPlugins.size(),
		cfg.nordic.lists.size(), cfg.nordic.replace.size());
	return cfg;
}
