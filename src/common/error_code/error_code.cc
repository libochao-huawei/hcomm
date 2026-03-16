/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "base/err_msg.h"
#include "log.h"
#include <string>
#include <stdio.h>
namespace {

const std::string hcomm_g_msg = R"(
{
    "error_info_list": [
    {
      "errClass": "HCCL Errors",
      "errTitle": "Config_Error_Invalid_Environment_Variable",
      "ErrCode": "EI0001",
      "ErrMessage": "Value %s for environment variable [%s] is invalid. Expected value: %s.",
      "Arglist": "value,env,expect",
      "suggestion": {
        "Possible Cause": "N/A",
        "Solution": "N/A"
      }
    },
    {
      "errClass": "HCCL Errors",
      "errTitle": "Communication_Error_Timeout",
      "ErrCode": "EI0002",
      "ErrMessage": "Communication operator execution waiting times out. Waiting peer end: %s; task information: %s; Communication operator information: %s; Communication: %s.",
      "Arglist": "remote_rankid, base_information, task_information, group_rank_content",
      "suggestion": {
        "Possible Cause": "1. An exception occurs during the execution on some NPUs in the cluster. As a result, collective communication operation failed.2. The execution speed on some NPU in the cluster is too slow to complete a communication operation within the timeout interval. (The default timeout interval is 1800s, You can set the interval by using HCCL_EXEC_TIMEOUT.)3. The number of training samples of each NPU is inconsistent.4. Packet loss or other connectivity problems occur on the communication link.",
        "Solution": "1. If this error is reported on part of these ranks, check other ranks to see whether other errors have been reported earlier.2. If this error is reported for all ranks, check whether the error reporting time is consistent (the maximum difference must not exceed 1800s). If not, locate the cause or set the HCCL_EXEC_TIMEOUT environment variable to a larger value. 3. Ensure that the number of training samples of each NPU is consistent. 4. Check whether the completion queue element (CQE) of the error exists in the plog(grep -rn 'error cqe'). If so, check the network connection status. For details about the troubleshooting method, search for the keyword \"EI0002\" on https://www.hiascend.com/en/document/."
      }
    },
    {
      "errClass": "HCCL Errors",
      "errTitle": "Invalid_Argument_Collective_Communication_Operator",
      "ErrCode": "EI0003",
      "ErrMessage": "Failed to verify parameters of operator %s (operator name). Value %s for parameter %s is invalid. The expected value is %s.",
      "Arglist": "ccl_op,value,parameter,expect",
      "suggestion": {
        "Possible Cause": "N/A",
        "Solution": "N/A"
      }
    },
    {
      "errClass": "HCCL Errors",
      "errTitle": "File_Operation_Error_Parse",
      "ErrCode": "EI0004",
      "ErrMessage": "Failed to parse the ranktable file %s. Reason: %s",
      "Arglist": "error_reason,ranktable_path",
      "suggestion": {
        "Possible Cause": "N/A",
        "Solution": "N/A"
      }
    },
    {
      "errClass": "HCCL Errors ",
      "errTitle": "Invalid_Argument",
      "ErrCode": "EI0005",
      "ErrMessage": "The arguments for collective communication are inconsistent between ranks, parameter %s, local end %s, remote end %s.",
      "Arglist": "para_name,local_para,remote_para",
      "suggestion": {
        "Possible Cause": "N/A",
        "Solution": "N/A"
      }
    },
    {
      "errClass": "HCCL Errors",
      "errTitle": "Communication_Error_Get_Socket",
      "ErrCode": "EI0006",
      "ErrMessage": "Getting socket times out. Reason: %s",
      "Arglist": "reason",
      "suggestion": {
        "Possible Cause": "N/A",
        "Solution": "N/A"
      }
    },
    {
      "errClass": "HCCL Errors",
      "errTitle": "Resource_Error",
      "ErrCode": "EI0007",
      "ErrMessage": "Failed to allocate resource %s with info %s. Reason: Resources are exhausted.",
      "Arglist": "resource_type, resource_info",
      "suggestion": {
        "Possible Cause": "N/A",
        "Solution": "N/A"
      }
    },
    {
      "errClass": "HCCL Errors",
      "errTitle": "Package_Error_Incorrect_HCCL_Version",
      "ErrCode": "EI0008",
      "ErrMessage": "The HCCL versions are inconsistent. The local version is %s, while the remote version is %s.",
      "Arglist": "local_version, remote_version",
      "suggestion": {
        "Possible Cause": "N/A",
        "Solution": "Install the same HCCL version."
      }
    },
    {
      "errClass": "HCCL Errors",
      "errTitle": "Communication_Error_Initialize_Transport",
      "ErrCode": "EI0009",
      "ErrMessage": "Device %s transport init error. Reason: %s.",
      "Arglist": "device_id,reason",
      "suggestion": {
        "Possible Cause": "N/A",
        "Solution": "Use the following hccn_tool commands to check whether the port link is down. (The scope of i represents the number of NPUs of each node. 8 is used as an example.) 1. for i in {0..7}; do hccn_tool -i $i -optical -g; done | grep present: Check whether the optical module is in position. 2. for i in {0..7}; do hccn_tool -i $i -ip -g; done. Check whether the IP address is configured. 3. for i in {0..7}; do hccn_tool -i $i -lldp -g: Check whether the switch is connected."
      }
    },
    {
      "errClass": "HCCL Errors",
      "errTitle": "Communication_Error_P2P",
      "ErrCode": "EI0010",
      "ErrMessage": "P2P communication failed. Reason: %s",
      "Arglist": "reason",
      "suggestion": {
        "Possible Cause": "N/A",
        "Solution": "Ensure that the NPU card is normal and entering environment variables 'export HCCL_INTRA_ROCE_ENABLE=1'."
      }
    },
    {
      "errClass": "HCCL Errors",
      "errTitle": "Resource_Error_Insufficient_Device_Memory",
      "ErrCode": "EI0011",
      "ErrMessage": "Failed to allocate [%s] bytes of NPU memory.",
      "Arglist": "memory_size",
      "suggestion": {
        "Possible Cause": "Allocation failure due to insufficient NPU memory.",
        "Solution": "Stop unnecessary processes and ensure the required memory is available."
      }
    },
    {
      "errClass": "HCCL Errors",
      "errTitle": "Execution_Error_SDMA",
      "ErrCode": "EI0012",
      "ErrMessage": "SDMA memory copy task exception occurred. Remote rank: [%s]. Base information: [%s]. Task information: [%s]. Communicator information: [%s].",
      "Arglist": "remote_rankid, base_information, task_information, group_rank_content",
      "suggestion": {
        "Possible Cause": "1. Network connection exception occurred during the SDMA task execution. 2. The peer process exits abnormally. 3. The input or output memory address is not allocated, the actual allocated size is smaller than the input data size, or the memory is freed before the operator execution is complete.",
        "Solution": "1. Check whether the network link is abnormal during the execution. 2. Check whether a process in the cluster exits before an error is reported. If yes, locate the cause of the process exit. 3. Check whether the size of the input/output memory passed to the communication operator meets the expectation, and whether the input/output memory or communicator is freed or destroyed before the operator execution is complete."
      }
    },
    {
      "errClass": "HCCL Errors",
      "errTitle": "Execution_Error_ROCE_CQE",
      "ErrCode": "EI0013",
      "ErrMessage": "An error CQE occurred during operator execution. Local information: server %s, device ID %s, device IP %s. Peer information: server %s, device ID %s, device IP %s.",
      "Arglist": "localServerId,localDeviceId,localDeviceIp,remoteServerId,remoteDeviceId,remoteDeviceIp",
      "suggestion": {
        "Possible Cause": "1. The network between two devices is abnormal. For example, the network port is intermittently disconnected. 2. The peer process exits abnormally in advance. As a result, the local end cannot receive the response from the peer end.",
        "Solution": "1. Check whether the network devices between the two ends are abnormal. 2. Check whether the peer process exits first. If yes, check the cause of the process exit."
      }
    },
    {
      "errClass": "HCCL Errors",
      "errTitle": "Config_Error_Ranktable",
      "ErrCode": "EI0014",
      "ErrMessage": "Value %s for ranktable variable %s is invalid. Expected value: %s.",
      "Arglist": "value, variable, expect",
      "suggestion": {
        "Possible Cause": "Failed to verify the content of the ranktable file, possibly due to inconsistency between the file content and the actual device information.",
        "Solution": "Try again with a valid cluster configuration in the ranktable file. Ensure that the configuration matches the operating environment."
      }
    },
    {
      "errClass": "HCCL Errors",
      "errTitle": "Communication_Error_Ranktable_Detect",
      "ErrCode": "EI0015",
      "ErrMessage": "Failed to collect cluster information of the communicator based on rootInfo detection. Reason: %s.",
      "Arglist": "error_reason",
      "suggestion": {
        "Possible Cause": "N/A",
        "Solution":"1. Check whether all ranks in the communicator have delivered the communicator creation interface. 2. Check the connectivity between the host networks of all nodes and the server node. 3. Check whether the HCCL_SOCKET_IFNAME environment variable of all nodes is correctly configured. 4. Increase the timeout by configuring the HCCL_CONNECT_TIMEOUT environment variable."
      }
    },
    {
      "errClass": "HCCL Errors",
      "errTitle": "Config_Error",
      "ErrCode": "EI0016",
      "ErrMessage": "Value %s for config %s is invalid. Expected value: %s.",
      "Arglist": "value, variable, expect",
      "suggestion": {
        "Possible Cause": "N/A",
        "Solution": "N/A"
      }
    },
    {
      "errClass": "HCCL Errors",
      "errTitle": "Config_Error_Ranktable",
      "ErrCode": "EI0017",
      "ErrMessage": "Config %s is missing in the ranktable file.",
      "Arglist": "config",
      "suggestion": {
        "Possible Cause": "N/A",
        "Solution": "N/A"
      }
    },
    {
      "errClass": "HCCL Errors",
      "errTitle": "Execution_Error_UB_CQE",
      "ErrCode": "EI0018",
      "ErrMessage": "An error CQE occurred during operator execution. Local information: server %s, device ID %s, device IP %s. Peer information: server %s, device ID %s, device IP %s.",
      "Arglist": "localServerId, localDeviceId, localDeviceIp, remoteServerId, remoteDeviceId, remoteDeviceIp",
      "suggestion": {
        "Possible Cause": "1. The network between two devices is abnormal. For example, the network port is intermittently disconnected. 2. The peer process exits unexpectedly in advance. As a result, the local end cannot receive the response from the peer end. 3.The hardware of the HBM or UB chip processing module of either device is abnormal.",
        "Solution": "1. Check whether the network devices between the two ends are abnormal. Generally, packet loss occurs due to intermittent disconnection of the port. If the ping test fails, check whether the port is linkdown or the network configuration is incorrect.2. Check whether the peer process exits first. If yes, check the reason why the process exit.3.Use the RAS fault check mechanism to check whether the hardware of the HBM or UB chip processing module of either device is abnormal.If the hardware is abnormal, contact Huawei technical support."
      }
    },
    {
      "errClass": "HCCL Errors",
      "errTitle": "Communication_Error_Bind_IP_Port",
      "ErrCode": "EI0019",
      "ErrMessage": "Failed to enable listening for the host network adapter socket. Reason: %s",
      "Arglist": "reason",
      "suggestion": {
        "Possible Cause": "N/A",
        "Solution": "1. Check whether this port has been occupied by another process. If yes, you can make adjustment using the environment variable HCCL_IF_BASE_PORT and use sysctl -w net.ipv4.ip_local_reserved_ports=****-**** to adjust the scope of reserved ports. 2. Check whether the service process is started multiple times on a device during this service."
      }
    },
    {
      "errClass": "HCCL Errors",
      "errTitle": "Communication_Error_Bind_IP_Port",
      "ErrCode": "EI0020",
      "ErrMessage": "Failed to enable listening for the NPU network adapter socket. Reason: %s",
      "Arglist": "reason",
      "suggestion": {
        "Possible Cause": "N/A",
        "Solution": "Check whether the single-card multi-process scenario is used. If yes, configure the port number using the environment variable HCCL_NPU_SOCKET_PORT_RANGE."
      }
    }
  ]
}
)";
}

REG_FORMAT_ERROR_MSG(hcomm_g_msg.c_str(), hcomm_g_msg.size());