#pragma once

#include "Gui.hpp"
#include "../nativerestore.hpp"

#include <string>
#include <vector>

namespace gui
{
class NativeRestoreScreen : public Screen
{
public:
    explicit NativeRestoreScreen(const SyncOptions& options);

    void update(u64 kDown) override;
    void render(Renderer& r) override;

private:
    SyncOptions options;
    std::vector<nativerestore::Backup> backups;
    std::vector<bool> selected;
    bool loaded = false;
    int loadResult = 0;
    int skipped = 0;
    int selectedIndex = 0;
    int scrollOffset = 0;
    std::string statusMessage;

    void load();
    void continueRestore();
};
}
