#pragma once
#include "Gui.hpp"

#include <functional>
#include <string>
#include <vector>


namespace gui
{

class ProgressScreen : public Screen
{
public:
    using WorkFunc = std::function<int(std::function<void(const std::string&)>)>;

    ProgressScreen(const std::string& title, WorkFunc work);

    void update(u64 kDown) override;
    void render(Renderer& r) override;

private:
    std::string title;
    WorkFunc work;
    std::vector<std::string> logLines;
    int scrollOffset = 0;
    int result = 0;
    bool finished = false;
    bool started = false;

    void runWork();
    void renderNow();
};

} // namespace gui
