#include "gen1restore.hpp"

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

#include "fileio.hpp"
#include "gen1recomp.hpp"
#include "miniz.h"
#include "remote.hpp"
#include "utils.hpp"

namespace
{
constexpr size_t MAX_FILES = 256;
constexpr mz_uint64 MAX_FILE_SIZE = 4 * 1024 * 1024;
constexpr mz_uint64 MAX_TOTAL_SIZE = 32ULL * 1024 * 1024;
constexpr int MAX_DEPTH = 8;

bool exists(const std::string& path, struct stat* out = nullptr)
{
    struct stat st;
    if (lstat(path.c_str(), &st) != 0) return false;
    if (out) *out = st;
    return true;
}

bool safeArchiveName(const std::string& name)
{
    if (name.empty() || name.size() >= 256 || name.front() == '/'
        || name.back() == '/' || name.find('\\') != std::string::npos
        || name.find(':') != std::string::npos) return false;

    int depth = 0;
    size_t at = 0;
    while (at < name.size())
    {
        const size_t slash = name.find('/', at);
        const size_t end = slash == std::string::npos ? name.size() : slash;
        const std::string part = name.substr(at, end - at);
        if (part.empty() || part == "." || part == ".." || ++depth > MAX_DEPTH)
            return false;
        at = end + 1;
    }

    return name == "options.lua" || name.compare(0, 6, "saves/") == 0;
}

void removeControlledTree(const std::string& path)
{
    struct stat st;
    if (lstat(path.c_str(), &st) != 0 || !S_ISDIR(st.st_mode) || S_ISLNK(st.st_mode))
        return;
    walk(path, [](const std::string& item, bool isDir)
    {
        if (isDir) rmdir(item.c_str());
        else remove(item.c_str());
    }, MAX_DEPTH + 1);
    rmdir(path.c_str());
}

bool extractValidated(const std::string& archive, const std::string& temporary,
                      const std::function<void(const std::string&)>& log)
{
    if (exists(temporary))
    {
        log("Gen1Recomp: stale restore staging exists; refusing to overwrite it");
        return false;
    }
    if (recursiveMkdir(temporary) != 0) return false;

    mz_zip_archive zip = {};
    if (!mz_zip_reader_init_file(&zip, archive.c_str(), 0))
    {
        removeControlledTree(temporary);
        log("Gen1Recomp: downloaded backup is not a valid archive");
        return false;
    }

    bool ok = true;
    bool hasOptions = false;
    bool hasSave = false;
    mz_uint64 total = 0;
    std::vector<std::string> names;
    const mz_uint count = mz_zip_reader_get_num_files(&zip);
    if (count == 0 || count > MAX_FILES) ok = false;

    for (mz_uint i = 0; ok && i < count; ++i)
    {
        mz_zip_archive_file_stat item = {};
        if (!mz_zip_reader_file_stat(&zip, i, &item)
            || mz_zip_reader_is_file_a_directory(&zip, i))
        { ok = false; break; }

        const std::string name(item.m_filename);
        if (!safeArchiveName(name) || item.m_uncomp_size == 0
            || item.m_uncomp_size > MAX_FILE_SIZE
            || total + item.m_uncomp_size > MAX_TOTAL_SIZE
            || std::find(names.begin(), names.end(), name) != names.end())
        { ok = false; break; }

        total += item.m_uncomp_size;
        names.push_back(name);
        hasOptions = hasOptions || name == "options.lua";
        hasSave = hasSave || name.compare(0, 6, "saves/") == 0;
    }
    ok = ok && hasOptions && hasSave;

    for (mz_uint i = 0; ok && i < count; ++i)
    {
        mz_zip_archive_file_stat item = {};
        if (!mz_zip_reader_file_stat(&zip, i, &item)) { ok = false; break; }
        const std::string destination = temporary + "/" + item.m_filename;
        const size_t slash = destination.find_last_of('/');
        if (slash == std::string::npos
            || recursiveMkdir(destination.substr(0, slash)) != 0
            || !mz_zip_reader_extract_to_file(&zip, i, destination.c_str(), 0))
        { ok = false; break; }

        struct stat extracted;
        if (lstat(destination.c_str(), &extracted) != 0 || !S_ISREG(extracted.st_mode)
            || S_ISLNK(extracted.st_mode)
            || (mz_uint64)extracted.st_size != item.m_uncomp_size)
        { ok = false; break; }
    }

    mz_zip_reader_end(&zip);
    if (!ok)
    {
        removeControlledTree(temporary);
        log("Gen1Recomp: backup rejected; active saves were not changed");
    }
    return ok;
}

bool writeSyncState(const std::string& path, u64 fingerprint)
{
    FILE* fp = fopen(path.c_str(), "w");
    if (!fp) return false;
    const bool ok = fprintf(fp, "%016llx %016llx\n",
        (unsigned long long)gen1recomp::TITLE_ID,
        (unsigned long long)fingerprint) > 0;
    fclose(fp);
    return ok;
}

bool validExisting(const std::string& path, bool directory)
{
    struct stat st;
    if (!exists(path, &st) || S_ISLNK(st.st_mode)) return false;
    return directory ? S_ISDIR(st.st_mode) : S_ISREG(st.st_mode);
}
}

