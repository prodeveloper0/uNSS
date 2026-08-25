#include "sync.hpp"

#include <cstdio>
#include <ctime>
#include <vector>

#include "fileio.hpp"
#include "remote.hpp"
#include "savedata.hpp"
#include "title.hpp"
#include "utils.hpp"


namespace
{

struct SaveRevision
{
    bool valid = false;
    u64 saveDataId = 0;
    u64 commitId = 0;
};

struct PendingRevision
{
    u64 titleID = 0;
    SaveRevision revision;
};


std::string lastSyncPath(const std::string& saveDataPath)
{
    return saveDataPath + "/.lastautosync";
}


std::string syncStatePath(const std::string& saveDataPath)
{
    return saveDataPath + "/.syncstate-v2";
}


std::string titleNameOrUnknown(u64 titleID)
{
    std::string titleName;
    if (getTitleName(titleID, titleName) != 0)
        titleName = "Unknown";
    return titleName;
}


bool sameAccount(const AccountUid& left, const AccountUid& right)
{
    return left.uid[0] == right.uid[0] && left.uid[1] == right.uid[1];
}


bool sameRevision(const SaveRevision& left, const SaveRevision& right)
{
    return left.valid && right.valid
        && left.saveDataId == right.saveDataId
        && left.commitId == right.commitId;
}


std::string revisionKey(const AccountUid& uid, u64 titleID)
{
    char buffer[50];
    snprintf(
        buffer,
        sizeof(buffer),
        "%016llX%016llX:%016llX",
        (unsigned long long)uid.uid[0],
        (unsigned long long)uid.uid[1],
        (unsigned long long)titleID
    );
    return std::string(buffer);
}


SaveRevision readSaveRevision(const AccountUid& uid, u64 titleID)
{
    SaveRevision revision;

    FsSaveDataInfoReader reader;
    Result rc = fsOpenSaveDataInfoReader(&reader, FsSaveDataSpaceId_User);
    if (R_FAILED(rc))
        return revision;

    FsSaveDataInfo match{};
    int matches = 0;

    while (true)
    {
        FsSaveDataInfo info{};
        s64 count = 0;
        rc = fsSaveDataInfoReaderRead(&reader, &info, 1, &count);
        if (R_FAILED(rc) || count == 0)
            break;

        if (info.save_data_type == FsSaveDataType_Account
            && info.application_id == titleID
            && sameAccount(info.uid, uid))
        {
            match = info;
            ++matches;
        }
    }

    fsSaveDataInfoReaderClose(&reader);

    // A missing or ambiguous match is not a safe basis for skipping a backup.
    if (R_FAILED(rc) || matches != 1)
        return revision;

    FsSaveDataExtraData extra{};
    rc = fsReadSaveDataFileSystemExtraDataBySaveDataSpaceId(
        &extra,
        sizeof(extra),
        (FsSaveDataSpaceId)match.save_data_space_id,
        match.save_data_id
    );
    if (R_FAILED(rc))
        return revision;

    revision.valid = true;
    revision.saveDataId = match.save_data_id;
    revision.commitId = extra.commit_id;
    return revision;
}


bool readSyncedRevision(
    const std::string& saveDataPath,
    const AccountUid& uid,
    u64 titleID,
    SaveRevision& revision)
{
    FILE* fp = fopen(syncStatePath(saveDataPath).c_str(), "r");
    if (!fp) return false;

    const std::string wanted = revisionKey(uid, titleID);
    bool found = false;
    char line[256];

    while (fgets(line, sizeof(line), fp))
    {
        char key[64];
        unsigned long long saveDataId = 0;
        unsigned long long commitId = 0;

        if (sscanf(line, "%63s %llx %llx", key, &saveDataId, &commitId) != 3)
            continue;

        if (wanted != key)
            continue;

        // Duplicate records are ambiguous. Fail open rather than choosing one.
        if (found)
        {
            fclose(fp);
            return false;
        }

        revision.valid = true;
        revision.saveDataId = (u64)saveDataId;
        revision.commitId = (u64)commitId;
        found = true;
    }

    fclose(fp);
    return found;
}


bool writeSyncedRevision(
    const std::string& saveDataPath,
    const AccountUid& uid,
    u64 titleID,
    const SaveRevision& revision)
{
    if (!revision.valid)
        return false;

    const std::string path = syncStatePath(saveDataPath);
    const std::string wanted = revisionKey(uid, titleID);
    std::string rebuilt;

    FILE* fp = fopen(path.c_str(), "r");
    if (fp)
    {
        char line[256];
        while (fgets(line, sizeof(line), fp))
        {
            char key[64];
            unsigned long long saveDataId = 0;
            unsigned long long commitId = 0;

            const bool parsed = sscanf(line, "%63s %llx %llx", key, &saveDataId, &commitId) == 3;
            if (parsed && wanted == key)
                continue;

            rebuilt += line;
        }
        fclose(fp);
    }

    char record[128];
    const int recordLength = snprintf(
        record,
        sizeof(record),
        "%s %016llX %016llX\n",
        wanted.c_str(),
        (unsigned long long)revision.saveDataId,
        (unsigned long long)revision.commitId
    );
    if (recordLength <= 0 || (size_t)recordLength >= sizeof(record))
        return false;

    rebuilt.append(record, (size_t)recordLength);

    FILE* out = fopen(path.c_str(), "w");
    if (!out) return false;

    const bool ok = fwrite(rebuilt.data(), 1, rebuilt.size(), out) == rebuilt.size();
    if (fclose(out) != 0)
        return false;

    return ok;
}


bool saveRevisionChanged(
    const SyncOptions& options,
    u64 titleID,
    SaveRevision* currentOut = nullptr)
{
    const SaveRevision current = readSaveRevision(options.uid, titleID);
    if (currentOut) *currentOut = current;

    // Unknown metadata must never suppress a backup.
    if (!current.valid)
        return true;

    SaveRevision synced;
    if (!readSyncedRevision(options.saveDataPath, options.uid, titleID, synced))
        return true;

    return !sameRevision(current, synced);
}


const SaveRevision* findPendingRevision(
    const std::vector<PendingRevision>& pending,
    u64 titleID)
{
    for (const PendingRevision& entry : pending)
    {
        if (entry.titleID == titleID)
            return &entry.revision;
    }
    return nullptr;
}


// Target title enumeration shared by pushAllSaves and countChangedTitles.
int collectTargetTitles(const SyncOptions& options, AccountUid uid, std::vector<u64>& titleIDs)
{
    const int ret = options.archiveBy == "all"
        ? probeAllTitles(uid, titleIDs)
        : probeSaveDataCreatedTitles(uid, titleIDs);
    if (ret != 0) return ret;

    filterExcludedTitles(titleIDs, options.excludedTitleIds, options.excludedTitleNames);
    return 0;
}

} // namespace


