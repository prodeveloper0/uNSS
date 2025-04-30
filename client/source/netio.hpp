#pragma once
#include <string>

class HTTPClient
{
public:
    HTTPClient();
    ~HTTPClient();

public:
    HTTPClient& setUrl(const std::string& url);
    HTTPClient& setMethod(const std::string& method);
    HTTPClient& setHeader(const std::string& key, const std::string& value);
    HTTPClient& setBody(const std::vector<uint8_t>& body);
    HTTPClient& setBody(const std::string& body);
    HTTPClient& setTimeout(int timeout);
    
    HTTPClient& setUserAgent(const std::string& userAgent);
    
};