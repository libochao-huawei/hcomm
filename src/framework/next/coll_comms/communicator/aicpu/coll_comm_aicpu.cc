#include "coll_comm_aicpu.h"

namespace hcomm {

void CollCommAicpu::NsCommClean()
{
    ubTransportMap_.clear();
}

}