bool isGameRunning()
{
    if (R_FAILED(pmdmntInitialize()))
        return false;

    u64 pid = 0;
    const Result rc = pmdmntGetApplicationProcessId(&pid);
    pmdmntExit();

    return R_SUCCEEDED(rc) && pid != 0;
}


bool hasSaveDataChanged(const SyncOptions& options, u64 titleID)
{
    return saveRevisionChanged(options, titleID);
}


int countChangedTitles(const SyncOptions& options)
{
    std::vector<u64> titleIDs;
    if (collectTargetTitles(options, options.uid, titleIDs) != 0) return -1;

    if (!options.skipUnchanged) return (int)titleIDs.size();

    int changed = 0;
    for (const u64 titleID : titleIDs)
    {
        if (hasSaveDataChanged(options, titleID))
            ++changed;
    }

    return changed;
}


int pushAllSaves(const SyncOptions& options, SyncLogFunc log)
{
    HTTPRemoteStore remoteStore(options.serverUrl, options.saveDataPath);
    recursiveMkdir(options.saveDataPath.c_str());

    // Capture the exact revision that caused each title to be selected. This is
    // also the revision the archive is expected to represent. After upload we
    // only advance state if Horizon still reports the same revision.
    std::vector<PendingRevision> pendingRevisions;

    const ProbeTitlesFunc probeFunc = [&](const AccountUid probeUid, std::vector<u64>& titleIDs) -> int
    {
        const int ret = collectTargetTitles(options, probeUid, titleIDs);
        if (ret != 0) return ret;

        if (options.skipUnchanged)
        {
            std::vector<u64> changed;
            changed.reserve(titleIDs.size());
            pendingRevisions.clear();
            pendingRevisions.reserve(titleIDs.size());

            for (const u64 titleID : titleIDs)
            {
                SaveRevision current;
                if (saveRevisionChanged(options, titleID, &current))
                {
                    changed.push_back(titleID);
                    pendingRevisions.push_back({titleID, current});
                }
            }

            const size_t skipped = titleIDs.size() - changed.size();
            if (skipped > 0)
                log("Skipping " + std::to_string(skipped) + " unchanged title(s)");

            titleIDs.swap(changed);
        }

        return 0;
    };

    int failures = 0;

    const int ret = archiveAllSaveData(
        options.uid,
        options.saveDataPath,
        probeFunc,
        [&log](int total, int current, u64 titleID) -> bool
        {
            log("[" + padding(current, 3) + "/" + padding(total, 3) + "] " + titleNameOrUnknown(titleID));
            return true;
        },
        [&](int total, int current, int ret, u64 titleID) -> bool
        {
            if (ret == SAVEDATA_NO_SAVE_DATA)
            {
                log("No save data for this account - skipped");
            }
            else if (ret != SAVEDATA_OK)
            {
                log("Failed to archive, ret=" + std::to_string(ret));
                ++failures;
            }
            else
            {
                int pushRet = remoteStore.push(options.nickname, titleID);
                if (pushRet != 0)
                {
                    log("Failed to push, ret=" + std::to_string(pushRet)
                        + " http=" + std::to_string(remoteStore.getLastHttpResult()));
                    ++failures;
                }
                else if (options.skipUnchanged)
                {
                    const SaveRevision* pre = findPendingRevision(pendingRevisions, titleID);
                    const SaveRevision post = readSaveRevision(options.uid, titleID);

                    if (!pre || !pre->valid || !post.valid)
                    {
                        // The backup itself succeeded, but there is no reliable
                        // token to suppress a future retry. Leave state unchanged.
                        log("Save revision unavailable - sync state not advanced");
                    }
                    else if (!sameRevision(*pre, post))
                    {
                        // A newer commit appeared while this backup was being
                        // archived/uploaded. Do not mark it as already backed up,
                        // and make the round fail so the sysmodule retries soon.
                        log("Save changed during upload - will retry");
                        ++failures;
                    }
                    else if (!writeSyncedRevision(options.saveDataPath, options.uid, titleID, *pre))
                    {
                        // State is only an optimisation. A write failure remains
                        // fail-open and therefore causes another backup later.
                        log("Failed to record sync state - title will be backed up again");
                    }
                }
            }
            return true;
        }
    );

    if (ret != 0) return ret;
    return failures > 0 ? -failures : 0;
}


