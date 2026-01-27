#include "stub_rank_table.h"
#include <fstream>
#include <iostream>
#include <unistd.h>
#include "nlohmann/json.hpp"

// ranktable.json 和正式代码中的文件名相同，参照 CommunicatorImpl::InitRankGraph()
const char filePath[] = "ranktable.json";

void GenRankTableFile(const std::string &rankTable)
{
    try {
        nlohmann::json rankTableJson = nlohmann::json::parse(rankTable);
        std::ofstream  out(filePath, std::ofstream::out);
        out << rankTableJson;
    } catch (...) {
        std::cout << filePath << " generate failed!" << std::endl;
        return;
    }
    std::cout << filePath << " generated." << std::endl;
}

void DelRankTableFile()
{
    int res = unlink(filePath);
    if (res == -1) {
        std::cout << filePath << " delete failed!" << std::endl;
        return;
    }
    std::cout << filePath << " deleted." << std::endl;
}
