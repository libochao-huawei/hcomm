#ifndef BASE_H
#define BASE_H

#include <memory>
#include <vector>
#include <cstdint>
#include "stdio.h"
#include "stdlib.h"


namespace hcomm {
// class RemoteNotifyImpl;
class RemoteNotifyNew {
public:
    RemoteNotifyNew() __attribute__((weak));
    ~RemoteNotifyNew() __attribute__((weak));
    uint32_t Init(const std::vector<char>& byteVector) __attribute__((weak));

    uint32_t Open() __attribute__((weak));
    uint32_t Close() __attribute__((weak));

    // 获取offset
    uint32_t GetNotifyOffset(uint64_t &notifyOffset) __attribute__((weak));

private:
    // std::unique_ptr<RemoteNotifyImpl> pimpl_;
};

void test();

}

#endif // BASE_H
