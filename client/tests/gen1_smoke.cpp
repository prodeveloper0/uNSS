#include "gen1recomp.hpp"
#include "remote.hpp"
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sys/stat.h>

// Minimal host stand-ins (host build only).
#include <dirent.h>
int walk(const std::string& path, const std::function<void(const std::string&, bool)>& cb, int depth)
{
    if (depth <= 0) return -2;
    DIR* d = opendir(path.c_str());
    if (!d) return -1;
    int ret = 0;
    for (struct dirent* e; (e = readdir(d));)
    {
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
        const std::string p = path + "/" + e->d_name;
        if (e->d_type == DT_DIR)
        {
            if (walk(p, cb, depth - 1) == -2) ret = -2;
            cb(p, true);
        }
        else cb(p, false);
    }
    closedir(d);
    return ret;
}
int recursiveMkdir(const std::string&, mode_t) { return 0; }
HTTPRemoteStore::HTTPRemoteStore(const std::string&, const std::string&) {}
HTTPRemoteStore::~HTTPRemoteStore() {}
int HTTPRemoteStore::push(const std::string, const u64) { return -1; }
int HTTPRemoteStore::pull(const std::string, const u64) { return -1; }
int HTTPRemoteStore::push(const std::string, const u64, const std::string&) { return -1; }
int HTTPRemoteStore::pull(const std::string, const u64, const std::string&) { return -1; }
extern "C" int __real_rename(const char*, const char*);
static bool failPublication = false;
extern "C" int __wrap_rename(const char* from, const char* to)
{
    struct stat st;
    if (lstat(to, &st) == 0) { errno = EEXIST; return -1; } // Horizon no-overwrite
    if (failPublication && std::string(to).size() >= 4
        && std::string(to).substr(std::string(to).size() - 4) == ".sar"
        && std::string(from).find(".previous") == std::string::npos)
    { errno = EIO; return -1; } // only the final publish fails, rollback works
    return __real_rename(from, to);
}

int main(int argc, char** argv)
{
    if (argc < 3) return 64;
    const std::string mode = argv[1];
    const std::string root = argv[2];
    const std::string out = argc > 3 ? argv[3] : "";
    u64 fingerprint = 0;
    int result;
    failPublication = (mode == "publish-failure");
    if (mode == "fingerprint")
    {
        fingerprint = gen1recomp::fingerprint(root);
        result = fingerprint ? gen1recomp::OK : gen1recomp::ABSENT;
    }
    else
    {
        std::function<bool()> gate;
        if (mode == "busy") gate = [] { return true; };
        if (mode == "busy-late")
        {
            int n = 0; gate = [&n] { return ++n == 1; };
        }
        if (mode == "aba")
        {
            int n = 0;
            std::string slot = root + "/saves/yellow/slot1.lua";
            std::string original;
            gate = [&]
            {
                if (++n == 2 && original.empty())
                {
                    std::ifstream in(slot, std::ios::binary);
                    original.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
                    if (!original.empty())
                    {
                        std::string changed = original;
                        changed[0] ^= 1;
                        std::ofstream(slot, std::ios::binary).write(changed.data(), changed.size());
                    }
                }
                return false;
            };
            result = gen1recomp::archive(root, out, gate);
            if (!original.empty())
                std::ofstream(slot, std::ios::binary).write(original.data(), original.size());
        }
        else
        {
            result = gen1recomp::archive(root, out, gate);
            fingerprint = gen1recomp::fingerprint(root);
        }
    }
    const int status = result == gen1recomp::OK ? 1 : result == gen1recomp::ABSENT ? 0
        : result == gen1recomp::BUSY ? 2 : 3;
    std::printf("%d %llu result=%d\n", status, (unsigned long long)fingerprint, result);
    return status > 1 ? 1 : 0;
}
