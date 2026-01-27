#include <fstream>
#include <iostream>
#include <unistd.h>
#include "nlohmann/json.hpp"
#include "whitelist_test.h"

void GenWhiteListFile()
{
    try {
        nlohmann::json rankTableJson = nlohmann::json::parse(whitelist);
        std::ofstream out(filePath, std::ofstream::out);
        out << rankTableJson;
    } catch(...) {
        std::cout << filePath << " generate failed!" << std::endl;
        return;
    }
    std::cout << filePath << " generated." << std::endl;
}

void DelWhiteListFile()
{
    int res = unlink(filePath);
    if (res == -1) {
        std::cout << filePath << " delete failed!" << std::endl;
        return;
    }
    std::cout << filePath << " deleted." << std::endl;
}
