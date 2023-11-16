#include "XR.hpp"

#include "GL.hpp"

#ifdef __ANDROID__
//android stuff:
#include <jni.h>

#define XR_USE_GRAPHICS_API_OPENGL_ES
#define XR_USE_PLATFORM_ANDROID
#include <openxr/openxr_platform.h>

#else
//linux stuff:
#include <X11/Xlib.h>
#include <GL/glx.h>

#define XR_USE_GRAPHICS_API_OPENGL
#define XR_USE_PLATFORM_XLIB
#include <openxr/openxr_platform.h>

#include <SDL_syswm.h>

#endif

#include "gl_errors.hpp"


#include <openxr/openxr_reflection.h>


#include <iostream>
#include <sstream>
#include <cstring>
#include <vector>
#include <array>
#include <cassert>
#include <thread>

XR *xr = nullptr;

XR::XR(
	PlatformInfo const &platform,
	std::string const &application_name,
	uint32_t application_version,
	std::string const &engine_name,
	uint32_t engine_version
) {
	std::cout << "--- initializing OpenXR ---" << std::endl;

#ifdef __ANDROID__
	{ //call xrInitializeLoaderKHR (based on XrApp.cpp from OVR OpenXR SDK):
		PFN_xrInitializeLoaderKHR xrInitializeLoaderKHR;
		xrGetInstanceProcAddr(XR_NULL_HANDLE, "xrInitializeLoaderKHR", reinterpret_cast< PFN_xrVoidFunction* >(&xrInitializeLoaderKHR));
		if (xrInitializeLoaderKHR != NULL) {
			XrLoaderInitInfoAndroidKHR loader_init_info = {XR_TYPE_LOADER_INIT_INFO_ANDROID_KHR};
			loader_init_info.applicationVM = platform.application_vm;
			loader_init_info.applicationContext = platform.application_activity; //TODO: is this right? OVR sample does it, but docs say this should be a "android.content.Context" (which seems different from the Activity below)
        	if (XrResult res = xrInitializeLoaderKHR(reinterpret_cast< XrLoaderInitInfoBaseHeaderKHR *>(&loader_init_info));
			    res != XR_SUCCESS) {
				std::cerr << "WARNING: xrInitializeLoaderKHR failed: " << to_string(res) << std::endl;
			}
		}
    }
#endif //__ANDROID__

	XrInstanceCreateInfo create_info{XR_TYPE_INSTANCE_CREATE_INFO};

	//with reference to openxr_program.cpp from openxr-sdk-source + the openxr specification

	//XR_MAX_APPLICATION_NAME_SIZE-1 because std::string.size() doesn't count null bytes
	std::memcpy(create_info.applicationInfo.applicationName, application_name.c_str(),
		std::min(application_name.size(), size_t(XR_MAX_APPLICATION_NAME_SIZE-1)) );

	create_info.applicationInfo.applicationVersion = application_version;

	std::memcpy(create_info.applicationInfo.engineName, engine_name.c_str(),
		std::min(engine_name.size(), size_t(XR_MAX_ENGINE_NAME_SIZE-1)) );

	create_info.applicationInfo.engineVersion = engine_version;

	create_info.applicationInfo.apiVersion = XR_CURRENT_API_VERSION;

#ifdef __ANDROID__
	//extra android-specific creation info:
	XrInstanceCreateInfoAndroidKHR create_info_android{XR_TYPE_INSTANCE_CREATE_INFO_ANDROID_KHR};

	create_info_android.applicationVM = platform.application_vm;
	create_info_android.applicationActivity = platform.application_activity;

	create_info.next = &create_info_android;
#endif //__ANDROID__

	std::vector< const char * > extensions{
		#ifdef __ANDROID__
		XR_KHR_ANDROID_CREATE_INSTANCE_EXTENSION_NAME,
		XR_KHR_OPENGL_ES_ENABLE_EXTENSION_NAME,
		#else
		XR_KHR_OPENGL_ENABLE_EXTENSION_NAME,
		#endif
	};

	create_info.enabledExtensionCount = uint32_t(extensions.size());
	create_info.enabledExtensionNames = extensions.data();

	if (XrResult res = xrCreateInstance(&create_info, &instance);
	    res != XR_SUCCESS) {
		instance = XR_NULL_HANDLE; //just in case create broke something
		throw std::runtime_error("Failed to create OpenXR instance: " + to_string(res));
	}

	{ //Query the instance to learn what runtime this is using:
		XrInstanceProperties properties{XR_TYPE_INSTANCE_PROPERTIES};
		if (XrResult res = xrGetInstanceProperties(instance, &properties);
		    res != XR_SUCCESS) {
			throw std::runtime_error("Failed to get OpenXR instance properties: " + to_string(res));
		}
	
		//Information:
		std::cout << "OpenXR Runtime is '" << properties.runtimeName << "', version "
			<< (properties.runtimeVersion >> 48)
			<< "." << ((properties.runtimeVersion >> 32) & 0xffff)
			<< "." << (properties.runtimeVersion & 0xffffffff)
			<< std::endl;
	}
	
	//----- now select the *system* -----

	XrSystemGetInfo system_info{XR_TYPE_SYSTEM_GET_INFO};

	system_info.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;

	XrSystemId system_id{XR_NULL_SYSTEM_ID};

	if (XrResult res = xrGetSystem(instance, &system_info, &system_id);
	    res != XR_SUCCESS) {
		throw std::runtime_error("Failed to get a system: " + to_string(res));
	}

	{ //Query the system to learn a bit more:
		XrSystemProperties properties{XR_TYPE_SYSTEM_PROPERTIES};

		if (XrResult res = xrGetSystemProperties(instance, system_id, &properties);
		    res != XR_SUCCESS) {
			throw std::runtime_error("Failed to get system properties: " + to_string(res));
		}
		std::cout << "System is '" << properties.systemName << "' with vendorId " << properties.vendorId << "." << std::endl;
		std::cout << "  graphics: maxSwapchainImageWidth x Height is " << properties.graphicsProperties.maxSwapchainImageWidth << "x" << properties.graphicsProperties.maxSwapchainImageHeight << ", maxLayerCount is " << properties.graphicsProperties.maxLayerCount << "." << std::endl;
		std::cout << "  tracking: orientationTracking is " << (properties.trackingProperties.orientationTracking == XR_TRUE ? "true" : "false");
		std::cout << ", positionTracking is " << (properties.trackingProperties.positionTracking == XR_TRUE ? "true" : "false") << "." << std::endl;

		//note that the next field might actually have a long list of structures that might be interesting; depending on (whatever).
	}

	//----- graphics requirements -----
	//NOTE: this is required before xrCreateSession(!)
	//based on ovr-openxr-sdk's XrApp.cpp

	{ //report OpenGL[ES} version support
		#ifdef __ANDROID__
		//OpenGL ES version:
		PFN_xrGetOpenGLESGraphicsRequirementsKHR xrGetOpenGLESGraphicsRequirementsKHR = nullptr;
		if (XrResult res = xrGetInstanceProcAddr(instance, "xrGetOpenGLESGraphicsRequirementsKHR",(PFN_xrVoidFunction*)(&xrGetOpenGLESGraphicsRequirementsKHR));
		    res != XR_SUCCESS) {
			throw std::runtime_error("Failed to get xrGetOpenGLESGraphicsRequrirementsKHR function pointer: " + to_string(res));
		}
		XrGraphicsRequirementsOpenGLESKHR graphics_requirements = {XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_ES_KHR};
		if (XrResult res = xrGetOpenGLESGraphicsRequirementsKHR(instance, system_id, &graphics_requirements);
		    res != XR_SUCCESS) {
			throw std::runtime_error("Failed to get OpenGLES graphics requirements: " + to_string(res));
		}
		#else
		//OpenGL version:
		PFN_xrGetOpenGLGraphicsRequirementsKHR xrGetOpenGLGraphicsRequirementsKHR = nullptr;
		if (XrResult res = xrGetInstanceProcAddr(instance, "xrGetOpenGLGraphicsRequirementsKHR",(PFN_xrVoidFunction*)(&xrGetOpenGLGraphicsRequirementsKHR));
		    res != XR_SUCCESS) {
			throw std::runtime_error("Failed to get xrGetOpenGLGraphicsRequrirementsKHR function pointer: " + to_string(res));
		}
		XrGraphicsRequirementsOpenGLKHR graphics_requirements = {XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_KHR};
		if (XrResult res = xrGetOpenGLGraphicsRequirementsKHR(instance, system_id, &graphics_requirements);
		    res != XR_SUCCESS) {
			throw std::runtime_error("Failed to get OpenGL graphics requirements: " + to_string(res));
		}
		#endif
		std::cout << "OpenGL requirements:\n";
		std::cout << "  min supported version: " << (graphics_requirements.minApiVersionSupported >> 48) << "." << ((graphics_requirements.minApiVersionSupported >> 32) & 0xffff) << "\n";
		std::cout << "  max known supported version: " << (graphics_requirements.maxApiVersionSupported >> 48) << "." << ((graphics_requirements.maxApiVersionSupported >> 32) & 0xffff) << " (OpenXR spec says newer may work fine, though)\n";
		std::cout.flush();

		{ //compare to OpenGL's reported version:
			GLint major = 0;
			GLint minor = 0;
			glGetIntegerv(GL_MAJOR_VERSION, &major);
			glGetIntegerv(GL_MINOR_VERSION, &minor);
			std::cout << "Current OpenGL version is " << major << "." << minor << std::endl;
			if (XR_MAKE_VERSION(major, minor, 0) < graphics_requirements.minApiVersionSupported) {
				std::cerr << "ERROR: reported OpenGL version (" << major << "." << minor << ") is less than the minimum that OpenXR reports as supporting. (Continuing, but don't expect things to work.)" << std::endl;
			}
			if (XR_MAKE_VERSION(major, minor, 0) > graphics_requirements.maxApiVersionSupported) {
				std::cerr << "WARNING: reported OpenGL version (" << major << "." << minor << ") is larger the maximum that OpenXR reports supporting. (Continuing, since this could still be okay.)" << std::endl;
			}
		}
	}

	{ //----- session creation -----

		#ifdef __ANDROID__
		//set up XrGraphicsBindingOpenGLESAndroid structure (OpenGL ES on Android):
		XrGraphicsBindingOpenGLESAndroidKHR binding{XR_TYPE_GRAPHICS_BINDING_OPENGL_ES_ANDROID_KHR};

		binding.display = platform.egl_display;
		binding.config = platform.egl_config;
		binding.context = platform.egl_context;

		#else //__ANDROID__
		//set up XrGraphicsBindingOpenGLXlibKHR structure (OpenGL on Xlib bindings):

		//getting window manager info as per example in SDL_syswm.h comment:
		SDL_SysWMinfo info;
		SDL_VERSION(&info.version);
		if ( !SDL_GetWindowWMInfo(window, &info) ) {
			throw std::runtime_error("Failed to get window manager info from SDL: " + std::string(SDL_GetError()));
		}

		//to extract the window's Visual (and, eventually, visualid):
		XWindowAttributes window_attributes;
		if (Status res = XGetWindowAttributes(info.info.x11.display, info.info.x11.window, &window_attributes);
		    res == 0) {
			throw std::runtime_error("Failed to get window attributes (needed to extract visual).");
		}

		//appears to be the case on SDL's x11 opengl driver:
		GLXContext glx_context = reinterpret_cast< GLXContext >(context);

		//How to get the current fbconfig; thanks to a suggestion from:
		//  https://stackoverflow.com/questions/74104449/getting-the-current-glxfbconfig-in-glx
		int fbconfig_id = 0;
		if (int ret = glXQueryContext(info.info.x11.display, glx_context, GLX_FBCONFIG_ID, &fbconfig_id);
		    ret != Success) {
			throw std::runtime_error("Failed to query FBConfig id from glX context.");
		}

		int fb_configs_size = 0;
		int attribs[3] = { GLX_FBCONFIG_ID, fbconfig_id, None };
		GLXFBConfig *fb_configs = glXChooseFBConfig(
			info.info.x11.display,
			XScreenNumberOfScreen(window_attributes.screen),
			attribs,
			&fb_configs_size);

		if (fb_configs == NULL || fb_configs_size == 0) {
			throw std::runtime_error("Failed to select FBConfig by id.");
		}
		if (fb_configs_size > 1) {
			std::cerr << "WARNING: got more than one config with the id from the current context; using the first, but this may break things." << std::endl;
		}

		XrGraphicsBindingOpenGLXlibKHR binding{XR_TYPE_GRAPHICS_BINDING_OPENGL_XLIB_KHR};
		binding.xDisplay = info.info.x11.display;
		binding.visualid = XVisualIDFromVisual(window_attributes.visual);
		binding.glxFBConfig = fb_configs[0];
		binding.glxDrawable = glXGetCurrentDrawable();
		binding.glxContext = glx_context;
		#endif


		XrSessionCreateInfo create_info{XR_TYPE_SESSION_CREATE_INFO};
		create_info.next = &binding;
		create_info.createFlags = 0; //no flags specified yet
		create_info.systemId = system_id;


		if (XrResult res = xrCreateSession(instance, &create_info, &session);
		    res != XR_SUCCESS) {
			throw std::runtime_error("Failed to create session: " + to_string(res));
		}

		#ifndef __ANDROID__
		//on linux, must free fb_configs:

		XFree(fb_configs); //"use XFree to free the memory returned by glXChooseFBConfig"

		#endif //__ANDROID__
	}

	{ //create stage space:
		XrReferenceSpaceCreateInfo create_info{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
		create_info.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_STAGE;
		create_info.poseInReferenceSpace.orientation.w = 1.0f; //everything else can be zero from value-initialization, but orientation needs to be unit-length

		if (XrResult res = xrCreateReferenceSpace(session, &create_info, &stage);
		    res != XR_SUCCESS) {
			throw std::runtime_error("Failed to create stage space: " + to_string(res));
		}
	}

	{ //look up view information
		std::array< XrViewConfigurationView, 2 > views;
		views.fill(XrViewConfigurationView{XR_TYPE_VIEW_CONFIGURATION_VIEW});
		const uint32_t views_capacity = uint32_t(views.size());
		uint32_t views_count = 0;

		if (XrResult res = xrEnumerateViewConfigurationViews(instance, system_id, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, views_capacity, &views_count, views.data());
		    res != XR_SUCCESS) {
			throw std::runtime_error("Failed to get view configuration views.");
		}
		assert(views_count == 2); //OpenXR spec says it *must* be two views
		size.x = views[0].recommendedImageRectWidth;
		size.y = views[0].recommendedImageRectHeight;

		std::cout << "Will create images of size " << size.x << "x" << size.y << "." << std::endl;
	}

	{ //swapchain creation
		uint32_t format_capacity = 0;
		//query needed format capacity:
		if (XrResult res = xrEnumerateSwapchainFormats(session, 0, &format_capacity, nullptr);
		    res != XR_SUCCESS) {
			throw std::runtime_error("Failed to count swapchain formats: " + to_string(res));
		}
		//allocate enough space and get them:
		std::vector< int64_t > formats(format_capacity, 0);
		uint32_t format_count = 0;
		if (XrResult res = xrEnumerateSwapchainFormats(session, format_capacity, &format_count, formats.data());
		    res != XR_SUCCESS) {
			throw std::runtime_error("Failed to enumerate swapchain formats: " + to_string(res));
		}

		GLenum wanted_format = GL_SRGB8_ALPHA8;
		std::string wanted_format_name = "GL_SRGB8_ALPHA8"; //for error/info messages
		bool have_wanted_format = false;

		std::cout << "Got " << format_count << " swapchain formats." << std::endl;
		for (uint32_t f = 0; f < format_count; ++f) {
			int64_t format = formats[f];
			if (format == wanted_format) have_wanted_format = true;

			std::cout << "  [" << f << "]: ";
			#define DO(fmt) if (format == fmt) { std::cout << #fmt; } else
			DO(GL_RGB8)
			DO(GL_RGBA8)
			DO(GL_SRGB8)
			DO(GL_SRGB8_ALPHA8)
			DO(GL_RGBA16F)
			DO(GL_RGBA32F)
			DO(GL_DEPTH_COMPONENT16)
			DO(GL_DEPTH_COMPONENT24)
			DO(GL_DEPTH_COMPONENT32F)
			{ std::cout << " as-of-yet untranslated enum 0x" << std::hex << format << std::dec; }
			std::cout << std::endl;
			#undef DO
		}

		if (!have_wanted_format) {
			throw std::runtime_error(wanted_format_name + " was not among the preferred formats; this code expects it to be.");
		} else {
			std::cerr << "Chose " << wanted_format_name << " for swapchain format." << std::endl;
		}

		XrSwapchainCreateInfo create_info{XR_TYPE_SWAPCHAIN_CREATE_INFO};

		create_info.createFlags = 0;
		create_info.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT; //NOTE: USAGE_SAMPLED_BIT was used by the ovr sdk sample; not sure if this is actually needed
		create_info.format = wanted_format;
		create_info.sampleCount = 1;
		create_info.width = size.x;
		create_info.height = size.y;
		create_info.faceCount = 1;
		create_info.arraySize = 1;
		create_info.mipCount = 1;

		for (auto &view : views) {
			if (XrResult res = xrCreateSwapchain(session, &create_info, &view.swapchain);
			    res != XR_SUCCESS) {
				throw std::runtime_error("Failed to create swapchain: " + to_string(res));
			}

			uint32_t chain_length = 0;

			//fetch length:
			if (XrResult res = xrEnumerateSwapchainImages(view.swapchain, 0, &chain_length, NULL);
			    res != XR_SUCCESS) {
				throw std::runtime_error("Failed to get swapchain length: " + to_string(res));
			}
			
			#ifdef __ANDROID__
			std::vector< XrSwapchainImageOpenGLESKHR > images(chain_length, XrSwapchainImageOpenGLESKHR{XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_ES_KHR});
			#else //__ANDROID__
			std::vector< XrSwapchainImageOpenGLKHR > images(chain_length, XrSwapchainImageOpenGLKHR{XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_KHR});
			#endif //__ANDROID__
			//actually fetch image structures:
			if (XrResult res = xrEnumerateSwapchainImages(view.swapchain, uint32_t(images.size()), &chain_length, reinterpret_cast< XrSwapchainImageBaseHeader * >(images.data()));
			    res != XR_SUCCESS) {
				throw std::runtime_error("Failed to get swapchain images: " + to_string(res));
			}
			assert(chain_length == uint32_t(images.size())); //chain hasn't changed length

			view.framebuffers.resize(images.size());
			for (uint32_t i = 0; i < view.framebuffers.size(); ++i) {
				view.framebuffers[i].color_tex = images[i].image;

				//set texture sampling state: (ovr sdk sample does this; not sure if it is needed)
				glBindTexture(GL_TEXTURE_2D, view.framebuffers[i].color_tex);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); //NEAREST might also be an option here
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
				glBindTexture(GL_TEXTURE_2D, 0);

				GL_ERRORS();

				//allocate depth renderbuffer:
				glGenRenderbuffers(1, &view.framebuffers[i].depth_rb);
				glBindRenderbuffer(GL_RENDERBUFFER, view.framebuffers[i].depth_rb);
				glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, size.x, size.y);
				glBindRenderbuffer(GL_RENDERBUFFER, 0);

				GL_ERRORS();

				//allocate framebuffer:
				glGenFramebuffers(1, &view.framebuffers[i].fb);
				glBindFramebuffer(GL_FRAMEBUFFER, view.framebuffers[i].fb);
				glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, view.framebuffers[i].depth_rb);
				glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, view.framebuffers[i].color_tex, 0);

				GL_ERRORS();

				GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);

				if (status != GL_FRAMEBUFFER_COMPLETE) {
					#define DO(name) \
						if (status == name) { throw std::runtime_error("Failed to create a complete framebuffer:" + std::string(#name)); } else

					DO(GL_FRAMEBUFFER_UNDEFINED);
					DO(GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT);
					DO(GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT);
					DO(GL_FRAMEBUFFER_UNSUPPORTED);
					DO(GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE);
					#ifndef __ANDROID__
					DO(GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER);
					DO(GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER);
					DO(GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS);
					#endif
					{
						std::ostringstream str;
						str << "0x" << std::hex << status;
						throw std::runtime_error("Failed to create a complete framebuffer: unknown status " + str.str());
					}
					#undef DO
				}
				glBindFramebuffer(GL_FRAMEBUFFER, 0);

				GL_ERRORS();
			}

			std::cout << "view[" << (&view - &views[0]) << "] has " << view.framebuffers.size() << " images in swapchain." << std::endl;
		}
	}

}

