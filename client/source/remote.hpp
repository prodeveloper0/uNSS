#pragma once
#include <string>
#include <vector>

#include <switch.h>


class IRemoteStoreIO
{
public:
    virtual int push(const std::string userName, const u64 titleId) = 0;
    virtual int pull(const std::string userName, const u64 titleId) = 0;
    virtual int push(const std::string userName, const u64 titleId, const std::string& revision) = 0;
    virtual int pull(const std::string userName, const u64 titleId, const std::string& revision) = 0;
};


// Not used yet
class IRemoteStoreQuery
{
public:
    virtual int hasTitle(const std::string userName, const u64 titleId) = 0;
    virtual int queryTitles(const std::string userName, std::vector<u64>& outTitles) = 0;
    virtual int queryRevisions(const std::string userName, const u64 titleId, std::vector<std::string>& outRevisions) = 0;
};


class HTTPRemoteStore : public IRemoteStoreIO
{
private:
    std::string serverUrl;
    std::string saveDataPath;

public:
    HTTPRemoteStore(const std::string& serverUrl, const std::string& saveDataPath);
    ~HTTPRemoteStore();

private:
    int _issueSaveDataRevisionId(const std::string userName, const u64 titleId, std::string& revisionId);
    int _uploadSaveDataRevision(const std::string userName, const u64 titleId, const std::string& revisionId);
    int _getLatestSaveDataRevision(const std::string userName, const u64 titleId, std::string& revisionId);
    int _downloadSaveDataRevision(const std::string userName, const u64 titleId, const std::string& revisionId);

public:
    int push(const std::string userName, const u64 titleId) override;
    int pull(const std::string userName, const u64 titleId) override;
    int push(const std::string userName, const u64 titleId, const std::string& revision) override;
    int pull(const std::string userName, const u64 titleId, const std::string& revision) override;
};
