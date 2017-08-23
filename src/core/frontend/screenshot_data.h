// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <vector>
#include "common/common_types.h"

/**
 * Screenshot data for a screen.
 * data is in RGB888 format, left to right then top to bottom
 */
class ScreenshotScreen {
public:
    size_t width;
    size_t height;

    std::vector<u8> data;
};
class ScreenshotData {
public:
    ScreenshotData(const size_t& top_width, const size_t& top_height, const size_t& bottom_width,
                   const size_t& bottom_height) {
        screens[0].width = top_width;
        screens[0].height = top_height;
        screens[0].data.resize(top_width * top_height * 3);

        screens[1].width = bottom_width;
        screens[1].height = bottom_height;
        screens[1].data.resize(bottom_width * bottom_height * 3);
    }

    std::array<ScreenshotScreen, 2> screens;
};