XR::~XR() {
	std::cout << "--- shutting down OpenXR ---" << std::endl;

	if (running) {
		if (XrResult res = xrRequestExitSession(session);
		    res != XR_SUCCESS) {
			std::cerr << "XR failed to xrRequestExitSession: " << to_string(res) << std::endl;
		}
		//wait a moment for session to stop running
		for (uint32_t iter = 0; iter < 10; ++iter) {
			poll_events();
			if (!running) break;
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
		}
		if (running) {
			std::cerr << "Waiting a fair bit of time, but session hasn't returned to idle yet." << std::endl;
		}
	}
	
	for (auto &view : views) {
		for (auto &framebuffer : view.framebuffers) {
			if (framebuffer.fb != 0) {
				glDeleteFramebuffers(1, &framebuffer.fb);
				framebuffer.fb = 0;
			}
			if (framebuffer.depth_rb != 0) {
				glDeleteRenderbuffers(1, &framebuffer.depth_rb);
				framebuffer.depth_rb = 0;
			}
			//don't free color_tex -- xrDestroySwapchain should manage that
		}
		view.framebuffers.clear();

		if (view.swapchain != XR_NULL_HANDLE) {
			if (XrResult res = xrDestroySwapchain(view.swapchain);
			    res != XR_SUCCESS) {
				std::cerr << "XR failed to destroy swapchain: " << to_string(res) << std::endl;
			}
			view.swapchain = XR_NULL_HANDLE;
		}
	}

	if (stage != XR_NULL_HANDLE) {
		std::cout << "  destroying space..." << std::endl;
		if (XrResult res = xrDestroySpace(stage);
		    res != XR_SUCCESS) {
			std::cerr << "XR failed to destroy stage space: " << to_string(res) << std::endl;
		}
		stage = XR_NULL_HANDLE;
	}


	if (session != XR_NULL_HANDLE) {
		std::cout << "  destroying session..." << std::endl;
		if (XrResult res = xrDestroySession(session);
		    res != XR_SUCCESS) {
			std::cerr << "XR failed to destroy session: " << to_string(res) << std::endl;
		}
		session = XR_NULL_HANDLE;
	}

	//FOR SOME REASON... this hangs on SteamVR on linux. Not sure why. Grumble.
	if (instance != XR_NULL_HANDLE) {
		std::cout << "  destroying instance..." << std::endl;
		if (XrResult res = xrDestroyInstance(instance);
		    res != XR_SUCCESS) {
			instance = XR_NULL_HANDLE;
			std::cerr << "XR failed to destroy instance: " << to_string(res) << std::endl;
		}
		instance = XR_NULL_HANDLE;
	}
}

