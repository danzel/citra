// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <glad/glad.h>
#include <string.h>
#include "common/logging/log.h"
#include "scripted_input/script_runner.h"
#include "scripted_input/scripted_buttons.h"

#define SCRIPT_MAX_LINE 200

namespace ScriptedInput {

void ScriptRunner::SetButtons(std::shared_ptr<ScriptedButtons> buttons) {
    scripted_buttons = buttons;
}

void ScriptRunner::LoadScript(std::string script_name) {
    FILE* file = fopen(script_name.c_str(), "r");
    if (!file) {
        LOG_ERROR(ScriptedInput, "script_file %s does not exist", script_name.c_str());
        return;
    }

    char line[SCRIPT_MAX_LINE];

    int lineno = 0;
    while (fgets(line, SCRIPT_MAX_LINE, file) != NULL) {
        lineno++;

        // Remove end of line bytes and comments
        char terminal_chars[] = "\r\n#";
        for (int i = 0; i < strlen(terminal_chars); i++) {
            char* index = strchr(line, terminal_chars[i]);
            if (index != NULL) {
                *index = '\0';
            }
        }

        ScriptItem item;

        char* pch = strtok(line, " ");
        while (pch != NULL) {
            if (item.type == ScriptItemType::Undefined) {
                // The first token is a number or "screenshot"
                if (strcmp("screenshot", pch) == 0) {
                    item.type = ScriptItemType::Screenshot;
                } else {
                    int res = sscanf(pch, "%d", &item.frames);
                    if (res == 1 && item.frames > 0) {
                        item.type = ScriptItemType::Run;
                    } else {
                        LOG_ERROR(ScriptedInput, "Unable to parse line %i of script file", lineno);
                    }
                }
            } else {
                // The following should be buttons
                int index = IndexOfButton(pch);
                if (index != -1) {
                    item.buttons_active.push_back(index);
                } else {
                    LOG_ERROR(ScriptedInput, "Unable to parse line %i, button %s is unknown",
                              lineno, pch);
                }
            }
            pch = strtok(NULL, " ,.-");
        }

        if (item.type != ScriptItemType::Undefined) {
            script.push_back(item);
        }
    }

    fclose(file);
}

bool ScriptRunner::HasScript() const {
    return script.size() > 0;
}

void ScriptRunner::NotifyFrameFinished() {
    frame_number++;

    // Script already finished
    if (script_index >= script.size()) {
        return;
    }

    // If we're done with the current item, move to the next
    script_frame++;
    if (script_frame == script[script_index].frames) {
        script_frame = 0;
        script_index++;

        // take screenshots if we hit a screenshot item
        while (script_index < script.size() &&
               script[script_index].type == ScriptItemType::Screenshot) {
            SaveScreenshot();
            script_index++;
        }

        // Now we'll be at the next run item, so set the buttons up
        if (script_index < script.size()) {
            scripted_buttons.get()->SetActiveButtons(script[script_index].buttons_active);
        }
    }

    if (script_index >= script.size()) {
        LOG_INFO(ScriptedInput, "Scripted Input finished at frame %i", frame_number);
    }
}

void ScriptRunner::SaveScreenshot() {
    char buf[12];
    sprintf(buf, "%i.bmp", frame_number);

    const int w = 400; // render_window->GetActiveConfig().min_client_area_size.first;
    const int h = 480; // render_window->GetActiveConfig().min_client_area_size.second;
    unsigned char* pixels = new unsigned char[w * h * 3];
    glReadPixels(0, 0, w, h, GL_BGR, GL_UNSIGNED_BYTE, pixels);

    // BMP
    // https://web.archive.org/web/20080912171714/http://www.fortunecity.com/skyscraper/windows/364/bmpffrmt.html
    int image_bytes = (w * h * 3);
    int size = 14 + 40 + image_bytes;
    unsigned char file_header[14] = {'B',
                                     'M',
                                     static_cast<unsigned char>(size),
                                     static_cast<unsigned char>(size >> 8),
                                     static_cast<unsigned char>(size >> 16),
                                     static_cast<unsigned char>(size >> 24),
                                     0,
                                     0,
                                     0,
                                     0,
                                     14 + 40,
                                     0,
                                     0,
                                     0};
    unsigned char info_header[40] = {40,
                                     0,
                                     0,
                                     0,
                                     static_cast<unsigned char>(w),
                                     static_cast<unsigned char>(w >> 8),
                                     static_cast<unsigned char>(w >> 16),
                                     static_cast<unsigned char>(w >> 24),
                                     static_cast<unsigned char>(h),
                                     static_cast<unsigned char>(h >> 8),
                                     static_cast<unsigned char>(h >> 16),
                                     static_cast<unsigned char>(h >> 24),
                                     1,
                                     0,
                                     24,
                                     0,
                                     0,
                                     0,
                                     0,
                                     0,
                                     static_cast<unsigned char>(image_bytes),
                                     static_cast<unsigned char>(image_bytes >> 8),
                                     static_cast<unsigned char>(image_bytes >> 16),
                                     static_cast<unsigned char>(image_bytes >> 24),
                                     0,
                                     0,
                                     0,
                                     0,
                                     0,
                                     0,
                                     0,
                                     0,
                                     0,
                                     0,
                                     0,
                                     0,
                                     0,
                                     0,
                                     0,
                                     0};

    FILE* fp = fopen(buf, "wb");
    fwrite(file_header, sizeof(file_header), 1, fp);
    fwrite(info_header, sizeof(info_header), 1, fp);
    fwrite(pixels, w * h * 3, 1, fp);
    fclose(fp);

    delete[] pixels;
}
}