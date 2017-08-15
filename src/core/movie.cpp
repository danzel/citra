// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cstring>
#include <string>
#include <vector>
#include <cryptopp/hex.h>
#include "common/bit_field.h"
#include "common/common_types.h"
#include "common/file_util.h"
#include "common/logging/log.h"
#include "common/scm_rev.h"
#include "core/core.h"
#include "core/hle/service/hid/hid.h"
#include "core/hle/service/ir/extra_hid.h"
#include "core/hle/service/ir/ir_rst.h"
#include "core/movie.h"

namespace Movie {

enum class PlayMode { None, Recording, Playing };

enum class ControllerStateType : u8 { PadAndCircle, Touch, Accelerometer, Gyroscope, CStick,
                                      CirclePad };

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

                // MAX_CIRCLEPAD_POS in hid.cpp fits in one byte
                BitField<16, 8, u32> circle_pad_x;
                BitField<24, 8, u32> circle_pad_y;
            };
        } pad_and_circle;

        struct {
            u16 x;
            u16 y;
            u8 valid;
        } touch;

        struct {
            s16 x;
            s16 y;
            s16 z;
        } accelerometer;

        struct {
            s16 x;
            s16 y;
            s16 z;
        } gyroscope;

        struct {
            // MAX_CSTICK_RADIUS in ir_rst.cpp fits in one byte
            u8 x;
            u8 y;
            bool zl;
            bool zr;
        } c_stick;

        struct {
            union {
                u32 hex;

                BitField<0, 5, u8> battery_level;
                BitField<5, 1, u8> zl_not_held;
                BitField<6, 1, u8> zr_not_held;
                BitField<7, 1, u8> r_not_held;
                BitField<8, 12, u32_le> c_stick_x;
                BitField<20, 12, u32_le> c_stick_y;
            };
        } circle_pad;
    };
};
static_assert(sizeof(ControllerState) == 7, "ControllerState should be 7 bytes");
#pragma pack(pop)

#pragma pack(push, 1)
struct CTMHeader {
    u8 filetype[4];  // Unique Identifier (always "CTM"0x1B)
    u64 program_id;  // Also called title_id
    u8 revision[20]; // Git hash

    u8 reserved[224]; // Make heading 256 bytes, just because we can
};
static_assert(sizeof(CTMHeader) == 256, "CTMHeader should be 256 bytes");
#pragma pack(pop)

static PlayMode play_mode = PlayMode::None;
static std::vector<u8> temp_input;
static size_t current_byte = 0;

static bool IsPlayingInput() {
    return play_mode == PlayMode::Playing;
}
static bool IsRecordingInput() {
    return play_mode == PlayMode::Recording;
}

static void CheckInputEnd() {
    if (current_byte + sizeof(ControllerState) > temp_input.size()) {
        LOG_INFO(Movie, "Playback finished");
        play_mode = PlayMode::None;
    }
}

static void Play(Service::HID::PadState& pad_state, s16& circle_pad_x, s16& circle_pad_y) {
    ControllerState s;
    memcpy(&s, &temp_input[current_byte], sizeof(ControllerState));
    current_byte += sizeof(ControllerState);

    if (s.type != ControllerStateType::PadAndCircle) {
        LOG_ERROR(Movie,
                  "Expected to read type %d, but found %d. Your playback will be out of sync",
                  ControllerStateType::PadAndCircle, s.type);
        return;
    }

    pad_state.a.Assign(s.pad_and_circle.a);
    pad_state.b.Assign(s.pad_and_circle.b);
    pad_state.select.Assign(s.pad_and_circle.select);
    pad_state.start.Assign(s.pad_and_circle.start);
    pad_state.right.Assign(s.pad_and_circle.right);
    pad_state.left.Assign(s.pad_and_circle.left);
    pad_state.up.Assign(s.pad_and_circle.up);
    pad_state.down.Assign(s.pad_and_circle.down);
    pad_state.r.Assign(s.pad_and_circle.r);
    pad_state.l.Assign(s.pad_and_circle.l);
    pad_state.x.Assign(s.pad_and_circle.x);
    pad_state.y.Assign(s.pad_and_circle.y);
    pad_state.circle_right.Assign(s.pad_and_circle.circle_right);
    pad_state.circle_left.Assign(s.pad_and_circle.circle_left);
    pad_state.circle_up.Assign(s.pad_and_circle.circle_up);
    pad_state.circle_down.Assign(s.pad_and_circle.circle_down);

    circle_pad_x = static_cast<s16>(s.pad_and_circle.circle_pad_x);
    circle_pad_y = static_cast<s16>(s.pad_and_circle.circle_pad_y);
}

