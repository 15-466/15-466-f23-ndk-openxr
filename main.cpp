//Mode.hpp declares the "Mode::current" static member variable, which is used to decide where event-handling, updating, and drawing events go:
#include "Mode.hpp"

//The 'PlayMode' mode plays the game:
#include "PlayMode.hpp"

//For asset loading:
#include "Load.hpp"

//GL.hpp will include a non-namespace-polluting set of opengl prototypes:
#include "GL.hpp"

//for screenshots:
#include "load_save_png.hpp"

#ifdef __ANDROID__
#include <android_native_app_glue.h>

#else
//Includes for desktop platforms:
#include <SDL.h>
#endif

//for OpenXR stuff:
#include "XR.hpp"

//...and for c++ standard library functions:
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <memory>
#include <algorithm>
#include <cstring>

#ifdef __ANDROID__

//modeled on OpenXR's "hello_xr" example's main.cpp:
//  https://github.com/KhronosGroup/OpenXR-SDK-Source/blob/main/src/tests/hello_xr/main.cpp
void android_main(struct android_app* app) {
	try {
		JNIEnv* Env;
		app->activity->vm->AttachCurrentThread(&Env, nullptr);

		//------- EGL setup -------

		//create an EGL context:
		//  based on hello_xr's gfxwrapper_opengl.c

		EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
		if (display == EGL_NO_DISPLAY) {
			std::cerr << "ERROR: failed to get EGL display connection." << std::endl;
			return;
		}

		EGLint major = 0;
		EGLint minor = 0;
		if (EGLBoolean res = eglInitialize(display, &major, &minor);
		    res != EGL_TRUE) {
			std::cerr << "ERROR: failed to initialize EGL connection." << std::endl;
			return;
		}

		std::cout << "INFO: EGL reports version " << major << "." << minor << std::endl;

		EGLConfig config;
		{ //select a valid configuration:
			//these are (basically) copied from ksGpuContext_CreateForSurface:
			// (though that code warns about avoiding eglChooseConfig because it can force [useless] antialiasing
			const EGLint attrib_list[] = {
				EGL_RED_SIZE, 8,
				EGL_GREEN_SIZE, 8,
				EGL_BLUE_SIZE, 8,
				EGL_ALPHA_SIZE, 8,
				EGL_DEPTH_SIZE, 24,
				EGL_SAMPLE_BUFFERS, 0,
				EGL_SAMPLES, 0,
				EGL_CONFORMANT, EGL_OPENGL_ES3_BIT,
				EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
				EGL_SURFACE_TYPE, EGL_WINDOW_BIT | EGL_PBUFFER_BIT,
				EGL_NONE
			};

			std::array< EGLConfig, 12 > configs;
			EGLint configs_count = 0;

			if (EGLBoolean res = eglChooseConfig(display, attrib_list, configs.data(), configs.size(), &configs_count);
			    res != EGL_TRUE) {
				std::cerr << "Failed to choose EGL config." << std::endl;
				return;
			}

			if (configs_count == 0) {
				std::cerr << "Failed to find any suitable configs." << std::endl;
				return;
			}
			config = configs[0];
		}

		//TODO: perhaps dump config info for later debugging help?

		//create a surface (required when making a context current):
		EGLSurface surface = EGL_NO_SURFACE;
		{
			const EGLint attrib_list[] = {
				EGL_WIDTH, 16,
				EGL_HEIGHT, 16,
				EGL_NONE
			};
			surface = eglCreatePbufferSurface(display, config, attrib_list);
			if (surface == EGL_NO_SURFACE) {
				std::cerr << "Failed to create surface." << std::endl;
				return;
			}
		}

		//create a context (required for rendering commands):
		EGLContext context = EGL_NO_CONTEXT;
		{
			const EGLint attrib_list[] = {
				EGL_CONTEXT_MAJOR_VERSION, 3,
				EGL_NONE
			};

			context = eglCreateContext(display, config, EGL_NO_CONTEXT, attrib_list);
			if (context == EGL_NO_CONTEXT) {
				std::cerr << "Failed to create context." << std::endl;
				return;
			}

		}

		{ //make context current:
			if (EGLBoolean res = eglMakeCurrent(display, surface, surface, context);
			    res != EGL_TRUE) {
				std::cerr << "Failed to make surface current." << std::endl;
				return;
			}
		}

		//--------------------
		//At this point, OpenGL ES should be good to go!

		XR::PlatformInfo platform;
		platform.application_vm = app->activity->vm;
		platform.application_activity = app->activity->clazz;
		platform.egl_display = display;
		platform.egl_config = config;
		platform.egl_context = context;

		std::unique_ptr< XR > xr(new XR(platform, "gp23 OpenXR example", 1));


	//....



		//----- teardown -----

		//OpenXR connection:
		xr.reset();

		//EGL stuff:

		eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);

		if (display && context) {
			eglDestroyContext(display, context);
			context = EGL_NO_CONTEXT;
		}

		if (display && surface) {
			eglDestroySurface(display, surface);
			surface = EGL_NO_SURFACE;
		}

		if (display) {
			eglTerminate(display);
			display = EGL_NO_DISPLAY;
		}

		//Detach from jvm:
		app->activity->vm->DetachCurrentThread();

	} catch (std::exception &e) {
		std::cerr << "Exception: " << e.what() << std::endl;
	} catch (...) {
		std::cerr << "some other error" << std::endl;
	}

}

