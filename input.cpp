#include "input.h"
#include <fcntl.h>
#include <unistd.h>
#include <linux/input.h>
#include <sys/ioctl.h>
#include <iostream>
#include <algorithm>

static int s_fd = -1;
static bool s_ctrl_held  = false;
static bool s_shift_held = false;

bool initialize_input() {
    s_fd = open("/dev/input/by-id/usb-Logitech_USB_Keyboard-event-kbd", O_RDONLY | O_NONBLOCK);
    if(s_fd != -1) {
        std::cout << "Input device opened successfully" << std::endl;
    } else {
        std::cout << "Failed to open input device" << std::endl;
        return false;
    }
    
    ioctl(s_fd, EVIOCGRAB, 1);
    std::cout << "Input device grabbed exclusively" << std::endl;
    return true;
}

void process_input(bool& running, Settings& settings) {
    struct input_event ev;
    
    while(read(s_fd, &ev, sizeof(ev)) == sizeof(ev)) {
        if(ev.type != EV_KEY) continue;

        // collect modifiers ? keep reading until non-modifier found
		if(ev.code == KEY_LEFTCTRL || ev.code == KEY_RIGHTCTRL)
		    { s_ctrl_held = (ev.value != 0); continue; }
		if(ev.code == KEY_LEFTSHIFT || ev.code == KEY_RIGHTSHIFT)
		    { s_shift_held = (ev.value != 0); continue; }
        
        if(ev.value == 0) continue;

        // ESC ? always processed immediately
        if(ev.code == KEY_ESC) { running = false; return; }

        // Tab ? exit alternative
        if(ev.code == KEY_TAB) { running = false; return; }

        // determine step size from current modifier state
        int step = 1;
        if     (s_ctrl_held && s_shift_held) step = 1000;
        else if(s_ctrl_held)                 step = 100;
        else if(s_shift_held)                step = 10;

        // pan
        if(ev.code == KEY_LEFT || ev.code == KEY_A) {
            g_settings_mutex.lock();
            settings.offset_x = std::max(0, settings.offset_x - step);
            g_settings_mutex.unlock();
            set_offset(settings);
        } else if(ev.code == KEY_RIGHT || ev.code == KEY_D) {
            g_settings_mutex.lock();
            settings.offset_x = std::min(GRID_W - 1, settings.offset_x + step);
            g_settings_mutex.unlock();
            set_offset(settings);
        } else if(ev.code == KEY_UP || ev.code == KEY_W) {
            g_settings_mutex.lock();
            settings.offset_y = std::max(0, settings.offset_y - step);
            g_settings_mutex.unlock();
            set_offset(settings);
        } else if(ev.code == KEY_DOWN || ev.code == KEY_S) {
            g_settings_mutex.lock();
            settings.offset_y = std::min(GRID_H - 1, settings.offset_y + step);
            g_settings_mutex.unlock();
            set_offset(settings);
        }

        // zoom
        else if(ev.code == KEY_KPPLUS || ev.code == KEY_Q) {
            g_settings_mutex.lock();
            settings.pixels_per_cell = std::min(
                settings.pixels_per_cell + step,
                std::min(WINDOW_W, WINDOW_H)
            );
            g_settings_mutex.unlock();
            set_zoom(settings);
        } else if(ev.code == KEY_KPMINUS || ev.code == KEY_E) {
            g_settings_mutex.lock();
            settings.pixels_per_cell = std::max(
                settings.pixels_per_cell - step,
                1
            );
            g_settings_mutex.unlock();
            set_zoom(settings);
        }

        // tick rate
        else if(ev.code == KEY_KPSLASH || ev.code == KEY_F) {
            g_settings_mutex.lock();
            settings.generation_loop_ms = std::min(
                settings.generation_loop_ms + step,
                10000
            );
            g_settings_mutex.unlock();
        } else if(ev.code == KEY_KPASTERISK || ev.code == KEY_R) {
            g_settings_mutex.lock();
            settings.generation_loop_ms = std::max(
                settings.generation_loop_ms - step,
                20
            );
            g_settings_mutex.unlock();
        }

        // pause
        // else if(ev.code == KEY_SPACE) { }

        // drain remaining queue ? only ESC and modifiers pass through
        while(read(s_fd, &ev, sizeof(ev)) == sizeof(ev)) {
            if(ev.type != EV_KEY) continue;
            if(ev.code == KEY_LEFTCTRL  || ev.code == KEY_RIGHTCTRL)
                s_ctrl_held  = (ev.value != 0);
            if(ev.code == KEY_LEFTSHIFT || ev.code == KEY_RIGHTSHIFT)
                s_shift_held = (ev.value != 0);
            if(ev.code == KEY_ESC && ev.value != 0) {
                running = false; return; 
            }
        }
        return;
    }
}

void cleanup_input() {
    ioctl(s_fd, EVIOCGRAB, 0);
    close(s_fd);
    std::cout << "Input device released" << std::endl;
}
