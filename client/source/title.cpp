#include "title.hpp"

#include "utils.hpp"

#include <string.h>
#include <algorithm>
#include <memory>
#include <new>
#include <set>


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

    // 스택이 아니라 힙이다. 이 구조체는 nacp 뒤에 0x20000 짜리 아이콘이
    // 붙어 있어 0x24000 - 144KB 다. 앱에서는 스택이 넉넉해 문제가 없지만
    // 시스템 모듈의 메인 스레드 스택은 16KB (config.json) 라, 스택에 두면
    // 함수 프롤로그에서 곧바로 넘친다. 실제로 그랬다: 2168-0002 data abort,
    // getTitleName+0x8.
    //
    // make_unique 가 아니라 nothrow new 다. 시스템 모듈은 -fno-exceptions 로
    // 빌드되는데 (client-sysmodule/Makefile) 힙은 2MiB 뿐이다. 할당이 실패하면
    // operator new 가 던지는 bad_alloc 을 받을 곳이 없어 그대로 abort 로 간다 -
    // 스택이 넘치던 것과 똑같이 모듈이 죽는다. 이 함수에는 이미 -1 로 물러나는
    // 길이 있으니 그쪽으로 보낸다. new(nothrow) T() 는 값 초기화라 0 으로
    // 채워져 나오므로 memset 은 따로 필요 없다.
    const std::unique_ptr<NsApplicationControlData> controlDataHolder(
        new (std::nothrow) NsApplicationControlData());
    if (!controlDataHolder) return -1;

    NsApplicationControlData& controlData = *controlDataHolder;

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


int probeAllTitles(const AccountUid accountUid, std::vector<u64>& titleIDs)
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

        // 이 계정의 것만 담는다. fsOpenSaveDataInfoReader 는 콘솔에 있는 모든
        // 세이브를 사용자 구분 없이 돌려주므로, 거르지 않으면 계정이 셋이든
        // 넷이든 모두 똑같은 목록을 받는다.
        //
        // 그 상태에서는 남의 세이브를 자기 것으로 열려다 실패하고, 그 실패가
        // "Failed to archive" 로 남았다 - 계정 하나당 스물세 번씩
        // (2026-08-01, Fränk 와 joseph 이 26 개 중 23 개에서 그랬다).
        // 고장이 아니라 "이 계정에는 그 세이브가 없다" 였다.
        //
        // 중복도 같은 이유로 사라진다: 한 게임을 세 사람이 저장해두면 목록에
        // 세 번 들어와 있었고, 로그에도 같은 제목이 세 줄 찍혔다.
        if (info.save_data_type == FsSaveDataType_Account
            && info.uid.uid[0] == accountUid.uid[0]
            && info.uid.uid[1] == accountUid.uid[1])
        {
            outputTitleIDs.push_back(info.application_id);
        }
    }
    fsSaveDataInfoReaderClose(&reader);

    titleIDs = outputTitleIDs;
    return 0;
}


static std::vector<std::string> splitBy(const std::string& str, const std::string& delimiter)
{
    std::vector<std::string> tokens;
    size_t start = 0;

    while (start < str.size())
    {
        size_t pos = str.find(delimiter, start);
        if (pos == std::string::npos)
        {
            tokens.push_back(str.substr(start));
            break;
        }

        tokens.push_back(str.substr(start, pos - start));
        start = pos + delimiter.size();
    }

    return tokens;
}


void filterExcludedTitles(std::vector<u64>& titleIDs, const std::string& excludedTitleIds, const std::string& excludedTitleNames)
{
    std::set<u64> excludedIds;
    if (!excludedTitleIds.empty())
    {
        for (const auto& hex : splitBy(excludedTitleIds, ","))
        {
            if (!hex.empty())
            {
                excludedIds.insert(fromHex<u64>(hex));
            }
        }
    }

    std::set<std::string> excludedNames;
    if (!excludedTitleNames.empty())
    {
        for (const auto& name : splitBy(excludedTitleNames, "||"))
        {
            if (!name.empty())
            {
                excludedNames.insert(name);
            }
        }
    }

    titleIDs.erase(
        std::remove_if(titleIDs.begin(), titleIDs.end(), [&](u64 titleID)
        {
            if (excludedIds.count(titleID))
            {
                return true;
            }

            if (!excludedNames.empty())
            {
                std::string titleName;
                if (getTitleName(titleID, titleName) == 0 && excludedNames.count(titleName))
                {
                    return true;
                }
            }

            return false;
        }),
        titleIDs.end()
    );
}


