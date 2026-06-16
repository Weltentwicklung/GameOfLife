#include "input.h"
#include <fcntl.h>
#include <unistd.h>
#include <linux/input.h>
#include <sys/ioctl.h>
#include <iostream>
#include <algorithm>

static int s_keyboard_fd = -1;
static int s_mouse_fd = -1;
static bool s_ctrl_held  = false;
static bool s_shift_held = false;
static int s_cursor_x = WINDOW_W / 2;
static int s_cursor_y = WINDOW_H / 2;

bool initialize_input() {
    s_keyboard_fd = open("/dev/input/by-id/usb-Logitech_USB_Keyboard-event-kbd", O_RDONLY | O_NONBLOCK);
    s_mouse_fd = open("/dev/input/by-id/usb-Razer_Razer_Basilisk_V3-event-mouse", O_RDONLY | O_NONBLOCK);
    if(s_keyboard_fd != -1) {
        std::cout << "Keyboard device opened successfully" << std::endl;
    } 
    else {
        std::cout << "Failed to open keyboard device" << std::endl;
        return false;
    }

    if(s_mouse_fd != -1) {
    	std::cout << "Mouse device opened sucessfully" << std::endl;
    }
    else {
    	std::cout << "Failed to open mouse device" << std::endl;
    	return false;
    }
    
	if(ioctl(s_keyboard_fd, EVIOCGRAB, 1) != -1) {
    	std::cout << "Keyboard grabbed exclusively" << std::endl;
	} 
	else {
    	std::cout << "Failed to grab keyboard" << std::endl;
    	return false;
	}

	if (ioctl(s_mouse_fd, EVIOCGRAB, 1) != -1) {
	    std::cout << "Mouse device grabbed exclusively" << std::endl;
	} 
	else {
	    std::cout << "Failed to grab mouse" << std::endl;
	    return false;
	}
	
    return true;
}

