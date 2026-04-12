#pragma once
#include <string>
#include <vector>

#include <switch.h>


struct Account
{
    char nickname[32];
    AccountUid uid;
};


struct AccountResolveOptions
{
    // config.ini 의 [account] defaultAccountName 값. 등록된 사용자 중 이 닉네임과
    // 정확히 일치하는 계정을 선택한다.
    std::string defaultAccountName = "";

    // true:  먼저 HOS 의 정상 경로 (psel 애플릿) 를 시도하고,
    //        불가능 / 실패한 경우 defaultAccountName 으로 fallback 한다.
    // false: psel 을 건너뛰고 항상 defaultAccountName 으로 계정을 결정한다.
    bool useProfileSelector = true;
};


int probeAccounts(Account** accounts, size_t* nAccounts);
int getCurrentAccount(Account* account, const AccountResolveOptions& options = {});
