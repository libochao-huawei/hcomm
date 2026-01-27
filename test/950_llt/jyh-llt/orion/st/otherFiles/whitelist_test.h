#ifndef HCCLV2_STUB_WHITE_LIST_H
#define HCCLV2_STUB_WHITE_LIST_H

#include <string>

const char filePath[] = "whitelist.json";

const std::string whitelist = R"(
    { "host_ip": ["10.78.145.8"], "device_ip": [] } 
    )";

void GenWhiteListFile();

void DelWhiteListFile();


#endif