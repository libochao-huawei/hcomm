#ifndef CCU_BUFFER_HPP
#define CCU_BUFFER_HPP

#include <cstdint>
#include <type_traits>
#include "ccu_types.h"

namespace ccu {

class Buffer final {
public:
    explicit Buffer() {}

    Buffer(const Buffer& other) {
        this->handle = other.handle;
    }

    void operator=(Buffer&& other) {
        this->handle = other.handle;
    }

    CcuBufferHandle handle{0};
};

} // namespace ccu

#endif // CCU_BUFFER_HPP
