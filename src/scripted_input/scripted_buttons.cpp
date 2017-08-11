// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "scripted_input/scripted_buttons.h"

std::string button_name_to_index[] = {
    "a",
    "b",
    "x",
    "y",
    "up",
    "down",
    "left",
    "right",
    "l",
    "r",
    "start",
    "select",
    "zl",
    "zr",
    "home"
};

int IndexOfButton(const std::string& button) {
    for (int i = 0; i < 15; i++) {
        if (button_name_to_index[i] == button) {
            return i;
        }
    }

    //TODO: Log an error
    return -1; //home
}

int frame = 0;

namespace ScriptedInput {

class ScriptedButton final : public Input::ButtonDevice {
    bool GetStatus() const override {
        return status.load();
    }

    friend class ScriptedButtons;
private:
    std::atomic<bool> status{false};
};

class ScriptedButtonList {
    //TODO: Do i need to memset(0) buttons?
public:
    ScriptedButton* buttons[15];
};


ScriptedButtons::ScriptedButtons() : scripted_button_list{std::make_shared<ScriptedButtonList>()} {}

std::unique_ptr<Input::ButtonDevice> ScriptedButtons::Create(const Common::ParamPackage& params) {
    auto button_str = params.Get("button", "");

    std::unique_ptr<ScriptedButton> button = std::make_unique<ScriptedButton>();
    int index = IndexOfButton(button_str);
    if (index >= 0) {
        scripted_button_list.get()->buttons[index] = button.get();
        is_in_use = true;
    }

    return std::move(button);
}

bool ScriptedButtons::IsInUse() {
    return is_in_use;
}

void ScriptedButtons::NotifyFrameFinished() {
    frame++;
    if (frame >= 10) {
        printf("TOGGLING\r\n");
        frame = 0;

        auto button = scripted_button_list.get()->buttons[0];
        button->status.store(!button->GetStatus());
    }
}
}