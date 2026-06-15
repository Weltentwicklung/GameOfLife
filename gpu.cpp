#include "gpu.h"

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <iostream>
#include <vector>
#include <cstdint>
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <chrono>

// --- module state ---
static GLuint s_calc_program   = 0;
static GLuint s_render_program = 0;
static GLuint s_texture        = 0;
static GLuint s_fbo            = 0;
static EGLDisplay s_egl_display = EGL_NO_DISPLAY;
static EGLSurface s_egl_surface = EGL_NO_SURFACE;
static EGLContext s_egl_context = EGL_NO_CONTEXT;
static Display*   s_x11_display  = nullptr;
static Window     s_x11_window   = 0;

// --- cached uniform locations ---
static GLint s_loc_pixels_per_cell = -1;
static GLint s_loc_offset_x = -1;
static GLint s_loc_offset_y = -1;

bool initialize_gpu(Settings& settings, GridSeed& grid_seed) {

	// -----------------------------------
	    // Setup the display and window
	// -----------------------------------

	s_x11_display = XOpenDisplay(nullptr);
	if(s_x11_display) {
		std::cout << "X display open successfully" << std::endl;
	}
	else {
		std::cout << "Failed to open X display" << std::endl;
	    return false;
	}

	s_x11_window = XCreateSimpleWindow(
	    s_x11_display,
	    RootWindow(s_x11_display, 0),
	    0, 0,           // x, y position
	    WINDOW_W, WINDOW_H,     // width, height
	    0,              // border width
	    0,              // border color
	    0               // background color
	);

	if(s_x11_window) {
	    std::cout << "X11 window created successfully" << std::endl;
	} else {
	    std::cout << "Failed to create X11 window" << std::endl;
	    return false;
	}
	
	XMapWindow(s_x11_display, s_x11_window);
	XStoreName(s_x11_display, s_x11_window, APP_NAME);
	Atom wm_state = XInternAtom(s_x11_display, "_NET_WM_STATE", False);
	Atom fullscreen = XInternAtom(s_x11_display, "_NET_WM_STATE_FULLSCREEN", False);
	XChangeProperty(s_x11_display,s_x11_window, wm_state, XA_ATOM, 32, PropModeReplace, (unsigned char*)&fullscreen, 1);
	XFlush(s_x11_display);
	std::cout << "X11 window mapped and configured" << std::endl;

	// ------------------------------------
	    // Initialize the GPU connection
	// ------------------------------------

	// initializing GPU connection
	s_egl_display = eglGetDisplay((EGLNativeDisplayType)s_x11_display);
	if(s_egl_display != EGL_NO_DISPLAY) {
	    std::cout << "EGL display obtained successfully" << std::endl;
	} else {
	    std::cout << "Failed to get EGL display" << std::endl;
	    return false;
	}
	
	// pass it by function
	eglInitialize(s_egl_display, nullptr, nullptr);
	if(eglGetError() == EGL_SUCCESS) {
	    std::cout << "EGL initialized successfully" << std::endl;
	} else {
	    std::cout << "Failed to initialize EGL" << std::endl;
	    return false;
	}

	// Setting up the attributes / GPU configurations
	EGLint attribs[] = {
	    EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
	    EGL_SURFACE_TYPE,    EGL_WINDOW_BIT,
	    EGL_RED_SIZE,        8,
	    EGL_GREEN_SIZE,      8,
	    EGL_BLUE_SIZE,       8,
	    EGL_NONE
	};
	EGLConfig egl_config;
	EGLint num_configs;
	// pass it by function	
	eglChooseConfig(s_egl_display, attribs, &egl_config, 1, &num_configs);
	if(eglGetError() == EGL_SUCCESS) {
	    std::cout << "EGL config chosen successfully" << std::endl;
	} else {
	    std::cout << "Failed to choose EGL config" << std::endl;
	    return false;
	}

	// ----------------------------------------------
	    // Setup EGL surface and context
	// ----------------------------------------------

	// surface handeling
	s_egl_surface = eglCreateWindowSurface(
	    s_egl_display,
	    egl_config,
	    (EGLNativeWindowType)s_x11_window,
    	nullptr
	);
	if(s_egl_surface != EGL_NO_SURFACE) {
	    std::cout << "EGL surface created successfully" << std::endl;
	} else {
	    std::cout << "Failed to create EGL surface" << std::endl;
	    return false;
	}

	// create the context (save the settings)
	EGLint context_attribs[] = {
	    EGL_CONTEXT_CLIENT_VERSION, 3,
	    EGL_NONE
	};
	s_egl_context = eglCreateContext(
	    s_egl_display,
	    egl_config,
	    EGL_NO_CONTEXT,
	    context_attribs
	);
	if(s_egl_context != EGL_NO_CONTEXT) {
	    std::cout << "EGL context created successfully" << std::endl;
	} else {
	    std::cout << "Failed to create EGL context" << std::endl;
	    return false;
	}


	// activate connection
	eglMakeCurrent(s_egl_display, s_egl_surface, s_egl_surface, s_egl_context);
	if(eglGetError() == EGL_SUCCESS) {
	    std::cout << "EGL context activated successfully" << std::endl;
	} else {
	    std::cout << "Failed to activate EGL context" << std::endl;
	    return false;
	}

	// --------------------------------------------
	    // Define the shader strings for the GPU
	// --------------------------------------------

	// Vertex shader setup
	// Vertex shader string
	const char* vertex_shader = R"(
	    #version 300 es
	    void main() {
	        vec2 positions[4] = vec2[4](
	            vec2(-1.0,  1.0),
	            vec2( 1.0,  1.0),
	            vec2(-1.0, -1.0),
	            vec2( 1.0, -1.0)
	        );
	        gl_Position = vec4(positions[gl_VertexID], 0.0, 1.0);
	    }
	)";


	// calc shader string
	const char* calc_shader = R"(
	    #version 300 es
	    precision lowp int;
		precision lowp isampler2D;
			    
	    uniform isampler2D u_game_grid;
	    out ivec4 cell_data;

		uniform int u_width;
		uniform int u_height;
	    
	    void main() {
	        ivec2 pos = ivec2(gl_FragCoord.xy);
	        ivec4 cell = texelFetch(u_game_grid, pos, 0);

			int living_neighbors = 0;
			int next_state = 0;

			if (cell.a == 0) {
		
				// top-left corner
				if(pos.x == 0 && pos.y == 0) {
	    			living_neighbors =
        				texelFetch(u_game_grid, ivec2(u_width - 1, u_height - 1), 0).r +
	        			texelFetch(u_game_grid, ivec2(0, u_height - 1), 0).r +
	    	    		texelFetch(u_game_grid, ivec2(1, u_height - 1), 0).r +
    	    			texelFetch(u_game_grid, ivec2(u_width - 1, 0), 0).r +
        				texelFetch(u_game_grid, ivec2(1, 0), 0).r +
        				texelFetch(u_game_grid, ivec2(u_width - 1, 1), 0).r +
	        			texelFetch(u_game_grid, ivec2(0, 1), 0).r +
    	    			texelFetch(u_game_grid, ivec2(1, 1), 0).r;
				}
	
				// top-right corner
				else if(pos.x == u_width - 1 && pos.y == 0) {
    				living_neighbors =
	    	    		texelFetch(u_game_grid, ivec2(pos.x - 1, u_height - 1), 0).r +
    	    			texelFetch(u_game_grid, ivec2(pos.x, u_height - 1), 0).r +
        				texelFetch(u_game_grid, ivec2(0, u_height - 1), 0).r +
        				texelFetch(u_game_grid, ivec2(pos.x - 1, 0), 0).r +
	        			texelFetch(u_game_grid, ivec2(0, 0), 0).r +
		        		texelFetch(u_game_grid, ivec2(pos.x - 1, 1), 0).r +
    		    		texelFetch(u_game_grid, ivec2(pos.x, 1), 0).r +
        				texelFetch(u_game_grid, ivec2(0, 1), 0).r;
				}
	
				// bottom-left corner
				else if(pos.x == 0 && pos.y == u_height - 1) {
    				living_neighbors =
	        			texelFetch(u_game_grid, ivec2(u_width - 1, pos.y - 1), 0).r +
    	    			texelFetch(u_game_grid, ivec2(0, pos.y - 1), 0).r +
        				texelFetch(u_game_grid, ivec2(1, pos.y - 1), 0).r +
	        			texelFetch(u_game_grid, ivec2(u_width - 1, pos.y), 0).r +
    	    			texelFetch(u_game_grid, ivec2(1, pos.y), 0).r +
	    	    		texelFetch(u_game_grid, ivec2(u_width - 1, 0), 0).r +
    	    			texelFetch(u_game_grid, ivec2(0, 0), 0).r +
        				texelFetch(u_game_grid, ivec2(1, 0), 0).r;
				}

				// bottom-right corner
				else if(pos.x == u_width - 1 && pos.y == u_height - 1) {
	    			living_neighbors =
    	    			texelFetch(u_game_grid, ivec2(pos.x - 1, pos.y - 1), 0).r +
        				texelFetch(u_game_grid, ivec2(pos.x, pos.y - 1), 0).r +
        				texelFetch(u_game_grid, ivec2(0, pos.y - 1), 0).r +
	        			texelFetch(u_game_grid, ivec2(pos.x - 1, pos.y), 0).r +
		        		texelFetch(u_game_grid, ivec2(0, pos.y), 0).r +
    		    		texelFetch(u_game_grid, ivec2(pos.x - 1, 0), 0).r +
        				texelFetch(u_game_grid, ivec2(pos.x, 0), 0).r +
        				texelFetch(u_game_grid, ivec2(0, 0), 0).r;
				}

				// left edge
				else if(pos.x == 0) {
	    			living_neighbors =
    	    			texelFetch(u_game_grid, ivec2(u_width - 1, pos.y - 1), 0).r +
        				texelFetch(u_game_grid, ivec2(0, pos.y - 1), 0).r +
        				texelFetch(u_game_grid, ivec2(1, pos.y - 1), 0).r +
	        			texelFetch(u_game_grid, ivec2(u_width - 1, pos.y), 0).r +
    	    			texelFetch(u_game_grid, ivec2(1, pos.y), 0).r +
	    	    		texelFetch(u_game_grid, ivec2(u_width - 1, pos.y + 1), 0).r +
    	    			texelFetch(u_game_grid, ivec2(0, pos.y + 1), 0).r +
        				texelFetch(u_game_grid, ivec2(1, pos.y + 1), 0).r;
				}

				// right edge
				else if(pos.x == u_width - 1) {
	    			living_neighbors =
    	    			texelFetch(u_game_grid, ivec2(pos.x - 1, pos.y - 1), 0).r +
        				texelFetch(u_game_grid, ivec2(pos.x, pos.y - 1), 0).r +
        				texelFetch(u_game_grid, ivec2(0, pos.y - 1), 0).r +
	        			texelFetch(u_game_grid, ivec2(pos.x - 1, pos.y), 0).r +
		        		texelFetch(u_game_grid, ivec2(0, pos.y), 0).r +
    		    		texelFetch(u_game_grid, ivec2(pos.x - 1, pos.y + 1), 0).r +
        				texelFetch(u_game_grid, ivec2(pos.x, pos.y + 1), 0).r +
        				texelFetch(u_game_grid, ivec2(0, pos.y + 1), 0).r;
				}

				// top edge
				else if(pos.y == 0) {
	    			living_neighbors =
    	    			texelFetch(u_game_grid, ivec2(pos.x - 1, u_height - 1), 0).r +
        				texelFetch(u_game_grid, ivec2(pos.x, u_height - 1), 0).r +
        				texelFetch(u_game_grid, ivec2(pos.x + 1, u_height - 1), 0).r +
	        			texelFetch(u_game_grid, ivec2(pos.x - 1, 0), 0).r +
    		    		texelFetch(u_game_grid, ivec2(pos.x + 1, 0), 0).r +
	    	    		texelFetch(u_game_grid, ivec2(pos.x - 1, 1), 0).r +
        				texelFetch(u_game_grid, ivec2(pos.x, 1), 0).r +
        				texelFetch(u_game_grid, ivec2(pos.x + 1, 1), 0).r;
				}
		
				// bottom edge
				else if(pos.y == u_height - 1) {
				    living_neighbors =
				        texelFetch(u_game_grid, ivec2(pos.x - 1, pos.y - 1), 0).r +
			    	    texelFetch(u_game_grid, ivec2(pos.x, pos.y - 1), 0).r +
		    	    	texelFetch(u_game_grid, ivec2(pos.x + 1, pos.y - 1), 0).r +
			        	texelFetch(u_game_grid, ivec2(pos.x - 1, pos.y), 0).r +
				        texelFetch(u_game_grid, ivec2(pos.x + 1, pos.y), 0).r +
				        texelFetch(u_game_grid, ivec2(pos.x - 1, 0), 0).r +
				        texelFetch(u_game_grid, ivec2(pos.x, 0), 0).r +
		    		    texelFetch(u_game_grid, ivec2(pos.x + 1, 0), 0).r;
				}
		
				// interior ? fast path
				else {
			    	living_neighbors =
				        texelFetch(u_game_grid, ivec2(pos.x - 1, pos.y - 1), 0).r +
			    	    texelFetch(u_game_grid, ivec2(pos.x, pos.y - 1), 0).r +
			        	texelFetch(u_game_grid, ivec2(pos.x + 1, pos.y - 1), 0).r +
			    	    texelFetch(u_game_grid, ivec2(pos.x - 1, pos.y), 0).r +
			        	texelFetch(u_game_grid, ivec2(pos.x + 1, pos.y), 0).r +
				        texelFetch(u_game_grid, ivec2(pos.x - 1, pos.y + 1), 0).r +
				        texelFetch(u_game_grid, ivec2(pos.x, pos.y + 1), 0).r +
			    	    texelFetch(u_game_grid, ivec2(pos.x + 1, pos.y + 1), 0).r;
				}
		
				// Conway rules
				if(cell.r == 0 && living_neighbors == 3) {
			    	next_state = 1;
				} 
				else if(cell.r == 1 && (living_neighbors == 2 || living_neighbors == 3)) {
		    	next_state = 1;
				}
		
				cell_data = ivec4(cell.r, next_state, cell.b, 1);

			}

			else {
		
				// top-left corner
				if(pos.x == 0 && pos.y == 0) {
	    			living_neighbors =
        				texelFetch(u_game_grid, ivec2(u_width - 1, u_height - 1), 0).g +
	        			texelFetch(u_game_grid, ivec2(0, u_height - 1), 0).g +
	    	    		texelFetch(u_game_grid, ivec2(1, u_height - 1), 0).g +
    	    			texelFetch(u_game_grid, ivec2(u_width - 1, 0), 0).g +
        				texelFetch(u_game_grid, ivec2(1, 0), 0).g +
        				texelFetch(u_game_grid, ivec2(u_width - 1, 1), 0).g +
	        			texelFetch(u_game_grid, ivec2(0, 1), 0).g +
    	    			texelFetch(u_game_grid, ivec2(1, 1), 0).g;
				}
	
				// top-right corner
				else if(pos.x == u_width - 1 && pos.y == 0) {
    				living_neighbors =
	        			texelFetch(u_game_grid, ivec2(pos.x - 1, u_height - 1), 0).g +
    	    			texelFetch(u_game_grid, ivec2(pos.x, u_height - 1), 0).g +
        				texelFetch(u_game_grid, ivec2(0, u_height - 1), 0).g +
	        			texelFetch(u_game_grid, ivec2(pos.x - 1, 0), 0).g +
    	    			texelFetch(u_game_grid, ivec2(0, 0), 0).g +
	    	    		texelFetch(u_game_grid, ivec2(pos.x - 1, 1), 0).g +
    	    			texelFetch(u_game_grid, ivec2(pos.x, 1), 0).g +
        				texelFetch(u_game_grid, ivec2(0, 1), 0).g;
				}
	
				// bottom-left corner
				else if(pos.x == 0 && pos.y == u_height - 1) {
    				living_neighbors =
	        			texelFetch(u_game_grid, ivec2(u_width - 1, pos.y - 1), 0).g +
    	    			texelFetch(u_game_grid, ivec2(0, pos.y - 1), 0).g +
        				texelFetch(u_game_grid, ivec2(1, pos.y - 1), 0).g +
	        			texelFetch(u_game_grid, ivec2(u_width - 1, pos.y), 0).g +
    	    			texelFetch(u_game_grid, ivec2(1, pos.y), 0).g +
	    	    		texelFetch(u_game_grid, ivec2(u_width - 1, 0), 0).g +
    	    			texelFetch(u_game_grid, ivec2(0, 0), 0).g +
        				texelFetch(u_game_grid, ivec2(1, 0), 0).g;
				}

				// bottom-right corner
				else if(pos.x == u_width - 1 && pos.y == u_height - 1) {
	    			living_neighbors =
    	    			texelFetch(u_game_grid, ivec2(pos.x - 1, pos.y - 1), 0).g +
        				texelFetch(u_game_grid, ivec2(pos.x, pos.y - 1), 0).g +
        				texelFetch(u_game_grid, ivec2(0, pos.y - 1), 0).g +
	        			texelFetch(u_game_grid, ivec2(pos.x - 1, pos.y), 0).g +
		        		texelFetch(u_game_grid, ivec2(0, pos.y), 0).g +
    		    		texelFetch(u_game_grid, ivec2(pos.x - 1, 0), 0).g +
        				texelFetch(u_game_grid, ivec2(pos.x, 0), 0).g +
        				texelFetch(u_game_grid, ivec2(0, 0), 0).g;
				}

				// left edge
				else if(pos.x == 0) {
	    			living_neighbors =
    	    			texelFetch(u_game_grid, ivec2(u_width - 1, pos.y - 1), 0).g +
        				texelFetch(u_game_grid, ivec2(0, pos.y - 1), 0).g +
        				texelFetch(u_game_grid, ivec2(1, pos.y - 1), 0).g +
	        			texelFetch(u_game_grid, ivec2(u_width - 1, pos.y), 0).g +
    	    			texelFetch(u_game_grid, ivec2(1, pos.y), 0).g +
	    	    		texelFetch(u_game_grid, ivec2(u_width - 1, pos.y + 1), 0).g +
    	    			texelFetch(u_game_grid, ivec2(0, pos.y + 1), 0).g +
        				texelFetch(u_game_grid, ivec2(1, pos.y + 1), 0).g;
				}

				// right edge
				else if(pos.x == u_width - 1) {
	    			living_neighbors =
    	    			texelFetch(u_game_grid, ivec2(pos.x - 1, pos.y - 1), 0).g +
        				texelFetch(u_game_grid, ivec2(pos.x, pos.y - 1), 0).g +
        				texelFetch(u_game_grid, ivec2(0, pos.y - 1), 0).g +
	        			texelFetch(u_game_grid, ivec2(pos.x - 1, pos.y), 0).g +
		        		texelFetch(u_game_grid, ivec2(0, pos.y), 0).g +
    		    		texelFetch(u_game_grid, ivec2(pos.x - 1, pos.y + 1), 0).g +
        				texelFetch(u_game_grid, ivec2(pos.x, pos.y + 1), 0).g +
        				texelFetch(u_game_grid, ivec2(0, pos.y + 1), 0).g;
				}

				// top edge
				else if(pos.y == 0) {
		    		living_neighbors =
    	    			texelFetch(u_game_grid, ivec2(pos.x - 1, u_height - 1), 0).g +
        				texelFetch(u_game_grid, ivec2(pos.x, u_height - 1), 0).g +
        				texelFetch(u_game_grid, ivec2(pos.x + 1, u_height - 1), 0).g +
	        			texelFetch(u_game_grid, ivec2(pos.x - 1, 0), 0).g +
    		    		texelFetch(u_game_grid, ivec2(pos.x + 1, 0), 0).g +
	    	    		texelFetch(u_game_grid, ivec2(pos.x - 1, 1), 0).g +
        				texelFetch(u_game_grid, ivec2(pos.x, 1), 0).g +
        				texelFetch(u_game_grid, ivec2(pos.x + 1, 1), 0).g;
				}
		
				// bottom edge
				else if(pos.y == u_height - 1) {
				    living_neighbors =
				        texelFetch(u_game_grid, ivec2(pos.x - 1, pos.y - 1), 0).g +
			    	    texelFetch(u_game_grid, ivec2(pos.x, pos.y - 1), 0).g +
		    	    	texelFetch(u_game_grid, ivec2(pos.x + 1, pos.y - 1), 0).g +
			        	texelFetch(u_game_grid, ivec2(pos.x - 1, pos.y), 0).g +
				        texelFetch(u_game_grid, ivec2(pos.x + 1, pos.y), 0).g +
				        texelFetch(u_game_grid, ivec2(pos.x - 1, 0), 0).g +
					    texelFetch(u_game_grid, ivec2(pos.x, 0), 0).g +
		    	    	texelFetch(u_game_grid, ivec2(pos.x + 1, 0), 0).g;
				}
		
				// interior ? fast path
				else {
			    	living_neighbors =
				        texelFetch(u_game_grid, ivec2(pos.x - 1, pos.y - 1), 0).g +
			    	    texelFetch(u_game_grid, ivec2(pos.x, pos.y - 1), 0).g +
			        	texelFetch(u_game_grid, ivec2(pos.x + 1, pos.y - 1), 0).g +
			    	    texelFetch(u_game_grid, ivec2(pos.x - 1, pos.y), 0).g +
			        	texelFetch(u_game_grid, ivec2(pos.x + 1, pos.y), 0).g +
				        texelFetch(u_game_grid, ivec2(pos.x - 1, pos.y + 1), 0).g +
					    texelFetch(u_game_grid, ivec2(pos.x, pos.y + 1), 0).g +
			        	texelFetch(u_game_grid, ivec2(pos.x + 1, pos.y + 1), 0).g;
				}
		
				// Conway rules
				if(cell.g == 0 && living_neighbors == 3) {
			    	next_state = 1;
				} 
				else if(cell.g == 1 && (living_neighbors == 2 || living_neighbors == 3)) {
		    	next_state = 1;
				}
		
				cell_data = ivec4(next_state, cell.g, cell.b, 0);
				
			}
		})";

	const char* render_shader = R"(
	    #version 300 es
	    precision lowp float;
		precision lowp isampler2D;
	    
	    uniform isampler2D u_game_grid;
	    uniform int u_pixels_per_cell;
	    uniform int u_offset_x;
	    uniform int u_offset_y;
	    
	    out vec4 cell_color;
	    
	    void main() {
	        ivec2 cell_pos = ivec2(
	            int(gl_FragCoord.x) / u_pixels_per_cell + u_offset_x,
	            int(gl_FragCoord.y) / u_pixels_per_cell + u_offset_y
	        );
	        ivec4 cell = texelFetch(u_game_grid, cell_pos, 0);
	        
	        int value = (cell.a == 1) ? 
	            cell.r : 
	            cell.g;
	        
	        cell_color = (value == 1) ? 
	            vec4(1.0, 1.0, 1.0, 1.0) : 
	            vec4(0.0, 0.0, 0.0, 1.0);
	    }
	)";

	// -----------------------------------
	    // Compile the shader programs
	// -----------------------------------

	// compile vertex shader ? used by both programs
	GLuint vs = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vs, 1, &vertex_shader, nullptr);
	glCompileShader(vs);
	// debugger and terminal-output for compiling vertex shader
	GLint success;
	glGetShaderiv(vs, GL_COMPILE_STATUS, &success);
	if(success) {
		std::cout << "Vertex shader successfully compiled" << std::endl;
	}
	else {
		GLint log_length;
		glGetShaderiv(vs, GL_INFO_LOG_LENGTH, &log_length);
		if(log_length > 0) {
			char* log = new char[log_length];
		    glGetShaderInfoLog(vs, log_length, nullptr, log);
		    std::cout << "Vertex shader error: " << log << std::endl;
	    	delete[] log;
	    }
	    return false;
	}

	// compile calc fragment shader
	GLuint calc_fs = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(calc_fs, 1, &calc_shader, nullptr);
	glCompileShader(calc_fs);
	// debugger and terminal-output for compile calculation fragment shader
	glGetShaderiv(calc_fs, GL_COMPILE_STATUS, &success);
	if(success) {
		std::cout << "Calc fragment shader successfully compiled" << std::endl;
	}
	else {
		GLint log_length;
		glGetShaderiv(calc_fs, GL_INFO_LOG_LENGTH, &log_length);
		if(log_length > 0) {
			char* log = new char[log_length];
		    glGetShaderInfoLog(calc_fs, log_length, nullptr, log);
	    	std::cout << "Calc fragment shader error: " << log << std::endl;
		    delete[] log;
		}
		return false;
	}
	
	// compile render fragment shader
	GLuint render_fs = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(render_fs, 1, &render_shader, nullptr);
	glCompileShader(render_fs);

	// debugger and terminal-output for compiling render fragment shader
	glGetShaderiv(render_fs, GL_COMPILE_STATUS, &success);
	if(success) {
		std::cout << "Render shader successfully compiled" << std::endl;
	}
	else {
		GLint log_length;
		glGetShaderiv(render_fs, GL_INFO_LOG_LENGTH, &log_length);
		if(log_length > 0) {
			char* log = new char[log_length];
		    glGetShaderInfoLog(render_fs, log_length, nullptr, log);
	    	std::cout << "Render shader error: " << log << std::endl;
		    delete[] log;
		}
		return false;
	}

	// -------------------------------------------------
	    // Creating shader programs and linking them
	// -------------------------------------------------

	// link calc program
	s_calc_program = glCreateProgram();
	glAttachShader(s_calc_program, vs);
	glAttachShader(s_calc_program, calc_fs);
	glLinkProgram(s_calc_program);

	// debugger and terminal output for calculation program linking
	glGetProgramiv(s_calc_program, GL_LINK_STATUS, &success);
	if(success) {
	    std::cout << "Calculation fragment shader program linked successfully" << std::endl;
	} else {
	    GLint log_length;
	    glGetProgramiv(s_calc_program, GL_INFO_LOG_LENGTH, &log_length);
	    if(log_length > 0) {
	        char* log = new char[log_length];
	        glGetProgramInfoLog(s_calc_program, log_length, nullptr, log);
	        std::cout << "Linking calculation program error: " << log << std::endl;
	        delete[] log;
	    }
	    return false;
	}
	
	// link uniforms to the calculation program
	glUseProgram(s_calc_program);
	glUniform1i(glGetUniformLocation(s_calc_program, "u_width"), GRID_W);
	glUniform1i(glGetUniformLocation(s_calc_program, "u_height"), GRID_H);
	GLint loc = glGetUniformLocation(s_calc_program, "u_game_grid");
	glUniform1i(loc, 0);
	if(loc != -1) {
	    std::cout << "Calc program uniforms linked successfully" << std::endl;
	} else {
	    std::cout << "Failed to get uniform location for calc program" << std::endl;
	    return false;
	}
		
	// link render program
	s_render_program = glCreateProgram();
	glAttachShader(s_render_program, vs);
	glAttachShader(s_render_program, render_fs);
	glLinkProgram(s_render_program);	
	glGetProgramiv(s_render_program, GL_LINK_STATUS, &success);
	// Terminal output and debugger for linking render program
	if(success) {
		std::cout << "render fragment shader program linked successfully." << std::endl;
	} 
	else {
		GLint log_length;
		glGetProgramiv(s_render_program, GL_INFO_LOG_LENGTH, &log_length);
		if(log_length > 0) {
			char* log = new char[log_length];
		    glGetProgramInfoLog(s_render_program, log_length, nullptr, log);
	    	std::cout << "Linking render program error: " << log << std::endl;
	    	delete[] log;
	    }
	    return false;
	}
	
	// bind texture to slot 0 for render program
	glUseProgram(s_render_program);
	loc = glGetUniformLocation(s_render_program, "u_game_grid");
	glUniform1i(loc, 0);
	if(loc != -1) {
	    std::cout << "Render program game grid uniform linked successfully" << std::endl;
	} else {
	    std::cout << "Failed to get game grid uniform location for render program" << std::endl;
	    return false;
	}

	s_loc_pixels_per_cell = glGetUniformLocation(s_render_program, "u_pixels_per_cell");
	s_loc_offset_x = glGetUniformLocation(s_render_program, "u_offset_x");
	s_loc_offset_y = glGetUniformLocation(s_render_program, "u_offset_y");

	std::cout << "pixels_per_cell = " << settings.pixels_per_cell << std::endl; // TODO Debugger -> delete afterwards
	
	glUniform1i(s_loc_pixels_per_cell, settings.pixels_per_cell);
	glUniform1i(s_loc_offset_x, settings.offset_x);
	glUniform1i(s_loc_offset_y, settings.offset_y);

	if(s_loc_pixels_per_cell != -1 && s_loc_offset_x != -1 && s_loc_offset_y != -1) {
	    std::cout << "Render program uniforms linked successfully" << std::endl;
	} else {
	    std::cout << "Failed to get uniform locations for render program" << std::endl;
	    return false;
	}

	// ------------------------------------
		//   upload texture to gpu
	// ------------------------------------
	glGenTextures(1, &s_texture);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, s_texture);
	
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	
	glTexImage2D(
	    GL_TEXTURE_2D,
	    0,
	    GL_RGBA8UI,
	    GRID_W, GRID_H,
	    0,
	    GL_RGBA_INTEGER,
	    GL_UNSIGNED_BYTE,
	    grid_seed.grid.data()
	);

	GLenum err = glGetError();
	if(err == GL_NO_ERROR) {
    	std::cout << "Texture uploaded successfully" << std::endl;
	} else {
    	std::cout << "Texture upload error: " << err << std::endl;
    	return false;
	}
	
	// -----------------------------------------------------
		// Add a Framebuffer (needed to write to texture)
	// -----------------------------------------------------

	glGenFramebuffers(1, &s_fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, s_fbo);
	
	glFramebufferTexture2D(
	    GL_FRAMEBUFFER,
	    GL_COLOR_ATTACHMENT0,
	    GL_TEXTURE_2D,
	    s_texture,
	    0
	);
	// Terminal output and debugger for the framebuffer
	if(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE){
		std::cout << "Framebuffer successfully created" << std::endl;
	}
	else {
	    std::cout << "FBO creation incomplete!" << std::endl;
	    return false;
	}

	// cleanup
	glDeleteShader(vs);
	glDeleteShader(calc_fs);
	glDeleteShader(render_fs);

	// successfully run the initialize_gpu() function
	std::cout << "GPU initialized successfully" << std::endl;
	return true;
}


