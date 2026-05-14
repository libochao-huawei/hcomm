#ifndef CCU_BUFFER_HPP
#define CCU_BUFFER_HPP

#include <cstdint>
#include <type_traits>
#include "ccu_types.h"
#include "ccu_data_api_impl.h"
#include "ccu_data_utils.hpp"

namespace ccu {

template <typename U> class Array;

class CcuBuffer final {
public:
    CcuBuffer() {
        auto ret = CcuBufferAlloc(&this->handle);
        if (ret != CcuResult::CCU_SUCCESS) {
            throw "CcuBufferAlloc: failed";
        }
    }

    CcuBuffer(const CcuBuffer& other) {
        this->handle = other.handle;
    }

    CcuBuffer(CcuBuffer&& other) noexcept {
        this->handle = other.handle;
    }

    void operator=(CcuBuffer&& other) {
        this->handle = other.handle;
    }

    CcuBufferHandle handle{0};

private:
    explicit CcuBuffer(NoAllocTag) {}
    template <typename U> friend class Array;
};

} // namespace ccu

#endif // CCU_BUFFER_HPP
