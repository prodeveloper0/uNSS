#pragma once

#include "Gui.hpp"

#include <functional>
#include <string>
#include <vector>

namespace gui
{
class ConfirmScreen : public Screen
{
public:
    ConfirmScreen(const std::string& title, std::vector<std::string> lines,
                  std::function<void()> onConfirm);

    void update(u64 kDown) override;
    void render(Renderer& r) override;

private:
    std::string title;
    std::vector<std::string> lines;
    std::function<void()> onConfirm;
};
}
