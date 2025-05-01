#include "remote.hpp"

#include <string.h>

#include "http.hpp"

#include "utils.hpp"


#define HTTP_REMOTE_SUCCESS (0)
#define HTTP_REMOTE_NETWORK_ERROR (-1)
#define HTTP_REMOTE_SERVER_ERROR (-2)
#define HTTP_REMOTE_FILE_ERROR (-3)


HTTPRemoteStore::HTTPRemoteStore(const std::string& serverUrl, const std::string& saveDataPath) 
    : serverUrl(serverUrl), saveDataPath(saveDataPath)
{
}


HTTPRemoteStore::~HTTPRemoteStore()
{
}


int HTTPRemoteStore::_issueSaveDataRevisionId(const std::string userName, const u64 titleId, std::string& revisionId)
{
    HTTPClient client;
    const std::string url = serverUrl + "/users/" + userName + "/saves/" + toHex(titleId) + "/revisions";
    client.setUrl(url).setMethod("POST")
    .setReceiveCallback([&revisionId](const void* data, size_t size, size_t& actualSize) {
        revisionId = std::string(static_cast<const char*>(data), size);
        actualSize = size;
        return true;
    });

    int ret = client.perform();

    if (ret < 0)
    {
        return HTTP_REMOTE_NETWORK_ERROR;
    }
    else if (ret != 200)
    {
        return HTTP_REMOTE_SERVER_ERROR;
    }

    return HTTP_REMOTE_SUCCESS;
}


int HTTPRemoteStore::_uploadSaveDataRevision(const std::string userName, const u64 titleId, const std::string& revisionId)
{
    FILE* file = NULL;

    const Defer defer([&]() {
        if (file != NULL) 
        {
            fclose(file);
        }
    });

    const std::string hexTitleId = toHex(titleId);
    const std::string filePath = saveDataPath + "/" + hexTitleId + ".sar";

    file = fopen(filePath.c_str(), "rb");
    if (file == NULL) 
    {
        return HTTP_REMOTE_FILE_ERROR;
    }

    HTTPClient client;
    const std::string url = serverUrl + "/users/" + userName + "/saves/" + hexTitleId + "/revisions/" + revisionId;
    client.setUrl(url).setMethod("POST")
    .setSendCallback([&file](void* data, size_t size, size_t& actualSize) {
        actualSize = fread(data, 1, size, file);
        if (actualSize < 0)
        {
            return false;
        }

        return true;
    });

    int ret = client.perform();

    if (ret < 0)
    {
        return HTTP_REMOTE_NETWORK_ERROR;
    }
    else if (ret != 200)
    {
        return HTTP_REMOTE_SERVER_ERROR;
    }

    return HTTP_REMOTE_SUCCESS;
}


int HTTPRemoteStore::_getLatestSaveDataRevision(const std::string userName, const u64 titleId, std::string& revisionId)
{
    HTTPClient client;
    const std::string url = serverUrl + "/users/" + userName + "/saves/" + toHex(titleId) + "/revisions";
    client.setUrl(url).setMethod("GET")
    .setReceiveCallback([&revisionId](const void* data, size_t size, size_t& actualSize) {
        char* buf = (char*)malloc(size + 1);
        strncpy(buf, static_cast<const char*>(data), size);
        buf[size] = '\0';
        revisionId = std::string(buf);
        free(buf);

        actualSize = size;
        return true;
    });

    int ret = client.perform();

    if (ret < 0)
    {
        return HTTP_REMOTE_NETWORK_ERROR;
    }
    else if (ret != 200)
    {
        return HTTP_REMOTE_SERVER_ERROR;
    }

    return HTTP_REMOTE_SUCCESS;
}


int HTTPRemoteStore::_downloadSaveDataRevision(const std::string userName, const u64 titleId, const std::string& revisionId)
{
    FILE* file = NULL;

    const Defer defer([&]() {
        if (file != NULL) {
            fclose(file);
        }
    });

    const std::string filePath = saveDataPath + "/" + toHex(titleId) + ".sar";
    remove(filePath.c_str());

    file = fopen(filePath.c_str(), "wb");
    if (file == NULL) 
    {
        return HTTP_REMOTE_FILE_ERROR;
    }

    HTTPClient client;
    const std::string url = serverUrl + "/users/" + userName + "/saves/" + toHex(titleId) + "/revisions/" + revisionId + "/data";
    client.setUrl(url).setMethod("GET")
    .setReceiveCallback([&file](const void* data, size_t size, size_t& actualSize) {
        if ((actualSize = fwrite(data, 1, size, file)) < 0)
        {
            return false;
        }

        return true;
    });

    int ret = client.perform();

    if (ret < 0)
    {
        return HTTP_REMOTE_NETWORK_ERROR;
    }
    else if (ret != 200)
    {
        return HTTP_REMOTE_SERVER_ERROR;
    }

    return HTTP_REMOTE_SUCCESS;
}


int HTTPRemoteStore::push(const std::string userName, const u64 titleId)
{
    int ret;
    std::string revisionId;
    
    ret = _issueSaveDataRevisionId(userName, titleId, revisionId);
    if (ret != HTTP_REMOTE_SUCCESS)
    {
        return -1;
    }

    return push(userName, titleId, revisionId);
}


int HTTPRemoteStore::pull(const std::string userName, const u64 titleId)
{
    int ret;
    std::string revisionId;
    
    ret = _getLatestSaveDataRevision(userName, titleId, revisionId);
    if (ret != HTTP_REMOTE_SUCCESS)
    {
        return -1;
    }

    return pull(userName, titleId, revisionId);
}


int HTTPRemoteStore::push(const std::string userName, const u64 titleId, const std::string& revision)
{
    int ret;

    ret = _uploadSaveDataRevision(userName, titleId, revision);
    if (ret != HTTP_REMOTE_SUCCESS)
    {
        return -1;
    }

    return 0;
}


int HTTPRemoteStore::pull(const std::string userName, const u64 titleId, const std::string& revision)
{
    int ret;

    ret = _downloadSaveDataRevision(userName, titleId, revision);
    if (ret != HTTP_REMOTE_SUCCESS)
    {
        return -1;
    }

    return 0;
}
