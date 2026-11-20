// SPDX-License-Identifier: GPL-3.0-or-later
// TombLoot — Copyright (C) 2026 narziel. See COPYING.

#include "Survey.h"

namespace
{
	void OnMessage(SKSE::MessagingInterface::Message* a_msg)
	{
		if (a_msg->type != SKSE::MessagingInterface::kDataLoaded) {
			return;
		}
		logger::info("[Startup] Survey: start");
		const auto start = std::chrono::steady_clock::now();
		Survey::Run();
		const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
		logger::info("[Startup] Survey: {} ms (read-only, nothing changed in the game)", ms);
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
