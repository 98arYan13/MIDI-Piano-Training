#pragma once
#include "Storage.hpp"
#include "FingeringJobs.hpp"
#include <httplib.h>
namespace sightreading {
void libraryRoutes(httplib::Server &server, Storage &storage, unsigned rate, FingeringJobs &jobs);
void installStarters(Storage &storage, const std::filesystem::path &folder);
} // namespace sightreading
