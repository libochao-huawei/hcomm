#include "app.h"

int main()
{
    hcomm::test();
    if(&hcomm::RemoteNotifyNew::Open != nullptr) {
        std::shared_ptr<hcomm::RemoteNotifyNew> remoteNotify = std::make_shared<hcomm::RemoteNotifyNew>();

        std::vector<char> byteVector;
        remoteNotify->Init(byteVector);
    } else {
        printf("nullptr\n")
    }

    return 0;
}
