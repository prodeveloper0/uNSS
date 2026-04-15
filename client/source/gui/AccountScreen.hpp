#pragma once
#include "Gui.hpp"
#include "../account.hpp"

#include <functional>
#include <vector>


namespace gui
{

class AccountScreen : public Screen
{
public:
    using OnSelectCallback = std::function<void(const Account&)>;

    AccountScreen(const OnSelectCallback& onSelect);
    ~AccountScreen();

    void update(u64 kDown) override;
    void render(Renderer& r) override;

private:
    OnSelectCallback onSelect;
    Account* accounts = nullptr;
    size_t nAccounts = 0;
    int selectedIndex = 0;
    bool loadError = false;
};

} // namespace gui
