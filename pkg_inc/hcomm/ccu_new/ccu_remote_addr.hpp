#ifndef CCU_REMOTE_ADDR_HPP
#define CCU_REMOTE_ADDR_HPP

#include <type_traits>
#include "ccu_types.h"
#include "ccu_variable.hpp"
#include "ccu_address.hpp"

namespace ccu {

class RemoteAddr final {
public:
    explicit RemoteAddr() {}

    RemoteAddr(const RemoteAddr& other) {
        this->handle = other.handle;
        this->addr.handle = other.addr.handle;
        this->token.handle = other.token.handle;
    }
    void operator=(const RemoteAddr& other) {
        this->addr = other.addr;     
        this->token = other.token;   
    }
    void operator=(RemoteAddr&& other) {
        this->handle = other.handle;
        this->addr.handle = other.addr.handle;
        this->token.handle = other.token.handle;
    }

    Address addr;
    Variable token;
    CcuRemoteAddrHandle handle{0};
};

} // namespace ccu

#endif // CCU_REMOTE_ADDR_HPP
