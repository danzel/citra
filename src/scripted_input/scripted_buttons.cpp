// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <atomic>
#include "scripted_input/scripted_buttons.h"

const int button_count = 15;

std::string button_name_to_index[] = {"a", "b", "x",     "y",      "up", "down", "left", "right",
                                      "l", "r", "start", "select", "zl", "zr",   "home"};

namespace ScriptedInput {

int IndexOfButton(const std::string& button) {
    for (int i = 0; i < button_count; i++) {
        if (button_name_to_index[i] == button) {
            return i;
        }
    }

    LOG_ERROR(ScriptedInput, "Don't know about a button named %s", button.c_str());
    return -1;
}

class ScriptedButton final : public Input::ButtonDevice {
    bool GetStatus() const override {
        return status.load();
    }

    friend class ScriptedButtons;

private:
    std::atomic<bool> status{false};
};

class ScriptedButtonList {
public:
    ScriptedButton* buttons[button_count];
};

ScriptedButtons::ScriptedButtons() : scripted_button_list{std::make_shared<ScriptedButtonList>()} {}

std::unique_ptr<Input::ButtonDevice> ScriptedButtons::Create(const Common::ParamPackage& params) {
    auto button_str = params.Get("button", "");

    std::unique_ptr<ScriptedButton> button = std::make_unique<ScriptedButton>();
    int index = IndexOfButton(button_str);
    if (index >= 0) {
        scripted_button_list.get()->buttons[index] = button.get();
    }

    return std::move(button);
}

void ScriptedButtons::SetActiveButtons(const std::vector<int>& buttons_active) {

    for (int i = 0; i < button_count; i++) {
        auto button = scripted_button_list.get()->buttons[i];
        if (button) {
            button->status.store(false);
        }
    }

    for (int i = 0; i < buttons_active.size(); i++) {
        auto button = scripted_button_list.get()->buttons[buttons_active[i]];
        if (button) {
            button->status.store(true);
        } else {
            LOG_ERROR(
                ScriptedInput,
                "Button %s isn't mapped but is scripted, it should have engine:scripted,button:%s",
                button_name_to_index[i], button_name_to_index[i]);
        }
    }
}
}