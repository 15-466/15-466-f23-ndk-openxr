#pragma once

#include <istream>

//work-around for asset loading on android where assets don't actually have filenames!

std::unique_ptr< std::istream > asset_stream(std::string const &filename);
