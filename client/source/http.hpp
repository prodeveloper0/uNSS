#pragma once
#include <string>
#include <vector>
#include <map>
#include <functional>
#include <curl/curl.h>

// HTTP 클라이언트 에러 코드
#define HTTPCLIENT_ERROR_INIT_FAILED    -1
#define HTTPCLIENT_ERROR_UNSUPPORTED    -100

// libcurl 에러 코드
#define HTTPCLIENT_ERROR_CURL_URL       -2    // CURLcode::CURLE_URL_MALFORMAT
#define HTTPCLIENT_ERROR_CURL_CONNECT   -3    // CURLcode::CURLE_COULDNT_CONNECT
#define HTTPCLIENT_ERROR_CURL_DNS       -4    // CURLcode::CURLE_COULDNT_RESOLVE_HOST
#define HTTPCLIENT_ERROR_CURL_SSL       -5    // CURLcode::CURLE_SSL_CONNECT_ERROR
#define HTTPCLIENT_ERROR_CURL_TIMEOUT   -6    // CURLcode::CURLE_OPERATION_TIMEDOUT
#define HTTPCLIENT_ERROR_CURL_MEMORY    -7    // CURLcode::CURLE_OUT_OF_MEMORY
#define HTTPCLIENT_ERROR_CURL_OTHER     -8    // 기타 libcurl 에러


class HTTPClient
{
private:
    CURL* curl;
    std::string url;
    std::string method;
    std::map<std::string, std::string> headers;
    struct curl_slist* curl_headers;

    std::function<bool(const void* data, size_t size, size_t& actualSize)> onReceive;
    std::function<bool(void* data, size_t size, size_t& actualSize)> onSend;

    // libcurl 콜백 함수
    static size_t writeCallback(void* contents, size_t size, size_t nmemb, void* userp);
    static size_t readCallback(void* ptr, size_t size, size_t nmemb, void* userp);

public:
    HTTPClient();
    ~HTTPClient();

public:
    HTTPClient& setUrl(const std::string& url);
    HTTPClient& setMethod(const std::string& method);
    HTTPClient& setHeader(const std::string& key, const std::string& value);
    HTTPClient& setReceiveCallback(const std::function<bool(const void* data, size_t size, size_t& actualSize)>& onReceive);
    HTTPClient& setSendCallback(const std::function<bool(void* data, size_t size, size_t& actualSize)>& onSend);
    int perform();
};
