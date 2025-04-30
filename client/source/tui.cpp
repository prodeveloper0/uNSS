#include "tui.hpp"


const std::map<u64, std::string> DISPLAY_OPTIONS = {
    {HidNpadButton_Plus, "+"},
    {HidNpadButton_Minus, "-"},
    {HidNpadButton_A, "A"},
    {HidNpadButton_B, "B"},
    {HidNpadButton_X, "X"},
    {HidNpadButton_Y, "Y"},
    {HidNpadButton_L, "L"},
    {HidNpadButton_R, "R"},
    {HidNpadButton_ZL, "ZL"},
    {HidNpadButton_ZR, "ZR"}
};


void drawText(bool update)
{
    puts("");
    consoleUpdate(NULL);
}


void drawText(const char* text, int x, int y, bool update)
{
    printf("\x1b[%d;%dH%s", y, x, text);
    if (update)
        consoleUpdate(NULL);
}


void drawText(const char* text, bool update)
{
    puts(text);
    if (update)
        consoleUpdate(NULL);
}


void drawText(const std::string& text, int x, int y, bool update)
{
    printf("\x1b[%d;%dH%s", y, x, text.c_str());
    if (update)
        consoleUpdate(NULL);
}


void drawText(const std::string& text, bool update)
{
    puts(text.c_str());
    if (update)
        consoleUpdate(NULL);
}


void getCurrentCursorPosition(Point* point)
{
    const PrintConsole* printConsole = consoleGetDefault();
    point->x = printConsole->cursorX;
    point->y = printConsole->cursorY;
}


void setCursorPosition(const Point* point)
{
    PrintConsole* printConsole = consoleGetDefault();
    printConsole->cursorX = point->x;
    printConsole->cursorY = point->y;
}


void getConsoleSize(Size* size)
{
    const PrintConsole* printConsole = consoleGetDefault();
    size->width = printConsole->consoleWidth;
    size->height = printConsole->consoleHeight;
}


u64 waitKey(const u64* keys, size_t count)
{
    static PadState pad;
    static bool initialized = false;

    if (!initialized) {
        padInitializeDefault(&pad);
        initialized = true;
    }

    while (appletMainLoop())
    {
        padUpdate(&pad);

        u64 kDown = padGetButtonsDown(&pad);

        for (size_t i = 0; i < count; i++)
        {
            if (kDown & keys[i])
            {
                return keys[i];
            }
        }
    }

    return -1;
}


void TextMessage::draw() const
{
    drawText(text);
}


void PromptMessage::draw() const
{
    drawText(text);
    for (const auto& option : options)
    {
        const std::string& key = DISPLAY_OPTIONS.at(option.first);
        const std::string& value = option.second;
        drawText(key + ": " + value);
    }
}


u64 PromptMessage::wait() const
{
    u64* keys = new u64[options.size()];

    size_t i = 0;
    for (const auto& option : options)
    {
        keys[i++] = option.first;
    }

    const u64 key = waitKey(keys, options.size());
    onKeyPressed(key);
    
    delete[] keys;
    return key;
}


void PromptMessage::onKeyPressed(u64 key) const
{
    onKeyPressedCallback(key);
}
