#include "gen1recomp.hpp"

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <sys/stat.h>
#include <vector>

#include "fileio.hpp"
#include "remote.hpp"
#include "utils.hpp"
#include "zipio.hpp"

namespace
{
constexpr u64 FNV_OFFSET = 14695981039346656037ULL;
constexpr u64 FNV_PRIME = 1099511628211ULL;
constexpr size_t MAX_FILES = 256;
constexpr off_t MAX_FILE_SIZE = 4 * 1024 * 1024;
constexpr u64 MAX_TOTAL_SIZE = 32ULL * 1024 * 1024;
constexpr int MAX_DEPTH = 8;

struct Entry { std::string path; std::string name; };

bool safeName(const std::string& name)
{
    if (name.empty() || name.size() >= 256 || name.find('\\') != std::string::npos
        || name.find(':') != std::string::npos) return false;
    size_t at = 0;
    while (at < name.size())
    {
        const size_t slash = name.find('/', at);
        const size_t end = slash == std::string::npos ? name.size() : slash;
        if (end - at == 2 && name.compare(at, 2, "..") == 0) return false;
        at = end + 1;
    }
    return true;
}

// Do not follow a symlink at root or any of its parents. The selected source
// must stay below the configured SD path.
bool safeRoot(const std::string& root)
{
    if (root.empty()) return false;
    std::string path = root;
    while (!path.empty())
    {
        struct stat st;
        if (lstat(path.c_str(), &st) != 0 || S_ISLNK(st.st_mode)) return false;
        if (path.size() >= 2 && path[path.size() - 2] == ':' && path.back() == '/') break;
        const size_t slash = path.find_last_of('/');
        if (slash == std::string::npos) break;
        if (slash == 0) { path = "/"; break; }
        // Keep the slash in a libnx mount root ("sdmc:/").
        path.resize(path[slash - 1] == ':' ? slash + 1 : slash);
    }
    return true;
}

int collect(const std::string& root, bool needOptions, std::vector<Entry>& entries)
{
    if (!safeRoot(root)) return gen1recomp::FAILED;
    const std::string saves = root + "/saves";
    struct stat st;
    if (lstat(saves.c_str(), &st) != 0)
        return errno == ENOENT ? gen1recomp::ABSENT : gen1recomp::FAILED;
    if (!S_ISDIR(st.st_mode) || S_ISLNK(st.st_mode)) return gen1recomp::FAILED;

    u64 total = 0;
    int ret = gen1recomp::OK;
    const int walked = walk(saves, [&](const std::string& path, bool isDir)
    {
        if (isDir || ret != gen1recomp::OK) return;
        struct stat item;
        if (lstat(path.c_str(), &item) != 0 || S_ISLNK(item.st_mode)
            || !S_ISREG(item.st_mode) || item.st_size <= 0 || item.st_size > MAX_FILE_SIZE
            || entries.size() >= MAX_FILES)
        { ret = gen1recomp::FAILED; return; }
        const std::string name = "saves/" + path.substr(saves.size() + 1);
        if (!safeName(name) || (total += (u64)item.st_size) > MAX_TOTAL_SIZE)
        { ret = gen1recomp::FAILED; return; }
        entries.push_back({path, name});
    }, MAX_DEPTH);
    if (walked != 0 || ret != gen1recomp::OK) return gen1recomp::FAILED;
    if (entries.empty()) return gen1recomp::ABSENT;

    const std::string options = root + "/options.lua";
    if (lstat(options.c_str(), &st) != 0)
        return needOptions ? gen1recomp::FAILED : gen1recomp::OK;
    if (!S_ISREG(st.st_mode) || S_ISLNK(st.st_mode) || st.st_size <= 0 || st.st_size > MAX_FILE_SIZE
        || entries.size() >= MAX_FILES || (total += (u64)st.st_size) > MAX_TOTAL_SIZE)
        return gen1recomp::FAILED;
    entries.push_back({options, "options.lua"});
    return gen1recomp::OK;
}

bool hashFile(const Entry& entry, u64& combined)
{
    FILE* fp = fopen(entry.path.c_str(), "rb");
    if (!fp) return false;
    u64 value = FNV_OFFSET;
    for (const char c : entry.name) { value ^= (unsigned char)c; value *= FNV_PRIME; }
    char buffer[4096];
    for (size_t got; (got = fread(buffer, 1, sizeof(buffer), fp)) != 0;)
        for (size_t i = 0; i < got; ++i) { value ^= (unsigned char)buffer[i]; value *= FNV_PRIME; }
    const bool ok = !ferror(fp);
    fclose(fp);
    if (!ok) return false;
    combined ^= value;
    return true;
}

u64 fingerprintEntries(const std::vector<Entry>& entries)
{
    u64 combined = 0;
    for (const Entry& entry : entries) if (!hashFile(entry, combined)) return 0;
    return (combined ^ ((u64)entries.size() * FNV_PRIME)) | 1ULL;
}

std::string statePath(const std::string& stage) { return stage + "/.syncstate.gen1"; }

u64 readState(const std::string& stage)
{
    FILE* fp = fopen(statePath(stage).c_str(), "r");
    if (!fp) return 0;
    unsigned long long id = 0, value = 0;
    const int got = fscanf(fp, "%llx %llx", &id, &value);
    fclose(fp);
    return got == 2 && id == gen1recomp::TITLE_ID ? (u64)value : 0;
}

bool writeState(const std::string& stage, u64 fingerprint)
{
    FILE* fp = fopen(statePath(stage).c_str(), "w");
    if (!fp) return false;
    const bool ok = fprintf(fp, "%016llx %016llx\n",
        (unsigned long long)gen1recomp::TITLE_ID, (unsigned long long)fingerprint) > 0;
    fclose(fp);
    return ok;
}

bool transient(int result)
{
    // Keep v20's retry policy without pulling the HTTP client implementation
    // into this adapter: -2 is malformed URL, -100 is unsupported.
    return (result < 0 && result != -2 && result != -100) || result >= 500;
}
}

