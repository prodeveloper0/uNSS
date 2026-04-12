#include "title.hpp"

#include "utils.hpp"

#include <string.h>


#include "tui.hpp"


int getTitleName(const u64 titleID, std::string& titleName)
{
    return getTitleName(titleID, titleName, -1);
}


int getTitleName(const u64 titleID, std::string& titleName, int language)
{
    const Defer defer(
        [&]()
        {
            nsInitialize();
        },
        [&]()
        {
            nsExit();
        }
    );

    NsApplicationControlData controlData = {0x00,};
    size_t actualSize;

    Result rc = nsGetApplicationControlData(NsApplicationControlSource_Storage, titleID, &controlData, sizeof(controlData), &actualSize);
  
    if (R_SUCCEEDED(rc)) 
    {
        if (language == -1)
        {
            // Use device preferred language
            NacpLanguageEntry *langentry;
            rc = nacpGetLanguageEntry(&controlData.nacp, &langentry);
            if (R_SUCCEEDED(rc))
            {
                char buf[0x201] = {0x00, };
                strncpy(buf, langentry->name, 0x200);
                titleName = buf;
            }
        }
        else
        {
            // Use specified language
            char buf[0x201] = {0x00, };
            strncpy(buf, controlData.nacp.lang[language].name, 0x200);
            titleName = buf;
        }

        return 0;
    }

    return -1;
}


int probeAllTitles(std::vector<u64>& titleIDs)
{
    const Defer defer(
        [&]()
        {
            nsInitialize();
        },
        [&]()
        {
            nsExit();
        }
    );

    std::vector<u64> outputTitleIDs;
    NsApplicationRecord records[32];
    s32 entryCount = 0;
    s32 offset = 0;

    while (true)
    {
        Result rc = nsListApplicationRecord(records, 32, offset, &entryCount);
        if (R_FAILED(rc) || entryCount == 0)
        {
            break;
        }

        for (s32 i = 0; i < entryCount; i++)
        {
            outputTitleIDs.push_back(records[i].application_id);
        }

        offset += entryCount;
    }

    titleIDs = outputTitleIDs;
    return 0;
}


int probeSaveDataCreatedTitles(const AccountUid accountUid, std::vector<u64>& titleIDs)
{
    std::vector<u64> outputTitleIDs;

    Result rc;
    FsSaveDataInfoReader reader;
    FsSaveDataInfo info;

    rc = fsOpenSaveDataInfoReader(&reader, FsSaveDataSpaceId_User);
    if (R_FAILED(rc))
    {
        return -1;
    }

    s64 totalEntries;

    while(1)
    {
        rc = fsSaveDataInfoReaderRead(&reader, &info, 1, &totalEntries);
        if (R_FAILED(rc) || totalEntries == 0)
        {
            break;
        }

        if (info.save_data_type == FsSaveDataType_Account)
        {
            outputTitleIDs.push_back(info.application_id);
        }
    }
    fsSaveDataInfoReaderClose(&reader);

    titleIDs = outputTitleIDs;
    return 0;
}


int probeTitlesBy(const std::string& probeBy, const AccountUid accountUid, std::vector<u64>& titleIDs)
{
    if (probeBy == "all")
    {
        return probeAllTitles(titleIDs);
    }

    return probeSaveDataCreatedTitles(accountUid, titleIDs);
}
