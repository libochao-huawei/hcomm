#include "base_pimpl.h"

namespace hcomm {

RemoteNotifyNewImpl::RemoteNotifyNewImpl()
{
}

RemoteNotifyNewImpl::~RemoteNotifyNewImpl()
{
}

uint32_t RemoteNotifyNewImpl::Init(const std::vector<char>& byteVector)
{
    printf("init\n");
    return 0;
}


uint32_t RemoteNotifyNewImpl::Open()
{
    printf("open\n");
    return 0;
}

uint32_t RemoteNotifyNewImpl::Close()
{
    printf("Close\n");
    return 0;
}

uint32_t RemoteNotifyNewImpl::GetNotifyOffset(uint64_t &notifyOffset)
{
    return 0;
}

uint32_t RemoteNotifyNewImpl::String(std::string test)
{
    return 0;
}

}