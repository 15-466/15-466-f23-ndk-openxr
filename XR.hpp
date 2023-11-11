#pragma once

//XR handles the interface to OpenXR.

#include "GL.hpp"

#include <openxr/openxr.h>
#include <glm/glm.hpp>

#ifdef __ANDROID__
//android-only includes:
#include <EGL/egl.h>

#else
//desktop-only includes:
#include <SDL.h>

#endif //__ANDROID__

#include <array>
#include <string>
#include <vector>

struct XR {

	//extra platform info needed for different variants of xrCreateInstance and 
	struct PlatformInfo {
		#ifdef __ANDROID__
		void *application_vm = nullptr; //app->activity->vm
		void *application_activity = nullptr; //app->activity->clazz
		EGLDisplay egl_display = EGL_NO_DISPLAY;
		EGLConfig  egl_config = NULL;
		EGLContext egl_context = EGL_NO_CONTEXT;
		#else
		SDL_Window *window = nullptr;
		SDL_GLContext context = nullptr;
		#endif
	};

	//set up xrInstance:
	// throws a std::runtime_error() if initialization fails
	//NOTE: must only do with a valid OpenGL (/ OpenGLES) context!
	XR(
		PlatformInfo const &platform,
		std::string const &application_name,
		uint32_t application_version,
		std::string const &engine_name = "",
		uint32_t engine_version = 0
	);

	//clean up; destroy xrInstance:
	~XR();

	//helper for error reporting:
	std::string to_string(XrResult result) const;

	//call frequently (e.g., every frame):
	void poll_events();

	//update by poll_events():
	XrSessionState session_state = XR_SESSION_STATE_UNKNOWN;
	bool running = false; //is the session running?

	//if running is true, you should call (in this order):
	void wait_frame(); //wait for the next frame that needs to be rendered; updates next_frame members:

	struct NextFrameInfo {
		int64_t display_time = 0; //nanoseconds; (also a predicted value)
		bool should_render = false;
	} next_frame;

	void begin_frame(); //indicate that rendering has started (call even if should_render = false; but don't do GPU work); updates views' fov, pose, and current_framebuffer
	void end_frame(); //indicate that rendering has finished


	//------------------

	//all OpenXR communication happens via an instance:
	XrInstance instance{XR_NULL_HANDLE};

	//actual XR interactions happen within a session:
	XrSession session{XR_NULL_HANDLE};

	//stage space is a space with the origin on the floor in the center of the play area, +Y up, and x and z aligned to the floor:
	XrSpace stage{XR_NULL_HANDLE};

	//sessions need a swapchain for every view they will present:
	glm::uvec2 size = glm::uvec2(0); //swapchain image size (same for both eyes)

	struct View {
		XrSwapchain swapchain{XR_NULL_HANDLE};
		struct Framebuffer {
			GLuint color_tex = 0; //managed by swapchain
			GLuint depth_rb = 0; //managed by XR
			GLuint fb = 0; //managed by XR
		};
		std::vector< Framebuffer > framebuffers;

		//set every frame (in begin_frame()):
		const Framebuffer *current_framebuffer = nullptr; //the framebuffer to render into
		XrFovf fov; //field of view as four angles from center
		XrPosef pose; //pose as orientation (quat) + position (vec3)
	};

	std::array< View, 2 > views; //[0] is left, [1] is right

};

extern XR *xr; //global variable to hold singleton XR instance (created in main)
