#pragma once
#include <string>
#include <vector>

#include <switch.h>


class IRemoteStoreIO
{
public:
    virtual int push(AccountUid accountUid, u64 titleId) = 0;
    virtual int pull(AccountUid accountUid, u64 titleId) = 0;
    virtual int push(AccountUid accountUid, u64 titleId, const std::string& revision) = 0;
    virtual int pull(AccountUid accountUid, u64 titleId, const std::string& revision) = 0;
};


// Not used yet
class IRemoteStoreQuery
{
public:
    virtual int hasTitle(AccountUid accountUid, u64 titleId) = 0;
    virtual int queryTitles(std::vector<u64>& outTitles) = 0;
    virtual int queryRevisions(AccountUid accountUid, u64 titleId, std::vector<std::string>& outRevisions) = 0;
};


class RestRemoteStore : public IRemoteStoreIO
{
private:
    std::string m_serverUrl;

public:
    RestRemoteStore(const std::string& serverUrl);
    ~RestRemoteStore();

public:
    int push(AccountUid accountUid, u64 titleId) override;
    int pull(AccountUid accountUid, u64 titleId) override;
    int push(AccountUid accountUid, u64 titleId, const std::string& revision) override;
    int pull(AccountUid accountUid, u64 titleId, const std::string& revision) override;
};
