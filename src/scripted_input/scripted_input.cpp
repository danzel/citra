// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <memory>
#include "scripted_input/script_runner.h"
#include "scripted_input/scripted_buttons.h"
#include "scripted_input/scripted_input.h"

namespace ScriptedInput {

static std::shared_ptr<ScriptedButtons> scripted_buttons;
// TODO: static std::shared_ptr<ScriptedAnalog> scripted_analog;
static ScriptRunner script_runner;

void Init() {
    scripted_buttons = std::make_shared<ScriptedInput::ScriptedButtons>();
    Input::RegisterFactory<Input::ButtonDevice>("scripted", scripted_buttons);
    script_runner.SetButtons(scripted_buttons);
}

void LoadScript(std::string script_name) {
    if (script_name.length() > 0) {
        script_runner.LoadScript(script_name);

        ScriptedButtons::OverrideControlsSettings();
    }
}

void Shutdown() {
    Input::UnregisterFactory<Input::ButtonDevice>("scripted");
}

bool IsInUse() {
    return script_runner.HasScript();
}

void NotifyFrameFinished() {
    script_runner.NotifyFrameFinished();
}

} // namespace ScriptedInput
