#ifndef CCU_REMOTE_ADDR_HPP
#define CCU_REMOTE_ADDR_HPP

#include <type_traits>
#include "ccu_types.h"
#include "ccu_variable.hpp"
#include "ccu_address.hpp"

class CcuRemoteAddr final {
public:
    explicit CcuRemoteAddr() {}

    CcuRemoteAddr(const CcuRemoteAddr& other) {
        this->handle = other.handle;
        this->addr.handle = other.addr.handle;
        this->token.handle = other.token.handle;
    }

    void operator=(CcuRemoteAddr&& other) {
        this->handle = other.handle;
        this->addr.handle = other.addr.handle;
        this->token.handle = other.token.handle;
    }

    CcuAddress addr;
    CcuVariable token;
    CcuRemoteAddrHandle handle{0};
};

#endif // CCU_REMOTE_ADDR_HPP
