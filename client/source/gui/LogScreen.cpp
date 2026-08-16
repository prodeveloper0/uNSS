#include "LogScreen.hpp"

#include <stdio.h>
#include <string.h>


namespace gui
{

namespace
{

// sysmodule 이 쓰는 곳과 같아야 한다 (client-sysmodule/source/main.cpp).
const char* SYSMODULE_LOG_PATH = "sdmc:/uNSS/sysmodule.log";

// 로그는 계속 자란다. 전부 들고 있을 이유가 없고, 앱의 힙도 무한하지 않다.
// 뒤쪽만 남긴다 - 알고 싶은 것은 언제나 마지막에 일어난 일이다.
constexpr size_t MAX_LINES = 500;

// 대략 1 초. 화면은 60fps 로 돈다.
constexpr int RELOAD_INTERVAL_FRAMES = 60;

} // namespace


LogScreen::LogScreen()
{
    reload();
}


void LogScreen::reload()
{
    errorMessage.clear();

    FILE* fp = fopen(SYSMODULE_LOG_PATH, "r");
    if (!fp)
    {
        lines.clear();
        lastSize = -1;
        errorMessage = "No log yet - the background service has not run.";
        return;
    }

    // 파일 크기를 먼저 본다. 바뀌지 않았으면 읽을 필요가 없다.
    fseek(fp, 0, SEEK_END);
    const long size = ftell(fp);
    if (size == lastSize)
    {
        fclose(fp);
        return;
    }
    lastSize = size;
    rewind(fp);

    std::vector<std::string> fresh;
    fresh.reserve(MAX_LINES);

    char buffer[512];
    while (fgets(buffer, sizeof(buffer), fp))
    {
        size_t len = strlen(buffer);
        while (len > 0 && (buffer[len - 1] == '\n' || buffer[len - 1] == '\r'))
            buffer[--len] = '\0';

        fresh.push_back(buffer);

        // 앞에서부터 버린다. 파일을 뒤에서부터 읽는 것보다 단순하고, 이
        // 크기에서는 차이가 느껴지지 않는다.
        if (fresh.size() > MAX_LINES)
            fresh.erase(fresh.begin());
    }

    fclose(fp);
    lines.swap(fresh);

    if (lines.empty())
        errorMessage = "The log is empty.";
}


int LogScreen::visibleLineCount(const Renderer& r) const
{
    const int top = 60 + 50 + 15;
    const int bottom = r.screenHeight() - 50 - 20;
    const int count = (bottom - top) / 26;
    return count < 1 ? 1 : count;
}


int LogScreen::maxScroll(int visible) const
{
    const int max = (int)lines.size() - visible;
    return max < 0 ? 0 : max;
}


void LogScreen::update(u64 kDown)
{
    if (kDown & HidNpadButton_B)
    {
        App::instance().popScreen();
        return;
    }

    // 화면이 만들어진 뒤에야 렌더러 크기를 물어볼 수 있으므로 여기서 센다.
    const int visible = visibleLineCount(App::instance().getRenderer());

    if (kDown & HidNpadButton_X)
    {
        lastSize = -1;   // 강제로 다시 읽는다
        reload();
    }

    if (kDown & HidNpadButton_Y)
        follow = !follow;

    if (kDown & HidNpadButton_AnyUp)
    {
        follow = false;
        if (--scrollOffset < 0) scrollOffset = 0;
    }
    if (kDown & HidNpadButton_AnyDown)
    {
        if (++scrollOffset >= maxScroll(visible))
        {
            scrollOffset = maxScroll(visible);
            // 바닥에 닿으면 다시 따라간다. 따로 켤 필요가 없다.
            follow = true;
        }
    }
    if (kDown & HidNpadButton_L)
    {
        follow = false;
        scrollOffset -= visible;
        if (scrollOffset < 0) scrollOffset = 0;
    }
    if (kDown & HidNpadButton_R)
    {
        scrollOffset += visible;
        if (scrollOffset >= maxScroll(visible))
        {
            scrollOffset = maxScroll(visible);
            follow = true;
        }
    }

    if (--framesUntilReload <= 0)
    {
        framesUntilReload = RELOAD_INTERVAL_FRAMES;
        reload();
    }

    if (follow)
        scrollOffset = maxScroll(visible);
    else if (scrollOffset > maxScroll(visible))
        scrollOffset = maxScroll(visible);
}


void LogScreen::render(Renderer& r)
{
    int x = 80;
    int y = 60;

    r.drawText("Background service log", x, y, 32, COLOR_ACCENT);
    y += 50;

    r.drawRect(x, y, r.screenWidth() - x * 2, 2, COLOR_ACCENT);
    y += 15;

    const int fy = r.screenHeight() - 50;
    const int logAreaBottom = fy - 20;
    const int lineHeight = 26;
    const int visible = visibleLineCount(r);

    if (!errorMessage.empty() && lines.empty())
    {
        r.drawText(errorMessage, x, y, 18, COLOR_DIM);
    }
    else
    {
        for (int i = scrollOffset; i < (int)lines.size() && i < scrollOffset + visible; i++)
        {
            // 실패한 줄은 눈에 띄어야 한다. 그것 하나 찾자고 여기에 온다.
            const bool bad =
                lines[i].find("Failed") != std::string::npos ||
                lines[i].find("WARNING") != std::string::npos ||
                lines[i].find("no network") != std::string::npos;

            r.drawText(lines[i], x, y, 18, bad ? COLOR_ERROR : COLOR_TEXT);
            y += lineHeight;
        }
    }

    r.drawRect(0, logAreaBottom, r.screenWidth(), r.screenHeight() - logAreaBottom, COLOR_BACKGROUND);
    r.drawRect(x, fy - 10, r.screenWidth() - x * 2, 2, {80, 80, 80, 255});

    const std::string position =
        lines.empty()
            ? std::string("0 lines")
            : std::to_string(scrollOffset + 1) + "-" +
              std::to_string(scrollOffset + visible < (int)lines.size()
                                 ? scrollOffset + visible
                                 : (int)lines.size()) +
              " / " + std::to_string(lines.size());

    r.drawText(position + (follow ? "  [following]" : "") +
                   "    Up/Down, L/R: Scroll    X: Reload    Y: Follow    B: Back",
               x, fy, 18, follow ? COLOR_ACCENT : COLOR_DIM);
}

} // namespace gui
