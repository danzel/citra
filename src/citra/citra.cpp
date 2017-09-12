// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <iostream>
#include <memory>
#include <string>
#include <thread>
#define SDL_MAIN_HANDLED
#include <SDL.h>
//#include <glad/glad.h>

// This needs to be included before getopt.h because the latter #defines symbols used by it
#include "common/microprofile.h"

#ifdef _MSC_VER
#include <getopt.h>
#else
#include <getopt.h>
#include <unistd.h>
#endif

#ifdef _WIN32
// windows.h needs to be included before shellapi.h
#include <windows.h>

#include <shellapi.h>
#endif

#include "citra/config.h"
#include "citra/emu_window/emu_window_sdl2.h"
#include "common/logging/backend.h"
#include "common/logging/filter.h"
#include "common/logging/log.h"
#include "common/scm_rev.h"
#include "common/scope_exit.h"
#include "common/string_util.h"
#include "core/core.h"
#include "core/gdbstub/gdbstub.h"
#include "core/hw/gpu.h"
#include "core/loader/loader.h"
#include "core/memory.h"
#include "core/movie.h"
#include "core/settings.h"

static void PrintHelp(const char* argv0) {
    std::cout << "Usage: " << argv0
              << " [options] <filename>\n"
                 "-g, --gdbport=NUMBER       Enable gdb stub on port NUMBER\n"
                 "-r, --movie-record=[file]  Record a movie (game inputs) to the given file\n"
                 "-p, --movie-play=[file]    Playback the movie (game inputs) from the given file\n"
                 "-t, --movie-test           When movie playback has finished, take a screenshot and close the emulator\n"
                 "-h, --help                 Display this help and exit\n"
                 "-v, --version              Output version information and exit\n";
}

static void PrintVersion() {
    std::cout << "Citra " << Common::g_scm_branch << " " << Common::g_scm_desc << std::endl;
}

