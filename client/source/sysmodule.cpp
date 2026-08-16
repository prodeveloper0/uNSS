#include "sysmodule.hpp"

#include <cstdio>
#include <cstring>
#include <sys/stat.h>
#include <unistd.h>

#include <switch.h>

#include "fileio.hpp"


namespace sysmodule
{

namespace
{

const char* ROMFS_MOUNT = "uNSSromfs";

std::string contentsDir()
{
    return std::string("sdmc:/atmosphere/contents/") + PROGRAM_ID;
}

std::string exefsPath()
{
    return contentsDir() + "/exefs.nsp";
}

std::string flagPath()
{
    return contentsDir() + "/flags/boot2.flag";
}

// 모듈이 스스로를 껐을 때 부팅 플래그가 옮겨지는 자리.
//
// 모듈은 자기가 도는 동안 시스템이 죽었다는 것을 확인했을 때만 이렇게 한다
// (client-sysmodule 의 RunMarker 참고). 여기 파일이 있다는 것은 "지난번에
// 이 모듈이 콘솔을 망가뜨렸고, 그래서 스스로 물러났다" 는 뜻이다.
std::string disabledFlagPath()
{
    return contentsDir() + "/flags/boot2.flag.crashed";
}

// 설치된 모듈의 버전을 남겨두는 파일. Atmosphere 가 신경쓰지 않는 이름이라
// 같은 폴더에 둬도 안전하다.
std::string versionPath()
{
    return contentsDir() + "/uNSS.version";
}


bool fileExists(const std::string& path)
{
    struct stat st;
    return stat(path.c_str(), &st) == 0;
}


int readInstalledVersion()
{
    FILE* fp = fopen(versionPath().c_str(), "r");
    if (!fp) return 0;

    int version = 0;
    if (fscanf(fp, "%d", &version) != 1)
        version = 0;
    fclose(fp);

    return version;
}


int copyFile(const std::string& from, const std::string& to)
{
    FILE* src = fopen(from.c_str(), "rb");
    if (!src) return -1;

    FILE* dst = fopen(to.c_str(), "wb");
    if (!dst)
    {
        fclose(src);
        return -2;
    }

    // 모듈은 100 KiB 대라 버퍼를 크게 잡을 이유가 없다.
    static u8 buffer[16 * 1024];
    int ret = 0;

    while (true)
    {
        const size_t got = fread(buffer, 1, sizeof(buffer), src);
        if (got == 0)
        {
            if (ferror(src)) ret = -3;
            break;
        }

        if (fwrite(buffer, 1, got, dst) != got)
        {
            ret = -4;
            break;
        }
    }

    fclose(dst);
    fclose(src);

    // 반쯤 쓰다 만 파일을 남기면 부팅 때 그대로 로드된다. 반드시 지운다.
    if (ret != 0) remove(to.c_str());

    return ret;
}

} // namespace


std::string installPath()
{
    return contentsDir();
}


State getState()
{
    if (!fileExists(exefsPath()))
        return State::NotInstalled;

    if (readInstalledVersion() < BUNDLED_VERSION)
        return State::Outdated;

    // 설치는 돼 있는데 플래그가 치워진 자리에 남아 있다면, 지난 실행이
    // 끝까지 가지 못한 것이다. 모듈이 죽었거나 - 백업 도중에 콘솔을 껐거나.
    // 어느 쪽인지는 여기서 알 수 없고, 그래서 되살리는 것은 사용자가 정한다.
    // 이 상태를 알려주지 않으면 모듈은 조용히 다시 뜨지 않는다.
    if (!fileExists(flagPath()) && fileExists(disabledFlagPath()))
        return State::Interrupted;

    return State::UpToDate;
}


// 치워둔 플래그를 제자리로 돌린다.
int resume()
{
    if (!fileExists(disabledFlagPath())) return -1;

    remove(flagPath().c_str());
    return rename(disabledFlagPath().c_str(), flagPath().c_str()) == 0 ? 0 : -2;
}


int install()
{
    const Result rc = romfsMountSelf(ROMFS_MOUNT);
    if (R_FAILED(rc)) return -1;

    int ret = 0;

    do
    {
        if (recursiveMkdir(contentsDir() + "/flags") != 0)
        {
            ret = -2;
            break;
        }

        const std::string source = std::string(ROMFS_MOUNT) + ":/exefs.nsp";
        if (copyFile(source, exefsPath()) != 0)
        {
            ret = -3;
            break;
        }

        // 내용은 없어도 된다. 존재 자체가 부팅 시 실행하라는 뜻이다.
        FILE* flag = fopen(flagPath().c_str(), "wb");
        if (!flag)
        {
            ret = -4;
            break;
        }
        fclose(flag);

        // 지난번에 치워둔 플래그가 남아 있으면 지운다. 방금 새로 만들었으니
        // 쓸모가 없고, 남겨두면 다음 판단이 헷갈린다.
        remove(disabledFlagPath().c_str());

        FILE* version = fopen(versionPath().c_str(), "w");
        if (version)
        {
            fprintf(version, "%d", BUNDLED_VERSION);
            fclose(version);
        }
    }
    while (false);

    romfsUnmount(ROMFS_MOUNT);
    return ret;
}


int uninstall()
{
    remove(flagPath().c_str());
    // 치워둔 플래그도 같이 지운다. 남겨두면 flags/ 가 비지 않아 디렉토리가
    // 그대로 남는다.
    remove(disabledFlagPath().c_str());
    remove(versionPath().c_str());
    remove(exefsPath().c_str());

    // 빈 디렉토리만 지워진다. 남은 파일이 있으면 실패해도 그냥 둔다.
    rmdir((contentsDir() + "/flags").c_str());
    rmdir(contentsDir().c_str());

    return fileExists(exefsPath()) ? -1 : 0;
}

} // namespace sysmodule