std::string XR::to_string(XrResult result) const {
	std::string ret;
	bool use_adhoc = true;

	if (instance) {
		char buffer[XR_MAX_RESULT_STRING_SIZE];
		XrResult res = xrResultToString(instance, result, buffer);
		if (res == XR_SUCCESS) {
			ret = buffer;
			use_adhoc = false;
		}
	}

	if (use_adhoc) {
		//based on example in openxr_reflection.h:
		#define XR_ENUM_CASE_STR(name, val) if (result == name) { ret = #name; } else
		#define XR_ENUM_STR(enumType) \
			XR_LIST_ENUM_##enumType(XR_ENUM_CASE_STR) \
			{ ret = "Unknown [" + std::to_string(result) + "]"; }
		XR_ENUM_STR(XrResult);
		#undef XR_ENUM_CASE_STR
		#undef XR_ENUM_STR
	}

	return ret;
}


void XR::poll_events() {
	//read events:
	while (true) {
		XrEventDataBuffer event{XR_TYPE_EVENT_DATA_BUFFER};
		//fill event structure:
		if (XrResult res = xrPollEvent(instance, &event);
		    res == XR_EVENT_UNAVAILABLE) {
			break;
		} else if (res != XR_SUCCESS) {
			std::cerr << "XR failed to poll event: " << to_string(res);
			break;
		}
		//interpret event structure:
		if (event.type == XR_TYPE_EVENT_DATA_EVENTS_LOST) {
			XrEventDataEventsLost const &event_ = *reinterpret_cast< XrEventDataEventsLost * >(&event);
			std::cout << "INFO: Lost " << event_.lostEventCount << " events." << std::endl;

		} else if (event.type == XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING) {
			XrEventDataInstanceLossPending const &event_ = *reinterpret_cast< XrEventDataInstanceLossPending * >(&event);
			std::cout << "INFO: Instance loss pending at " << event_.lossTime << "." << std::endl;
		} else if (event.type == XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED) {
			XrEventDataInteractionProfileChanged const &event_ = *reinterpret_cast< XrEventDataInteractionProfileChanged * >(&event);
			std::cout << "INFO: Interaction profile changed in " << (event_.session == session ? "this session" : "some other session (?!)") << "." << std::endl;
		} else if (event.type == XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING) {
			XrEventDataReferenceSpaceChangePending const &event_ = *reinterpret_cast< XrEventDataReferenceSpaceChangePending * >(&event);
			std::cout << "INFO: reference space changing at " << event_.changeTime << "." << std::endl;
		} else if (event.type == XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED) {
			XrEventDataSessionStateChanged const &event_ = *reinterpret_cast< XrEventDataSessionStateChanged * >(&event);
			std::cout << "INFO: session state changed at " << event_.time << " to ";
			#define XR_ENUM_CASE_STR(name, val) if (event_.state == name) { std::cout << #name; } else
			#define XR_ENUM_STR(enumType) \
			XR_LIST_ENUM_##enumType(XR_ENUM_CASE_STR) \
				{ std::cout << "unknown [" << std::to_string(event_.state) << "]"; }
			XR_ENUM_STR(XrSessionState);
			#undef XR_ENUM_CASE_STR
			#undef XR_ENUM_STR
			std::cout << "." << std::endl;

			session_state = event_.state;

			if (session_state == XR_SESSION_STATE_READY) {
				XrSessionBeginInfo begin_info{XR_TYPE_SESSION_BEGIN_INFO};
				begin_info.primaryViewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
				if (XrResult res = xrBeginSession(session, &begin_info);
				    res != XR_SUCCESS) {
					std::cerr << "Error reported beginning session: " << to_string(res) << std::endl;
				}
				running = true;
			} else if (session_state == XR_SESSION_STATE_STOPPING) {
				if (XrResult res = xrEndSession(session);
				    res != XR_SUCCESS) {
					std::cerr << "Error reported ending session: " << to_string(res) << std::endl;
				}
				running = false;
			}
		} else {
			std::cerr << "WARNING: ignoring unrecognized event type " << event.type << "." << std::endl;
		}
	}
}

