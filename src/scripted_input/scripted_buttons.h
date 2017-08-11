// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <atomic>
#include <memory>
#include "core/frontend/input.h"

namespace ScriptedInput {

class ScriptedButtonList;

/**
* A button device factory that returns inputs from a script file
*/
class ScriptedButtons final : public Input::Factory<Input::ButtonDevice> {
public:
    ScriptedButtons();

    /**
    * Creates a button device
    * @param params unused
    */
    std::unique_ptr<Input::ButtonDevice> Create(const Common::ParamPackage& params) override;

private:
    std::shared_ptr<ScriptedButtonList> scripted_button_list;
};

} // namespace InputCommon
