#include <string.h>
#include <string>

#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

#include <switch.h>

#include <curl/curl.h>

#include "account.hpp"
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
    gl_Config["account"]["defaultAccountName"].has("");
    gl_Config["account"]["useProfileSelector"].has(true);
    gl_Config["title"]["archiveBy"].has("created");
    gl_Config["title"]["restoreBy"].has("all");
    gl_Config["title"]["excludedTitleIds"].has("");
    gl_Config["title"]["excludedTitleNames"].has("");
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

    auto* mainScreen = new gui::MainScreen(gl_Config);

    gui::App::instance().run(mainScreen);

    curl_global_cleanup();
    socketExit();

    return 0;
}
