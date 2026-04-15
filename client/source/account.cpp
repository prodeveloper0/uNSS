#include "account.hpp"
#include "utils.hpp"

#include <string.h>
#include <vector>


static int findUserByNickname(const std::string& nickname, AccountUid* outUid)
{
    if (nickname.empty())
    {
        return -1;
    }

    s32 nTotal = 0, nActual = 0;
    AccountUid uids[ACC_USER_LIST_SIZE];

    if (R_FAILED(accountGetUserCount(&nTotal)) || nTotal <= 0)
    {
        return -1;
    }
    if (R_FAILED(accountListAllUsers(uids, ACC_USER_LIST_SIZE, &nActual)) || nActual <= 0)
    {
        return -1;
    }

    for (s32 i = 0; i < nActual; i++)
    {
        AccountProfile prof;
        AccountProfileBase base;
        if (R_FAILED(accountGetProfile(&prof, uids[i])))
        {
            continue;
        }
        bool matched = false;
        if (R_SUCCEEDED(accountProfileGet(&prof, NULL, &base)))
        {
            if (nickname == base.nickname)
            {
                *outUid = uids[i];
                matched = true;
            }
        }
        accountProfileClose(&prof);
        if (matched)
        {
            return 0;
        }
    }
    return -1;
}


int probeAccounts(Account** accounts, size_t* nAccounts)
{
    AccountUid* userIds = NULL;
    Account* currentAccounts = NULL;

    s32 nTotalUsers, nActualUsers;
    AccountProfile profile;
    AccountProfileBase profileBase;

    Result rc = accountInitialize(AccountServiceType_System);
    if (R_FAILED(rc))
    {
        goto cleanup;
    }

    rc = accountGetUserCount(&nTotalUsers);
    if (R_FAILED(rc))
    {
        goto cleanup;
    }

    userIds = (AccountUid*)malloc(sizeof(AccountUid) * nTotalUsers);
    rc = accountListAllUsers(userIds, nTotalUsers, &nActualUsers);
    if (R_FAILED(rc))
    {
        goto cleanup;
    }
    currentAccounts = (Account*)malloc(sizeof(Account) * nActualUsers);

    for (s32 i = 0; i < nActualUsers; i++)
    {
        rc = accountGetProfile(&profile, userIds[i]);
        if (R_FAILED(rc))
        {
            continue;
        }
        rc = accountProfileGet(&profile, NULL, &profileBase);
        if (R_FAILED(rc))
        {
            accountProfileClose(&profile);
            continue;
        }
        strcpy(currentAccounts[i].nickname, profileBase.nickname);
        currentAccounts[i].uid = userIds[i];

        accountProfileClose(&profile);
    }

    *accounts = currentAccounts;
    *nAccounts = nActualUsers;
    accountExit();
    return 0;

cleanup:
    freeSafely(&userIds);
    freeSafely(&accounts);
    accountExit();
    return -1;
}


int getCurrentAccount(Account* account, const AccountResolveOptions& options)
{
    AccountProfile profile;
    AccountProfileBase profileBase;
    AccountUid uid;

    Result rc = accountInitialize(AccountServiceType_System);
    if (R_FAILED(rc))
    {
        // Failed to initialize account service
        goto cleanup;
    }

    // 어카운트 선택창을 띄워 어카운트 이름을 조회
    if (options.useProfileSelector)
    {
        const AppletType appletType = appletGetAppletType();
        const bool isFullApplicationMode =
            appletType == AppletType_Application ||
            appletType == AppletType_SystemApplication;

        if (isFullApplicationMode)
        {
            PselUserSelectionSettings settings = {};
            if (R_SUCCEEDED(pselShowUserSelector(&uid, &settings)))
            {
                goto processAccountProfile;
            }
        }
    }

    // useProfileSelector=0 이거나 psel 실패 시, 설정 파일의 어카운트 이름을 사용
    if (options.defaultAccountName.empty())
    {
        if (!options.useProfileSelector)
        {
            // useProfileSelector=0 but defaultAccountName is empty
        }
        else
        {
            // Failed to resolve account: set defaultAccountName as fallback
        }
        goto cleanup;
    }

    // 설정 파일에 저장된 어카운트 이름으로 조회
    if (findUserByNickname(options.defaultAccountName, &uid) != 0)
    {
        // No account matches defaultAccountName
        goto cleanup;
    }

processAccountProfile:
    rc = accountGetProfile(&profile, uid);
    if (R_FAILED(rc))
    {
        // Failed to get profile
        goto cleanup;
    }

    rc = accountProfileGet(&profile, NULL, &profileBase);
    if (R_FAILED(rc))
    {
        // Failed to get profile base
        goto cleanup;
    }

    strcpy(account->nickname, profileBase.nickname);
    account->uid = uid;
    return 0;

cleanup:
    accountProfileClose(&profile);
    accountExit();
    return -1;
}
