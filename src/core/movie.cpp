// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <vector>
#include "common/bit_field.h"
#include "common/file_util.h"
#include "common/logging/log.h"
#include "core/movie.h"

namespace Movie {

enum class ControllerStateType : u8 { PadAndCircle, Touch, Accelerometer, Gyroscope, CStick };

#pragma pack(push, 1)
struct ControllerState {
    ControllerStateType type;

    union {
        struct {
            union {
                u32 hex;

                BitField<0, 1, u32> a;
                BitField<1, 1, u32> b;
                BitField<2, 1, u32> select;
                BitField<3, 1, u32> start;
                BitField<4, 1, u32> right;
                BitField<5, 1, u32> left;
                BitField<6, 1, u32> up;
                BitField<7, 1, u32> down;
                BitField<8, 1, u32> r;
                BitField<9, 1, u32> l;
                BitField<10, 1, u32> x;
                BitField<11, 1, u32> y;
                BitField<12, 1, u32> circle_right;
                BitField<13, 1, u32> circle_left;
                BitField<14, 1, u32> circle_up;
                BitField<15, 1, u32> circle_down;

                //MAX_CIRCLEPAD_POS in hid.cpp fits in one byte
                BitField<16, 8, u32> circle_pad_x;
                BitField<24, 8, u32> circle_pad_y;
            };
        } PadAndCircle;

        struct {
            u16 x;
            u16 y;
            bool valid;
        } Touch;

        struct {
            s16 x;
            s16 y;
            s16 z;
        } Accelerometer;

        struct {
            s16 x;
            s16 y;
            s16 z;
        } Gyroscope;

