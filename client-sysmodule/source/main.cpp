// uNSS 백그라운드 자동 백업 sysmodule.
//
// 부팅할 때 Atmosphere 가 띄운다. 하는 일은 하나뿐이다:
// 조건이 맞으면 세이브를 서버로 올리고, 끝나면 잠든다.
// 복원은 하지 않는다 - 그건 사용자가 GUI 에서 눈으로 보며 결정할 일이다.
//
// 측정 결과 (실기, 22.5.0): 이 프로세스가 쓸 수 있는 주소 공간은 약 14 MiB.
// 프로토타입이 2.3 MiB 를 썼고 mbedTLS 도 문제없이 올라갔다.

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include <switch.h>

#include "account.hpp"
#include "fileio.hpp"
#include "ini.hpp"
#include "sync.hpp"


// 절대 함부로 올리지 말 것.
//
// 6 MiB 로 잡았다가 부팅이 망가졌다 (2026-07-31). HID(0100000000000013) 가
// 메모리를 못 받아 죽었고, 콘솔이 2001-0132 로 부팅 루프에 빠졌다.
// 20.0.0 이후 sysmodule 풀은 아주 빠듯하다. 이 프로세스가 크게 잡으면
// 우리 모듈이 안 뜨는 정도가 아니라 시스템 모듈이 같이 죽는다.
//
// 2 MiB 는 프로토타입이 실기에서 끝까지 돌았던 값이다 (측정: used 2296 KiB).
// 압축 버퍼가 부족하면 업로드가 실패할 뿐 부팅은 멀쩡하다. 그쪽이 안전하다.
#define INNER_HEAP_SIZE 0x200000

static const char* LOG_PATH = "sdmc:/uNSS/sysmodule.log";
static const char* CONFIG_PATH = "sdmc:/uNSS/config.ini";
static const char* SAVE_DATA_PATH = "sdmc:/uNSS/saves";


extern "C" {

u32 __nx_applet_type = AppletType_None;
u32 __nx_fs_num_sessions = 1;

extern void __libnx_init_time(void);

static bool g_socketReady = false;
static bool g_nifmReady = false;

void __libnx_initheap(void)
{
    static u8 inner_heap[INNER_HEAP_SIZE];
    extern void* fake_heap_start;
    extern void* fake_heap_end;

    fake_heap_start = inner_heap;
    fake_heap_end   = inner_heap + sizeof(inner_heap);
}

void __appInit(void)
{
    Result rc;

    rc = smInitialize();
    if (R_FAILED(rc))
        diagAbortWithResult(MAKERESULT(Module_Libnx, LibnxError_InitFail_SM));

    rc = setsysInitialize();
    if (R_SUCCEEDED(rc))
    {
        SetSysFirmwareVersion fw;
        if (R_SUCCEEDED(setsysGetFirmwareVersion(&fw)))
            hosversionSet(MAKEHOSVERSION(fw.major, fw.minor, fw.micro));
        setsysExit();
    }

    rc = timeInitialize();
    if (R_SUCCEEDED(rc))
        __libnx_init_time();

    rc = fsInitialize();
    if (R_FAILED(rc))
        diagAbortWithResult(MAKERESULT(Module_Libnx, LibnxError_InitFail_FS));

    fsdevMountSdmc();

    // 세이브 목록과 타이틀 이름에 필요하다.
    nsInitialize();
    accountInitialize(AccountServiceType_System);

    // 소켓과 nifm 은 여기서 열어야 한다. 아래 smExit() 뒤에는 못 연다.
    // 프로토타입에서 실기 검증된 작은 값. 여기도 키우면 그만큼 풀을 먹는다.
    static const SocketInitConfig sockConf = {
        .tcp_tx_buf_size     = 0x2000,
        .tcp_rx_buf_size     = 0x4000,
        .tcp_tx_buf_max_size = 0x8000,
        .tcp_rx_buf_max_size = 0x10000,
        .udp_tx_buf_size     = 0x800,
        .udp_rx_buf_size     = 0x1000,
        .sb_efficiency       = 1,
        .num_bsd_sessions    = 2,
        .bsd_service_type    = BsdServiceType_User,
    };

    g_socketReady = R_SUCCEEDED(socketInitialize(&sockConf));
    g_nifmReady = R_SUCCEEDED(nifmInitialize(NifmServiceType_User));

    smExit();
}

void __appExit(void)
{
    if (g_nifmReady) nifmExit();
    if (g_socketReady) socketExit();
    accountExit();
    nsExit();
    fsdevUnmountAll();
    timeExit();
    fsExit();
}

} // extern "C"


