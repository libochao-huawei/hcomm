#include <gtest/gtest.h>
#include "rootinfo_types.h"
#include "topo.h"
#include <map>
#include <string>

TEST(TOPO, test_ue_id) {
    // 标卡全部EID
    char file_path[128] = {0};
    GetTopoFilePathFromFile("../test/test1.json", file_path, sizeof(file_path));
    printf("file_path: %s\n", file_path);
}