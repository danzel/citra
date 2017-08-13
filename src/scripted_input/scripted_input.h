// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <string>

namespace ScriptedInput {

/// Initializes and registers the input device factories.
void Init();

void LoadScript(std::string script_name, bool close_at_end);

/// Deregisters the input device factories and shuts them down.
void Shutdown();

class ScriptedButtons;

bool IsInUse();

void NotifyFrameFinished();

} // namespace InputCommon
