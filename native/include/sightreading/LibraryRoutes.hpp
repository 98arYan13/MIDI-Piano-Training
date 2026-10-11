#pragma once
#include "Storage.hpp"
#include <httplib.h>
namespace sightreading {
void libraryRoutes(httplib::Server &server, Storage &storage, unsigned rate);
void installStarters(Storage &storage, const std::filesystem::path &folder);
} // namespace sightreading
