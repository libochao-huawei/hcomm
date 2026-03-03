#include "base_pimpl.h"
#include "base.h"

namespace hcomm {

RemoteNotifyNew::RemoteNotifyNew()
{
    pimpl_.reset((new (std::nothrow) RemoteNotifyNewImpl()));
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
    return pimpl_->Open();
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

uint32_t RemoteNotifyNew::String(std::string test)
{
    return pimpl_->String(test);
}

void test()
{}
}