static void Play(Service::HID::TouchDataEntry& touch_data) {
    ControllerState s;
    memcpy(&s, &temp_input[current_byte], sizeof(ControllerState));
    current_byte += sizeof(ControllerState);

    if (s.type != ControllerStateType::Touch) {
        LOG_ERROR(Movie,
                  "Expected to read type %d, but found %d. Your playback will be out of sync",
                  ControllerStateType::Touch, s.type);
        return;
    }

    touch_data.x = s.touch.x;
    touch_data.y = s.touch.y;
    touch_data.valid.Assign(s.touch.valid);
}

static void Play(Service::HID::AccelerometerDataEntry& accelerometer_data) {
    ControllerState s;
    memcpy(&s, &temp_input[current_byte], sizeof(ControllerState));
    current_byte += sizeof(ControllerState);

    if (s.type != ControllerStateType::Accelerometer) {
        LOG_ERROR(Movie,
                  "Expected to read type %d, but found %d. Your playback will be out of sync",
                  ControllerStateType::Accelerometer, s.type);
        return;
    }

    accelerometer_data.x = s.accelerometer.x;
    accelerometer_data.y = s.accelerometer.y;
    accelerometer_data.z = s.accelerometer.z;
}

static void Play(Service::HID::GyroscopeDataEntry& gyroscope_data) {
    ControllerState s;
    memcpy(&s, &temp_input[current_byte], sizeof(ControllerState));
    current_byte += sizeof(ControllerState);

    if (s.type != ControllerStateType::Gyroscope) {
        LOG_ERROR(Movie,
                  "Expected to read type %d, but found %d. Your playback will be out of sync",
                  ControllerStateType::Gyroscope, s.type);
        return;
    }

    gyroscope_data.x = s.gyroscope.x;
    gyroscope_data.y = s.gyroscope.y;
    gyroscope_data.z = s.gyroscope.z;
}

static void Play(Service::IR::PadState& pad_state, s16& c_stick_x, s16& c_stick_y) {
    ControllerState s;
    memcpy(&s, &temp_input[current_byte], sizeof(ControllerState));
    current_byte += sizeof(ControllerState);

    if (s.type != ControllerStateType::CStick) {
        LOG_ERROR(Movie,
                  "Expected to read type %d, but found %d. Your playback will be out of sync",
                  ControllerStateType::CStick, s.type);
        return;
    }

    c_stick_x = s.c_stick.x;
    c_stick_y = s.c_stick.y;
    pad_state.zl.Assign(s.c_stick.zl);
    pad_state.zr.Assign(s.c_stick.zr);
}

static void Play(Service::IR::CirclePadResponse& circle_pad) {
    ControllerState s;
    memcpy(&s, &temp_input[current_byte], sizeof(ControllerState));
    current_byte += sizeof(ControllerState);

    if (s.type != ControllerStateType::CirclePad) {
        LOG_ERROR(Movie,
            "Expected to read type %d, but found %d. Your playback will be out of sync",
            ControllerStateType::CirclePad, s.type);
        return;
    }

    circle_pad.buttons.battery_level.Assign(s.circle_pad.battery_level);
    circle_pad.c_stick.c_stick_x.Assign(s.circle_pad.c_stick_x);
    circle_pad.c_stick.c_stick_y.Assign(s.circle_pad.c_stick_y);
    circle_pad.buttons.r_not_held.Assign(s.circle_pad.r_not_held);
    circle_pad.buttons.zl_not_held.Assign(s.circle_pad.zl_not_held);
    circle_pad.buttons.zr_not_held.Assign(s.circle_pad.zr_not_held);
}

