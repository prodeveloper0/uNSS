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
#include "http.hpp"
#include "remote.hpp"


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
    gl_Config["account"]["defaultAccountName"].has("");
    gl_Config["account"]["useProfileSelector"].has(true);
}


void initData()
{
    recursiveMkdir(gl_SaveDataPath);
}


const PromptMessage::OnKeyPressedCallback MENU_ON_KEY_PRESSED_CALLBACK = [](u64 key)
{
    HTTPRemoteStore remoteStore(gl_Config["remote"]["serverUrl"], gl_SaveDataPath);

    if (key == HidNpadButton_A)
    {
        drawText(false);
        drawText("Archiving save data...");
        recursiveMkdir(gl_SaveDataPath);

        if (!gl_Config["remote"]["enabled"])
        {
            drawText("WARNING: pushing to remote is not enabled");
            return;
        }

        archiveAllSaveData(
            gl_currentAccount.uid,
            gl_SaveDataPath,
            [](int total, int current, u64 titleID)
            {
                std::string titleName;

                if (getTitleName(titleID, titleName, 0) != 0)
                {
                    titleName = "Unknown";
                }

                drawText("[" + padding(current, 3) + "/" + padding(total, 3) + "] " + titleName);
                return true;
            },
            [&remoteStore](int total, int current, int ret, u64 titleID)
            {
                if (ret != SAVEDATA_OK)
                {
                    drawText("Failed to archive save data, ret=" + std::to_string(ret));
                }
                else if (gl_Config["remote"]["enabled"])
                {
                    int pullRet = remoteStore.push(gl_currentAccount.nickname, titleID);
                    if (pullRet != 0)
                    {
                        drawText(std::string() + "Failed to push to server, ret=" + std::to_string(pullRet));
                    }
                }

                return true;
            }
        );
        drawText(false);
    }
    else if (key == HidNpadButton_B)
    {
        drawText(false);
        drawText("Restoring save data...");
        recursiveMkdir(gl_SaveDataPath);

        if (!gl_Config["remote"]["enabled"])
        {
            drawText("WARNING: pulling from remote is disabled, save data will be restored from local");
            restoreAllSaveData(
                gl_currentAccount.uid,
                gl_SaveDataPath,
                [](int total, int current, u64 titleID)
                {
                    std::string titleName;

                    if (getTitleName(titleID, titleName, 0) != 0)
                    {
                        titleName = "Unknown";
                    }

                    drawText("[" + padding(current, 3) + "/" + padding(total, 3) + "] " + titleName);
                    return true;
                },
                [&remoteStore](int total, int current, int ret, u64 titleID)
                {
                    if (ret != SAVEDATA_OK)
                    {
                        drawText("Failed to restore save data, ret=" + std::to_string(ret));
                    }
                    else if (gl_Config["remote"]["enabled"])
                    {
                        int pullRet = remoteStore.pull(gl_currentAccount.nickname, titleID);
                        if (pullRet != 0)
                        {
                            drawText(std::string() + "Failed to pull from server, ret=" + std::to_string(pullRet));
                        }
                    }

                    return true;
                }
            );
        }
        else
        {
            std::vector<u64> titleIDs;
            if (probeTitles(gl_currentAccount.uid, titleIDs) != 0)
            {
                drawText("Failed to probe titles");
                return;
            }

            for (size_t i = 0; i < titleIDs.size(); ++i)
            {
                std::string titleName;
                if (getTitleName(titleIDs[i], titleName, 0) != 0)
                {
                    titleName = "Unknown";
                }

                drawText("[" + padding(i + 1, 3) + "/" + padding(titleIDs.size(), 3) + "] " + titleName);
                if (remoteStore.pull(gl_currentAccount.nickname, titleIDs[i]) != 0)
                {
                    drawText("Failed to pull from server");
                }
                else
                {
                    restoreSaveData(gl_currentAccount.uid, titleIDs[i], gl_SaveDataPath);
                }
            }
        }
    }
    else if (key == HidNpadButton_Plus)
    {
        drawText("Exiting...\n");
        gl_bRunning = false;
    }
};


const PromptMessage MENU_PROMPT_MESSAGE = PromptMessage("", MENU_OPTIONS, MENU_ON_KEY_PRESSED_CALLBACK);


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

    AccountResolveOptions accountOptions;
    accountOptions.defaultAccountName = gl_Config["account"]["defaultAccountName"].value;
    accountOptions.useProfileSelector = (bool)gl_Config["account"]["useProfileSelector"];
    auto ret = getCurrentAccount(&gl_currentAccount, accountOptions);
    if (ret != 0)
    {
        drawText("Failed to get current account");
        drawText();

        const std::map<u64, std::string> EXIT_ONLY_OPTIONS = {
            {HidNpadButton_Plus, "Exit"}
        };
        const PromptMessage exitPrompt("", EXIT_ONLY_OPTIONS, [](u64 key)
        {
            if (key == HidNpadButton_Plus)
            {
                gl_bRunning = false;
            }
        });
        while (gl_bRunning)
        {
            exitPrompt.draw();
            exitPrompt.wait();
        }
    }
    else
    {
        drawText(std::string() + "Current account: " + gl_currentAccount.nickname);

        while (gl_bRunning)
        {
            MENU_PROMPT_MESSAGE.draw();
            MENU_PROMPT_MESSAGE.wait();
        }
    }

    curl_global_cleanup();
    socketExit();
    consoleExit(NULL);

    return 0;
}