        struct {
            //MAX_CSTICK_RADIUS in ir_rst.cpp fits in one byte
            u8 x;
            u8 y;
            bool zl;
            bool zr;
        } CStick;
    };
};
static_assert(sizeof(ControllerState) == 7, "ControllerState should be 7 bytes");

PlayMode play_mode = PlayMode::None;
std::vector<u8> temp_input;
int current_byte = 0;

bool IsPlayingInput() {
    return play_mode == PlayMode::Playing;
}
bool IsRecordingInput() {
    return play_mode == PlayMode::Recording;
}

void CheckInputEnd() {
    if (current_byte + sizeof(ControllerState) > temp_input.size()) {
        LOG_INFO(Movie, "Playback finished");
        play_mode = PlayMode::None;
    }
}

void Play(Service::HID::PadState& pad_state, s16& circle_pad_x, s16& circle_pad_y) {
    ControllerState s;
    memcpy(&s, &temp_input[current_byte], sizeof(ControllerState));
    current_byte += sizeof(ControllerState);

    if (s.type != ControllerStateType::PadAndCircle) {
        LOG_ERROR(Movie, "Expected to read type %d, but found %d. Your playback will be out of sync", ControllerStateType::PadAndCircle, s.type);
        return;
    }

    pad_state.a.Assign(s.PadAndCircle.a);
    pad_state.b.Assign(s.PadAndCircle.b);
    pad_state.select.Assign(s.PadAndCircle.select);
    pad_state.start.Assign(s.PadAndCircle.start);
    pad_state.right.Assign(s.PadAndCircle.right);
    pad_state.left.Assign(s.PadAndCircle.left);
    pad_state.up.Assign(s.PadAndCircle.up);
    pad_state.down.Assign(s.PadAndCircle.down);
    pad_state.r.Assign(s.PadAndCircle.r);
    pad_state.l.Assign(s.PadAndCircle.l);
    pad_state.x.Assign(s.PadAndCircle.x);
    pad_state.y.Assign(s.PadAndCircle.y);
    pad_state.circle_right.Assign(s.PadAndCircle.circle_right);
    pad_state.circle_left.Assign(s.PadAndCircle.circle_left);
    pad_state.circle_up.Assign(s.PadAndCircle.circle_up);
    pad_state.circle_down.Assign(s.PadAndCircle.circle_down);

    circle_pad_x = s.PadAndCircle.circle_pad_x;
    circle_pad_y = s.PadAndCircle.circle_pad_y;
}

void Play(Service::HID::TouchDataEntry &touch_data) {
    ControllerState s;
    memcpy(&s, &temp_input[current_byte], sizeof(ControllerState));
    current_byte += sizeof(ControllerState);

    if (s.type != ControllerStateType::Touch) {
        LOG_ERROR(Movie, "Expected to read type %d, but found %d. Your playback will be out of sync", ControllerStateType::Touch, s.type);
        return;
    }

    touch_data.x = s.Touch.x;
    touch_data.y = s.Touch.y;
    touch_data.valid.Assign(s.Touch.valid);
}

void Play(Service::HID::AccelerometerDataEntry &accelerometer_data) {
    ControllerState s;
    memcpy(&s, &temp_input[current_byte], sizeof(ControllerState));
    current_byte += sizeof(ControllerState);

    if (s.type != ControllerStateType::Accelerometer) {
        LOG_ERROR(Movie, "Expected to read type %d, but found %d. Your playback will be out of sync", ControllerStateType::Accelerometer, s.type);
        return;
    }

    accelerometer_data.x = s.Accelerometer.x;
    accelerometer_data.y = s.Accelerometer.y;
    accelerometer_data.z = s.Accelerometer.z;
}

void Play(Service::HID::GyroscopeDataEntry &gyroscope_data) {
    ControllerState s;
    memcpy(&s, &temp_input[current_byte], sizeof(ControllerState));
    current_byte += sizeof(ControllerState);

    if (s.type != ControllerStateType::Gyroscope) {
        LOG_ERROR(Movie, "Expected to read type %d, but found %d. Your playback will be out of sync", ControllerStateType::Gyroscope, s.type);
        return;
    }

    gyroscope_data.x = s.Gyroscope.x;
    gyroscope_data.y = s.Gyroscope.y;
    gyroscope_data.z = s.Gyroscope.z;
}

void Play(Service::IR::PadState& pad_state, s16& c_stick_x, s16& c_stick_y) {
    ControllerState s;
    memcpy(&s, &temp_input[current_byte], sizeof(ControllerState));
    current_byte += sizeof(ControllerState);

    if (s.type != ControllerStateType::CStick) {
        LOG_ERROR(Movie, "Expected to read type %d, but found %d. Your playback will be out of sync", ControllerStateType::CStick, s.type);
        return;
    }

    c_stick_x = s.CStick.x;
    c_stick_y = s.CStick.y;
    pad_state.zl.Assign(s.CStick.zl);
    pad_state.zr.Assign(s.CStick.zr);
}

void Record(const Service::HID::PadState& pad_state, const s16& circle_pad_x, const s16& circle_pad_y) {
    ControllerState s;
    s.type = ControllerStateType::PadAndCircle;

    s.PadAndCircle.a.Assign(pad_state.a);
    s.PadAndCircle.b.Assign(pad_state.b);
    s.PadAndCircle.select.Assign(pad_state.select);
    s.PadAndCircle.start.Assign(pad_state.start);
    s.PadAndCircle.right.Assign(pad_state.right);
    s.PadAndCircle.left.Assign(pad_state.left);
    s.PadAndCircle.up.Assign(pad_state.up);
    s.PadAndCircle.down.Assign(pad_state.down);
    s.PadAndCircle.r.Assign(pad_state.r);
    s.PadAndCircle.l.Assign(pad_state.l);
    s.PadAndCircle.x.Assign(pad_state.x);
    s.PadAndCircle.y.Assign(pad_state.y);
    s.PadAndCircle.circle_right.Assign(pad_state.circle_right);
    s.PadAndCircle.circle_left.Assign(pad_state.circle_left);
    s.PadAndCircle.circle_up.Assign(pad_state.circle_up);
    s.PadAndCircle.circle_down.Assign(pad_state.circle_down);

    s.PadAndCircle.circle_pad_x.Assign(circle_pad_x);
    s.PadAndCircle.circle_pad_y.Assign(circle_pad_y);

    temp_input.resize(current_byte + sizeof(ControllerState));
    memcpy(&temp_input[current_byte], &s, sizeof(ControllerState));
    current_byte += sizeof(ControllerState);
}

void Record(const Service::HID::TouchDataEntry &touch_data) {
    ControllerState s;
    s.type = ControllerStateType::Touch;

    s.Touch.x = touch_data.x;
    s.Touch.y = touch_data.y;
    s.Touch.valid = touch_data.valid;

    temp_input.resize(current_byte + sizeof(ControllerState));
    memcpy(&temp_input[current_byte], &s, sizeof(ControllerState));
    current_byte += sizeof(ControllerState);
}

void Record(const Service::HID::AccelerometerDataEntry &accelerometer_data) {
    ControllerState s;
    s.type = ControllerStateType::Accelerometer;

    s.Accelerometer.x = accelerometer_data.x;
    s.Accelerometer.y = accelerometer_data.y;
    s.Accelerometer.z = accelerometer_data.z;

    temp_input.resize(current_byte + sizeof(ControllerState));
    memcpy(&temp_input[current_byte], &s, sizeof(ControllerState));
    current_byte += sizeof(ControllerState);
}

void Record(const Service::HID::GyroscopeDataEntry &gyroscope_data) {
    ControllerState s;
    s.type = ControllerStateType::Gyroscope;

    s.Gyroscope.x = gyroscope_data.x;
    s.Gyroscope.y = gyroscope_data.y;
    s.Gyroscope.z = gyroscope_data.z;

    temp_input.resize(current_byte + sizeof(ControllerState));
    memcpy(&temp_input[current_byte], &s, sizeof(ControllerState));
    current_byte += sizeof(ControllerState);
}

void Record(const Service::IR::PadState& pad_state, const s16& c_stick_x, const s16& c_stick_y) {
    ControllerState s;
    s.type = ControllerStateType::CStick;

    s.CStick.x = static_cast<u8>(c_stick_x);
    s.CStick.y = static_cast<u8>(c_stick_y);
    s.CStick.zl = pad_state.zl;
    s.CStick.zr = pad_state.zr;

    temp_input.resize(current_byte + sizeof(ControllerState));
    memcpy(&temp_input[current_byte], &s, sizeof(ControllerState));
    current_byte += sizeof(ControllerState);
}

void Init() {
    if (!Settings::values.movie_play.empty()) {
        LOG_INFO(Movie, "Loading Movie for playback");
        FileUtil::IOFile save_record(Settings::values.movie_play, "rb");
        if (save_record.IsGood()) {
            play_mode = PlayMode::Playing;
            temp_input.resize(save_record.GetSize());
            save_record.ReadArray(temp_input.data(), temp_input.size());
            current_byte = 0;
        } else {
            LOG_ERROR(Movie, "Failed to playback movie: Unable to open '%'", Settings::values.movie_play.c_str());
        }
    }

    if (!Settings::values.movie_record.empty()) {
        LOG_INFO(Movie, "Enabling Movie recording");
        play_mode = PlayMode::Recording;
    }
}

void Shutdown() {
    if (play_mode == PlayMode::Recording) {
        LOG_INFO(Movie, "Saving movie");
        FileUtil::IOFile save_record(Settings::values.movie_record, "wb");

        save_record.WriteBytes(temp_input.data(), temp_input.size());
        if (!save_record.IsGood()) {
            LOG_ERROR(Movie, "Error saving movie");
        }
    }
}

void HandlePadAndCircleStatus(Service::HID::PadState& pad_state, s16& circle_pad_x, s16& circle_pad_y) {
    if (IsPlayingInput()) {
        Play(pad_state, circle_pad_x, circle_pad_y);
        CheckInputEnd();
    } else if (IsRecordingInput()) {
        Record(pad_state, circle_pad_x, circle_pad_y);
    }
}

void HandleTouchStatus(Service::HID::TouchDataEntry& touch_data) {
    if (IsPlayingInput()) {
        Play(touch_data);
        CheckInputEnd();
    } else if (IsRecordingInput()) {
        Record(touch_data);
    }
}

void HandleAccelerometerStatus(Service::HID::AccelerometerDataEntry& accelerometer_data) {
    if (IsPlayingInput()) {
        Play(accelerometer_data);
        CheckInputEnd();
    } else if (IsRecordingInput()) {
        Record(accelerometer_data);
    }
}

void HandleGyroscopeStatus(Service::HID::GyroscopeDataEntry& gyroscope_data) {
    if (IsPlayingInput()) {
        Play(gyroscope_data);
        CheckInputEnd();
    } else if (IsRecordingInput()) {
        Record(gyroscope_data);
    }
}

void HandleCStick(Service::IR::PadState& pad_state, s16& c_stick_x, s16& c_stick_y) {
    if (IsPlayingInput()) {
        Play(pad_state, c_stick_x, c_stick_y);
        CheckInputEnd();
    } else if (IsRecordingInput()) {
        Record(pad_state, c_stick_x, c_stick_y);
    }
}
}
