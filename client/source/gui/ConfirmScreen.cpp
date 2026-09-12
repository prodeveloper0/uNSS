#include "ConfirmScreen.hpp"

#include <utility>

namespace gui
{
ConfirmScreen::ConfirmScreen(const std::string& title,
                             std::vector<std::string> lines,
                             std::function<void()> onConfirm)
    : title(title), lines(std::move(lines)), onConfirm(std::move(onConfirm))
{
}

void ConfirmScreen::update(u64 kDown)
{
    if (kDown & HidNpadButton_B)
    {
        App::instance().popScreen();
        return;
    }
    if (kDown & HidNpadButton_X)
    {
        // Pop deletes this screen, so move the callback out before popping.
        std::function<void()> action = std::move(onConfirm);
        App::instance().popScreen();
        if (action) action();
    }
}

void ConfirmScreen::render(Renderer& r)
{
    const int x = 80;
    int y = 60;
    r.drawText(title, x, y, 32, COLOR_ERROR);
    y += 50;
    r.drawRect(x, y, r.screenWidth() - x * 2, 2, COLOR_ERROR);
    y += 30;

    for (const std::string& line : lines)
    {
        r.drawText(line, x, y, 22, COLOR_TEXT);
        y += 36;
    }

    y += 25;
    r.drawText("Nothing happens until you press X.", x, y, 22, COLOR_ACCENT);

    const int fy = r.screenHeight() - 50;
    r.drawRect(x, fy - 10, r.screenWidth() - x * 2, 2, {80, 80, 80, 255});
    r.drawText("X: Confirm restore    B: Cancel    +: Exit", x, fy, 18, COLOR_DIM);
}
}
