// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/hid/hid.h"

namespace Movie {

enum class PlayMode
{
    None = 0,
    Recording,
    Playing
};

void HandlePadAndCircleStatus(Service::HID::PadState& pad_state, s16& circle_pad_x, s16& circle_pad_y);

void HandleTouchStatus(Service::HID::TouchDataEntry& touch_data);

void HandleAccelerometerStatus(Service::HID::AccelerometerDataEntry& accelerometer_data);

void HandleGyroscopeStatus(Service::HID::GyroscopeDataEntry& gyroscope_data);

// TODO: Handle C-Stick
}