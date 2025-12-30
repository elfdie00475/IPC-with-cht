#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "cht_ipc_client.h"

int main(void)
{
    stCamStatusByIdReq testReq;
    stCamStatusByIdRep testRep;

    snprintf(testReq.tenantId, CHT_IPC_STRING_SIZE, "abcd");
    snprintf(testReq.netNo, CHT_IPC_STRING_SIZE, "abcd");
    snprintf(testReq.camId, CHT_IPC_STRING_SIZE, "abcd");
    snprintf(testReq.userId, CHT_IPC_STRING_SIZE, "abcd");
    testReq.camSid = 0x123;
    int rc = cht_ipc_getCamStatusById(&testReq, &testRep);

    return 0;
}
