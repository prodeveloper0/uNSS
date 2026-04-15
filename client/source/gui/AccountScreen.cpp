#include "AccountScreen.hpp"

#include <cstdlib>


namespace gui
{

AccountScreen::AccountScreen(const OnSelectCallback& onSelect)
    : onSelect(onSelect)
{
    if (probeAccounts(&accounts, &nAccounts) != 0)
    {
        loadError = true;
    }
}


AccountScreen::~AccountScreen()
{
    if (accounts)
        free(accounts);
}


void AccountScreen::update(u64 kDown)
{
    if (kDown & HidNpadButton_B)
    {
        App::instance().popScreen();
        return;
    }

    if (loadError || nAccounts == 0) return;

    if (kDown & HidNpadButton_AnyUp)
    {
        selectedIndex--;
        if (selectedIndex < 0) selectedIndex = nAccounts - 1;
    }
    if (kDown & HidNpadButton_AnyDown)
    {
        selectedIndex++;
        if (selectedIndex >= (int)nAccounts) selectedIndex = 0;
    }
    if (kDown & HidNpadButton_A)
    {
        onSelect(accounts[selectedIndex]);
        App::instance().popScreen();
    }
}


void AccountScreen::render(Renderer& r)
{
    int x = 80;
    int y = 60;

    r.drawText("Select Account", x, y, 32, COLOR_ACCENT);
    y += 50;

    r.drawRect(x, y, r.screenWidth() - x * 2, 2, COLOR_ACCENT);
    y += 20;

    if (loadError)
    {
        r.drawText("Failed to load accounts.", x, y, 24, COLOR_ERROR);
        y += 40;
        r.drawText("Press B to go back.", x, y, 18, COLOR_DIM);
        return;
    }

    if (nAccounts == 0)
    {
        r.drawText("No accounts found.", x, y, 24, COLOR_DIM);
        y += 40;
        r.drawText("Press B to go back.", x, y, 18, COLOR_DIM);
        return;
    }

    for (int i = 0; i < (int)nAccounts; i++)
    {
        int btnW = r.screenWidth() - x * 2;
        int btnH = 50;

        Color bgColor = (i == selectedIndex) ? COLOR_HIGHLIGHT : COLOR_BUTTON;

        r.drawRect(x, y, btnW, btnH, bgColor);
        r.drawText(accounts[i].nickname, x + 20, y + 12, 24, COLOR_TEXT);

        y += btnH + 10;
    }

    // footer
    int fy = r.screenHeight() - 50;
    r.drawRect(x, fy - 10, r.screenWidth() - x * 2, 2, {80, 80, 80, 255});
    r.drawText("A: Select    B: Back    +: Exit", x, fy, 18, COLOR_DIM);
}

} // namespace gui
