#include <sys/epoll.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include "securec.h"
#include "dl_hal_function.h"
#include "hccp.h"
#include "ra_client_host.h"
#include "ra.h"
#include "ra_init.h"
#include "ra_rdma.h"
#include "ra_hdc.h"
#include "ra_hdc_rdma_notify.h"
#include "ra_hdc_rdma.h"
#include "ra_hdc_socket.h"
#include "ra_peer.h"
#include "ra_peer_socket.h"
#include "ra_rs_comm.h"
#include "ra_rs_err.h"

__attribute__((constructor))
static void mock_init(void) {
    printf("\n");
    printf(" ====================================== \n");
    printf("  MOCK LIBRARY LOADED SUCCESSFULLY!       \n");
    printf("  PID: %d                                    \n", getpid());
    printf(" ====================================== \n");
    printf("\n");
    fflush(stdout);
}

int RsGetIfaddrsV2(struct InterfaceInfo interfaceInfos[], unsigned int *num,unsigned int phyId, bool isAll)
{
    *num = 1;
    interfaceInfos[0].family = AF_INET;
    interfaceInfos[0].ifaddr.ip.addr.s_addr = 16777343;
    strcpy(interfaceInfos[0].ifname, "eth0");
    interfaceInfos[0].scopeId = 0;
    interfaceInfos[0].ifaddr.mask.s_addr = 0x000000FF;
    return 0;
}