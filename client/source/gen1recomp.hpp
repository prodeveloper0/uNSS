#pragma once

#include <functional>
#include <string>

#include <switch.h>

// Isolated SD adapter. It deliberately does not use the native-save engine's
// walk, fingerprint, archive, or sync-state paths.
namespace gen1recomp
{
constexpr u64 TITLE_ID = 0x484247454E315243ULL; // HBGEN1RC
constexpr const char* ROOT = "sdmc:/switch/gen1recomp/pokemon-love2d";
constexpr const char* NAME = "Gen1Recomp (SD)";

enum Result
{
    OK = 0,
    ABSENT = 1,
    FAILED = 2,
    BUSY = 3,
};

struct Options
{
    std::string root = ROOT;
    std::string stagePath;
    std::string accountName;
    std::string serverUrl;
    bool remoteEnabled = true;
    std::function<bool()> sourceBusy;
    std::function<bool()> ensureNetwork;
};

bool present(const std::string& root = ROOT);
u64 fingerprint(const std::string& root = ROOT);

// Atomic: a failed/busy/mutating source never replaces outSar.
int archive(const std::string& root, const std::string& outSar,
            const std::function<bool()>& sourceBusy = {});

// Own state file (.syncstate.gen1), own archive and upload round.
// It must be called after the ordinary uNSS round; its result is independent.
int runRound(const Options& options, const std::function<void(const std::string&)>& log);
}
