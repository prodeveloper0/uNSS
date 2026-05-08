#include "TitleSelectionScreen.hpp"
#include "../utils.hpp"
#include "../account.hpp"
#include <set>
#include <algorithm> // Required for std::sort

namespace gui
{

TitleSelectionScreen::TitleSelectionScreen(Config& config, AccountUid uid) : config(config), uid(uid)
{
    loadTitles();
}

void TitleSelectionScreen::loadTitles()
{
    std::set<u64> excludedIds;
    std::string currentExclusions = config["title"]["excludedTitleIds"].value;
    
    // Parse current exclusions
    size_t start = 0;
    while (start < currentExclusions.size()) {
        size_t pos = currentExclusions.find(',', start);
        std::string hex = (pos == std::string::npos) ? currentExclusions.substr(start) : currentExclusions.substr(start, pos - start);
        
        hex.erase(0, hex.find_first_not_of(" \t\r\n"));
        hex.erase(hex.find_last_not_of(" \t\r\n") + 1);
        
        if (!hex.empty()) {
            excludedIds.insert(fromHex<u64>(hex));
        }
        if (pos == std::string::npos) break;
        start = pos + 1;
    }

    // FIX: Gather ALL installed titles AND titles with save data to match Push/Pull logic
    std::vector<u64> allInstalledIds;
    std::vector<u64> createdSaveIds;
    probeAllTitles(uid, allInstalledIds);             // <-- Change here
    probeSaveDataCreatedTitles(uid, createdSaveIds);

    // Combine and deduplicate
    std::set<u64> combinedIds(allInstalledIds.begin(), allInstalledIds.end());
    combinedIds.insert(createdSaveIds.begin(), createdSaveIds.end());

    for (u64 id : combinedIds) {
        TitleItem item;
        item.id = id;
        
        // Invert logic: If it is IN the exclusion list, it is NOT selected.
        item.selected = (excludedIds.count(id) == 0);
        
        if (getTitleName(id, item.name) != 0) {
            item.name = "Unknown Title (" + toHex(id) + ")";
        }
        titles.push_back(item);
    }

    // FIX: Sort alphabetically to make finding games easier
    std::sort(titles.begin(), titles.end(), [](const TitleItem& a, const TitleItem& b) {
        return a.name < b.name;
    });
}

void TitleSelectionScreen::saveSelection()
{
    std::set<u64> finalExcludedIds;

    // 1. Keep previously excluded IDs that might not be installed anymore
    //    (prevents them from being wiped when you uninstall a game)
    std::string currentExclusions = config["title"]["excludedTitleIds"].value;
    size_t start = 0;
    while (start < currentExclusions.size()) {
        size_t pos = currentExclusions.find(',', start);
        std::string hex = (pos == std::string::npos) ? currentExclusions.substr(start) : currentExclusions.substr(start, pos - start);
        hex.erase(0, hex.find_first_not_of(" \t\r\n"));
        hex.erase(hex.find_last_not_of(" \t\r\n") + 1);
        if (!hex.empty()) finalExcludedIds.insert(fromHex<u64>(hex));
        if (pos == std::string::npos) break;
        start = pos + 1;
    }

    // 2. Add unselected items to exclusion, remove selected items from exclusion
    for (const auto& item : titles) {
        if (!item.selected) {
            finalExcludedIds.insert(item.id);
        } else {
            finalExcludedIds.erase(item.id);
        }
    }

    // 3. Build string
    std::string newExclusions = "";
    for (u64 id : finalExcludedIds) {
        if (!newExclusions.empty()) newExclusions += ", ";
        newExclusions += toHex(id);
    }
    
    config["title"]["excludedTitleIds"] = newExclusions;
    config.save("sdmc:/uNSS/config.ini");
}

void TitleSelectionScreen::update(u64 kDown)
{
    if (titles.empty()) {
        if (kDown & HidNpadButton_B) App::instance().popScreen();
        return;
    }

    int maxVisible = 10;

    if (kDown & HidNpadButton_AnyUp) {
        selectedIndex--;
        if (selectedIndex < 0) selectedIndex = titles.size() - 1;
    }
    if (kDown & HidNpadButton_AnyDown) {
        selectedIndex++;
        if (selectedIndex >= (int)titles.size()) selectedIndex = 0;
    }

    if (selectedIndex < scrollOffset) scrollOffset = selectedIndex;
    if (selectedIndex >= scrollOffset + maxVisible) scrollOffset = selectedIndex - maxVisible + 1;

    if (kDown & HidNpadButton_A) {
        titles[selectedIndex].selected = !titles[selectedIndex].selected;
    }

    if (kDown & HidNpadButton_X) {
        for (auto& item : titles) item.selected = true;
    }

    if (kDown & HidNpadButton_Y) {
        for (auto& item : titles) item.selected = false;
    }

    if (kDown & HidNpadButton_B) {
        saveSelection();
        App::instance().popScreen();
    }
}

void TitleSelectionScreen::render(Renderer& r)
{
    int x = 80;
    int y = 60;

    r.drawText("Select Games to Sync", x, y, 32, COLOR_ACCENT);
    y += 50;
    r.drawRect(x, y, r.screenWidth() - x * 2, 2, COLOR_ACCENT);
    y += 20;

    if (titles.empty()) {
        r.drawText("No save data found.", x, y, 24, COLOR_DIM);
    } else {
        int maxVisible = 10;
        int lineHeight = 40;

        for (int i = scrollOffset; i < (int)titles.size() && i < scrollOffset + maxVisible; i++) {
            Color textColor = (i == selectedIndex) ? COLOR_HIGHLIGHT : COLOR_TEXT;
            
            std::string checkbox = titles[i].selected ? "[X] " : "[ ] ";
            r.drawText(checkbox + titles[i].name, x, y, 24, textColor);
            
            y += lineHeight;
        }
    }

    int fy = r.screenHeight() - 50;
    r.drawRect(x, fy - 10, r.screenWidth() - x * 2, 2, {80, 80, 80, 255});
    r.drawText("A: Toggle   X: Select All   Y: Deselect All   B: Save & Back", x, fy, 18, COLOR_DIM);
}

} // namespace gui