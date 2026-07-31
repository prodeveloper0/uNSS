#include "sync.hpp"

#include <cstdio>
#include <ctime>
#include <vector>

#include <sys/stat.h>

#include "fileio.hpp"
#include "remote.hpp"
#include "savedata.hpp"
#include "title.hpp"
#include "utils.hpp"


namespace
{

std::string lastSyncPath(const std::string& saveDataPath)
{
    return saveDataPath + "/.lastautosync";
}


std::string titleNameOrUnknown(u64 titleID)
{
    std::string titleName;
    if (getTitleName(titleID, titleName) != 0)
        titleName = "Unknown";
    return titleName;
}


// 어떤 타이틀이 마지막으로 어떤 상태였는지 적어두는 파일.
// 한 줄에 "타이틀ID 시각" 형식.
std::string syncStatePath(const std::string& saveDataPath)
{
    return saveDataPath + "/.syncstate";
}


std::string toHexId(u64 titleID)
{
    char buffer[17];
    snprintf(buffer, sizeof(buffer), "%016lX", titleID);
    return std::string(buffer);
}


// 세이브 안에서 가장 최근 수정 시각을 찾는다.
// 마운트에 실패하면 0 을 돌려주고, 그 경우 호출한 쪽은 "바뀌었다" 로 본다.
u64 latestSaveDataTimestamp(const AccountUid uid, u64 titleID)
{
    const std::string mountPoint = "unsschk";

    if (mountSaveData(mountPoint, uid, titleID) != 0)
        return 0;

    u64 latest = 0;
    walk(mountPoint + ":/", [&latest](const std::string& path, bool isDir)
    {
        if (isDir) return;

        struct stat st;
        if (stat(path.c_str(), &st) == 0)
        {
            const u64 mtime = (u64)st.st_mtime;
            if (mtime > latest) latest = mtime;
        }
    });

    unmount(mountPoint);
    return latest;
}


u64 readSyncedTimestamp(const std::string& saveDataPath, u64 titleID)
{
    FILE* fp = fopen(syncStatePath(saveDataPath).c_str(), "r");
    if (!fp) return 0;

    const std::string wanted = toHexId(titleID);
    char idBuffer[32];
    unsigned long long stamp = 0;
    u64 found = 0;

    while (fscanf(fp, "%31s %llu", idBuffer, &stamp) == 2)
    {
        if (wanted == idBuffer)
        {
            found = (u64)stamp;
            break;
        }
    }

    fclose(fp);
    return found;
}


void writeSyncedTimestamp(const std::string& saveDataPath, u64 titleID, u64 stamp)
{
    const std::string path = syncStatePath(saveDataPath);
    const std::string wanted = toHexId(titleID);

    // 통째로 읽어서 해당 줄만 갈아끼운다. 항목이 수십 개라 이 정도면 충분하다.
    std::string rebuilt;
    FILE* fp = fopen(path.c_str(), "r");
    if (fp)
    {
        char idBuffer[32];
        unsigned long long existing = 0;
        while (fscanf(fp, "%31s %llu", idBuffer, &existing) == 2)
        {
            if (wanted == idBuffer) continue;
            rebuilt += std::string(idBuffer) + " " + std::to_string(existing) + "\n";
        }
        fclose(fp);
    }

    rebuilt += wanted + " " + std::to_string(stamp) + "\n";

    FILE* out = fopen(path.c_str(), "w");
    if (!out) return;
    fwrite(rebuilt.data(), 1, rebuilt.size(), out);
    fclose(out);
}

} // namespace


bool isGameRunning()
{
    if (R_FAILED(pmdmntInitialize()))
        return false;

    u64 pid = 0;
    const Result rc = pmdmntGetApplicationProcessId(&pid);
    pmdmntExit();

    // 실행 중인 애플리케이션이 없으면 실패를 돌려준다.
    return R_SUCCEEDED(rc) && pid != 0;
}


bool hasSaveDataChanged(const SyncOptions& options, u64 titleID)
{
    const u64 current = latestSaveDataTimestamp(options.uid, titleID);

    // 시각을 못 읽었으면 판단할 근거가 없다. 안전한 쪽으로 (업로드).
    if (current == 0) return true;

    return current != readSyncedTimestamp(options.saveDataPath, titleID);
}


void markSaveDataSynced(const SyncOptions& options, u64 titleID)
{
    const u64 current = latestSaveDataTimestamp(options.uid, titleID);
    if (current == 0) return;

    writeSyncedTimestamp(options.saveDataPath, titleID, current);
}


int pushAllSaves(const SyncOptions& options, SyncLogFunc log)
{
    HTTPRemoteStore remoteStore(options.serverUrl, options.saveDataPath);
    recursiveMkdir(options.saveDataPath.c_str());

    const ProbeTitlesFunc probeFunc = [&](const AccountUid probeUid, std::vector<u64>& titleIDs) -> int
    {
        int ret = options.archiveBy == "all"
            ? probeAllTitles(probeUid, titleIDs)
            : probeSaveDataCreatedTitles(probeUid, titleIDs);
        if (ret != 0) return ret;

        filterExcludedTitles(titleIDs, options.excludedTitleIds, options.excludedTitleNames);

        // 안 바뀐 타이틀은 압축조차 하지 않는다. 여기서 걸러야 의미가 있다.
        if (options.skipUnchanged)
        {
            std::vector<u64> changed;
            changed.reserve(titleIDs.size());

            for (const u64 titleID : titleIDs)
            {
                if (hasSaveDataChanged(options, titleID))
                    changed.push_back(titleID);
            }

            const size_t skipped = titleIDs.size() - changed.size();
            if (skipped > 0)
                log("Skipping " + std::to_string(skipped) + " unchanged title(s)");

            titleIDs.swap(changed);
        }

        return 0;
    };

    return archiveAllSaveData(
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
            if (ret != SAVEDATA_OK)
                log("Failed to archive, ret=" + std::to_string(ret));
            else
            {
                int pushRet = remoteStore.push(options.nickname, titleID);
                if (pushRet != 0)
                    log("Failed to push, ret=" + std::to_string(pushRet));
                else if (options.skipUnchanged)
                {
                    // 성공한 것만 기록한다. 실패한 타이틀은 다음에 다시 올라간다.
                    markSaveDataSynced(options, titleID);
                }
            }
            return true;
        }
    );
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
    // 시스템 시계가 뒤로 간 경우 (RTC 재설정 등) 그냥 실행한다.
    if (now < last) return true;

    return (now - last) >= (time_t)intervalHours * 3600;
}
