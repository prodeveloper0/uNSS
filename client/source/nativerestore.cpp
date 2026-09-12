#include "nativerestore.hpp"

#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

#include "fileio.hpp"
#include "http.hpp"
#include "miniz.h"
#include "remote.hpp"
#include "savedata.hpp"
#include "title.hpp"
#include "utils.hpp"

namespace
{
std::string accountStagePath(const SyncOptions& options)
{
    return options.saveDataPath + "/"
        + toHex(options.uid.uid[0]) + toHex(options.uid.uid[1]);
}

void replaceText(std::string& text, const std::string& from,
                 const std::string& to)
{
    for (size_t at = 0; (at = text.find(from, at)) != std::string::npos;)
    {
        text.replace(at, from.size(), to);
        at += to.size();
    }
}

std::string titleNameOrUnknown(u64 titleId)
{
    std::string name;
    if (getTitleName(titleId, name) != 0 || name.empty()) name = "Unknown";

    // The shared Switch font renders some punctuation used by title metadata
    // as a missing-glyph X. Keep labels readable; identity always remains the
    // numeric title ID.
    replaceText(name, "\xE2\x80\x93", "-");  // en dash
    replaceText(name, "\xE2\x80\x94", "-");  // em dash
    replaceText(name, "\xE2\x80\x98", "'");
    replaceText(name, "\xE2\x80\x99", "'");
    replaceText(name, "\xE2\x80\x9C", "\"");
    replaceText(name, "\xE2\x80\x9D", "\"");
    replaceText(name, "\xE2\x84\xA2", "(TM)");
    return name;
}

struct ServerBackup
{
    u64 titleId;
    std::string revision;
};

bool parseTitleId(const std::string& text, u64& value)
{
    if (text.size() != 16) return false;
    value = 0;
    for (const char c : text)
    {
        value <<= 4;
        if (c >= '0' && c <= '9') value |= (u64)(c - '0');
        else if (c >= 'a' && c <= 'f') value |= (u64)(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') value |= (u64)(c - 'A' + 10);
        else return false;
    }
    return true;
}

bool safeRevision(const std::string& revision)
{
    if (revision.empty() || revision.size() > 64) return false;
    for (const unsigned char c : revision)
        if (!std::isalnum(c) && c != '-') return false;
    return true;
}

int queryServerBackups(const SyncOptions& options,
                       std::vector<ServerBackup>& backups)
{
    std::string response;
    HTTPClient client;
    client.setUrl(options.serverUrl + "/users/" + options.nickname + "/saves")
        .setMethod("GET")
        .setReceiveCallback([&response](const void* data, size_t size, size_t& actual)
        {
            if (response.size() + size > 1024 * 1024) return false;
            response.append(static_cast<const char*>(data), size);
            actual = size;
            return true;
        });

    const int result = client.perform();
    if (result != 200) return result;

    size_t at = 0;
    while (at < response.size())
    {
        const size_t newline = response.find('\n', at);
        const size_t end = newline == std::string::npos ? response.size() : newline;
        std::string line = response.substr(at, end - at);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        at = newline == std::string::npos ? response.size() : newline + 1;
        if (line.empty()) continue;

        if (line.size() < 18 || line[16] != '|') return -1;
        u64 titleId = 0;
        const std::string revision = line.substr(17);
        if (!parseTitleId(line.substr(0, 16), titleId) || !safeRevision(revision))
            return -1;

        auto duplicate = std::find_if(backups.begin(), backups.end(),
            [titleId](const ServerBackup& item) { return item.titleId == titleId; });
        if (duplicate == backups.end()) backups.push_back({titleId, revision});
        else duplicate->revision = revision;
    }
    return 200;
}

bool safeArchiveName(const std::string& name)
{
    if (name.empty() || name.size() >= 512 || name.front() == '/'
        || name.back() == '/' || name.find('\\') != std::string::npos
        || name.find(':') != std::string::npos) return false;

    int depth = 0;
    size_t at = 0;
    while (at < name.size())
    {
        const size_t slash = name.find('/', at);
        const size_t end = slash == std::string::npos ? name.size() : slash;
        const std::string part = name.substr(at, end - at);
        if (part.empty() || part == "." || part == ".." || ++depth > 64)
            return false;
        at = end + 1;
    }

    return name.compare(0, 6, "saves/") == 0
        || name.compare(0, 5, "bcat/") == 0;
}

size_t discardExtracted(void*, mz_uint64, const void*, size_t size)
{
    return size;
}

// restoreSaveData clears the live save before extraction, so merely opening the
// ZIP is not enough. Stream every member once to verify headers, decompression
// and CRC before the destructive primitive is called.
bool validateArchive(const std::string& path)
{
    mz_zip_archive zip = {};
    if (!mz_zip_reader_init_file(&zip, path.c_str(), 0)) return false;

    bool ok = true;
    bool hasSave = false;
    std::vector<std::string> names;
    const mz_uint count = mz_zip_reader_get_num_files(&zip);
    if (count == 0 || count > 65535) ok = false;

    for (mz_uint i = 0; ok && i < count; ++i)
    {
        mz_zip_archive_file_stat item = {};
        if (!mz_zip_reader_file_stat(&zip, i, &item)
            || mz_zip_reader_is_file_a_directory(&zip, i))
        { ok = false; break; }

        const std::string name(item.m_filename);
        if (!safeArchiveName(name)
            || std::find(names.begin(), names.end(), name) != names.end()
            || !mz_zip_reader_extract_to_callback(
                &zip, i, discardExtracted, nullptr, 0))
        { ok = false; break; }

        names.push_back(name);
        hasSave = hasSave || name.compare(0, 6, "saves/") == 0;
    }

    mz_zip_reader_end(&zip);
    return ok && hasSave;
}
}

namespace nativerestore
{
int listAvailable(const SyncOptions& options, std::vector<Backup>& backups,
                  int* skipped)
{
    if (skipped) *skipped = 0;
    if (!options.remoteEnabled) return -1;

    std::vector<u64> titleIds;
    const int probeResult = options.restoreBy == "all"
        ? probeAllTitles(options.uid, titleIds)
        : probeSaveDataCreatedTitles(options.uid, titleIds);
    if (probeResult != 0) return probeResult;
    filterExcludedTitles(titleIds, options.excludedTitleIds, options.excludedTitleNames);

    std::vector<ServerBackup> serverBackups;
    const int manifestResult = queryServerBackups(options, serverBackups);
    if (manifestResult != 200) return manifestResult == 0 ? -1 : manifestResult;

    for (const u64 titleId : titleIds)
    {
        const auto server = std::find_if(serverBackups.begin(), serverBackups.end(),
            [titleId](const ServerBackup& item) { return item.titleId == titleId; });
        if (server == serverBackups.end())
        {
            if (skipped) ++*skipped;
            continue;
        }
        backups.push_back({titleId, server->revision, titleNameOrUnknown(titleId)});
    }
    return 0;
}

namespace
{
int verifySelected(const SyncOptions& options,
                   const std::vector<Backup>& selected,
                   const std::string& stagePath, SyncLogFunc log)
{
    HTTPRemoteStore remote(options.serverUrl, stagePath);
    int valid = 0;
    int failures = 0;
    for (size_t i = 0; i < selected.size(); ++i)
    {
        const Backup& backup = selected[i];
        log("[verify " + padding(i + 1, 3) + "/" + padding(selected.size(), 3)
            + "] " + backup.titleName);

        if (remote.pull(options.nickname, backup.titleId, backup.revision) != 0)
        {
            log("  Download failed (HTTP "
                + std::to_string(remote.getLastHttpResult()) + ")");
            ++failures;
            continue;
        }

        const std::string archive = stagePath + "/"
            + toHex(backup.titleId) + ".sar";
        if (!validateArchive(archive))
        {
            log("  INVALID backup - empty, corrupt or unsafe");
            ++failures;
        }
        else
        {
            log("  Backup verified");
            ++valid;
        }
    }
    log("Verification finished: " + std::to_string(valid)
        + " valid, " + std::to_string(failures) + " invalid/failed");
    return failures == 0 ? 0 : -failures;
}
}

int verifyLatest(const SyncOptions& options, SyncLogFunc log)
{
    const std::string stagePath = accountStagePath(options);
    if (recursiveMkdir(stagePath) != 0)
    {
        log("Failed to create verification staging directory");
        return -1;
    }

    std::vector<Backup> selected;
    int skipped = 0;
    const int listed = listAvailable(options, selected, &skipped);
    if (listed != 0)
    {
        log("Failed to read installed titles/server backup list (code "
            + std::to_string(listed) + ")");
        return listed;
    }
    log("Native backups found: " + std::to_string(selected.size())
        + "; installed titles without a backup skipped: "
        + std::to_string(skipped));
    return verifySelected(options, selected, stagePath, log);
}

int restoreSelected(const SyncOptions& options,
                    const std::vector<Backup>& selected, SyncLogFunc log)
{
    if (selected.empty())
    {
        log("No games selected");
        return -1;
    }

    const std::string stagePath = accountStagePath(options);
    if (recursiveMkdir(stagePath) != 0)
    {
        log("Failed to create verification staging directory");
        return -1;
    }

    const int verified = verifySelected(options, selected, stagePath, log);
    if (verified != 0)
    {
        log("Restore aborted before changing any live save");
        return verified;
    }

    log("All selected backups passed preflight; starting restore");
    int failures = 0;
    int restored = 0;
    for (size_t i = 0; i < selected.size(); ++i)
    {
        const Backup& backup = selected[i];
        log("[restore " + padding(i + 1, 3) + "/" + padding(selected.size(), 3)
            + "] " + backup.titleName);
        const int result = restoreSaveData(options.uid, backup.titleId, stagePath);
        if (result != SAVEDATA_OK)
        {
            log("  Restore failed (code " + std::to_string(result) + ")");
            ++failures;
        }
        else
        {
            log("  Restored successfully");
            ++restored;
        }
    }

    log("Native restore finished: " + std::to_string(restored)
        + " restored, " + std::to_string(failures) + " failed");
    return failures == 0 ? 0 : -failures;
}
}
