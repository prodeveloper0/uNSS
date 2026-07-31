#pragma once

#include <string>


// 백그라운드 자동 백업을 담당하는 sysmodule 의 설치를 관리한다.
//
// 모듈 자체는 NRO 의 romfs 에 들어있다. Atmosphere 는
// atmosphere/contents/<PROGRAM_ID>/ 만 인식하므로 그 위치로 풀어준다.
namespace sysmodule
{

// config.json 의 program_id 와 반드시 같아야 한다.
//
// 0x0100... 대역은 시스템 모듈용으로 0x...0FFF 정도까지만 쓰인다.
// 그보다 높은 값은 애플리케이션으로 취급돼 boot2 가 아예 띄우지 않는다.
// 그래서 sys-patch(420000000000000B), nx-ovlloader(420000000007E51A) 와
// 같은 0x42 대역을 쓴다. 이 콘솔에서 실제로 도는 것이 확인된 대역이다.
constexpr const char* PROGRAM_ID = "4200000000554E53";

// romfs 에 들어있는 모듈의 버전. 모듈을 고칠 때마다 올린다.
// ID 가 바뀌었으므로 올린다 — 기존 설치본과 구분해야 한다.
constexpr int BUNDLED_VERSION = 2;


enum class State
{
    NotInstalled,   // 설치된 적 없음
    Outdated,       // 설치돼 있지만 NRO 가 들고 있는 것이 더 새것
    UpToDate,       // 최신
};


State getState();

// romfs 의 모듈을 atmosphere/contents 로 복사하고 boot2 플래그를 만든다.
// 성공하면 0.
int install();

// 설치한 파일들을 지운다. 성공하면 0.
int uninstall();

// 사용자에게 보여줄 설치 경로.
std::string installPath();

}
