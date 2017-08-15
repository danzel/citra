// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/hid/hid.h"
#include "core/hle/service/ir/ir_rst.h"

namespace Movie {

void Init();

void Shutdown();

void HandlePadAndCircleStatus(Service::HID::PadState& pad_state, s16& circle_pad_x, s16& circle_pad_y);

void HandleTouchStatus(Service::HID::TouchDataEntry& touch_data);

void HandleAccelerometerStatus(Service::HID::AccelerometerDataEntry& accelerometer_data);

void HandleGyroscopeStatus(Service::HID::GyroscopeDataEntry& gyroscope_data);

void HandleCStick(Service::IR::PadState& pad_state, s16& c_stick_x, s16& c_stick_y);

// TODO: Handle C-Stick
}