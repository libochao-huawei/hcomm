#include "base.h"

namespace hcomm {

RemoteNotifyNew::RemoteNotifyNew()
{
}

RemoteNotifyNew::~RemoteNotifyNew()
{
}

uint32_t RemoteNotifyNew::Init(const std::vector<char>& byteVector)
{
    printf("init\n");
    return 0;
}


uint32_t RemoteNotifyNew::Open()
{
    printf("open\n");
    return 0;
}

uint32_t RemoteNotifyNew::Close()
{
    printf("Close\n");
    return 0;
}

uint32_t RemoteNotifyNew::GetNotifyOffset(uint64_t &notifyOffset)
{
    return 0;
}

void test()
{}
}