namespace
{

void writeLog(const std::string& line)
{
    FILE* fp = fopen(LOG_PATH, "a");
    if (!fp) return;

    const time_t now = time(NULL);
    struct tm* tm = localtime(&now);

    if (tm)
    {
        fprintf(fp, "[%02d:%02d:%02d] %s\n",
            tm->tm_hour, tm->tm_min, tm->tm_sec, line.c_str());
    }
    else
    {
        fprintf(fp, "%s\n", line.c_str());
    }

    fclose(fp);
}


// SD 가 올라올 때까지 기다린다. boot2 는 아주 이른 시점에 돌기 때문에
// 한 번 실패했다고 끝내면 아무것도 못 한다.
bool waitForSdCard(int maxSeconds)
{
    for (int i = 0; i < maxSeconds; ++i)
    {
        FILE* fp = fopen(LOG_PATH, "a");
        if (fp)
        {
            fclose(fp);
            return true;
        }
        svcSleepThread(1000000000ULL);
    }
    return false;
}


// 무선랜이 붙을 때까지 기다린다. 실측 16 초였다.
bool waitForNetwork(int maxSeconds)
{
    if (!g_nifmReady) return false;

    for (int i = 0; i < maxSeconds; ++i)
    {
        NifmInternetConnectionType type;
        u32 strength = 0;
        NifmInternetConnectionStatus status;

        if (R_SUCCEEDED(nifmGetInternetConnectionStatus(&type, &strength, &status))
            && status == NifmInternetConnectionStatus_Connected)
        {
            writeLog("network ready after " + std::to_string(i) + " s");
            return true;
        }

        svcSleepThread(1000000000ULL);
    }

    writeLog("no network after " + std::to_string(maxSeconds) + " s - giving up");
    return false;
}


// 게임이 끝날 때까지 기다린다. 게임이 세이브를 열어둔 상태의 백업은
// 반쯤 쓰인 파일을 담을 수 있다.
void waitUntilNoGameRunning(int maxSeconds)
{
    for (int i = 0; i < maxSeconds; ++i)
    {
        if (!isGameRunning()) return;

        if (i == 0)
            writeLog("game is running - waiting");

        svcSleepThread(10000000000ULL); // 10 초
    }
}

} // namespace


int main(int argc, char* argv[])
{
    if (!waitForSdCard(60))
        return 0;

    writeLog("--- uNSS sysmodule started ---");

    Config config(CONFIG_PATH);
    if (!(bool)config["remote"]["enabled"])
    {
        writeLog("remote disabled in config - nothing to do");
        return 0;
    }
    if (!(bool)config["sync"]["autoPushOnLaunch"])
    {
        writeLog("autoPushOnLaunch is off - nothing to do");
        return 0;
    }

    const int intervalHours = atoi(config["sync"]["autoPushIntervalHours"].value.c_str());
    if (!isAutoSyncDue(SAVE_DATA_PATH, intervalHours))
    {
        writeLog("last backup is recent - nothing to do");
        return 0;
    }

    if (!g_socketReady)
    {
        writeLog("no socket service - aborting");
        return 0;
    }

    if (!waitForNetwork(180))
        return 0;

    // 게임 중이면 최대 30 분까지 기다려 본다.
    waitUntilNoGameRunning(180);
    if (isGameRunning())
    {
        writeLog("game still running - postponed to next boot");
        return 0;
    }

    // 백업할 계정을 정한다. sysmodule 에는 선택 화면이 없으므로
    // 설정값으로만 결정한다.
    //
    // allAccounts=1: 콘솔에 등록된 모든 사용자를 백업한다.
    //   서버는 /users/<닉네임>/ 으로 사용자를 나누므로 서로 섞이지 않는다.
    // allAccounts=0 또는 없음: defaultAccountName 하나만.
    // 기본값은 앱이 config.ini 에 써넣는다. 여기서는 읽기만 한다.
    std::vector<Account> targets;

    if ((bool)config["sync"]["allAccounts"])
    {
        Account* list = NULL;
        size_t count = 0;

        if (probeAccounts(&list, &count) != 0 || list == NULL)
        {
            writeLog("failed to list accounts");
            return 0;
        }

        for (size_t i = 0; i < count; ++i)
            targets.push_back(list[i]);

        free(list);
        writeLog("accounts found: " + std::to_string(targets.size()));
    }
    else
    {
        Account account{};
        AccountResolveOptions accountOptions;
        accountOptions.defaultAccountName = config["account"]["defaultAccountName"].value;
        accountOptions.useProfileSelector = false;

        if (accountOptions.defaultAccountName.empty())
        {
            writeLog("defaultAccountName is empty - set it in config.ini");
            return 0;
        }

        if (getCurrentAccount(&account, accountOptions) != 0)
        {
            // 닉네임 비교는 대소문자를 구분한다. 오타보다 흔한 원인이라 같이 적어둔다.
            writeLog("account not found (case-sensitive): " + accountOptions.defaultAccountName);
            return 0;
        }

        targets.push_back(account);
    }

    if (targets.empty())
    {
        writeLog("no account to back up");
        return 0;
    }

    recursiveMkdir(SAVE_DATA_PATH);

    bool allOk = true;

    for (const Account& account : targets)
    {
        writeLog(std::string("account: ") + account.nickname);

        SyncOptions options;
        options.uid = account.uid;
        options.nickname = account.nickname;
        options.saveDataPath = SAVE_DATA_PATH;
        options.serverUrl = (std::string)config["remote"]["serverUrl"];
        options.remoteEnabled = true;
        options.archiveBy = config["title"]["archiveBy"].value;
        options.excludedTitleIds = config["title"]["excludedTitleIds"].value;
        options.excludedTitleNames = config["title"]["excludedTitleNames"].value;
        // 바뀐 것만 올린다. 매번 전부 올리면 SD 와 서버를 모두 낭비한다.
        options.skipUnchanged = true;

        const int ret = pushAllSaves(options, [](const std::string& line)
        {
            writeLog("  " + line);
        });

        if (ret != 0)
        {
            writeLog("  failed, ret=" + std::to_string(ret));
            allOk = false;
        }
    }

    // 하나라도 실패하면 시각을 남기지 않는다. 다음 부팅에서 다시 시도한다.
    if (allOk)
    {
        writeLastAutoSyncTime(SAVE_DATA_PATH, time(NULL));
        writeLog("backup finished");
    }
    else
    {
        writeLog("backup finished with errors - will retry on next boot");
    }

    return 0;
}