int pullAllSaves(const SyncOptions& options, SyncLogFunc log)
{
    HTTPRemoteStore remoteStore(options.serverUrl, options.saveDataPath);
    recursiveMkdir(options.saveDataPath.c_str());

    if (!options.remoteEnabled)
    {
        log("Remote is disabled, restoring from local...");
        return restoreAllSaveData(
            options.uid, options.saveDataPath,
            [&log](int total, int current, u64 titleID) -> bool
            {
                log("[" + padding(current, 3) + "/" + padding(total, 3) + "] " + titleNameOrUnknown(titleID));
                return true;
            },
            [&log](int total, int current, int ret, u64 titleID) -> bool
            {
                if (ret != SAVEDATA_OK)
                    log("Failed to restore, ret=" + std::to_string(ret));
                return true;
            }
        );
    }

    std::vector<u64> titleIDs;
    int probeRet = options.restoreBy == "all"
        ? probeAllTitles(options.uid, titleIDs)
        : probeSaveDataCreatedTitles(options.uid, titleIDs);
    if (probeRet != 0)
    {
        log("Failed to probe titles");
        return probeRet;
    }
    filterExcludedTitles(titleIDs, options.excludedTitleIds, options.excludedTitleNames);

    for (size_t i = 0; i < titleIDs.size(); ++i)
    {
        log("[" + padding(i + 1, 3) + "/" + padding(titleIDs.size(), 3) + "] " + titleNameOrUnknown(titleIDs[i]));

        if (remoteStore.pull(options.nickname, titleIDs[i]) != 0)
        {
            log("Failed to pull from server");
        }
        else
        {
            restoreSaveData(options.uid, titleIDs[i], options.saveDataPath);
        }
    }

    return 0;
}


time_t readLastAutoSyncTime(const std::string& saveDataPath)
{
    FILE* fp = fopen(lastSyncPath(saveDataPath).c_str(), "r");
    if (!fp) return 0;

    long long value = 0;
    if (fscanf(fp, "%lld", &value) != 1)
        value = 0;
    fclose(fp);

    return (time_t)value;
}


void writeLastAutoSyncTime(const std::string& saveDataPath, time_t when)
{
    recursiveMkdir(saveDataPath.c_str());

    FILE* fp = fopen(lastSyncPath(saveDataPath).c_str(), "w");
    if (!fp) return;

    fprintf(fp, "%lld", (long long)when);
    fclose(fp);
}


bool isAutoSyncDue(const std::string& saveDataPath, int intervalHours)
{
    if (intervalHours <= 0) return true;

    const time_t last = readLastAutoSyncTime(saveDataPath);
    if (last == 0) return true;

    const time_t now = time(NULL);
    if (now < last) return true;

    return (now - last) >= (time_t)intervalHours * 3600;
}
