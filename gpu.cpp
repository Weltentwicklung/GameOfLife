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

void initialize_gpu(Settings& settings, GridSeed& grid_seed) {

	s_x11_display = XOpenDisplay(nullptr);
	if(!s_x11_display) {
	    std::cout << "Failed to open X display" << std::endl;
	    return;
	}

	s_x11_window = XCreateSimpleWindow(
	    s_x11_display,
	    RootWindow(s_x11_display, 0),
	    0, 0,           // x, y position
	    1920, 1080,     // width, height
	    0,              // border width
	    0,              // border color
	    0               // background color
	);

	XMapWindow(s_x11_display, s_x11_window);
	XStoreName(s_x11_display, s_x11_window, APP_NAME);
	Atom wm_state = XInternAtom(s_x11_display, "_NET_WM_STATE", False);
	Atom fullscreen = XInternAtom(s_x11_display, "_NET_WM_STATE_FULLSCREEN", False);
	XChangeProperty(s_x11_display,s_x11_window, wm_state, XA_ATOM, 32, PropModeReplace, (unsigned char*)&fullscreen, 1);
	XFlush(s_x11_display);


	// initializing GPU connection
	s_egl_display = eglGetDisplay((EGLNativeDisplayType)s_x11_display);
	// pass it by function
	eglInitialize(s_egl_display, nullptr, nullptr);

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


	// surface handeling
	EGLSurface s_egl_surface = eglCreateWindowSurface(
	    s_egl_display,
	    egl_config,
	    (EGLNativeWindowType)s_x11_window,
    	nullptr
	);


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


	// activate connection
	eglMakeCurrent(s_egl_display, s_egl_surface, s_egl_surface, s_egl_context);


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
	    out vec4 cell_color;
	    
	    void main() {
	        ivec2 pos = ivec2(gl_FragCoord.xy);
	        ivec4 cell = texelFetch(u_game_grid, pos, 0);
	        
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
	// DEBUGGER -> REMOVE FOR EFFICIENCY
	GLint success;
	glGetShaderiv(vs, GL_COMPILE_STATUS, &success);
	if(!success) {
	    char log[512];
	    glGetShaderInfoLog(vs, 512, nullptr, log);
	    std::cout << "Vertex shader error: " << log << std::endl;
	}
	GLint log_length;
	glGetShaderiv(vs, GL_INFO_LOG_LENGTH, &log_length);
	char* log = new char[log_length];
	glGetShaderInfoLog(vs, log_length, nullptr, log);
	// ... use log ...
	delete[] log;
	// compile calc fragment shader
	GLuint calc_fs = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(calc_fs, 1, &calc_shader, nullptr);
	glCompileShader(calc_fs);
	// DEBUGGER -> REMOVE FOR EFFICIENCY
	glGetShaderiv(calc_fs, GL_COMPILE_STATUS, &success);  // calc_fs not vs
	if(!success) {
	    char log[512];
	    glGetShaderInfoLog(calc_fs, 512, nullptr, log);        // calc_fs not vs
	    std::cout << "Calc shader error: " << log << std::endl;
	}
	
	// compile render fragment shader
	GLuint render_fs = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(render_fs, 1, &render_shader, nullptr);
	glCompileShader(render_fs);
	// DEBUGGER -> REMOVE FOR EFFICIENCY
	glGetShaderiv(render_fs, GL_COMPILE_STATUS, &success);
	if(!success) {
	    char log[512];

	    glGetShaderInfoLog(render_fs, 512, nullptr, log);
	    std::cout << "Render shader error: " << log << std::endl;
	}

	
	// linking the Width and length
	// after glLinkProgram(s_calc_program)
	glUseProgram(s_calc_program);
	glUniform1i(glGetUniformLocation(s_calc_program, "u_width"), GRID_W);
	glUniform1i(glGetUniformLocation(s_calc_program, "u_height"), GRID_H);		

}


void gpu_generation_loop() {  // runs continuously in its own thread

	
}

void set_zoom(Settings& settings) {
	
}

void set_offset(Settings& settings) {
	
}

void set_tick_rate(Settings& settings) {
	
}

void cleanup_gpu() {
	
}
