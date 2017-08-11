// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <string>

namespace ScriptedInput {

/// Initializes and registers all built-in input device factories.
void Init();

/// Unresisters all build-in input device factories and shut them down.
void Shutdown();

class ScriptedButtons;

bool IsInUse();

void NotifyFrameFinished();

} // namespace InputCommon
