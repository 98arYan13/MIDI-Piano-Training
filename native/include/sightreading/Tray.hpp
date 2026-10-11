#pragma once
#include <atomic>
#include <string>
namespace sightreading {
void runTray(const std::string &url, std::atomic<bool> &stopping);
}