static void Record(const Service::HID::PadState& pad_state, const s16& circle_pad_x,
            const s16& circle_pad_y) {
    ControllerState s;
    s.type = ControllerStateType::PadAndCircle;

    s.pad_and_circle.a.Assign(pad_state.a);
    s.pad_and_circle.b.Assign(pad_state.b);
    s.pad_and_circle.select.Assign(pad_state.select);
    s.pad_and_circle.start.Assign(pad_state.start);
    s.pad_and_circle.right.Assign(pad_state.right);
    s.pad_and_circle.left.Assign(pad_state.left);
    s.pad_and_circle.up.Assign(pad_state.up);
    s.pad_and_circle.down.Assign(pad_state.down);
    s.pad_and_circle.r.Assign(pad_state.r);
    s.pad_and_circle.l.Assign(pad_state.l);
    s.pad_and_circle.x.Assign(pad_state.x);
    s.pad_and_circle.y.Assign(pad_state.y);
    s.pad_and_circle.circle_right.Assign(pad_state.circle_right);
    s.pad_and_circle.circle_left.Assign(pad_state.circle_left);
    s.pad_and_circle.circle_up.Assign(pad_state.circle_up);
    s.pad_and_circle.circle_down.Assign(pad_state.circle_down);

    s.pad_and_circle.circle_pad_x.Assign(circle_pad_x);
    s.pad_and_circle.circle_pad_y.Assign(circle_pad_y);

    temp_input.resize(current_byte + sizeof(ControllerState));
    memcpy(&temp_input[current_byte], &s, sizeof(ControllerState));
    current_byte += sizeof(ControllerState);
}

static void Record(const Service::HID::TouchDataEntry& touch_data) {
    ControllerState s;
    s.type = ControllerStateType::Touch;

    s.touch.x = touch_data.x;
    s.touch.y = touch_data.y;
    s.touch.valid = static_cast<u8>(touch_data.valid);

    temp_input.resize(current_byte + sizeof(ControllerState));
    memcpy(&temp_input[current_byte], &s, sizeof(ControllerState));
    current_byte += sizeof(ControllerState);
}

static void Record(const Service::HID::AccelerometerDataEntry& accelerometer_data) {
    ControllerState s;
    s.type = ControllerStateType::Accelerometer;

    s.accelerometer.x = accelerometer_data.x;
    s.accelerometer.y = accelerometer_data.y;
    s.accelerometer.z = accelerometer_data.z;

    temp_input.resize(current_byte + sizeof(ControllerState));
    memcpy(&temp_input[current_byte], &s, sizeof(ControllerState));
    current_byte += sizeof(ControllerState);
}

static void Record(const Service::HID::GyroscopeDataEntry& gyroscope_data) {
    ControllerState s;
    s.type = ControllerStateType::Gyroscope;

    s.gyroscope.x = gyroscope_data.x;
    s.gyroscope.y = gyroscope_data.y;
    s.gyroscope.z = gyroscope_data.z;

    temp_input.resize(current_byte + sizeof(ControllerState));
    memcpy(&temp_input[current_byte], &s, sizeof(ControllerState));
    current_byte += sizeof(ControllerState);
}

static void Record(const Service::IR::PadState& pad_state, const s16& c_stick_x, const s16& c_stick_y) {
    ControllerState s;
    s.type = ControllerStateType::CStick;

    s.c_stick.x = static_cast<u8>(c_stick_x);
    s.c_stick.y = static_cast<u8>(c_stick_y);
    s.c_stick.zl = pad_state.zl;
    s.c_stick.zr = pad_state.zr;

    temp_input.resize(current_byte + sizeof(ControllerState));
    memcpy(&temp_input[current_byte], &s, sizeof(ControllerState));
    current_byte += sizeof(ControllerState);
}