namespace gen1recomp
{
bool present(const std::string& root)
{
    struct stat st;
    return lstat((root + "/saves").c_str(), &st) == 0 || errno != ENOENT;
}

u64 fingerprint(const std::string& root)
{
    std::vector<Entry> entries;
    return collect(root, true, entries) == OK ? fingerprintEntries(entries) : 0;
}

int archive(const std::string& root, const std::string& outSar,
            const std::function<bool()>& sourceBusy)
{
    std::vector<Entry> entries;
    const int collected = collect(root, true, entries);
    if (collected != OK) return collected;
    const u64 before = fingerprintEntries(entries);
    if (!before) return FAILED;

    const std::string temporary = outSar + ".gen1.tmp";
    const std::string previous = outSar + ".gen1.previous";
    remove(temporary.c_str());
    remove(previous.c_str());
    ZipWriter zip;
    if (!zip.open(temporary)) return FAILED;
    bool ok = true;
    bool busy = false;
    for (const Entry& entry : entries)
    {
        busy = sourceBusy && sourceBusy();
        if (busy || !zip.add(entry.path, entry.name)) { ok = false; break; }
    }
    zip.close();
    if (!ok)
    { remove(temporary.c_str()); return busy ? BUSY : FAILED; }

    std::vector<Entry> afterEntries;
    if (collect(root, true, afterEntries) != OK || fingerprintEntries(afterEntries) != before)
    { remove(temporary.c_str()); return FAILED; }

    struct stat st;
    const bool hadOld = lstat(outSar.c_str(), &st) == 0;
    if (hadOld && rename(outSar.c_str(), previous.c_str()) != 0)
    { remove(temporary.c_str()); return FAILED; }
    if (rename(temporary.c_str(), outSar.c_str()) != 0)
    {
        if (hadOld) rename(previous.c_str(), outSar.c_str());
        remove(temporary.c_str());
        return FAILED;
    }
    if (hadOld) remove(previous.c_str());
    return OK;
}

int runRound(const Options& options, const std::function<void(const std::string&)>& log)
{
    if (!present(options.root)) { log("Gen1Recomp: source save not found"); return OK; }
    const u64 current = fingerprint(options.root);
    if (!current) { log("Gen1Recomp: no valid SD save to back up"); return OK; }
    if (current == readState(options.stagePath))
    {
        log("Gen1Recomp: unchanged - backup already uploaded");
        return OK;
    }
    if (recursiveMkdir(options.stagePath) != 0) { log("Gen1Recomp: cannot create staging path"); return FAILED; }
    const std::string sar = options.stagePath + "/" + toHex(TITLE_ID) + ".sar";
    const int archived = archive(options.root, sar, options.sourceBusy);
    if (archived != OK) { log("Gen1Recomp: archive postponed/failed (" + std::to_string(archived) + ")"); return archived; }
    if (!options.remoteEnabled) return writeState(options.stagePath, current) ? OK : FAILED;
    if (options.ensureNetwork && !options.ensureNetwork()) { log("Gen1Recomp: no network"); return FAILED; }
    HTTPRemoteStore remote(options.serverUrl, options.stagePath);
    int ret = remote.push(options.accountName, TITLE_ID);
    for (int retry = 1; ret != 0 && retry < 3 && transient(remote.getLastHttpResult()); ++retry)
    {
        svcSleepThread(5000000000ULL);
        ret = remote.push(options.accountName, TITLE_ID);
    }
    if (ret != 0) { log("Gen1Recomp: upload failed"); return FAILED; }
    if (!writeState(options.stagePath, current)) return FAILED;
    log("Gen1Recomp: backup finished");
    return OK;
}
}