/// Application entry point
int main(int argc, char** argv) {
    Config config;
    int option_index = 0;
    bool use_gdbstub = Settings::values.use_gdbstub;
    u32 gdb_port = static_cast<u32>(Settings::values.gdbstub_port);
    std::string movie_record;
    std::string movie_play;
    bool movie_test = false;

    char* endarg;
#ifdef _WIN32
    int argc_w;
    auto argv_w = CommandLineToArgvW(GetCommandLineW(), &argc_w);

    if (argv_w == nullptr) {
        LOG_CRITICAL(Frontend, "Failed to get command line arguments");
        return -1;
    }
#endif
    std::string filepath;

    static struct option long_options[] = {
        {"gdbport", required_argument, 0, 'g'},    {"movie-record", required_argument, 0, 'r'},
        {"movie-play", required_argument, 0, 'p'}, {"movie-test", no_argument, 0, 't'},
        {"help", no_argument, 0, 'h'},             {"version", no_argument, 0, 'v'},
        {0, 0, 0, 0},
    };

    while (optind < argc) {
        char arg = getopt_long(argc, argv, "g:r:p:thv", long_options, &option_index);
        if (arg != -1) {
            switch (arg) {
            case 'g':
                errno = 0;
                gdb_port = strtoul(optarg, &endarg, 0);
                use_gdbstub = true;
                if (endarg == optarg)
                    errno = EINVAL;
                if (errno != 0) {
                    perror("--gdbport");
                    exit(1);
                }
                break;
            case 'r':
                movie_record = optarg;
                break;
            case 't':
                movie_test = true;
                break;
            case 'p':
                movie_play = optarg;
                break;
            case 'h':
                PrintHelp(argv[0]);
                return 0;
            case 'v':
                PrintVersion();
                return 0;
            }
        } else {
#ifdef _WIN32
            filepath = Common::UTF16ToUTF8(argv_w[optind]);
#else
            filepath = argv[optind];
#endif
            optind++;
        }
    }

#ifdef _WIN32
    LocalFree(argv_w);
#endif

    Log::Filter log_filter(Log::Level::Debug);
    Log::SetFilter(&log_filter);

    MicroProfileOnThreadCreate("EmuThread");
    SCOPE_EXIT({ MicroProfileShutdown(); });

    if (filepath.empty()) {
        LOG_CRITICAL(Frontend, "Failed to load ROM: No ROM specified");
        return -1;
    }

    if (!movie_record.empty() && !movie_play.empty()) {
        LOG_CRITICAL(Frontend, "Cannot both play and record a movie");
    }

    log_filter.ParseFilterString(Settings::values.log_filter);

    // Apply the command line arguments
    Settings::values.gdbstub_port = gdb_port;
    Settings::values.use_gdbstub = use_gdbstub;
    Settings::values.movie_play = std::move(movie_play);
    Settings::values.movie_record = std::move(movie_record);
    Settings::Apply();

    std::unique_ptr<EmuWindow_SDL2> emu_window{std::make_unique<EmuWindow_SDL2>()};

    Core::System& system{Core::System::GetInstance()};

    SCOPE_EXIT({ system.Shutdown(); });

    const Core::System::ResultStatus load_result{system.Load(emu_window.get(), filepath)};

    switch (load_result) {
    case Core::System::ResultStatus::ErrorGetLoader:
        LOG_CRITICAL(Frontend, "Failed to obtain loader for %s!", filepath.c_str());
        return -1;
    case Core::System::ResultStatus::ErrorLoader:
        LOG_CRITICAL(Frontend, "Failed to load ROM!");
        return -1;
    case Core::System::ResultStatus::ErrorLoader_ErrorEncrypted:
        LOG_CRITICAL(Frontend, "The game that you are trying to load must be decrypted before "
                               "being used with Citra. \n\n For more information on dumping and "
                               "decrypting games, please refer to: "
                               "https://citra-emu.org/wiki/dumping-game-cartridges/");
        return -1;
    case Core::System::ResultStatus::ErrorLoader_ErrorInvalidFormat:
        LOG_CRITICAL(Frontend, "Error while loading ROM: The ROM format is not supported.");
        return -1;
    case Core::System::ResultStatus::ErrorNotInitialized:
        LOG_CRITICAL(Frontend, "CPUCore not initialized");
        return -1;
    case Core::System::ResultStatus::ErrorSystemMode:
        LOG_CRITICAL(Frontend, "Failed to determine system mode!");
        return -1;
    case Core::System::ResultStatus::ErrorVideoCore:
        LOG_CRITICAL(Frontend, "VideoCore not initialized");
        return -1;
    case Core::System::ResultStatus::Success:
        break; // Expected case
    }

    if (movie_test) {
        LOG_INFO(Frontend, "Enabling CI Mode");
        Movie::OnComplete = []() {
            LOG_INFO(Frontend, "Movie done, saving screenshot and exiting");

            for (int i : {0, 1}) {
                auto framebuffer = GPU::g_regs.framebuffer_config[i];

                const int w = framebuffer.width;
                const int h = framebuffer.height;

                int bpp = GPU::Regs::BytesPerPixel(framebuffer.color_format);
                size_t line_size = w * bpp;

                const PAddr framebuffer_addr =
                    framebuffer.active_fb == 0 ? framebuffer.address_left1 : framebuffer.address_left2;
                Memory::RasterizerFlushRegion(framebuffer_addr, framebuffer.stride * framebuffer.height);
                const u8* framebuffer_data = Memory::GetPhysicalPointer(framebuffer_addr);

                LOG_INFO(Frontend, "Saving image of size %d,%d stride %d, lw %d @ cf %d, bpp %d", w, h, framebuffer.stride, line_size, framebuffer.color_format, bpp);

                SDL_Surface* image = NULL;
                switch (framebuffer.color_format)
                {
                case GPU::Regs::PixelFormat::RGBA8:
                    image = SDL_CreateRGBSurface(0, w, h, bpp * 8, 0xFF000000, 0x00FF0000, 0x0000FF00, 0x000000FF);
                    break;
                case GPU::Regs::PixelFormat::RGB8:
                    image = SDL_CreateRGBSurface(0, w, h, bpp * 8, 0x00FF0000, 0x0000FF00, 0x000000FF, 0);
                    break;
                case GPU::Regs::PixelFormat::RGB565:
                    image = SDL_CreateRGBSurface(0, w, h, bpp * 8, 0x0000F800, 0x000007E0, 0x0000001F, 0);
                    break;
                case GPU::Regs::PixelFormat::RGB5A1:
                    image = SDL_CreateRGBSurface(0, w, h, bpp * 8, 0x0000F800, 0x000007C0, 0x0000003E, 0x00000001);
                    break;
                case GPU::Regs::PixelFormat::RGBA4:
                    image = SDL_CreateRGBSurface(0, w, h, bpp * 8, 0x0000F000, 0x00000F00, 0x000000F0, 0x0000000F);
                    break;
                default:
                    LOG_ERROR(Frontend, "[UNIMPLEMENTED] Not sure how to save an image in PixelFormat %d", framebuffer.color_format);
                    break;
                }
                if (image == NULL) {
                    LOG_ERROR(Frontend, "Error creating a surface %s", SDL_GetError());
                    continue;
                }

                SDL_LockSurface(image);

                auto pixels = static_cast<u8*>(image->pixels);
                for (int y = 0; y < h; y++) {
                    std::memcpy(pixels + line_size * y, framebuffer_data + framebuffer.stride * y, line_size);
                }

                SDL_UnlockSurface(image);

                SDL_Surface* image2 = SDL_ConvertSurfaceFormat(image, SDL_PIXELFORMAT_RGB888, 0);

                SDL_SaveBMP(image2, i == 0 ?"screenshot_0.bmp" : "screenshot_1.bmp");

                SDL_FreeSurface(image);
                SDL_FreeSurface(image2);
            }

            //Tell SDL to quit
            SDL_Event sdlevent;
            sdlevent.type = SDL_QUIT;
            SDL_PushEvent(&sdlevent);
        };
    }

    while (emu_window->IsOpen()) {
        system.RunLoop();
    }

    return 0;
}
