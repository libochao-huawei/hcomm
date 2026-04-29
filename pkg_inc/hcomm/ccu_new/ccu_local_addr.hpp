#ifndef CCU_LOCAL_ADDR_HPP
#define CCU_LOCAL_ADDR_HPP

#include <type_traits>
#include "ccu_types.h"
#include "ccu_variable.hpp"
#include "ccu_address.hpp"

namespace ccu {

class LocalAddr final {
public:
    explicit LocalAddr() {}

    LocalAddr(const LocalAddr& other) {
        this->handle = other.handle;
        this->addr.handle = other.addr.handle;
        this->token.handle = other.token.handle;
    }

    void operator=(LocalAddr&& other) {
        this->handle = other.handle;
        this->addr.handle = other.addr.handle;
        this->token.handle = other.token.handle;
    }

    Address addr;
    Variable token;
    CcuLocalAddrHandle handle{0};
};

} // namespace ccu

#endif // CCU_LOCAL_ADDR_HPP
