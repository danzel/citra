// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <atomic>
#include <memory>
#include "common/color.h"
#include "core/frontend/emu_window.h"
#include "core/frontend/screenshot_data.h"
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

void RendererBase::SaveScreenshot() {
    // We swap width and height here as we are going to rotate the image as we copy it
    // 3ds framebuffers are stored rotate 90 degrees
    std::unique_ptr<ScreenshotData> screenshot = std::make_unique<ScreenshotData>(
        GPU::g_regs.framebuffer_config[0].height, GPU::g_regs.framebuffer_config[0].width,
        GPU::g_regs.framebuffer_config[1].height, GPU::g_regs.framebuffer_config[1].width);

    for (int i : {0, 1}) {
        const auto& framebuffer = GPU::g_regs.framebuffer_config[i];
        LCD::Regs::ColorFill color_fill{GetColorFillForFramebuffer(i)};

        u8* dest_buffer = screenshot->screens[i].data.data();

        if (color_fill.is_enabled) {
            std::array<u8, 3> source;
            source[0] = color_fill.color_b;
            source[1] = color_fill.color_g;
            source[2] = color_fill.color_r;

            for (u32 y = 0; y < framebuffer.width; y++) {
                for (u32 x = 0; x < framebuffer.height; x++) {
                    u8* px_dest = dest_buffer + 3 * (x + framebuffer.height * y);
                    std::memcpy(px_dest, source.data(), 3);
                }
            }
        } else {
            const PAddr framebuffer_addr =
                framebuffer.active_fb == 0 ? framebuffer.address_left1 : framebuffer.address_left2;
            Memory::RasterizerFlushRegion(framebuffer_addr,
                                          framebuffer.stride * framebuffer.height);
            const u8* framebuffer_data = Memory::GetPhysicalPointer(framebuffer_addr);

            int bpp = GPU::Regs::BytesPerPixel(framebuffer.color_format);

            // x,y here are in destination pixels
            for (u32 y = 0; y < framebuffer.width; y++) {
                for (u32 x = 0; x < framebuffer.height; x++) {
                    u8* px_dest =
                        dest_buffer + 3 * (x + framebuffer.height * (framebuffer.width - y - 1));
                    const u8* px_source = framebuffer_data + bpp * (y + framebuffer.width * x);
                    Math::Vec4<u8> source;
                    switch (framebuffer.color_format) {
                    case GPU::Regs::PixelFormat::RGB8:
                        source = Color::DecodeRGB8(px_source);
                        break;
                    case GPU::Regs::PixelFormat::RGBA8:
                        source = Color::DecodeRGBA8(px_source);
                        break;
                    case GPU::Regs::PixelFormat::RGBA4:
                        source = Color::DecodeRGBA4(px_source);
                        break;
                    case GPU::Regs::PixelFormat::RGB5A1:
                        source = Color::DecodeRGB5A1(px_source);
                        break;
                    case GPU::Regs::PixelFormat::RGB565:
                        source = Color::DecodeRGB565(px_source);
                        break;
                    }
                    std::memcpy(px_dest, &source, 3);
                }
            }
        }
    }

    render_window->ReceiveScreenshot(std::move(screenshot));
}
