#include "base.h"

namespace hcomm {

RemoteNotify::RemoteNotify()
{
}

RemoteNotify::~RemoteNotify()
{
}

uint32_t RemoteNotify::Init(const std::vector<char>& byteVector)
{
    printf("init\n");
    return 0;
}


uint32_t RemoteNotify::Open()
{
    printf("open\n");
    return 0;
}

uint32_t RemoteNotify::Close()
{
    printf("Close\n");
    return 0;
}

uint32_t RemoteNotify::GetNotifyOffset(uint64_t &notifyOffset)
{
    return 0;
}

uint32_t RemoteNotify::V2Func()
{
    printf("V2Func\n");
    return 0;
}





RemoteNotifyV2::RemoteNotifyV2()
{
}

RemoteNotifyV2::~RemoteNotifyV2()
{
}

uint32_t RemoteNotifyV2::Init(const std::vector<char>& byteVector)
{
    printf("RemoteNotifyV2 init\n");
    return 0;
}


uint32_t RemoteNotifyV2::Open()
{
    printf("RemoteNotifyV2 open\n");
    return 0;
}

uint32_t RemoteNotifyV2::Close()
{
    printf("RemoteNotifyV2 Close\n");
    return 0;
}

uint32_t RemoteNotifyV2::GetNotifyOffset(uint64_t &notifyOffset)
{
    return 0;
}

uint32_t RemoteNotifyV2::V2Func()
{
    printf("RemoteNotifyV2 V2Func\n");
    return 0;
}
}