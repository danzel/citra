// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <glad/glad.h>
#include "scripted_input/script_runner.h"
#include "scripted_input/scripted_buttons.h"

#define SCRIPT_MAX_LINE 200

namespace ScriptedInput {

void ScriptRunner::SetButtons(std::shared_ptr<ScriptedButtons> buttons) {
    scripted_buttons = buttons;
}

void ScriptRunner::LoadScript(std::string script_name) {
    //TODO: Load from a file specified in config
    FILE* file = fopen(script_name.c_str(), "r");
    if (!file) {
        //TODO: Log error
        return;
    }

    char line[SCRIPT_MAX_LINE];

    int lineno = 0;
    while (fgets(line, SCRIPT_MAX_LINE, file) != NULL) {
        lineno++;

        //Remove end of line bytes and comments
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
                //The first token is a number or "screenshot"
                if (strcmp("screenshot", pch) == 0) {
                    item.type = ScriptItemType::Screenshot;
                }
                else {
                    int res = sscanf(pch, "%d", &item.frames);
                    if (res == 1 && item.frames > 0) {
                        item.type = ScriptItemType::Run;
                    }
                    else {
                        //TODO: Parse error
                    }
                }
            }
            else {
                //The following should be buttons
                int index = IndexOfButton(pch);
                if (index != -1) {
                    item.buttons_active.push_back(index);
                }
                else {
                    //TODO: Parse error
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

void ScriptRunner::NotifyFrameFinished() {
    frame_number++;

    //Script already finished
    if (script_index >= script.size()) {
        //printf("already done\n");
        return;
    }

    //If we're done with the current item, move to the next
    script_frame++;
    if (script_frame == script[script_index].frames) {
        printf("At frame %i, progressing to index %i\n", frame_number, script_index + 1);
        script_frame = 0;
        script_index++;

        //take screenshots if we hit a screenshot item
        while (script_index < script.size() && script[script_index].type == ScriptItemType::Screenshot) {
            //TODO: Screenshot
            SaveScreenshot();
            script_index++;
        }

        //Now we'll be at the next run item, so set the buttons up
        if (script_index < script.size()) {
            printf("changing button\n");
            scripted_buttons.get()->SetActiveButtons(script[script_index].buttons_active);
        }
    }
}

void ScriptRunner::SaveScreenshot() {
    const int w = 400;//render_window->GetActiveConfig().min_client_area_size.first;
    const int h = 480;//render_window->GetActiveConfig().min_client_area_size.second;
    unsigned char* pixels = new unsigned char[w * h * 3];
    glReadPixels(0, 0, w, h, GL_BGR, GL_UNSIGNED_BYTE, pixels);

    //BMP
    //ref https://web.archive.org/web/20080912171714/http://www.fortunecity.com/skyscraper/windows/364/bmpffrmt.html
    int image_bytes = (w * h * 3);
    int size = 14 + 40 + image_bytes;
    unsigned char file_header[14] = { 'B', 'M', size, size >> 8, size >> 16, size >> 24, 0, 0, 0, 0, 14 + 40, 0, 0, 0 };
    unsigned char info_header[40] = { 40, 0, 0, 0, w, w >> 8, w >> 16, w >> 24, h, h >> 8, h >> 16, h >> 24, 1, 0, 24, 0, 0, 0, 0, 0, image_bytes, image_bytes >> 8, image_bytes >> 16, image_bytes >> 24, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

    FILE *fp = fopen("out.bmp", "wb");
    fwrite(file_header, sizeof(file_header), 1, fp);
    fwrite(info_header, sizeof(info_header), 1, fp);
    fwrite(pixels, w * h * 3, 1, fp);
    fclose(fp);


    delete[] pixels;
}
}