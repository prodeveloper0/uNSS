#pragma once
#include <string>
#include <map>
#include <functional>

#include <switch.h>


typedef struct _Point {
    int x, y;
} Point;


typedef struct _Size {
    int width, height;
} Size;


void drawText(bool update = true);
void drawText(const char* text, int x, int y, bool update = true);
void drawText(const char* text, bool update = true);
void drawText(const std::string& text, int x, int y, bool update = true);
void drawText(const std::string& text, bool update = true);

void getCurrentCursorPosition(Point* point);
void setCursorPosition(const Point* point);

void getConsoleSize(Size* size);

u64 waitKey(const u64* keys, size_t count);


class IDrawable
{
public:
    virtual void draw() const = 0;
};


class TextMessage : public IDrawable
{
private:
    const std::string text;

public:
    TextMessage(const std::string& text) : text(text) {}
    void draw() const;
};


class PromptMessage : public IDrawable
{
public:
    using OnKeyPressedCallback = std::function<void(u64)>;

private:
    const std::string text;
    const std::map<u64, std::string> options;
    const OnKeyPressedCallback onKeyPressedCallback;

public:
    PromptMessage(const std::string& text, const std::map<u64, std::string>& options) : text(text), options(options) {}
    PromptMessage(const std::string& text, const std::map<u64, std::string>& options, const OnKeyPressedCallback& onKeyPressedCallback) : text(text), options(options), onKeyPressedCallback(onKeyPressedCallback) {}
    void draw() const;
    u64 wait() const;

protected:
    void onKeyPressed(u64 key) const;
};
