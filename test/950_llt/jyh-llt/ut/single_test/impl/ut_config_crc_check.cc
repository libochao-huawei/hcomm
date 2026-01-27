#include "gtest/gtest.h"
#include <mockcpp/mockcpp.hpp>
#include <stdio.h>
#include <slog.h>
#include "mem_host_pub.h"
#include <string>
#include <stdlib.h>
#include <iostream>
#include <fstream>
#include "llt_hccl_stub_sal_pub.h"
#include "rank_consistentcy_checker.h"
using namespace std;
using namespace hccl;



class configCrcCheck : public testing::Test
{
protected:
    static void SetUpTestCase()
    {
        std::cout << "\033[36m--configCrcCheck SetUP--\033[0m" << std::endl;
    }
    static void TearDownTestCase()
    {
        std::cout << "\033[36m--configCrcCheck TearDown--\033[0m" << std::endl;
    }
    virtual void SetUp()
    {
        std::cout << "A Test SetUP" << std::endl;
    }
    virtual void TearDown()
    {
        std::cout << "A Test TearDown" << std::endl;
    }
};

