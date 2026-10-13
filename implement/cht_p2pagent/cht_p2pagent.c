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
    printf("rc %d\n", rc);

    printf("result %d\n", testRep.result);
    printf("tenantId %s\n", testRep.tenantId);
    printf("netNo %s\n", testRep.netNo);
    printf("camSid %d\n", testRep.camSid);
    printf("camId %s\n", testRep.camId);
    printf("firmwareVer %s\n", testRep.firmwareVer);
    printf("latestVersion %s\n", testRep.latestVersion);
    printf("isMicrophone %d\n", testRep.isMicrophone);
    printf("speakVolume %d\n", testRep.speakVolume);
    printf("imageQuality %d\n", testRep.imageQuality);
    printf("activeStatus %d\n", testRep.activeStatus);
    printf("description %s\n", testRep.description);
    printf("name %s\n", testRep.name);
    printf("status %s\n", testRep.status);
    printf("externalStorageHealth %s\n", testRep.externalStorageHealth);
    printf("externalStorageCapacity %s\n", testRep.externalStorageCapacity);
    printf("externalStorageAvailable %s\n", testRep.externalStorageAvailable);
    printf("wifiSsid %s\n", testRep.wifiSsid);
    printf("wifiDbm %d\n", testRep.wifiDbm);
    return 0;
}
