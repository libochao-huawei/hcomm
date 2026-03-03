#include "app.h"
#include <functional>

int main()
{
    std::shared_ptr<hcomm::RemoteNotify> remoteNotify = std::make_shared<hcomm::RemoteNotify>();

    std::vector<char> byteVector;
    remoteNotify->Init(byteVector);

    if (&hcomm::RemoteNotify::V2Func != nullptr) {
        remoteNotify->V2Func();
    }

    if (&hcomm::RemoteNotifyV2::RemoteNotifyV2 != nullptr) {
        remoteNotify->V2Func();
    }

    return 0;
}
