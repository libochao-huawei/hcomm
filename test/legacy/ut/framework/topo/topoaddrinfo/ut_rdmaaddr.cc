#include "gtest/gtest.h"
#include <time.h>
#include <limits.h>
#include <stdlib.h>
#include <mockcpp/mokc.h>
#include <mockcpp/mockcpp.hpp>
#include <string>
#include <stdint.h>
#include <dlfcn.h>
#include "securec.h"
#include "hal.h"
#include "rdma_hca.h"
#include "host_rdma.h"

// 添加这个，避免mock识别为到C++库中的realpath签名
extern "C" char* realpath(const char* path, char* resolved_buf);

class RDMAAddrInfoTest : public testing::Test {
protected:
    static void SetUpTestCase()
    {
        std::cout << "TopoAddrInfo tests set up." << std::endl;
    }

    static void TearDownTestCase()
    {
        std::cout << "TopoAddrInfo tests tear down." << std::endl;
    }

    virtual void SetUp()
    {
        std::cout << "A Test case in TopoAddrInfoTest SetUP" << std::endl;
    }

    virtual void TearDown()
    {
        GlobalMockObject::verify();
        std::cout << "A Test case in TopoAddrInfoTest TearDown" << std::endl;
    }
};

TEST_F(RDMAAddrInfoTest, ut_get_ip_addr)
{
    char ip[32] = {0};
    size_t len = 32;
    GetIpByIfName("lo", ip, len);
    EXPECT_TRUE(strcmp("127.0.0.1", ip) == 0);
}

TEST_F(RDMAAddrInfoTest, ut_is_same_pcie_sw1)
{
    const char* p1 = "/sys/devices/pci0000:f0/0000:f0:01.1/0000:f1:00.0/0000:f2:00.0/0000:f3:00.0";
    const char* p2 = "/sys/devices/pci0000:f0/0000:f0:01.1/0000:f1:00.0/0000:f2:00.1/0000:f4:00.0";
    EXPECT_TRUE(IsSamePcieSwitch(p1, p2) != 0);
}

TEST_F(RDMAAddrInfoTest, ut_is_same_pcie_sw2)
{
    NPU npu;
    HCA hca;  
    char npuPciePath[256] = {0};
    const char* p1 = "/sys/devices/pci0000:f0/0000:f0:01.1/0000:f1:00.0/0000:f2:00.0/0000:f3:00.0";
    const char* p2 = "/sys/devices/pci0000:f0/0000:f0:01.1/0000:f1:00.0/0000:f2:00.1/0000:f4:00.0";
    strcpy(npuPciePath, p1);
    strcpy(hca.pciePath, p2);
    MOCKER(realpath).stubs().with(any(), outBoundP(npuPciePath, sizeof(npuPciePath)))
        .will(returnValue(&npuPciePath[0]));
    EXPECT_TRUE(IsNpuAndNicInSamePcieSwitch(&npu, &hca) != 0);
}