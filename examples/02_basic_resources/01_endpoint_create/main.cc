/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software; you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * You may not use this file except in compliance with the License.
 * See the License for the specific language governing permissions and limitations under the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <iostream>
#include <cstring>
#include <cstdint>

#include <hccl/hccl.h>
#include <hccl/hccl_types.h>
#include <hcomm/hcomm_res.h>
#include <hcomm/hcomm_res_defs.h>

#define ACLCHECK(ret) \
    do { \
        if (ret != ACL_SUCCESS) { \
            printf("acl interface return err %s:%d, retcode: %d \n", __FILE__, __LINE__, ret); \
            return ret; \
        } \
    } while (0)

#define HCOMMCHECK(ret) \
    do { \
        if (ret != HCCL_SUCCESS) { \
            printf("hcomm interface return err %s:%d, retcode: %d \n", __FILE__, __LINE__, ret); \
            return ret; \
        } \
    } while (0)

int main(int argc, char *argv[])
{
    uint32_t devId = 0;

    std::cout << "=== HcommEndpointCreate Example ===" << std::endl;
    std::cout << "This example demonstrates how to create a communication endpoint." << std::endl;
    std::cout << std::endl;

    std::cout << "Note: This API is only supported on Ascend 950PR/Ascend 950DT devices." << std::endl;
    std::cout << "      It is NOT supported on Atlas A3/A2 series products." << std::endl;
    std::cout << std::endl;

    ACLCHECK(aclInit(NULL));
    ACLCHECK(aclrtSetDevice(static_cast<int32_t>(devId)));

    EndpointDesc endpointDesc;
    HcommResult ret = EndpointDescInit(&endpointDesc, 1);
    if (ret != 0) {
        std::cerr << "Failed to initialize EndpointDesc, ret: " << ret << std::endl;
        ACLCHECK(aclrtResetDevice(static_cast<int32_t>(devId)));
        ACLCHECK(aclFinalize());
        return ret;
    }

    endpointDesc.protocol = COMM_PROTOCOL_ROCE;
    endpointDesc.commAddr.type = COMM_ADDR_TYPE_IP_V4;
    endpointDesc.commAddr.addr.s_addr = inet_addr("192.168.1.100");
    endpointDesc.loc.locType = ENDPOINT_LOC_TYPE_HOST;
    endpointDesc.loc.device.devPhyId = devId;
    endpointDesc.loc.device.superDevId = 0;
    endpointDesc.loc.device.serverIdx = 0;
    endpointDesc.loc.device.superPodIdx = 0;

    std::cout << "Creating endpoint with:" << std::endl;
    std::cout << "  Protocol: COMM_PROTOCOL_ROCE" << std::endl;
    std::cout << "  IP Address: 192.168.1.100" << std::endl;
    std::cout << "  Device ID: " << devId << std::endl;
    std::cout << "  Location: DEVICE" << std::endl;
    std::cout << std::endl;

    EndpointHandle endpointHandle = nullptr;
    try {
        ret = HcommEndpointCreate(&endpointDesc, &endpointHandle);
        if (ret != 0) {
            std::cerr << "Failed to create endpoint, ret: " << ret << std::endl;
            std::cerr << "This may happen if the device does not support this API." << std::endl;
            ACLCHECK(aclrtResetDevice(static_cast<int32_t>(devId)));
            ACLCHECK(aclFinalize());
            return ret;
        }

        std::cout << "Endpoint created successfully!" << std::endl;
        std::cout << "Endpoint handle: " << endpointHandle << std::endl;
        std::cout << std::endl;

        ret = HcommEndpointDestroy(endpointHandle);
        if (ret != 0) {
            std::cerr << "Failed to destroy endpoint, ret: " << ret << std::endl;
        } else {
            std::cout << "Endpoint destroyed successfully!" << std::endl;
        }
    } catch (const std::exception &e) {
        std::cerr << "Exception caught: " << e.what() << std::endl;
        std::cerr << "This API may not be supported on this device type." << std::endl;
        ACLCHECK(aclrtResetDevice(static_cast<int32_t>(devId)));
        ACLCHECK(aclFinalize());
        return -1;
    }

    ACLCHECK(aclrtResetDevice(static_cast<int32_t>(devId)));
    ACLCHECK(aclFinalize());

    std::cout << std::endl;
    std::cout << "=== Example completed ===" << std::endl;
    return 0;
}
