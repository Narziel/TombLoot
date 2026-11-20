// SPDX-License-Identifier: GPL-3.0-or-later
// TombLoot — Copyright (C) 2026 narziel. See COPYING.

#include "Config.h"
#include "Survey.h"
#include "Swap.h"

namespace
{
	template <class F>
	void Step(const char* a_name, F&& a_fn)
	{
		logger::info("[Startup] {}: start", a_name);
		const auto start = std::chrono::steady_clock::now();
		a_fn();
		const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
		logger::info("[Startup] {}: {} ms", a_name, ms);
	}

	void OnMessage(SKSE::MessagingInterface::Message* a_msg)
	{
		switch (a_msg->type) {
		case SKSE::MessagingInterface::kDataLoaded:
			{
				Config config;
				Step("Config", [&] { config = Config::Load(); });
				Step("Swap", [&] { Swap::Apply(config); });
				if (config.survey) {
					Step("Survey", [&] { Survey::Run(config); });
				}
			}
			break;
		case SKSE::MessagingInterface::kPostLoadGame:
		case SKSE::MessagingInterface::kNewGame:
			// The config is read again, so a change applies without restarting the game.
			Step("Swap (game load)", [] { Swap::Apply(Config::Load()); });
			break;
		default:
			break;
		}
	}
}

SKSE_PLUGIN_LOAD(const SKSE::LoadInterface* a_skse)
{
	SKSE::Init(a_skse, { .logLevel = spdlog::level::info });

	logger::info("Loading {} v{} (runtime {})", SKSE::GetPluginName(), SKSE::GetPluginVersion(), REL::Module::get().version());

	if (!SKSE::GetMessagingInterface()->RegisterListener(OnMessage)) {
		logger::error("Cannot listen to SKSE messages");
		return false;
	}

	logger::info("Plugin loaded");
	return true;
}
