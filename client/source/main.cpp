#include <string.h>
#include <string>

#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

#include <switch.h>

#include <curl/curl.h>

#include "account.hpp"
#include "http.hpp"
#include "ini.hpp"
#include "fileio.hpp"
#include "utils.hpp"
#include "gui/Gui.hpp"
#include "gui/MainScreen.hpp"


const char* gl_SaveDataPath = "sdmc:/uNSS/saves";
const char* gl_ConfigPath = "sdmc:/uNSS/config.ini";

Config gl_Config(gl_ConfigPath);


void initConfig()
{
    gl_Config["remote"]["enabled"].has(false);
    gl_Config["remote"]["serverUrl"].has("http://0.0.0.0:8989");
    // 자격증명은 URL 에 들어가고 libcurl 이 그것을 Authorization 헤더로 먼저
    // 보낸다. 검증을 끄면 핸드셰이크에 응답하는 누구나 평문 비밀번호를 받는다.
    // 콘솔이 모르는 루트를 쓴다면 먼저 sdmc:/uNSS/cacert.pem 을 놓아볼 것.
    gl_Config["remote"]["insecureSkipVerify"].has(false);
    gl_Config["account"]["defaultAccountName"].has("");
    gl_Config["account"]["useProfileSelector"].has(true);
    gl_Config["title"]["archiveBy"].has("created");
    gl_Config["title"]["restoreBy"].has("all");
    gl_Config["title"]["excludedTitleIds"].has("");
    gl_Config["title"]["excludedTitleNames"].has("");
    gl_Config["sync"]["autoPushOnLaunch"].has(false);
    gl_Config["sync"]["autoPushIntervalHours"].has(24);
    // 백업은 콘솔 전체가 대상인 편이 자연스럽다. sysmodule 이 이 값을 읽는다.
    gl_Config["sync"]["allAccounts"].has(true);
}


void initData()
{
    recursiveMkdir(gl_SaveDataPath);
}


int main(int argc, char** argv)
{
    socketInitializeDefault();
    curl_global_init(CURL_GLOBAL_DEFAULT);

    initData();
    initConfig();

    HTTPClient::setVerifyTls(!(bool)gl_Config["remote"]["insecureSkipVerify"]);

    auto* mainScreen = new gui::MainScreen(gl_Config);

    gui::App::instance().run(mainScreen);

    curl_global_cleanup();
    socketExit();

    return 0;
}
