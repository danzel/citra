// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include "core/frontend/input.h"

namespace ScriptedInput {

int IndexOfButton(const std::string& button);

class ScriptedButtonList;

/**
* A button device factory that returns inputs from a script file
*/
class ScriptedButtons final : public Input::Factory<Input::ButtonDevice> {
public:
    /**
    * Overrides the Settings for buttons so they are controlled by us
    */
    static void OverrideControlsSettings();

    ScriptedButtons();

    /**
    * Creates a button device
    * @param params unused
    */
    std::unique_ptr<Input::ButtonDevice> Create(const Common::ParamPackage& params) override;

    void SetActiveButtons(const std::vector<int>& buttons_active);

private:
    std::shared_ptr<ScriptedButtonList> scripted_button_list;
};

} // namespace InputCommon
