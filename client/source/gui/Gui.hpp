#pragma once
#include <string>
#include <vector>
#include <functional>

#include <switch.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>


namespace gui
{

struct Color
{
    Uint8 r, g, b, a;
};

constexpr Color COLOR_BACKGROUND = {45, 45, 45, 255};
constexpr Color COLOR_TEXT       = {255, 255, 255, 255};
constexpr Color COLOR_DIM        = {180, 180, 180, 255};
constexpr Color COLOR_ACCENT     = {0, 200, 130, 255};
constexpr Color COLOR_BUTTON     = {70, 70, 70, 255};
constexpr Color COLOR_HIGHLIGHT  = {0, 200, 130, 255};
constexpr Color COLOR_ERROR      = {255, 80, 80, 255};


class Renderer
{
public:
    bool init();
    void quit();

    void beginFrame();
    void endFrame();

    void drawText(const std::string& text, int x, int y, int size, Color color = COLOR_TEXT);
    void drawRect(int x, int y, int w, int h, Color color);
    void drawRectOutline(int x, int y, int w, int h, Color color);

    int getTextWidth(const std::string& text, int size);
    int screenWidth() const { return 1280; }
    int screenHeight() const { return 720; }

    TTF_Font* getFont(int size);

private:
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    TTF_Font* fontSmall = nullptr;
    TTF_Font* fontMedium = nullptr;
    TTF_Font* fontLarge = nullptr;

    void* fontData = nullptr;
    size_t fontDataSize = 0;

    bool loadSystemFont();
};


struct MenuItem
{
    std::string label;
    std::function<void()> action;
    bool enabled = true;
};


class Screen
{
public:
    virtual ~Screen() = default;
    virtual void update(u64 kDown) = 0;
    virtual void render(Renderer& r) = 0;
};


class App
{
public:
    void run(Screen* initialScreen);
    void pushScreen(Screen* screen);
    void popScreen();
    Renderer& getRenderer() { return renderer; }

    static App& instance();

private:
    Renderer renderer;
    std::vector<Screen*> screenStack;
    bool running = true;

    App() = default;
};

} // namespace gui
