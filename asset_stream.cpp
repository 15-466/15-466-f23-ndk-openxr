#include "asset_stream.hpp"

//Note, based in part on tchow Rainbow's "GameData.cpp"

#if __ANDROID__
#include <sstream>
#include <cassert>

#include <android/native_activity.h>
extern ANativeActivity *activity;
#else
#include <fstream>
#endif

std::unique_ptr< std::istream > asset_stream(std::string const &filename) {
#ifdef __ANDROID__
	assert(activity); //DEBUG

	AAsset *asset = AAssetManager_open(activity->assetManager, filename.c_str(), AASSET_MODE_STREAMING);

	if (asset == NULL) {
		throw std::runtime_error("Can't open asset '" + filename + "'.");
	}

	off64_t size = AAsset_getLength64(asset);
	void const *buffer = AAsset_getBuffer(asset);

	if (buffer == NULL) {
		throw std::runtime_error("Failed to get pointer to entire contents of asset '" + filename + "'.");
	}

	//copy buffer into a string wrapped in a string stream:
	std::unique_ptr< std::istringstream > ret(new std::istringstream(std::string(reinterpret_cast< const char * >(buffer), size)));

	AAsset_close(asset);

	return ret;
#else //__ANDROID__
	return std::make_unique< std::ifstream >(filename, std::ios::binary);
#endif
}
