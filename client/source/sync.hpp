#pragma once

#include <functional>
#include <string>

#include <switch.h>


using SyncLogFunc = std::function<void(const std::string&)>;


struct SyncOptions
{
    AccountUid uid = {};
    std::string nickname;
    std::string saveDataPath;
    std::string serverUrl;
    bool remoteEnabled = false;

    std::string archiveBy = "created";
    std::string restoreBy = "all";

    std::string excludedTitleIds;
    std::string excludedTitleNames;

    // Skip titles whose Horizon SaveDataId + CommitId matches the last
    // successfully uploaded revision for this account and title.
    bool skipUnchanged = true;
};


bool isGameRunning();

// Unknown metadata or missing state always returns true (fail-open).
bool hasSaveDataChanged(const SyncOptions& options, u64 titleID);

// Number of titles whose SaveData revision differs from .syncstate-v2.
// Returns -1 when title enumeration fails.
int countChangedTitles(const SyncOptions& options);


int pushAllSaves(const SyncOptions& options, SyncLogFunc log);
int pullAllSaves(const SyncOptions& options, SyncLogFunc log);


time_t readLastAutoSyncTime(const std::string& saveDataPath);
void writeLastAutoSyncTime(const std::string& saveDataPath, time_t when);

bool isAutoSyncDue(const std::string& saveDataPath, int intervalHours);
