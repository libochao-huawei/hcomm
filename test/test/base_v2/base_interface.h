#ifndef BASE_INTERFACE_H
#define BASE_INTERFACE_H

#include <memory>
#include <vector>
#include <cstdint>
#include "stdio.h"
#include "stdlib.h"


namespace hcomm {
class IRemoteNotify {
public:
    virtual ~IRemoteNotify() = default;
    virtual uint32_t Init(const std::vector<char>& byteVector);

    virtual uint32_t Open();
    virtual uint32_t Close();

    // 获取offset
    virtual uint32_t GetNotifyOffset(uint64_t &notifyOffset);

    virtual uint32_t V2Func() __attribute__((weak));

private:
    // std::unique_ptr<RemoteNotifyImpl> pimpl_;
};
}

extern "C" {
hcomm::IRemoteNotify* CreateRemoteNotify();
void DestroyRemoteNotify(hcomm::IRemoteNotify* ptr);
}

#endif // BASE_H
