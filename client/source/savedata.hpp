#pragma once
#include <string>
#include <functional>
#include <vector>

#include <switch.h>


#define SAVEDATA_OK 0
#define SAVEDATA_FAILED_TO_MOUNT -1
#define SAVEDATA_FAILED_TO_OPEN_ARCHIVE -2
#define SAVEDATA_FAILED_TO_ADD_FILE -3
#define SAVEDATA_FAILED_TO_EXTRACT_FILE -4
#define SAVEDATA_FAILED_TO_PROBE_TITLES -5


int archiveSaveData(AccountUid uid, const u64 titleID, const std::string& outputPath);
int archiveAllSaveData(AccountUid uid, const std::string& outputPath);
int archiveAllSaveData(AccountUid uid, const std::string& outputPath, const std::function<bool(int, int, u64)>& callback, const std::function<bool(int, int, int, u64)>& errorCallback);

int restoreSaveData(AccountUid uid, const u64 titleID, const std::string& sourcePath);
int restoreAllSaveData(AccountUid uid, const std::string& sourcePath);
int restoreAllSaveData(AccountUid uid, const std::string& sourcePath, const std::function<bool(int, int, u64)>& callback, const std::function<bool(int, int, int, u64)>& errorCallback);
