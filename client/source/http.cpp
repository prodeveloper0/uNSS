#include "http.hpp"
#include <string.h>


HTTPClient::HTTPClient()
{
    curl = curl_easy_init();
    curl_headers = NULL;
    method = "GET";
}


HTTPClient::~HTTPClient()
{
    if (curl_headers) 
    {
        curl_slist_free_all(curl_headers);
    }
    
    if (curl) 
    {
        curl_easy_cleanup(curl);
    }
}


HTTPClient& HTTPClient::setUrl(const std::string& url)
{
    this->url = url;
    return *this;
}


HTTPClient& HTTPClient::setMethod(const std::string& method)
{
    this->method = method;
    return *this;
}


HTTPClient& HTTPClient::setHeader(const std::string& key, const std::string& value)
{
    headers[key] = value;
    std::string header = key + ": " + value;
    curl_headers = curl_slist_append(curl_headers, header.c_str());
    return *this;
}


HTTPClient& HTTPClient::setReceiveCallback(const std::function<bool(const void* data, size_t size, size_t& actualSize)>& onReceive)
{
    this->onReceive = onReceive;
    return *this;
}


HTTPClient& HTTPClient::setSendCallback(const std::function<bool(void* data, size_t size, size_t& actualSize)>& onSend)
{
    this->onSend = onSend;
    return *this;
}


size_t HTTPClient::writeCallback(void* contents, size_t size, size_t nmemb, void* userp)
{
    HTTPClient* client = static_cast<HTTPClient*>(userp);
    if (!client || !client->onReceive) 
    {
        return size * nmemb;
    }

    size_t actualSize = 0;
    return client->onReceive(contents, size * nmemb, actualSize) ? actualSize : 0;
}


size_t HTTPClient::readCallback(void* ptr, size_t size, size_t nmemb, void* userp)
{
    HTTPClient* client = static_cast<HTTPClient*>(userp);
    if (!client || !client->onSend) 
    {
        return size * nmemb;
    }

    size_t actualSize = 0;
    return client->onSend(ptr, size * nmemb, actualSize) ? actualSize : 0;
}


int HTTPClient::perform()
{
    if (!curl) 
    {
        return HTTPCLIENT_ERROR_INIT_FAILED;
    }

    // 지원하지 않는 메소드 체크
    if (method != "GET" && method != "POST")
    {
        return HTTPCLIENT_ERROR_UNSUPPORTED;
    }

    // 기본 설정
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, curl_headers);

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, this);

    curl_easy_setopt(curl, CURLOPT_READFUNCTION, readCallback);
    curl_easy_setopt(curl, CURLOPT_READDATA, this);

    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

    // HTTP 메서드 설정
    if (method == "GET") 
    {
        curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
    } 
    else if (method == "POST") 
    {
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
    }

    // 비동기 모드 설정
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) 
    {
        // libcurl 에러 코드를 우리의 에러 코드로 매핑
        switch (res)
        {
            case CURLE_URL_MALFORMAT:
                return HTTPCLIENT_ERROR_CURL_URL;
            case CURLE_COULDNT_CONNECT:
                return HTTPCLIENT_ERROR_CURL_CONNECT;
            case CURLE_COULDNT_RESOLVE_HOST:
                return HTTPCLIENT_ERROR_CURL_DNS;
            case CURLE_SSL_CONNECT_ERROR:
                return HTTPCLIENT_ERROR_CURL_SSL;
            case CURLE_OPERATION_TIMEDOUT:
                return HTTPCLIENT_ERROR_CURL_TIMEOUT;
            case CURLE_OUT_OF_MEMORY:
                return HTTPCLIENT_ERROR_CURL_MEMORY;
            default:
                return HTTPCLIENT_ERROR_CURL_OTHER;
        }
    }

    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    return static_cast<int>(http_code);
}
