#ifndef BASE_H
#define BASE_H

#include <memory>
#include <vector>
#include <cstdint>
#include "stdio.h"
#include "stdlib.h"
#include "base_interface.h"

namespace hcomm {
// class RemoteNotifyImpl;
class RemoteNotify {
public:
    RemoteNotify();
    ~RemoteNotify();
    uint32_t Init(const std::vector<char>& byteVector);

    uint32_t Open();
    uint32_t Close();

    // 获取offset
    uint32_t GetNotifyOffset(uint64_t &notifyOffset);

    uint32_t V2Func() __attribute__((weak));

private:
    // std::unique_ptr<RemoteNotifyImpl> pimpl_;
};

class RemoteNotifyV2 : public IRemoteNotify {
public:
    RemoteNotifyV2();
    ~RemoteNotifyV2();
    uint32_t Init(const std::vector<char>& byteVector);

    uint32_t Open();
    uint32_t Close();

    // 获取offset
    uint32_t GetNotifyOffset(uint64_t &notifyOffset);

    uint32_t V2Func() __attribute__((weak));

private:
    // std::unique_ptr<RemoteNotifyImpl> pimpl_;
};

}

#endif // BASE_H
