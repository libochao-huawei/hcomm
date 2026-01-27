#include "gtest/gtest.h"
#include <mockcpp/mokc.h>
#include <mockcpp/mockcpp.hpp>
#include "alg_topo_package_helper.h"

using namespace Hccl;

class AlgTopoPackageHelperTest : public testing::Test {
protected:
    static void SetUpTestCase()
    {
        std::cout << "AlgTopoPackageHelperTest SetUP" << std::endl;
    }

    static void TearDownTestCase()
    {
        std::cout << "AlgTopoPackageHelperTest TearDown" << std::endl;
    }

    virtual void SetUp()
    {
        std::cout << "A Test case in AlgTopoPackageHelperTest SetUp" << std::endl;
    }

    virtual void TearDown()
    {
        GlobalMockObject::verify();
        std::cout << "A Test case in AlgTopoPackageHelperTest TearDown" << std::endl;
    }
};

TEST_F(AlgTopoPackageHelperTest, serialize_and_deserialize)
{
    AlgTopoPackageHelper tool;
    AlgTopoInfo info;
    std::map<RankId, u32> myMap = {{1, 2}};
    std::map<RankId, u32> myMap2 = {{3, 4}};
    std::vector<std::map<RankId, u32>> virtRankMap;
    virtRankMap.push_back(myMap);
    virtRankMap.push_back(myMap2);
    info.virtRankMap = virtRankMap;
    std::vector<std::vector<std::vector<RankId>>> vTopo = {{{1, 2}, {3, 4}}, {{5, 6}, {7, 8}}};
    info.vTopo = vTopo;
    std::vector<std::vector<RankId>>              virtRanks = {{1, 2}, {3, 4}};
    info.virtRanks = virtRanks;

    auto data = tool.GetPackedData(info);
    auto parsedInfo = tool.GetAlgTopoInfo(data);

    EXPECT_EQ(info.virtRanks, parsedInfo.virtRanks);
    EXPECT_EQ(info.virtRankMap, parsedInfo.virtRankMap);
    EXPECT_EQ(info.vTopo, parsedInfo.vTopo);
}