namespace gen1restore
{
int restoreLatest(const Options& options,
                  const std::function<void(const std::string&)>& log)
{
    if (recursiveMkdir(options.downloadPath) != 0)
    {
        log("Gen1Recomp: cannot create download staging");
        return 2;
    }

    log("Gen1Recomp: downloading latest backup...");
    HTTPRemoteStore remote(options.serverUrl, options.downloadPath);
    if (remote.pull(options.accountName, gen1recomp::TITLE_ID) != 0)
    {
        log("Gen1Recomp: download failed (HTTP "
            + std::to_string(remote.getLastHttpResult()) + ")");
        return 2;
    }

    const std::string archive = options.downloadPath + "/"
        + toHex(gen1recomp::TITLE_ID) + ".sar";
    const std::string temporary = options.root + ".unss-restore-tmp";
    if (!extractValidated(archive, temporary, log)) return 2;

    if (recursiveMkdir(options.root) != 0)
    {
        removeControlledTree(temporary);
        log("Gen1Recomp: cannot create game directory");
        return 2;
    }

    const std::string saves = options.root + "/saves";
    const std::string settings = options.root + "/options.lua";
    const std::string oldSaves = options.root + "/saves.before-unss-restore";
    const std::string oldSettings = options.root + "/options.lua.before-unss-restore";
    const bool hadSaves = exists(saves);
    const bool hadSettings = exists(settings);

    if ((hadSaves && !validExisting(saves, true))
        || (hadSettings && !validExisting(settings, false))
        || exists(oldSaves) || exists(oldSettings))
    {
        removeControlledTree(temporary);
        log("Gen1Recomp: safety copy already exists or live data is unsafe; refusing restore");
        return 2;
    }

    bool movedSaves = false;
    bool movedSettings = false;
    if (hadSaves)
    {
        if (rename(saves.c_str(), oldSaves.c_str()) != 0) goto rollback;
        movedSaves = true;
    }
    if (hadSettings)
    {
        if (rename(settings.c_str(), oldSettings.c_str()) != 0) goto rollback;
        movedSettings = true;
    }
    if (rename((temporary + "/saves").c_str(), saves.c_str()) != 0) goto rollback;
    if (rename((temporary + "/options.lua").c_str(), settings.c_str()) != 0)
    {
        rename(saves.c_str(), (temporary + "/saves").c_str());
        goto rollback;
    }

    rmdir(temporary.c_str());
    {
        const u64 restored = gen1recomp::fingerprint(options.root);
        const size_t slash = options.syncStatePath.find_last_of('/');
        const bool stateParentReady = slash != std::string::npos
            && recursiveMkdir(options.syncStatePath.substr(0, slash)) == 0;
        if (restored && (!stateParentReady || !writeSyncState(options.syncStatePath, restored)))
            log("Gen1Recomp: restored, but could not update sync state");
    }
    log("Gen1Recomp: restore finished");
    if (movedSaves || movedSettings)
        log("Previous files kept as *.before-unss-restore");
    return 0;

rollback:
    if (movedSettings) rename(oldSettings.c_str(), settings.c_str());
    if (movedSaves) rename(oldSaves.c_str(), saves.c_str());
    removeControlledTree(temporary);
    log("Gen1Recomp: restore failed; previous files put back");
    return 2;
}
}
