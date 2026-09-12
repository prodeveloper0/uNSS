#pragma once

#include <functional>
#include <string>

namespace gen1restore
{
struct Options
{
    std::string root;
    std::string downloadPath;
    std::string syncStatePath;
    std::string accountName;
    std::string serverUrl;
};

// Download and transactionally restore the latest Gen1Recomp backup.
// Only saves/ and options.lua are accepted. Existing data is preserved beside
// the live files and no native Horizon save APIs are used.
int restoreLatest(const Options& options,
                  const std::function<void(const std::string&)>& log);
}
