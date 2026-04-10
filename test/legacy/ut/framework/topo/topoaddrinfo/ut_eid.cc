#include <gtest/gtest.h>
#include "rootinfo_types.h"
#include "eid_util.h"
#include <map>
#include <string>


TEST(EID, test_ue_id) {
    // 标卡全部EID
    std::map<std::string, int> port_map = {
        {"000000000000002000100000dfdf0020", 4},
        {"000000000000002000100000dfdf0028", 5},
        {"000000000000002000100000dfdf0030", 6},
        {"000000000000002000100000dfdf0021", 4},
        {"000000000000002000100000dfdf0029", 5},
        {"000000000000002000100000dfdf0031", 6},
        {"000000000000002000100000dfdf0022", 4},
        {"000000000000002000100000dfdf002a", 5},
        {"000000000000002000100000dfdf0032", 6},
        {"000000000000002000100000dfdf0023", 4},
        {"000000000000002000100000dfdf002b", 5},
        {"000000000000002000100000dfdf0033", 6},
    };
    for (auto it = port_map.begin(); it != port_map.end(); it++) {
        const char *eid = it->first.c_str();
        int port = it->second;
        int port1;
        int die_id;
        EidGetPortId(eid, &port1);
        EidGetDieId(eid, &die_id);
        EXPECT_EQ(port1, port);
        EXPECT_EQ(die_id, 0);
    }
}