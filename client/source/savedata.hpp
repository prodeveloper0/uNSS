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

// 이 계정에 그 타이틀의 세이브가 없다. 고장이 아니라 흔한 일이다 -
// archiveBy=all 은 설치된 게임을 전부 훑으므로, 한 번도 켜보지 않은 게임이
// 계정마다 수두룩하다. 실패로 세면 백업 한 바퀴가 늘 "오류로 끝남" 이 되고,
// 그러면 마지막 성공 시각이 기록되지 않아 다음 바퀴가 또 전부를 다시 한다.
#define SAVEDATA_NO_SAVE_DATA -6


typedef std::function<int(const AccountUid, std::vector<u64>&)> ProbeTitlesFunc;

int archiveSaveData(AccountUid uid, const u64 titleID, const std::string& outputPath);
int archiveAllSaveData(AccountUid uid, const std::string& outputPath, const ProbeTitlesFunc& probeFunc);
int archiveAllSaveData(AccountUid uid, const std::string& outputPath, const ProbeTitlesFunc& probeFunc, const std::function<bool(int, int, u64)>& callback, const std::function<bool(int, int, int, u64)>& doneCallback);

int restoreSaveData(AccountUid uid, const u64 titleID, const std::string& sourcePath);
int restoreAllSaveData(AccountUid uid, const std::string& sourcePath);
int restoreAllSaveData(AccountUid uid, const std::string& sourcePath, const std::function<bool(int, int, u64)>& callback, const std::function<bool(int, int, int, u64)>& doneCallback);
