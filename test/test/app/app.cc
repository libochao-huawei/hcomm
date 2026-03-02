#include "app.h"

int main()
{
    std::shared_ptr<hcomm::RemoteNotify> remoteNotify = std::make_shared<hcomm::RemoteNotify>();

    std::vector<char> byteVector;
    remoteNotify->Init(byteVector);

    return 0;
}
