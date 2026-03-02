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
}