#include "account.hpp"
#include "utils.hpp"

#include <string.h>


#include "tui.hpp"


int probeAccounts(Account** accounts, size_t* nAccounts)
{
    AccountUid* userIds = NULL;
    Account* currentAccounts = NULL;

    s32 nTotalUsers, nActualUsers;
    AccountProfile profile;
    AccountProfileBase profileBase;

    Result rc = accountInitialize(AccountServiceType_System);
    if (R_FAILED(rc)) {
        goto cleanup;
    }

    rc = accountGetUserCount(&nTotalUsers);
    if (R_FAILED(rc)) {
        goto cleanup;
    }

    userIds = (AccountUid*)malloc(sizeof(AccountUid) * nTotalUsers);
    rc = accountListAllUsers(userIds, nTotalUsers, &nActualUsers);
    if (R_FAILED(rc)) {
        goto cleanup;
    }
    currentAccounts = (Account*)malloc(sizeof(Account) * nActualUsers);

    for (s32 i = 0; i < nActualUsers; i++) {
        rc = accountGetProfile(&profile, userIds[i]);
        if (R_FAILED(rc)) {
            continue;
        }
        rc = accountProfileGet(&profile, NULL, &profileBase);
        if (R_FAILED(rc)) {
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


int getCurrentAccount(Account* account)
{
    AccountProfile profile;
    AccountProfileBase profileBase;

    Result rc = accountInitialize(AccountServiceType_System);
    if (R_FAILED(rc)) {
        drawText("Failed to initialize account service");
        goto cleanup;
    }

    AccountUid uid; 
    rc = accountGetPreselectedUser(&uid);
    if (R_FAILED(rc)) {
        drawText("Failed to get preselected user");
        goto cleanup;
    }

    rc = accountGetProfile(&profile, uid);
    if (R_FAILED(rc)) {
        drawText("Failed to get profile");
        goto cleanup;
    }

    rc = accountProfileGet(&profile, NULL, &profileBase);
    if (R_FAILED(rc)) {
        drawText("Failed to get profile base");
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