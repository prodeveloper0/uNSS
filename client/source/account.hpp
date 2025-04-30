#pragma once
#include <string>
#include <vector>

#include <switch.h>


struct Account
{
    char nickname[32];
    AccountUid uid;
};


int probeAccounts(Account** accounts, size_t* nAccounts);
int getCurrentAccount(Account* account);
