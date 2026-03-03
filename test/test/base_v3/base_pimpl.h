#ifndef BASE_IMPL_H
#define BASE_IMPL_H

#include <memory>
#include <vector>
#include <cstdint>
#include <string>
#include "stdio.h"
#include "stdlib.h"


namespace hcomm {
class RemoteNotifyNewImpl {
public:
    RemoteNotifyNewImpl() __attribute__((weak));
    ~RemoteNotifyNewImpl() __attribute__((weak));
    uint32_t Init(const std::vector<char>& byteVector) __attribute__((weak));

    uint32_t Open() __attribute__((weak));
    uint32_t Close() __attribute__((weak));

    // 获取offset
    uint32_t GetNotifyOffset(uint64_t &notifyOffset) __attribute__((weak));

    uint32_t String(std::string test) __attribute__((weak));

private:

};

}

#endif // BASE_IMPL_H
