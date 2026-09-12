#pragma once
#include <string>
#include <functional>

#include <dirent.h>

#include <switch.h>


// Recursively visit files, then directories. Keep one callback instance to
// avoid growing the small sysmodule stack with std::function copies. Return
// -2 when maxDepth is exceeded so callers never publish a partial traversal.
int walk(const std::string& path, const std::function<void(const std::string&, bool isDir)>& callback, int maxDepth = 64);

int recursiveMkdir(const std::string& path, mode_t mode = 0777);

// 그 계정에 그 타이틀의 세이브가 아예 없을 때. 실패와 구별해야 한다.
#define MOUNT_TARGET_NOT_FOUND -2

int createSaveData(const AccountUid accountUid, const u64 titleID);

// 0 = 성공, MOUNT_TARGET_NOT_FOUND = 세이브 없음, 그 밖의 음수 = 실패.
int mountSaveData(const std::string& mountPoint, const AccountUid accountUid, const u64 titleID);
int mountBcatSaveData(const std::string& mountPoint, const u64 titleID);
int unmount(const std::string& mountPoint);
