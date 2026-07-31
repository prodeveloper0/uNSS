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

    return State::UpToDate;
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
    remove(versionPath().c_str());
    remove(exefsPath().c_str());

    // 빈 디렉토리만 지워진다. 남은 파일이 있으면 실패해도 그냥 둔다.
    rmdir((contentsDir() + "/flags").c_str());
    rmdir(contentsDir().c_str());

    return fileExists(exefsPath()) ? -1 : 0;
}

} // namespace sysmodule