#else //__ANDROID__

#ifdef _WIN32
extern "C" { uint32_t GetACP(); }
#endif

int main(int argc, char **argv) {
#ifdef _WIN32
	{ //when compiled on windows, check that code page is forced to utf-8 (makes file loading/saving work right):
		//see: https://docs.microsoft.com/en-us/windows/apps/design/globalizing/use-utf8-code-page
		uint32_t code_page = GetACP();
		if (code_page == 65001) {
			std::cout << "Code page is properly set to UTF-8." << std::endl;
		} else {
			std::cout << "WARNING: code page is set to " << code_page << " instead of 65001 (UTF-8). Some file handling functions may fail." << std::endl;
		}
	}

	//when compiled on windows, unhandled exceptions don't have their message printed, which can make debugging simple issues difficult.
	try {
#endif


	//------------ initialization ------------

	//Initialize SDL library:
	SDL_Init(SDL_INIT_VIDEO);

	//Ask for an OpenGL context version 3.3, core profile, enable debug:
	SDL_GL_ResetAttributes();
	SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);
	//4.3 is *MIN SUPPORTED* by SteamVR's OpenXR
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);

	//create window:
	SDL_Window *window = SDL_CreateWindow(
		"gp23: openxr demo code", //TODO: remember to set a title for your game!
		SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
		1280, 720, //TODO: modify window size if you'd like
		SDL_WINDOW_OPENGL
		| SDL_WINDOW_RESIZABLE //uncomment to allow resizing
		| SDL_WINDOW_ALLOW_HIGHDPI //uncomment for full resolution on high-DPI screens
	);

	//prevent exceedingly tiny windows when resizing:
	SDL_SetWindowMinimumSize(window,100,100);

	if (!window) {
		std::cerr << "Error creating SDL window: " << SDL_GetError() << std::endl;
		return 1;
	}

	//Create OpenGL context:
	SDL_GLContext context = SDL_GL_CreateContext(window);

	if (!context) {
		SDL_DestroyWindow(window);
		std::cerr << "Error creating OpenGL context: " << SDL_GetError() << std::endl;
		return 1;
	}

	//On windows, load OpenGL entrypoints: (does nothing on other platforms)
	init_GL();

	//Set VSYNC + Late Swap (prevents crazy FPS):
	if (SDL_GL_SetSwapInterval(-1) != 0) {
		std::cerr << "NOTE: couldn't set vsync + late swap tearing (" << SDL_GetError() << ")." << std::endl;
		if (SDL_GL_SetSwapInterval(1) != 0) {
			std::cerr << "NOTE: couldn't set vsync (" << SDL_GetError() << ")." << std::endl;
		}
	}

	int non_xr_swap_interval = SDL_GL_GetSwapInterval();

	//Hide mouse cursor (note: showing can be useful for debugging):
	//SDL_ShowCursor(SDL_DISABLE);

	//------------ OpenXR init ------------

	try {
		xr = new XR(
			window, context, //needed for SDL / desktop OpenGL mode
			"gp23 OpenXR example", 1); //params are application name, application version
	} catch (std::runtime_error &e) {
		std::cerr << "Failed to initialize OpenXR: " << e.what() << std::endl;
		return 1;
	}

	//------------ load assets --------------
	call_load_functions();

	//------------ create game mode + make current --------------
	Mode::set_current(std::make_shared< PlayMode >());

	//------------ main loop ------------

	//this inline function will be called whenever the window is resized,
	// and will update the window_size and drawable_size variables:
	glm::uvec2 window_size; //size of window (layout pixels)
	glm::uvec2 drawable_size; //size of drawable (physical pixels)
	//On non-highDPI displays, window_size will always equal drawable_size.
	auto on_resize = [&](){
		int w,h;
		SDL_GetWindowSize(window, &w, &h);
		window_size = glm::uvec2(w, h);
		SDL_GL_GetDrawableSize(window, &w, &h);
		drawable_size = glm::uvec2(w, h);
		glViewport(0, 0, drawable_size.x, drawable_size.y);
	};
	on_resize();

	//This will loop until the current mode is set to null:
	while (Mode::current) {
		//every pass through the game loop creates one frame of output
		//  by performing three steps:

		{ //(1) process any events that are pending
			static SDL_Event evt;
			while (SDL_PollEvent(&evt) == 1) {
				//handle resizing:
				if (evt.type == SDL_WINDOWEVENT && evt.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
					on_resize();
				}
				//handle input:
				if (Mode::current && Mode::current->handle_event(evt, window_size)) {
					// mode handled it; great
				} else if (evt.type == SDL_QUIT) {
					Mode::set_current(nullptr);
					break;
				} else if (evt.type == SDL_KEYDOWN && evt.key.keysym.sym == SDLK_PRINTSCREEN) {
					// --- screenshot key ---
					std::string filename = "screenshot.png";
					std::cout << "Saving screenshot to '" << filename << "'." << std::endl;
					glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
					glReadBuffer(GL_FRONT);
					int w,h;
					SDL_GL_GetDrawableSize(window, &w, &h);
					std::vector< glm::u8vec4 > data(w*h);
					glReadPixels(0,0,w,h, GL_RGBA, GL_UNSIGNED_BYTE, data.data());
					for (auto &px : data) {
						px.a = 0xff;
					}
					save_png(filename, glm::uvec2(w,h), data.data(), LowerLeftOrigin);
				}
			}
			if (!Mode::current) break;
		}

		//process xr events also (these are things like state changes):
		if (xr) xr->poll_events();

		if (xr && xr->running) {
			xr->wait_frame(); //wait for the next frame that needs to be rendered
			xr->begin_frame(); //indicate that rendering has started on this frame (NOTE: could *probably* do this later to lower latency if head position isn't important for update)
		}

		{ //(2) call the current mode's "update" function to deal with elapsed time:
			float elapsed;
			{ //time update from system clock:
				auto current_time = std::chrono::high_resolution_clock::now();
				static auto previous_time = current_time;
				elapsed = std::chrono::duration< float >(current_time - previous_time).count();
				previous_time = current_time;
			}

			//override with time update from OpenXR (if available):
			if (xr && xr->running) {
				static auto previous_time = xr->next_frame.display_time;
				auto current_time = xr->next_frame.display_time;
				elapsed = (current_time - previous_time) * 1.0e-9; //nanoseconds -> seconds
				previous_time = current_time;
			}

			//if frames are taking a very long time to process,
			//lag to avoid spiral of death:
			elapsed = std::min(0.1f, elapsed);

			Mode::current->update(elapsed);
			if (!Mode::current) break;
		}

		{ //(3) call the current mode's "draw" function to produce output:
			//(note: playmode has extra logic in here to deal with xr's views array)
			Mode::current->draw(drawable_size);
		}

		if (xr && xr->running) {
			xr->end_frame();
		}

		//Wait until the recently-drawn frame is shown before doing it all again:
		if (xr && xr->running) {
			if (SDL_GL_GetSwapInterval() != 0) {
				//NOTE: in xr mode actually don't wait!
				SDL_GL_SetSwapInterval(0);
				std::cout << "Note: XR is running, so don't wait on vsync." << std::endl;
			}
		} else {
			if (SDL_GL_GetSwapInterval() != non_xr_swap_interval) {
				SDL_GL_SetSwapInterval(non_xr_swap_interval);
				std::cout << "Note: XR is not running, switching back to vsync." << std::endl;
			}
		}
		SDL_GL_SwapWindow(window); //NOTE: should probably *not* do this while also trying to render to vr stuff because it will block(!)
	}

	//------------  teardown ------------

	if (xr) {
		delete xr;
		xr = nullptr;
	}

	SDL_GL_DeleteContext(context);
	context = 0;

	SDL_DestroyWindow(window);
	window = NULL;

	return 0;

#ifdef _WIN32
	} catch (std::exception const &e) {
		std::cerr << "Unhandled exception:\n" << e.what() << std::endl;
		return 1;
	} catch (...) {
		std::cerr << "Unhandled exception (unknown type)." << std::endl;
		throw;
	}
#endif
}

#endif //__ANDROID__ else
