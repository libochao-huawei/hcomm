#ifndef BASE_H
#define BASE_H

#include <memory>
#include <vector>
#include <cstdint>
#include "stdio.h"
#include "stdlib.h"


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

private:
    // std::unique_ptr<RemoteNotifyImpl> pimpl_;
};

void test();
}

#endif // BASE_H
