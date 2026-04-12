#pragma once
#include <string>
#include <functional>

#include <dirent.h>

#include <switch.h>


int walk(const std::string& path, std::function<void(const std::string&, bool isDir)> callback);

int recursiveMkdir(const std::string& path, mode_t mode = 0777);

int mountSaveData(const std::string& mountPoint, const AccountUid accountUid, const u64 titleID);
int mountBcatSaveData(const std::string& mountPoint, const u64 titleID);
int unmount(const std::string& mountPoint);
