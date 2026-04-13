#pragma once

#include <string>
#include <vector>

#include <switch.h>


int getTitleName(const u64 titleID, std::string& titleName);
int getTitleName(const u64 titleID, std::string& titleName, int language);
int probeAllTitles(const AccountUid accountUid, std::vector<u64>& titleIDs);
int probeSaveDataCreatedTitles(const AccountUid accountUid, std::vector<u64>& titleIDs);
void filterExcludedTitles(std::vector<u64>& titleIDs, const std::string& excludedTitleIds, const std::string& excludedTitleNames);
