#include "MainScreen.hpp"
#include "ProgressScreen.hpp"
#include "AccountScreen.hpp"

#include "../title.hpp"
#include "../savedata.hpp"
#include "../remote.hpp"
#include "../utils.hpp"
#include "../fileio.hpp"


namespace gui
{

MainScreen::MainScreen(Config& config)
    : config(config)
{
}


void MainScreen::resolveAccount()
{
    bool useSelector = (bool)config["account"]["useProfileSelector"];
    std::string defaultName = config["account"]["defaultAccountName"].value;

    // useProfileSelector=0 이고 유효한 defaultAccountName이 있으면 선택창 없이 매칭
    if (!useSelector && !defaultName.empty())
    {
        AccountResolveOptions opts;
        opts.defaultAccountName = defaultName;
        opts.useProfileSelector = false;

        if (getCurrentAccount(&account, opts) == 0)
        {
            accountResolved = true;
            rebuildMenu();
        }
        // 매칭 실패해도 선택창 안 띄움 (accountResolved=false 상태로 에러 표시)
        return;
    }

    // useProfileSelector=1 이거나, defaultAccountName이 비어있으면 선택창
    // full application mode에서는 psel 먼저 시도
    const AppletType appletType = appletGetAppletType();
    const bool isFullApp =
        appletType == AppletType_Application ||
        appletType == AppletType_SystemApplication;

    if (isFullApp)
    {
        AccountResolveOptions opts;
        opts.defaultAccountName = defaultName;
        opts.useProfileSelector = true;

        if (getCurrentAccount(&account, opts) == 0)
        {
            accountResolved = true;
            rebuildMenu();
            return;
        }
    }

    // applet mode이거나 psel 실패: 내장 계정 선택 화면
    App::instance().pushScreen(new AccountScreen([this](const Account& selected)
    {
        onAccountSelected(selected);
    }));
}


void MainScreen::onAccountSelected(const Account& selected)
{
    account = selected;
    accountResolved = true;
    rebuildMenu();
}


void MainScreen::rebuildMenu()
{
    menuItems.clear();
    selectedIndex = 0;

    if (accountResolved)
    {
        bool remoteEnabled = (bool)config["remote"]["enabled"];

        menuItems.push_back({"Push to Server", [this]() { startPush(); }, remoteEnabled});
        menuItems.push_back({"Pull from Server", [this]() { startPull(); }, true});
    }
}


void MainScreen::update(u64 kDown)
{
    // 첫 프레임: 계정 해석
    if (!initialSelectDone)
    {
        initialSelectDone = true;
        resolveAccount();
        return;
    }

    if (!accountResolved) return;

    if (kDown & HidNpadButton_AnyUp)
    {
        selectedIndex--;
        if (selectedIndex < 0) selectedIndex = menuItems.size() - 1;
    }
    if (kDown & HidNpadButton_AnyDown)
    {
        selectedIndex++;
        if (selectedIndex >= (int)menuItems.size()) selectedIndex = 0;
    }
    if (kDown & HidNpadButton_A)
    {
        if (!menuItems.empty() && menuItems[selectedIndex].enabled)
        {
            menuItems[selectedIndex].action();
        }
    }
    if (kDown & HidNpadButton_Minus)
    {
        switchAccount();
    }
}


void MainScreen::switchAccount()
{
    const AppletType appletType = appletGetAppletType();
    const bool isFullApplicationMode =
        appletType == AppletType_Application ||
        appletType == AppletType_SystemApplication;

    if (isFullApplicationMode)
    {
        AccountUid uid;
        PselUserSelectionSettings settings = {};
        if (R_SUCCEEDED(pselShowUserSelector(&uid, &settings)))
        {
            AccountProfile profile;
            AccountProfileBase profileBase;
            if (R_SUCCEEDED(accountGetProfile(&profile, uid)))
            {
                if (R_SUCCEEDED(accountProfileGet(&profile, NULL, &profileBase)))
                {
                    strcpy(account.nickname, profileBase.nickname);
                    account.uid = uid;
                    accountResolved = true;
                    rebuildMenu();
                }
                accountProfileClose(&profile);
            }
        }
    }
    else
    {
        App::instance().pushScreen(new AccountScreen([this](const Account& selected)
        {
            onAccountSelected(selected);
        }));
    }
}


void MainScreen::render(Renderer& r)
{
    int x = 80;
    int y = 60;

    r.drawText("micro NX Save Sync", x, y, 32, COLOR_ACCENT);

    {
        const AppletType appletType = appletGetAppletType();
        const bool isFullApp =
            appletType == AppletType_Application ||
            appletType == AppletType_SystemApplication;
        if (!isFullApp)
        {
            int tagW = r.getTextWidth("Applet Mode", 18);
            r.drawText("Applet Mode", r.screenWidth() - x - tagW, y + 8, 18, COLOR_ERROR);
        }
    }

    y += 50;

    r.drawRect(x, y, r.screenWidth() - x * 2, 2, COLOR_ACCENT);
    y += 20;

    if (!accountResolved)
    {
        r.drawText("Waiting for account selection...", x, y, 24, COLOR_DIM);
        return;
    }

    r.drawText(std::string("Account: ") + account.nickname, x, y, 24);
    y += 36;

    bool remoteEnabled = (bool)config["remote"]["enabled"];
    r.drawText(std::string("Remote: ") + (remoteEnabled ? "Enabled" : "Disabled"), x, y, 18, COLOR_DIM);
    y += 28;

    if (remoteEnabled)
    {
        r.drawText(std::string("Server: ") + (std::string)config["remote"]["serverUrl"], x, y, 18, COLOR_DIM);
        y += 28;
    }

    y += 30;

    for (int i = 0; i < (int)menuItems.size(); i++)
    {
        int btnW = r.screenWidth() - x * 2;
        int btnH = 50;

        Color bgColor = (i == selectedIndex) ? COLOR_HIGHLIGHT : COLOR_BUTTON;
        Color textColor = menuItems[i].enabled ? COLOR_TEXT : COLOR_DIM;

        if (!menuItems[i].enabled && i == selectedIndex)
        {
            bgColor = {80, 80, 80, 255};
        }

        r.drawRect(x, y, btnW, btnH, bgColor);
        r.drawText(menuItems[i].label, x + 20, y + 12, 24, textColor);

        y += btnH + 10;
    }

    int fy = r.screenHeight() - 50;
    r.drawRect(x, fy - 10, r.screenWidth() - x * 2, 2, {80, 80, 80, 255});
    r.drawText("A: Select    -: Account    +: Exit", x, fy, 18, COLOR_DIM);
}


void MainScreen::startPush()
{
    const std::string saveDataPath = "sdmc:/uNSS/saves";
    const std::string serverUrl = (std::string)config["remote"]["serverUrl"];
    const std::string archiveBy = config["title"]["archiveBy"].value;
    const std::string excludedIds = config["title"]["excludedTitleIds"].value;
    const std::string excludedNames = config["title"]["excludedTitleNames"].value;
    const std::string nickname = account.nickname;
    const AccountUid uid = account.uid;

    auto work = [=](std::function<void(const std::string&)> log) -> int
    {
        HTTPRemoteStore remoteStore(serverUrl, saveDataPath);
        recursiveMkdir(saveDataPath.c_str());

        const ProbeTitlesFunc probeFunc = [&](const AccountUid probeUid, std::vector<u64>& titleIDs) -> int
        {
            int ret = archiveBy == "all"
                ? probeAllTitles(probeUid, titleIDs)
                : probeSaveDataCreatedTitles(probeUid, titleIDs);
            if (ret == 0)
            {
                filterExcludedTitles(titleIDs, excludedIds, excludedNames);
            }
            return ret;
        };

        return archiveAllSaveData(
            uid,
            saveDataPath,
            probeFunc,
            [&log](int total, int current, u64 titleID) -> bool
            {
                std::string titleName;
                if (getTitleName(titleID, titleName) != 0)
                    titleName = "Unknown";
                log("[" + padding(current, 3) + "/" + padding(total, 3) + "] " + titleName);
                return true;
            },
            [&log, &remoteStore, &nickname](int total, int current, int ret, u64 titleID) -> bool
            {
                if (ret != SAVEDATA_OK)
                    log("Failed to archive, ret=" + std::to_string(ret));
                else
                {
                    int pushRet = remoteStore.push(nickname, titleID);
                    if (pushRet != 0)
                        log("Failed to push, ret=" + std::to_string(pushRet));
                }
                return true;
            }
        );
    };

    App::instance().pushScreen(new ProgressScreen("Push to Server", std::move(work)));
}


void MainScreen::startPull()
{
    const std::string saveDataPath = "sdmc:/uNSS/saves";
    const std::string serverUrl = (std::string)config["remote"]["serverUrl"];
    const std::string restoreBy = config["title"]["restoreBy"].value;
    const std::string excludedIds = config["title"]["excludedTitleIds"].value;
    const std::string excludedNames = config["title"]["excludedTitleNames"].value;
    const std::string nickname = account.nickname;
    const AccountUid uid = account.uid;
    const bool remoteEnabled = (bool)config["remote"]["enabled"];

    auto work = [=](std::function<void(const std::string&)> log) -> int
    {
        HTTPRemoteStore remoteStore(serverUrl, saveDataPath);
        recursiveMkdir(saveDataPath.c_str());

        if (!remoteEnabled)
        {
            log("Remote is disabled, restoring from local...");
            return restoreAllSaveData(
                uid, saveDataPath,
                [&log](int total, int current, u64 titleID) -> bool
                {
                    std::string titleName;
                    if (getTitleName(titleID, titleName) != 0)
                        titleName = "Unknown";
                    log("[" + padding(current, 3) + "/" + padding(total, 3) + "] " + titleName);
                    return true;
                },
                [&log](int total, int current, int ret, u64 titleID) -> bool
                {
                    if (ret != SAVEDATA_OK)
                        log("Failed to restore, ret=" + std::to_string(ret));
                    return true;
                }
            );
        }

        std::vector<u64> titleIDs;
        int probeRet = restoreBy == "all"
            ? probeAllTitles(uid, titleIDs)
            : probeSaveDataCreatedTitles(uid, titleIDs);
        if (probeRet == 0)
            filterExcludedTitles(titleIDs, excludedIds, excludedNames);
        if (probeRet != 0)
        {
            log("Failed to probe titles");
            return probeRet;
        }

        for (size_t i = 0; i < titleIDs.size(); ++i)
        {
            std::string titleName;
            if (getTitleName(titleIDs[i], titleName) != 0)
                titleName = "Unknown";
            log("[" + padding(i + 1, 3) + "/" + padding(titleIDs.size(), 3) + "] " + titleName);

            if (remoteStore.pull(nickname, titleIDs[i]) != 0)
            {
                log("Failed to pull from server");
            }
            else
            {
                restoreSaveData(uid, titleIDs[i], saveDataPath);
            }
        }

        return 0;
    };

    App::instance().pushScreen(new ProgressScreen("Pull from Server", std::move(work)));
}

} // namespace gui
