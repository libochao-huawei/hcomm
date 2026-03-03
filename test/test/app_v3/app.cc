#include "app.h"

int main()
{
    hcomm::test();
    if(&hcomm::RemoteNotifyNew::String != nullptr) {
        std::shared_ptr<hcomm::RemoteNotifyNew> remoteNotify = std::make_shared<hcomm::RemoteNotifyNew>();

        std::vector<char> byteVector;
        remoteNotify->Init(byteVector);
        remoteNotify->Open();
    } else {
        printf("nullptr\n");
    }

    return 0;
}
