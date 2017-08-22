// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <atomic>
#include <memory>
#include "core/hw/hw.h"
#include "core/hw/lcd.h"
#include "video_core/renderer_base.h"
#include "video_core/renderer_opengl/gl_rasterizer.h"
#include "video_core/swrasterizer/swrasterizer.h"
#include "video_core/video_core.h"

void RendererBase::RefreshRasterizerSetting() {
    bool hw_renderer_enabled = VideoCore::g_hw_renderer_enabled;
    if (rasterizer == nullptr || opengl_rasterizer_active != hw_renderer_enabled) {
        opengl_rasterizer_active = hw_renderer_enabled;

        if (hw_renderer_enabled) {
            rasterizer = std::make_unique<RasterizerOpenGL>();
        } else {
            rasterizer = std::make_unique<VideoCore::SWRasterizer>();
        }
    }
}

u32 RendererBase::GetColorFillForFramebuffer(int framebuffer_index) {
    // Main LCD (0): 0x1ED02204, Sub LCD (1): 0x1ED02A04
    u32 lcd_color_addr =
        (framebuffer_index == 0) ? LCD_REG_INDEX(color_fill_top) : LCD_REG_INDEX(color_fill_bottom);
    lcd_color_addr = HW::VADDR_LCD + 4 * lcd_color_addr;
    LCD::Regs::ColorFill color_fill = {0};
    LCD::Read(color_fill.raw, lcd_color_addr);
    return color_fill.raw;
}
