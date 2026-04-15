#include "Gui.hpp"
#include <cstring>


namespace gui
{

// --- Renderer ---

bool Renderer::loadSystemFont()
{
    PlFontData koFont;
    Result rc = plInitialize(PlServiceType_User);
    if (R_FAILED(rc))
        return false;

    // 한글 폰트 로드 (영문/숫자도 포함)
    rc = plGetSharedFontByType(&koFont, PlSharedFontType_KO);
    if (R_FAILED(rc))
    {
        // fallback: Standard 폰트
        PlFontData stdFont;
        rc = plGetSharedFontByType(&stdFont, PlSharedFontType_Standard);
        if (R_FAILED(rc))
        {
            plExit();
            return false;
        }
        this->fontDataSize = stdFont.size;
        this->fontData = malloc(stdFont.size);
        memcpy(this->fontData, stdFont.address, stdFont.size);
        plExit();
        return true;
    }

    this->fontDataSize = koFont.size;
    this->fontData = malloc(koFont.size);
    memcpy(this->fontData, koFont.address, koFont.size);

    plExit();
    return true;
}


bool Renderer::init()
{
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK) < 0)
        return false;

    if (TTF_Init() < 0)
    {
        SDL_Quit();
        return false;
    }

    window = SDL_CreateWindow("uNSS",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        1280, 720, SDL_WINDOW_FULLSCREEN);
    if (!window)
    {
        TTF_Quit();
        SDL_Quit();
        return false;
    }

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer)
    {
        SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
        return false;
    }

    if (!loadSystemFont())
        return false;

    SDL_RWops* rw = SDL_RWFromMem(fontData, fontDataSize);
    fontSmall = TTF_OpenFontRW(rw, 0, 18);
    rw = SDL_RWFromMem(fontData, fontDataSize);
    fontMedium = TTF_OpenFontRW(rw, 0, 24);
    rw = SDL_RWFromMem(fontData, fontDataSize);
    fontLarge = TTF_OpenFontRW(rw, 0, 32);

    return fontSmall && fontMedium && fontLarge;
}


void Renderer::quit()
{
    if (fontSmall) TTF_CloseFont(fontSmall);
    if (fontMedium) TTF_CloseFont(fontMedium);
    if (fontLarge) TTF_CloseFont(fontLarge);
    if (fontData) free(fontData);
    if (renderer) SDL_DestroyRenderer(renderer);
    if (window) SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();
}


TTF_Font* Renderer::getFont(int size)
{
    if (size <= 20) return fontSmall;
    if (size <= 28) return fontMedium;
    return fontLarge;
}


void Renderer::beginFrame()
{
    SDL_SetRenderDrawColor(renderer, COLOR_BACKGROUND.r, COLOR_BACKGROUND.g, COLOR_BACKGROUND.b, COLOR_BACKGROUND.a);
    SDL_RenderClear(renderer);
}


void Renderer::endFrame()
{
    SDL_RenderPresent(renderer);
}


void Renderer::drawText(const std::string& text, int x, int y, int size, Color color)
{
    if (text.empty()) return;

    TTF_Font* font = getFont(size);
    SDL_Color sdlColor = {color.r, color.g, color.b, color.a};
    SDL_Surface* surface = TTF_RenderUTF8_Blended(font, text.c_str(), sdlColor);
    if (!surface) return;

    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (texture)
    {
        SDL_Rect dst = {x, y, surface->w, surface->h};
        SDL_RenderCopy(renderer, texture, NULL, &dst);
        SDL_DestroyTexture(texture);
    }
    SDL_FreeSurface(surface);
}


void Renderer::drawRect(int x, int y, int w, int h, Color color)
{
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    SDL_Rect rect = {x, y, w, h};
    SDL_RenderFillRect(renderer, &rect);
}


void Renderer::drawRectOutline(int x, int y, int w, int h, Color color)
{
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    SDL_Rect rect = {x, y, w, h};
    SDL_RenderDrawRect(renderer, &rect);
}


int Renderer::getTextWidth(const std::string& text, int size)
{
    TTF_Font* font = getFont(size);
    int w = 0, h = 0;
    TTF_SizeUTF8(font, text.c_str(), &w, &h);
    return w;
}


// --- App ---

App& App::instance()
{
    static App app;
    return app;
}


void App::run(Screen* initialScreen)
{
    if (!renderer.init())
        return;

    screenStack.push_back(initialScreen);

    PadState pad;
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    padInitializeDefault(&pad);

    while (running && appletMainLoop())
    {
        padUpdate(&pad);
        u64 kDown = padGetButtonsDown(&pad);

        if (kDown & HidNpadButton_Plus)
        {
            running = false;
            break;
        }

        if (!screenStack.empty())
        {
            screenStack.back()->update(kDown);
            renderer.beginFrame();
            screenStack.back()->render(renderer);
            renderer.endFrame();
        }
    }

    for (auto* s : screenStack)
        delete s;
    screenStack.clear();

    renderer.quit();
}


void App::pushScreen(Screen* screen)
{
    screenStack.push_back(screen);
}


void App::popScreen()
{
    if (screenStack.size() > 1)
    {
        delete screenStack.back();
        screenStack.pop_back();
    }
}

} // namespace gui
