// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <memory>
#include "scripted_input/scripted_buttons.h"
#include "scripted_input/scripted_input.h"


namespace ScriptedInput {

static std::shared_ptr<ScriptedButtons> scripted_buttons;
//TODO: static std::shared_ptr<ScriptedAnalog> scripted_analog;

void Init() {
    scripted_buttons = std::make_shared<ScriptedInput::ScriptedButtons>();
    Input::RegisterFactory<Input::ButtonDevice>("scripted", scripted_buttons);
}

void Shutdown() {
    Input::UnregisterFactory<Input::ButtonDevice>("scripted");
}

ScriptedButtons* GetScriptedButtons() {
    return scripted_buttons.get();
}

} // namespace ScriptedInput
