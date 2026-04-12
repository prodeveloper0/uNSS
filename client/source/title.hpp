#pragma once

#include <string>
#include <vector>

#include <switch.h>


int getTitleName(const u64 titleID, std::string& titleName);
int getTitleName(const u64 titleID, std::string& titleName, int language);
int probeAllTitles(std::vector<u64>& titleIDs);
int probeSaveDataCreatedTitles(const AccountUid accountUid, std::vector<u64>& titleIDs);
int probeTitlesBy(const std::string& probeBy, const AccountUid accountUid, std::vector<u64>& titleIDs);
