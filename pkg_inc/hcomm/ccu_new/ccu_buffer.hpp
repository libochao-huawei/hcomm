#ifndef CCU_BUFFER_HPP
#define CCU_BUFFER_HPP

#include <cstdint>
#include <type_traits>
#include "ccu_types.h"

class CcuBuffer final {
public:
    explicit CcuBuffer() {}

    CcuBuffer(const CcuBuffer& other) {
        this->handle = other.handle;
    }

    void operator=(CcuBuffer&& other) {
        this->handle = other.handle;
    }

    CcuBufferHandle handle{0};
};

static_assert(std::is_standard_layout<CcuBuffer>::value,
    "CcuBuffer must be standard layout for .so ABI stability");
static_assert(sizeof(CcuBuffer) == sizeof(CcuBufferHandle),
    "CcuBuffer layout changed - will break .so ABI!");

#endif // CCU_BUFFER_HPP