void XR::wait_frame() {
	XrFrameState frame_state{XR_TYPE_FRAME_STATE};

	if (XrResult res = xrWaitFrame(session, NULL /* XrWaitFrameInfo, is empty as of 1.0 */, &frame_state);
	    res != XR_SUCCESS) {
		std::cerr << "Failed to xrWaitFrame: " << to_string(res) << std::endl;
	}
	next_frame.display_time = frame_state.predictedDisplayTime;
	next_frame.should_render = (frame_state.shouldRender == XR_TRUE);
}

void XR::begin_frame() {
	if (XrResult res = xrBeginFrame(session, NULL /* XrBeginFrameInfo, is empty as of 1.0 */);
	    res != XR_SUCCESS) {
		std::cerr << "Failed to xrBeginFrame: " << to_string(res) << std::endl;
	}

	//set up current image to render into:
	for (auto &view : views) {
		//get the index of the next image to render into:
		uint32_t index = 0;
		if (XrResult res = xrAcquireSwapchainImage(view.swapchain, NULL /* XrSwapchainImageAcquireInfo, empty as of 1.0 */, &index);
		    res != XR_SUCCESS) {
			std::cerr << "Failed to xrAcquireSwapchainImage: " << to_string(res) << std::endl;
		}

		//wait for that image to be ready for rendering:
		XrSwapchainImageWaitInfo wait_info{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
		wait_info.timeout = XR_INFINITE_DURATION;
		if (XrResult res = xrWaitSwapchainImage(view.swapchain, &wait_info);
		    res != XR_SUCCESS) {
			std::cerr << "Failed to xrWaitSwapchainImage: " << to_string(res) << std::endl;
		}

		//update the current_framebuffer pointer appropriately:
		view.current_framebuffer = &view.framebuffers.at(index);
	}


	//update views with current transform in stage space:
	XrViewLocateInfo info{XR_TYPE_VIEW_LOCATE_INFO};
	info.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
	info.displayTime = next_frame.display_time;
	info.space = stage;

	XrViewState view_state{XR_TYPE_VIEW_STATE};
	std::array< XrView, 2 > located_views;
	located_views.fill(XrView{XR_TYPE_VIEW});

	uint32_t view_capacity = located_views.size();
	uint32_t view_count = 0;
	if (XrResult res = xrLocateViews(session, &info, &view_state, view_capacity, &view_count, located_views.data());
	    res != XR_SUCCESS) {
		std::cerr << "Failed to xrLocateViews: " << to_string(res) << std::endl;
	}

	if (view_count != 2) {
		std::cerr << "ERROR: Somehow didn't get exactly two views located?!" << std::endl;
		return;
	}

	for (uint32_t v = 0; v < views.size(); ++v) {
		if (view_state.viewStateFlags & XR_VIEW_STATE_ORIENTATION_VALID_BIT) {
			views[v].pose.orientation = located_views[v].pose.orientation;
		} else {
			views[v].pose.orientation = XrQuaternionf{0.0f, 0.0f, 0.0f, 1.0f};
		}
		if (view_state.viewStateFlags & XR_VIEW_STATE_POSITION_VALID_BIT) {
			views[v].pose.position = located_views[v].pose.position;
		} else {
			views[v].pose.position = XrVector3f{0.0f, 0.0f, 0.0f};
		}
		views[v].fov = located_views[v].fov;
	}


}

void XR::end_frame() {

	//done rendering: release swapchain images
	for (auto &view : views) {
		if (XrResult res = xrReleaseSwapchainImage(view.swapchain, NULL /* XrSwapchainImageReleaseInfo, empty as of 1.0 */);
		    res != XR_SUCCESS) {
			std::cerr << "Failed to xrReleaseSwapchainImage: " << to_string(res) << std::endl;
		}
		view.current_framebuffer = nullptr;
	}

	//tell compositor about rendered images:
	std::array< XrCompositionLayerProjectionView, 2 > projection_views;
	projection_views.fill(XrCompositionLayerProjectionView{XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW});

	static_assert(std::tuple_size< decltype(views) >::value == std::tuple_size< decltype(projection_views) >::value, "Correct number of eyes.");

	for (uint32_t v = 0; v < projection_views.size(); ++v) {
		projection_views[v].pose = views[v].pose;
		projection_views[v].fov = views[v].fov;
		projection_views[v].subImage.swapchain = views[v].swapchain;
		projection_views[v].subImage.imageRect.offset.x = 0;
		projection_views[v].subImage.imageRect.offset.y = 0;
		projection_views[v].subImage.imageRect.extent.width = size.x;
		projection_views[v].subImage.imageRect.extent.height = size.y;
		projection_views[v].subImage.imageArrayIndex = 0;

	}

	XrCompositionLayerProjection layer{XR_TYPE_COMPOSITION_LAYER_PROJECTION};
	layer.layerFlags = XR_COMPOSITION_LAYER_CORRECT_CHROMATIC_ABERRATION_BIT //note: "planned for deprecation"
	                 //| XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT //probably not needed for OPAQUE mode?
	;
	layer.space = stage;
	layer.viewCount = projection_views.size();
	layer.views = projection_views.data();

	std::array< XrCompositionLayerBaseHeader *, 1 > layers;
	layers[0] = reinterpret_cast< XrCompositionLayerBaseHeader * >(&layer);

	XrFrameEndInfo info{XR_TYPE_FRAME_END_INFO};
	info.displayTime = next_frame.display_time;
	info.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
	info.layerCount = layers.size();
	info.layers = layers.data();


	if (XrResult res = xrEndFrame(session, &info);
	    res != XR_SUCCESS) {
		std::cerr << "Failed to xrEndFrame: " << to_string(res) << std::endl;
	}
}
