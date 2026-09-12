#include "NativeRestoreScreen.hpp"

#include "ConfirmScreen.hpp"
#include "ProgressScreen.hpp"
#include "../utils.hpp"

#include <algorithm>

namespace gui
{
NativeRestoreScreen::NativeRestoreScreen(const SyncOptions& options)
    : options(options)
{
}

void NativeRestoreScreen::load()
{
    loaded = true;
    loadResult = nativerestore::listAvailable(options, backups, &skipped);
    if (loadResult != 0)
    {
        statusMessage = "Could not load server backup list (code "
            + std::to_string(loadResult) + ")";
        return;
    }
    selected.assign(backups.size(), false);
    statusMessage = std::to_string(backups.size()) + " restorable game(s); "
        + std::to_string(skipped) + " title(s) without backup hidden";
}

void NativeRestoreScreen::continueRestore()
{
    std::vector<nativerestore::Backup> chosen;
    for (size_t i = 0; i < backups.size(); ++i)
        if (selected[i]) chosen.push_back(backups[i]);
    if (chosen.empty())
    {
        statusMessage = "Select at least one game first.";
        return;
    }

    const SyncOptions restoreOptions = options;
    const size_t count = chosen.size();
    auto work = [restoreOptions, chosen](SyncLogFunc log) -> int
    {
        return nativerestore::restoreSelected(restoreOptions, chosen, log);
    };

    App::instance().pushScreen(new ConfirmScreen("Restore selected native saves?", {
        std::to_string(count) + " selected game(s) will be restored.",
        "All selected backups are verified before any save changes.",
        "If one preflight fails, the whole restore is cancelled."
    }, [work]() mutable
    {
        App::instance().pushScreen(
            new ProgressScreen("Restore Native Saves", std::move(work)));
    }));
}

void NativeRestoreScreen::update(u64 kDown)
{
    if (!loaded)
    {
        load();
        return;
    }
    if (kDown & HidNpadButton_B)
    {
        App::instance().popScreen();
        return;
    }
    if (loadResult != 0 || backups.empty()) return;

    if (kDown & HidNpadButton_AnyUp)
    {
        selectedIndex = std::max(0, selectedIndex - 1);
    }
    if (kDown & HidNpadButton_AnyDown)
    {
        selectedIndex = std::min((int)backups.size() - 1, selectedIndex + 1);
    }
    if (kDown & HidNpadButton_A)
    {
        selected[selectedIndex] = !selected[selectedIndex];
    }
    if (kDown & HidNpadButton_X)
    {
        const bool selectAll = std::find(selected.begin(), selected.end(), false)
            != selected.end();
        std::fill(selected.begin(), selected.end(), selectAll);
    }
    if (kDown & HidNpadButton_Y)
    {
        continueRestore();
        return;
    }

    const int maxVisible = 13;
    if (selectedIndex < scrollOffset) scrollOffset = selectedIndex;
    if (selectedIndex >= scrollOffset + maxVisible)
        scrollOffset = selectedIndex - maxVisible + 1;
}

void NativeRestoreScreen::render(Renderer& r)
{
    const int x = 60;
    int y = 35;
    r.drawText("Select Native Saves to Restore", x, y, 30, COLOR_ACCENT);
    y += 45;
    r.drawRect(x, y, r.screenWidth() - x * 2, 2, COLOR_ACCENT);
    y += 12;

    if (!loaded)
    {
        r.drawText("Loading installed titles and server backups...", x, y, 22, COLOR_DIM);
    }
    else if (loadResult == 0)
    {
        const int maxVisible = 13;
        for (int i = scrollOffset; i < (int)backups.size()
             && i < scrollOffset + maxVisible; ++i)
        {
            const Color color = i == selectedIndex ? COLOR_ACCENT : COLOR_TEXT;
            const std::string line = std::string(selected[i] ? "[x] " : "[ ] ")
                + backups[i].titleName + "  [" + toHex(backups[i].titleId) + "]";
            if (i == selectedIndex)
                r.drawRect(x - 8, y - 2, r.screenWidth() - x * 2 + 16, 30, COLOR_BUTTON);
            r.drawText(line, x, y, 18, color);
            y += 34;
        }
    }

    const int fy = r.screenHeight() - 75;
    if (!statusMessage.empty()) r.drawText(statusMessage, x, fy - 30, 18,
        loadResult == 0 ? COLOR_DIM : COLOR_ERROR);
    r.drawRect(x, fy, r.screenWidth() - x * 2, 2, {80, 80, 80, 255});
    r.drawText("A: Toggle   X: All/None   Y: Continue   B: Back", x, fy + 12, 18, COLOR_DIM);
}
}
