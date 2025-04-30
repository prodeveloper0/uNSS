#include <string.h>
#include <functional>
#include <string>
#include <sstream>

#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

#include <switch.h>

#include <curl/curl.h>

#include "tui.hpp"
#include "account.hpp"
#include "title.hpp"
#include "fileio.hpp"
#include "zipio.hpp"
#include "utils.hpp"
#include "savedata.hpp"
#include "ini.hpp"


#define VERSION ("0.0.1")


const char* gl_SaveDataPath = "sdmc:/uNSS/saves";
const char* gl_ConfigPath = "sdmc:/uNSS/config.ini";

bool gl_bRunning = true;
Account gl_currentAccount;
Config gl_Config(gl_ConfigPath);


const std::map<u64, std::string> MENU_OPTIONS = {
    {HidNpadButton_A, "Push to server"},
    {HidNpadButton_B, "Pull from server"},
    {HidNpadButton_Plus, "Exit"}
};


void initConfig()
{
    gl_Config["remote"]["enabled"].has(false);
    gl_Config["remote"]["serverUrl"].has("http://0.0.0.0:8989");
}


void initData()
{
    recursiveMkdir(gl_SaveDataPath);
}


const PromptMessage::OnKeyPressedCallback MENU_ON_KEY_PRESSED_CALLBACK = [](u64 key) {
    if (key == HidNpadButton_A) {
        drawText(false);
        drawText("Archiving save data...");
        recursiveMkdir(gl_SaveDataPath);
        archiveAllSaveData(
            gl_currentAccount.uid, 
            gl_SaveDataPath, 
            [](int total, int current, u64 titleID) {
                std::string titleName;

                if (getTitleName(titleID, titleName, 0) != 0)
                {
                    titleName = "Unknown";
                }

                drawText("[" + padding(current, 3) + "/" + padding(total, 3) + "] " + titleName);
                return true;
            },
            [](int total, int current, int ret, u64 titleID) {
                std::string titleName;
                if (getTitleName(titleID, titleName, 0) != 0)
                {
                    titleName = "Unknown";
                }

                drawText("Failed to archive save data, ret=" + std::to_string(ret));
                return true;
            }
        );

        drawText(false);
        drawText("Uploading to server...");
    } else if (key == HidNpadButton_B) {
        drawText(false);
        drawText("Restoring save data...");
        restoreAllSaveData(
            gl_currentAccount.uid, 
            gl_SaveDataPath, 
            [](int total, int current, u64 titleID) {
                std::string titleName;

                if (getTitleName(titleID, titleName, 0) != 0)
                {
                    titleName = "Unknown";
                }

                drawText("[" + padding(current, 3) + "/" + padding(total, 3) + "] " + titleName);
                return true;
            },
            [](int total, int current, int ret, u64 titleID) {
                std::string titleName;
                if (getTitleName(titleID, titleName, 0) != 0)
                {
                    titleName = "Unknown";
                }

                drawText("Failed to restore save data, ret=" + std::to_string(ret));
                return true;
            }
        );
    } else if (key == HidNpadButton_Plus) {
        drawText("Exiting...\n");
        gl_bRunning = false;
    }
};


const PromptMessage MENU_PROMPT_MESSAGE = PromptMessage("", MENU_OPTIONS, MENU_ON_KEY_PRESSED_CALLBACK);


void testCurl()
{
    const std::string url = "http://192.168.255.43:8000/";

    CURL *curl;
    CURLcode res;

    curl = curl_easy_init();
    if (curl) 
    {
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "libnx-curl/1.0");
        curl_easy_setopt(curl, CURLOPT_POST, 1L);

        drawText("curl_easy_perform");

        res = curl_easy_perform(curl);
        if (res != CURLE_OK) 
        {
            drawText("curl_easy_perform() failed: " + std::string(curl_easy_strerror(res)));
        }

        drawText("cleanup");
        curl_easy_cleanup(curl);
    }
}


int main(int argc, char **argv)
{
    consoleInit(NULL);
    socketInitializeDefault();
    curl_global_init(CURL_GLOBAL_DEFAULT);

    padConfigureInput(1, HidNpadStyleSet_NpadStandard);

    initData();
    initConfig();

    drawText("micro NX Save Sync\nVersion: " + std::string(VERSION));
    drawText();
    drawText(std::string() + "Remote Enabled: " + ((bool)gl_Config["remote"]["enabled"] ? "YES" : "NO"));
    drawText(std::string() + "Remote Server URL: " + (std::string)gl_Config["remote"]["serverUrl"]);
    drawText();

    auto ret = getCurrentAccount(&gl_currentAccount);
    if (ret != 0) 
    {
        drawText("Failed to get current account");
    } else 
    {
        drawText(std::string() + "Current account: " + gl_currentAccount.nickname);
    }

    while (gl_bRunning) 
    {
        MENU_PROMPT_MESSAGE.draw();
        MENU_PROMPT_MESSAGE.wait();
    }

    curl_global_cleanup();
    socketExit();
    consoleExit(NULL);

    return 0;
}
