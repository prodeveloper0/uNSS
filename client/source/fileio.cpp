#include "fileio.hpp"

#include <string>
#include <functional>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <fcntl.h>

#include "tui.hpp"


int walk(const std::string& path, std::function<void(const std::string&, bool isDir)> callback)
{
    DIR* dir = opendir(path.c_str());
    if (dir == NULL)
    {
        return -1;
    }

    dirent* entry;
    while ((entry = readdir(dir)) != NULL)
    {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
        {
            continue;
        }

        const std::string fullPath = path[path.size() - 1] == '/' ? path + entry->d_name : path + "/" + entry->d_name;

        if (entry->d_type == DT_DIR)
        {
            walk(fullPath, callback);
            callback(fullPath, true);
        }
        else
        {
            callback(fullPath, false);
        }
    }

    closedir(dir);
    return 0;
}


int recursiveMkdir(const std::string& path, mode_t mode) 
{
    std::string::size_type pos = path.find(':');
    if (pos == std::string::npos) 
    {
        return -1;
    }
    
    pos = path.find('/', pos);
    if (pos == std::string::npos) 
    {
        return 0;
    }
    
    while ((pos = path.find('/', pos + 1)) != std::string::npos) 
    {
        std::string subpath = path.substr(0, pos);
        int status = mkdir(subpath.c_str(), mode);
        if (status != 0 && errno != EEXIST) 
        {
            return status;
        }
    }
    
    int status = mkdir(path.c_str(), mode);
    if (status != 0 && errno != EEXIST) 
    {
        return status;
    }
    
    return 0;
}


int mountSaveData(const std::string& mountPoint, const AccountUid accountUid, const u64 titleID)
{
    Result rc = fsdevMountSaveData(mountPoint.c_str(), titleID, accountUid);
    if (R_FAILED(rc))
    {
        return -1;
    }

    return 0;
}


int mountBcatSaveData(const std::string& mountPoint, const u64 titleID)
{
    Result rc = fsdevMountBcatSaveData(mountPoint.c_str(), titleID);
    if (R_FAILED(rc))
    {
        return -1;
    }

    return 0;
}


int unmount(const std::string& mountPoint)
{
    Result rc = fsdevUnmountDevice(mountPoint.c_str());
    if (R_FAILED(rc))
    {
        return -1;
    }

    return 0;
}