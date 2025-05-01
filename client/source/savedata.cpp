#include "savedata.hpp"

#include <string.h>

#include "title.hpp"
#include "fileio.hpp"
#include "zipio.hpp"

#include "utils.hpp"
#include "tui.hpp"


int archiveSaveData(AccountUid uid, const u64 titleID, const std::string& outputPath)
{
    const Defer defer(
        []() {
            unmount("save");
            unmount("bcat");
        },
        []() {
            unmount("save");
            unmount("bcat");
        }
    );


    if (mountSaveData("save", uid, titleID) != 0)
    {
        return SAVEDATA_FAILED_TO_MOUNT;
    }
    
    const std::string stringfyTitleID = toHex(titleID);

    ZipWriter zipWriter;
    if (!zipWriter.open(outputPath + "/" + stringfyTitleID + ".sar"))
    {
        return SAVEDATA_FAILED_TO_OPEN_ARCHIVE;
    }

    bool success = true;

    walk("save:/", [&](const std::string& path) {
        if (!success)
        {
            return;
        }

        const std::string relativePath = path.substr(strlen("save:/"));
        success = zipWriter.add(path, "saves/" + relativePath);
    });
    
    if (!success)
    {
        return SAVEDATA_FAILED_TO_ADD_FILE;
    }

    if (mountBcatSaveData("bcat", titleID) == 0)
    {
        walk("bcat:/", [&](const std::string& path) {
            if (!success)
            {
                return;
            }

            const std::string relativePath = path.substr(strlen("bcat:/"));
            success = zipWriter.add(path, "bcat/" + relativePath);
        });
    }
    zipWriter.close();

    if (!success)
    {
        return SAVEDATA_FAILED_TO_ADD_FILE;
    }
    
    return 0;
}


int archiveAllSaveData(AccountUid uid, const std::string& outputPath)
{
    return archiveAllSaveData(
        uid, 
        outputPath, 
        [](int, int, u64) { 
            return true; 
        }, 
        [](int, int, int, u64) { 
            return true; 
        }
    );
}


int archiveAllSaveData(AccountUid uid, const std::string& outputPath, const std::function<bool(int, int, u64)>& callback, const std::function<bool(int, int, int, u64)>& doneCallback)
{
    std::vector<u64> titleIDs;
    if (probeTitles(uid, titleIDs) != 0)
    {
        return SAVEDATA_FAILED_TO_PROBE_TITLES;
    }

    for (size_t i = 0; i < titleIDs.size(); i++)
    {
        if (!callback(titleIDs.size(), i + 1, titleIDs[i]))
            continue;

        const int ret = archiveSaveData(uid, titleIDs[i], outputPath);

        if (!doneCallback(titleIDs.size(), i + 1, ret, titleIDs[i]))
            return ret;
    }

    return SAVEDATA_OK;
}


int restoreSaveData(AccountUid uid, const u64 titleID, const std::string& sourcePath)
{
    const std::string stringfyTitleID = toHex(titleID);
    const std::string sourceFilePath = sourcePath + "/" + stringfyTitleID + ".sar";

    ZipReader zipReader;
    if (!zipReader.open(sourceFilePath))
    {
        return SAVEDATA_FAILED_TO_OPEN_ARCHIVE;
    }

    const Defer defer(
        []() {
            unmount("save");
            unmount("bcat");
        },
        []() {
            unmount("save");
            unmount("bcat");
        }
    );


    if (mountSaveData("save", uid, titleID) != 0)
    {
        return SAVEDATA_FAILED_TO_MOUNT;
    }

    walk("save:/", [&](const std::string& path) {
        remove(path.c_str());
        fsdevCommitDevice("save");
    });

    if (mountBcatSaveData("bcat", titleID) == 0)
    {
        walk("bcat:/", [&](const std::string& path) {
            remove(path.c_str());
            fsdevCommitDevice("bcat");
        });
    }

    bool success = true;
    zipReader.walk([&](const std::string& path, ZipReader::EXTRACT_ONE_FUNC extractOneFunc) {
        if (!success)
        {
            return false;
        }

        if (startWith(path, "saves/"))
        {
            const std::string savePath = "save:/" + path.substr(strlen("saves/"));
            success = extractOneFunc(savePath);
            if (success)
            {
                fsdevCommitDevice("save");
            }
        }
        else if (startWith(path, "bcat/"))
        {
            const std::string bcatPath = "bcat:/" + path.substr(strlen("bcat/"));
            success = extractOneFunc(bcatPath);
            if (success)
            {
                fsdevCommitDevice("bcat");
            }
        }
        return success;
    });
    zipReader.close();

    if (!success)
    {
        return SAVEDATA_FAILED_TO_EXTRACT_FILE;
    }

    return SAVEDATA_OK;
}


int restoreAllSaveData(AccountUid uid, const std::string& sourcePath)
{
    return restoreAllSaveData(
        uid, 
        sourcePath, 
        [](int total, int current, u64 titleID) {
            return true;
        }, 
        [](int total, int current, int ret, u64 titleID) {
            return true;
        }
    );
}


int restoreAllSaveData(AccountUid uid, const std::string& sourcePath, const std::function<bool(int, int, u64)>& callback, const std::function<bool(int, int, int, u64)>& doneCallback)
{
    std::vector<u64> titleIDs;
    walk(sourcePath, [&](const std::string& path) {
        if (!endWith(path, ".sar"))
        {
            return;
        }

        const std::string stringfyTitleID = path.substr(sourcePath.size() + 1, 16);
        titleIDs.push_back(fromHex<u64>(stringfyTitleID));
    });

    for (size_t i = 0; i < titleIDs.size(); i++)
    {
        if (!callback(titleIDs.size(), i + 1, titleIDs[i])) 
            continue;

        const int ret = restoreSaveData(uid, titleIDs[i], sourcePath);
        if (!doneCallback(titleIDs.size(), i + 1, ret, titleIDs[i]))
            return ret;
    }

    return SAVEDATA_OK;
}