void process_input(std::atomic<bool>& running, Settings& settings) {
    struct input_event ev;
    int wheel = 0;
    bool key_processed = false;
    int step = 1;

	// step 1: update cursor position from all pending mouse events
	while(read(s_mouse_fd, &ev, sizeof(ev)) == sizeof(ev)) {
		if(ev.type != EV_SYN)
    	    std::cout << "mouse ev type=" << ev.type << " code=" << ev.code << " value=" << ev.value << std::endl; // DELETE AFTER DEBUGGING WITH IF STATEMENT
	    if(ev.type == EV_REL) {
	        if(ev.code == REL_X) s_cursor_x = std::clamp(s_cursor_x + ev.value, 0, WINDOW_W - 1);
	        if(ev.code == REL_Y) s_cursor_y = std::clamp(s_cursor_y + ev.value, 0, WINDOW_H - 1);
	        if(ev.code == REL_WHEEL) wheel += ev.value;
	    }
	}
	update_cursor(s_cursor_x, s_cursor_y);
    
    while(read(s_keyboard_fd, &ev, sizeof(ev)) == sizeof(ev)) {
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
        if     (s_ctrl_held && s_shift_held) step = 1000;
        else if(s_ctrl_held)                 step = 100;
        else if(s_shift_held)                step = 10;

        // scroll steps
        if(ev.code == KEY_LEFT || ev.code == KEY_A) {
            g_settings_mutex.lock();
            settings.offset_x = std::max(0, settings.offset_x - step);
            g_settings_mutex.unlock();
            key_processed = true;
        } else if(ev.code == KEY_RIGHT || ev.code == KEY_D) {
            g_settings_mutex.lock();
            settings.offset_x = std::min(GRID_W - WINDOW_W / settings.pixels_per_cell, settings.offset_x + step);
            g_settings_mutex.unlock();
            key_processed = true;
        } else if(ev.code == KEY_DOWN || ev.code == KEY_S) {
            g_settings_mutex.lock();
            settings.offset_y = std::max(0, settings.offset_y - step);
            g_settings_mutex.unlock();
            key_processed = true;
        } else if(ev.code == KEY_UP || ev.code == KEY_W) {
            g_settings_mutex.lock();
            settings.offset_y = std::min(GRID_H - WINDOW_H / settings.pixels_per_cell, settings.offset_y + step);
            g_settings_mutex.unlock();
            key_processed = true;
        }

		// zoom
        else if(ev.code == KEY_KPPLUS || ev.code == KEY_Q) {
            g_settings_mutex.lock();
            int old_ppc = settings.pixels_per_cell;
            settings.pixels_per_cell = std::min(settings.pixels_per_cell + step, std::min(WINDOW_W, WINDOW_H));
            settings.offset_x += (WINDOW_W / old_ppc - WINDOW_W / settings.pixels_per_cell) / 2;
            settings.offset_y += (WINDOW_H / old_ppc - WINDOW_H / settings.pixels_per_cell) / 2;
            settings.offset_x = std::max(0, std::min(settings.offset_x, GRID_W - WINDOW_W / settings.pixels_per_cell));
            settings.offset_y = std::max(0, std::min(settings.offset_y, GRID_H - WINDOW_H / settings.pixels_per_cell));
            g_settings_mutex.unlock();
            key_processed = true;
        }
        
        else if(ev.code == KEY_KPMINUS || ev.code == KEY_E) {
            g_settings_mutex.lock();
            int old_ppc = settings.pixels_per_cell;
            settings.pixels_per_cell = std::max(settings.pixels_per_cell - step, 1);
            settings.offset_x += (WINDOW_W / old_ppc - WINDOW_W / settings.pixels_per_cell) / 2;
            settings.offset_y += (WINDOW_H / old_ppc - WINDOW_H / settings.pixels_per_cell) / 2;
            settings.offset_x = std::max(0, std::min(settings.offset_x, GRID_W - WINDOW_W / settings.pixels_per_cell));
            settings.offset_y = std::max(0, std::min(settings.offset_y, GRID_H - WINDOW_H / settings.pixels_per_cell));
            g_settings_mutex.unlock();
            key_processed = true;
        }

        // tick rate
        else if(ev.code == KEY_KPSLASH || ev.code == KEY_F) {
            g_settings_mutex.lock();
            settings.generation_loop_ms = std::min(
                settings.generation_loop_ms + step,
                10000
            );
            g_settings_mutex.unlock();
            key_processed = true;
        } else if(ev.code == KEY_KPASTERISK || ev.code == KEY_R) {
            g_settings_mutex.lock();
            settings.generation_loop_ms = std::max(
                settings.generation_loop_ms - step,
                20
            );
            g_settings_mutex.unlock();
            key_processed = true;
        }

        // pause
        // else if(ev.code == KEY_SPACE) { }

        // drain remaining queue ? only ESC and modifiers pass through
        while(read(s_keyboard_fd, &ev, sizeof(ev)) == sizeof(ev)) {
            if(ev.type != EV_KEY) continue;
            if(ev.code == KEY_LEFTCTRL  || ev.code == KEY_RIGHTCTRL)
                s_ctrl_held  = (ev.value != 0);
            if(ev.code == KEY_LEFTSHIFT || ev.code == KEY_RIGHTSHIFT)
                s_shift_held = (ev.value != 0);
            if(ev.code == KEY_ESC && ev.value != 0) {
                running = false; return; 
            }
        }
    }

    if(key_processed == false && wheel != 0) {
    std::cout << "mouse ev type=" << ev.type << " code=" << ev.code << " value=" << ev.value << std::endl; // DELETE AFTER DEBUGGING
    
    	g_settings_mutex.lock();
    	int old_ppc = settings.pixels_per_cell;
    	if(wheel > 0) {
    	    settings.pixels_per_cell = std::min(settings.pixels_per_cell + step, std::min(WINDOW_W, WINDOW_H));
    	}
    	else {
    		settings.pixels_per_cell = std::max(settings.pixels_per_cell - step, 1);	
    	}
    	// zoom toward cursor position
    	settings.offset_x += s_cursor_x / old_ppc - s_cursor_x / settings.pixels_per_cell;
    	settings.offset_y += s_cursor_y / old_ppc - s_cursor_y / settings.pixels_per_cell;
    	settings.offset_x = std::max(0, std::min(settings.offset_x, GRID_W - WINDOW_W / settings.pixels_per_cell));
    	settings.offset_y = std::max(0, std::min(settings.offset_y, GRID_H - WINDOW_H / settings.pixels_per_cell));
    	g_settings_mutex.unlock();            	
    }
}

void cleanup_input() {
    ioctl(s_keyboard_fd, EVIOCGRAB, 0);
    ioctl(s_mouse_fd, EVIOCGRAB, 0);
    close(s_keyboard_fd);
    close(s_mouse_fd);
    std::cout << "Input devices released" << std::endl;
}
