#pragma once
#include "Gui.hpp"

#include <string>
#include <vector>


namespace gui
{

// 백그라운드 서비스가 무엇을 하고 있는지 보여준다.
//
// sysmodule 은 화면이 없다. 지금까지 그것이 무엇을 했는지 알아내려면 SD 를
// 뽑거나 FTP 로 로그를 꺼내와야 했고, 실제로 그러느라 고장 하나를 며칠
// 늦게 찾았다 (2026-07-31, TLS 가 열리지 않던 건). 로그는 이미 파일로
// 남으므로, 여기서는 그것을 읽어 보여주기만 한다.
class LogScreen : public Screen
{
public:
    LogScreen();

    void update(u64 kDown) override;
    void render(Renderer& r) override;

private:
    std::vector<std::string> lines;
    int scrollOffset = 0;

    // 파일이 자라면 따라 내려간다. 사용자가 위로 올리면 놓아준다 - 읽는
    // 도중에 화면이 제멋대로 뛰면 읽을 수가 없다.
    bool follow = true;

    // 새로 고칠지 판단하는 기준. 내용을 매번 다시 읽는 것보다 싸다.
    long lastSize = -1;
    int framesUntilReload = 0;

    std::string errorMessage;

    void reload();
    int visibleLineCount(const Renderer& r) const;
    int maxScroll(int visible) const;
};

} // namespace gui
