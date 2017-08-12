// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/frontend/input.h"
#include "scripted_input/scripted_buttons.h"

namespace ScriptedInput {

enum class ScriptItemType { Undefined, Run, Screenshot };

class ScriptItem {
public:
    static ScriptItem Screenshot;

    ScriptItemType type{ScriptItemType::Undefined};
    int frames{0};
    std::vector<int> buttons_active;
};

/**
* A button device factory that returns inputs from a script file
*/
class ScriptRunner final {
public:
    void SetButtons(std::shared_ptr<ScriptedButtons> buttons);
    void LoadScript(std::string script_name);
    bool HasScript() const;

    void NotifyFrameFinished();

private:
    std::vector<ScriptItem> script;
    int frame_number{0};
    int script_index{0};
    int script_frame{0};

    std::shared_ptr<ScriptedButtons> scripted_buttons;

    void SaveScreenshot();
};

} // namespace InputCommon
