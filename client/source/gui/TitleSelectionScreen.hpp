#pragma once
#include "Gui.hpp"
#include "../title.hpp"
#include "../ini.hpp"
#include <vector>
#include <string>

namespace gui
{

struct TitleItem {
    u64 id;
    std::string name;
    bool selected; // UI uses "selected" (whitelist)
};

class TitleSelectionScreen : public Screen
{
public:
    TitleSelectionScreen(Config& config, AccountUid uid);
    void update(u64 kDown) override;
    void render(Renderer& r) override;

private:
    Config& config;
    AccountUid uid;
    std::vector<TitleItem> titles;
    int selectedIndex = 0;
    int scrollOffset = 0;

    void loadTitles();
    void saveSelection();
};

} // namespace gui