#pragma once
#include "Gui.hpp"
#include "../account.hpp"
#include "../ini.hpp"
#include "../sync.hpp"
#include "../sysmodule.hpp"

#include <vector>


namespace gui
{

class MainScreen : public Screen
{
public:
    MainScreen(Config& config);

    void update(u64 kDown) override;
    void render(Renderer& r) override;

private:
    Config& config;

    Account account{};
    bool accountResolved = false;
    bool initialSelectDone = false;
    bool autoPushChecked = false;

    std::vector<MenuItem> menuItems;
    int selectedIndex = 0;

    void resolveAccount();
    void onAccountSelected(const Account& selected);
    void startPush();
    void startPull();
    void switchAccount();
    void rebuildMenu();

    SyncOptions buildSyncOptions() const;
    void startAutoPushIfDue();

    std::string statusMessage;

    void updateSysmoduleIfOutdated();
    void installSysmodule();
    void uninstallSysmodule();
    void resumeSysmodule();
};

} // namespace gui
