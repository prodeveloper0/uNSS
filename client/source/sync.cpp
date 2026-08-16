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


namespace
{

// 백업 대상 타이틀 목록. pushAllSaves 와 countChangedTitles 가 같은 기준을
// 써야 "바뀐 게 없다" 와 "올릴 게 없다" 가 어긋나지 않는다.
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

    const ProbeTitlesFunc probeFunc = [&](const AccountUid probeUid, std::vector<u64>& titleIDs) -> int
    {
        const int ret = collectTargetTitles(options, probeUid, titleIDs);
        if (ret != 0) return ret;

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

    // 개별 타이틀의 실패는 콜백 안에서만 보인다. archiveAllSaveData 는 목록을
    // 훑는 데 성공하면 OK 를 주기 때문에, 세어두지 않으면 서버가 아예 죽어
    // 있어도 이 함수는 0 을 돌려준다. 그러면 호출하는 쪽이 백업을 마쳤다고
    // 믿고 마지막 시각을 남기고, 24 시간 동안 다시 시도하지 않는다.
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
                // 이 계정은 그 게임을 저장한 적이 없다. 실패가 아니므로 세지
                // 않는다 - 세면 한 바퀴가 늘 "오류로 끝남" 이 되고, 그러면
                // 마지막 성공 시각이 남지 않아 다음 바퀴가 전부를 다시 한다.
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
                    // ret 은 늘 -1 이라 아무것도 말해주지 않는다. 뒤의 값이
                    // 진짜 원인이다: 음수면 연결 자체가 안 된 것
                    // (HTTPCLIENT_ERROR_*, 예: -5 = TLS), 양수면 서버가
                    // 돌려준 상태 코드다.
                    log("Failed to push, ret=" + std::to_string(pushRet)
                        + " http=" + std::to_string(remoteStore.getLastHttpResult()));
                    ++failures;
                }
                else if (options.skipUnchanged)
                {
                    // 성공한 것만 기록한다. 실패한 타이틀은 다음에 다시 올라간다.
                    markSaveDataSynced(options, titleID);
                }
            }
            return true;
        }
    );

    if (ret != 0) return ret;

    // 실패한 타이틀 수를 음수로 돌려준다. SAVEDATA_* 코드는 양수라 서로
    // 헷갈리지 않는다. 0 은 "하나도 빠짐없이 올라갔다" 는 뜻이고,
    // 마지막 백업 시각은 그때만 남겨야 한다.
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
    // 시스템 시계가 뒤로 간 경우 (RTC 재설정 등) 그냥 실행한다.
    if (now < last) return true;

    return (now - last) >= (time_t)intervalHours * 3600;
}
