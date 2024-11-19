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
#include <android/log.h>
#include <unistd.h>

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

//This delightful redirect hack based on:
// https://codelab.wordpress.com/2014/11/03/how-to-use-standard-output-streams-for-logging-in-android-apps/

static int pfd[2];
static pthread_t thr;
static const char *tag = "GP23-Example";

static void *thread_func(void*) {
	ssize_t rdsz;
	char buf[128];
	__android_log_write(ANDROID_LOG_DEBUG, tag, "[log thread begin]");
	while((rdsz = read(pfd[0], buf, sizeof buf - 1)) > 0) {
		if(buf[rdsz - 1] == '\n') --rdsz;
		buf[rdsz] = 0; // add null-terminator
		__android_log_write(ANDROID_LOG_DEBUG, tag, buf);
	}
	__android_log_write(ANDROID_LOG_DEBUG, tag, "[log thread exit]");
	return 0;
}

int start_logger() {

	// make stdout line-buffered and stderr unbuffered
	setvbuf(stdout, 0, _IOLBF, 0);
	setvbuf(stderr, 0, _IONBF, 0);

	// create the pipe and redirect stdout and stderr 
	pipe(pfd);
	dup2(pfd[1], 1);
	dup2(pfd[1], 2);

	// spawn the logging thread
	if (pthread_create(&thr, 0, thread_func, 0) == -1) return -1;

	pthread_detach(thr);
	return 0;
}

bool resumed = false;

//Based on tchow Rainbow's android/main.cpp
// + modifications inspired by OVR OpenXR SDK's XrApp.cpp
static void handle_cmd(android_app *app, int32_t cmd) {
	if (cmd == APP_CMD_INIT_WINDOW) {
		std::cout << "APP_CMD_INIT_WINDOW" << std::endl;
		//ignore
	} else if (cmd == APP_CMD_TERM_WINDOW) {
		std::cout << "APP_CMD_TERM_WINDOW" << std::endl;
		//ignore
	} else if (cmd == APP_CMD_START) {
		std::cout << "APP_CMD_START" << std::endl;
		//ignore
	} else if (cmd == APP_CMD_PAUSE) {
		std::cout << "APP_CMD_PAUSE" << std::endl;
		resumed = false;
	} else if (cmd == APP_CMD_STOP) {
		std::cout << "APP_CMD_STOP" << std::endl;
		//ignore
	} else if (cmd == APP_CMD_RESUME) {
		std::cout << "APP_CMD_RESUME" << std::endl;
		resumed = true;
	} else if (cmd == APP_CMD_DESTROY) {
		std::cout << "APP_CMD_DESTROY" << std::endl;
		//ignore
	} else {
		//there are others! android has a lot of states :-/
		//ignore
	}
}

//used by asset_stream():
ANativeActivity *activity = nullptr;