void gpu_generation_loop(Settings& settings) {  // runs continuously in its own thread

	g_settings_mutex.lock();
	glUniform1i(s_loc_offset_x, settings.offset_x);
	glUniform1i(s_loc_offset_y, settings.offset_y);
	glUniform1i(s_loc_pixels_per_cell, settings.pixels_per_cell);
	g_settings_mutex.unlock();

    // calc pass ? render into FBO (updates texture)
    glBindFramebuffer(GL_FRAMEBUFFER, s_fbo);
    glUseProgram(s_calc_program);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

	glFinish(); // ensures the CPU waits in between the steps
    
    // render pass ? render to screen
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glUseProgram(s_render_program);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    
    // show on screen
    eglSwapBuffers(s_egl_display, s_egl_surface);
}

void cleanup_gpu() {
    glDeleteProgram(s_calc_program);
    glDeleteProgram(s_render_program);
    glDeleteTextures(1, &s_texture);
    glDeleteFramebuffers(1, &s_fbo);
    
    eglMakeCurrent(s_egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglDestroyContext(s_egl_display, s_egl_context);
    eglDestroySurface(s_egl_display, s_egl_surface);
    eglTerminate(s_egl_display);
    
    XDestroyWindow(s_x11_display, s_x11_window);
    XCloseDisplay(s_x11_display);
    
    std::cout << "GPU cleaned up successfully" << std::endl;
}