static void Record(const Service::IR::CirclePadResponse& circle_pad) {
    ControllerState s;
    s.type = ControllerStateType::CirclePad;

    s.circle_pad.battery_level.Assign(circle_pad.buttons.battery_level);
    s.circle_pad.c_stick_x.Assign(circle_pad.c_stick.c_stick_x);
    s.circle_pad.c_stick_y.Assign(circle_pad.c_stick.c_stick_y);
    s.circle_pad.r_not_held.Assign(circle_pad.buttons.r_not_held);
    s.circle_pad.zl_not_held.Assign(circle_pad.buttons.zl_not_held);
    s.circle_pad.zr_not_held.Assign(circle_pad.buttons.zr_not_held);

    temp_input.resize(current_byte + sizeof(ControllerState));
    memcpy(&temp_input[current_byte], &s, sizeof(ControllerState));
    current_byte += sizeof(ControllerState);
}

static bool ValidateHeader(const CTMHeader& header) {
    if (header.filetype[0] != 'C' || header.filetype[1] != 'T' || header.filetype[2] != 'M' ||
        header.filetype[3] != 0x1B) {
        LOG_ERROR(Movie, "Playback file does not have valid header");
        return false;
    }

    std::string revision;
    CryptoPP::StringSource ss(header.revision, sizeof(header.revision), true,
                              new CryptoPP::HexEncoder(new CryptoPP::StringSink(revision)));
    if (revision != Common::g_scm_rev) {
        LOG_WARNING(Movie,
                    "This movie was created on a different version of Citra, playback may desync");
    }

    u64 program_id;
    Core::System::GetInstance().GetAppLoader().ReadProgramId(program_id);
    if (program_id != header.program_id) {
        LOG_WARNING(Movie, "This movie was recorded using a ROM with a different program id");
    }

    return true;
}

void Init() {
    if (!Settings::values.movie_play.empty()) {
        LOG_INFO(Movie, "Loading Movie for playback");
        FileUtil::IOFile save_record(Settings::values.movie_play, "rb");
        u64 size = save_record.GetSize();
        save_record.Seek(0, SEEK_SET);

        if (save_record.IsGood() && size > sizeof(CTMHeader)) {
            CTMHeader header;
            save_record.ReadArray(&header, 1);
            if (ValidateHeader(header)) {
                play_mode = PlayMode::Playing;
                temp_input.resize(size - sizeof(CTMHeader));
                save_record.ReadArray(temp_input.data(), temp_input.size());
                current_byte = 0;
            }
        } else {
            LOG_ERROR(Movie, "Failed to playback movie: Unable to open '%s'",
                      Settings::values.movie_play.c_str());
        }
    }

    if (!Settings::values.movie_record.empty()) {
        LOG_INFO(Movie, "Enabling Movie recording");
        play_mode = PlayMode::Recording;
    }
}

void Shutdown() {
    if (!IsRecordingInput()) {
        return;
    }

    LOG_INFO(Movie, "Saving movie");
    FileUtil::IOFile save_record(Settings::values.movie_record, "wb");

    CTMHeader header = {};

    header.filetype[0] = 'C';
    header.filetype[1] = 'T';
    header.filetype[2] = 'M';
    header.filetype[3] = 0x1B;

    Core::System::GetInstance().GetAppLoader().ReadProgramId(header.program_id);

    std::string rev_bytes;
    CryptoPP::StringSource(Common::g_scm_rev, true,
                            new CryptoPP::HexDecoder(new CryptoPP::StringSink(rev_bytes)));
    std::memcpy(header.revision, rev_bytes.data(), sizeof(CTMHeader::revision));

    save_record.WriteBytes(&header, sizeof(CTMHeader));
    save_record.WriteBytes(temp_input.data(), temp_input.size());
    if (!save_record.IsGood()) {
        LOG_ERROR(Movie, "Error saving movie");
    }
}

void HandlePadAndCircleStatus(Service::HID::PadState& pad_state, s16& circle_pad_x,
                              s16& circle_pad_y) {
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

void HandleCirclePad(Service::IR::CirclePadResponse& circle_pad) {
    if (IsPlayingInput()) {
        Play(circle_pad);
        CheckInputEnd();
    }
    else if (IsRecordingInput()) {
        Record(circle_pad);
    }
}

}