//modeled on OpenXR's "hello_xr" example's main.cpp:
//  https://github.com/KhronosGroup/OpenXR-SDK-Source/blob/main/src/tests/hello_xr/main.cpp
void android_main(struct android_app* app) {
	activity = app->activity;

	app->onAppCmd = handle_cmd;
	app->destroyRequested = 0;

	if (start_logger() != 0) {
		__android_log_write(ANDROID_LOG_FATAL, tag, "Failed to start log thread!");
		return;
	}

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
				//EGL_BIND_TO_TEXTURE_RGB, EGL_TRUE,
				EGL_BIND_TO_TEXTURE_RGBA, EGL_TRUE,
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

		{ //dump info about config for debugging purposes
			EGLint value = 0;
			std::string info = "Chosen config attributes:\n";
			#define DO(name) \
				info += "  " #name ": "; \
				if (EGLBoolean res = eglGetConfigAttrib(display, config, name, &value); \
				    res == EGL_TRUE) { \
					info += std::to_string(value) + "\n"; \
				} else { \
					info += "(failed to get value!)\n"; \
				}

			DO(EGL_RED_SIZE)
			DO(EGL_GREEN_SIZE)
			DO(EGL_BLUE_SIZE)
			DO(EGL_ALPHA_SIZE)
			DO(EGL_DEPTH_SIZE)
			DO(EGL_STENCIL_SIZE)
			DO(EGL_ALPHA_MASK_SIZE)
			DO(EGL_SAMPLE_BUFFERS)
			DO(EGL_SAMPLES)
			DO(EGL_BIND_TO_TEXTURE_RGB)
			DO(EGL_BIND_TO_TEXTURE_RGBA)
			DO(EGL_BUFFER_SIZE)
			DO(EGL_COLOR_BUFFER_TYPE)
			DO(EGL_CONFIG_CAVEAT)
			DO(EGL_CONFIG_ID)
			DO(EGL_CONFORMANT)
			DO(EGL_LEVEL)
			DO(EGL_LUMINANCE_SIZE)
			DO(EGL_MAX_PBUFFER_WIDTH)
			DO(EGL_MAX_PBUFFER_HEIGHT)
			DO(EGL_MAX_PBUFFER_PIXELS)
			DO(EGL_MAX_SWAP_INTERVAL)
			DO(EGL_MIN_SWAP_INTERVAL)
			DO(EGL_NATIVE_RENDERABLE)
			DO(EGL_NATIVE_VISUAL_ID)
			DO(EGL_NATIVE_VISUAL_TYPE)
			DO(EGL_RENDERABLE_TYPE)
			DO(EGL_SURFACE_TYPE)
			DO(EGL_TRANSPARENT_TYPE)
			DO(EGL_TRANSPARENT_RED_VALUE)
			DO(EGL_TRANSPARENT_GREEN_VALUE)
			DO(EGL_TRANSPARENT_BLUE_VALUE)

			#undef DO
			std::cout << info; std::cout.flush();
		}

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
				EGL_CONTEXT_MINOR_VERSION, 2,
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

		//At this point, OpenGL ES should be good to go!

		//--------------------
		// OpenXR setup (using XR helper struct -- see XR.*pp)

		XR::PlatformInfo platform;
		platform.application_vm = app->activity->vm;
		platform.application_activity = app->activity->clazz;
		platform.egl_display = display;
		platform.egl_config = config;
		platform.egl_context = context;

		xr = new XR(platform, "gp23 OpenXR example", 1);

		// At this point OpenXR stuff should be ready to use!

		//--------------------
		//load resources
		call_load_functions();
		
		//------------ create game mode + make current --------------
		Mode::set_current(std::make_shared< PlayMode >());

		//--------------------
		//main loop

		while (!app->destroyRequested && Mode::current) {
			//based on https://developer.android.com/ndk/samples/sample_na
			// with some modifications from XrApp.cpp from OVR OpenXR SDK
			{ //read android events:
				int ident = 0;
				int events = 0;
				struct android_poll_source *source = NULL;

				bool animating = resumed || xr->session != XR_NULL_HANDLE || app->destroyRequested != 0;
				while ((ident = ALooper_pollAll((animating ? 0 : -1), NULL, &events, (void **)&source)) >= 0) {
					if (source != NULL) {
						source->process(app, source);
					}

					if (app->destroyRequested != 0) {
						break;
					}
				}
			}

			//read XR events:
			bool was_running = xr->running;
			xr->poll_events();
			if (xr->running != was_running) {
				if (xr->running) {
					std::cerr << "XR is running!" << std::endl;
				} else {
					std::cerr << "XR is stopped." << std::endl;
				}
			}

			if (xr->running) {
				xr->wait_frame(); //wait for the next frame that needs to be rendered
				xr->begin_frame(); //indicate that rendering has started on this frame (NOTE: could *probably* do this later to lower latency if head position isn't important for update)
			}

			//compute elapsed time for update:
			float elapsed;
			if (xr->running) {
				static auto previous_time = xr->next_frame.display_time;
				auto current_time = xr->next_frame.display_time;
				elapsed = (current_time - previous_time) * 1.0e-9; //nanoseconds -> seconds
				previous_time = current_time;
			} else {
				//let's just make time not pass when xr isn't running:
				elapsed = 0.0f;
			}

			//get current mode to update:
			Mode::current->update(std::min(0.1f, elapsed));
			if (!Mode::current) break;

			//(note: playmode has extra logic in here to deal with xr's views array)
			Mode::current->draw(glm::uvec2(10,10)); //passing dummy drawable_size; ignored because it will just render to swapchain images instead

			if (xr->running) {
				xr->end_frame();
			}

		} //end of main loop



		//----- teardown -----

		//OpenXR connection:
		if (xr) {
			delete xr;
			xr = nullptr;
		}

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

	activity = nullptr;
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
			XR::PlatformInfo{
				.window = window, .context = context, //needed for SDL / desktop OpenGL mode
			},
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
