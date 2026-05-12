#ifndef CCU_BUFFER_HPP
#define CCU_BUFFER_HPP

#include <cstdint>
#include <type_traits>
#include "ccu_types.h"

namespace ccu {

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

} // namespace ccu

#endif // CCU_BUFFER_HPP
