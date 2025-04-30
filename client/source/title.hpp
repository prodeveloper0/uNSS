#pragma once

#include <string>
#include <vector>

#include <switch.h>


int getTitleName(const u64 titleID, std::string& titleName);
int getTitleName(const u64 titleID, std::string& titleName, int language);
int probeTitles(const AccountUid accountUid, std::vector<u64>& titleIDs);
