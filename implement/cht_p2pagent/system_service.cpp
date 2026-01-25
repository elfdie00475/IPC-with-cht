#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "nngipc.h"
#include "cht_ipc_client.h"

using namespace llt;

void request_callback(void *param, const uint8_t *req_payload, size_t req_len, uint8_t **res_payload, size_t *res_len)
{
    printf("req_payload %s %ld\n", req_payload, req_len);
    if (res_payload) *res_payload = NULL;
    if (res_len) *res_len = 0;

    if (req_len <= sizeof(stChtIpcHdr)) return ;
    uint32_t tmpFourCC = *((uint32_t *)req_payload);
    if (cht_ipc_msg_checkFourCC(tmpFourCC) != 1) return ;

    stChtIpcHdr *pIpcHdr = (stChtIpcHdr *)req_payload;
    uint16_t u16CmdType = pIpcHdr->u16Headers[1];
    uint32_t u32PayloadSize = pIpcHdr->u32PayloadSize;
    printf("pIpcHdr u16CmdType %x %d\n", u16CmdType, u32PayloadSize);
    if (u16CmdType == _GetCamStatusById && u32PayloadSize == sizeof(stCamStatusByIdReq)) {
        stCamStatusByIdReq *pReq = (stCamStatusByIdReq *)(req_payload + sizeof(stChtIpcHdr));
        printf("tenantId %s\n", pReq->tenantId);
        printf("netNo %s\n", pReq->netNo);
        printf("camSid %d\n", pReq->camSid);
        printf("camId %s\n", pReq->camId);
        printf("userId %s\n", pReq->userId);

        stCamStatusByIdRep rep;
        rep.result = 0;
        snprintf(rep.tenantId, CHT_IPC_STRING_SIZE, "%s", pReq->tenantId);
        snprintf(rep.netNo, CHT_IPC_STRING_SIZE, "%s", pReq->netNo);
        rep.camSid = pReq->camSid;
        snprintf(rep.camId, CHT_IPC_STRING_SIZE, "%s", pReq->camId);
        snprintf(rep.firmwareVer, CHT_IPC_STRING_SIZE, "%s", "v0.2.1");
        snprintf(rep.latestVersion, CHT_IPC_STRING_SIZE, "%s", "v0.2.1");
        rep.isMicrophone = 0;
        rep.speakVolume = 10;
        rep.imageQuality = 1;
        rep.activeStatus = 0;
        snprintf(rep.description, CHT_IPC_STRING_SIZE, "%s", "abc");
        snprintf(rep.name, CHT_IPC_STRING_SIZE, "%s", "abc");
        snprintf(rep.status, CHT_IPC_STRING_SIZE, "%s", "abc");
        snprintf(rep.externalStorageHealth, CHT_IPC_STRING_SIZE, "%s", "ok");
        snprintf(rep.externalStorageCapacity, CHT_IPC_STRING_SIZE, "%s", "1002002");
        snprintf(rep.externalStorageAvailable, CHT_IPC_STRING_SIZE, "%s", "1002002");
        snprintf(rep.wifiSsid, CHT_IPC_STRING_SIZE, "%s", "abdsavd");
        rep.wifiDbm = -32;

        pIpcHdr->u16Headers[0] = (pIpcHdr->u16Headers[0] & 1);
        pIpcHdr->u32PayloadSize = sizeof(stCamStatusByIdRep);
        pIpcHdr->u16Headers[2] = 0;
        pIpcHdr->u32HdrSize = 3;

        if (res_payload) {
            uint8_t *response = (uint8_t *)malloc(sizeof(stChtIpcHdr) + sizeof(stCamStatusByIdRep));
            if (!response) return ;

            uint8_t *write = response;
            uint32_t write_size = 0;

            memcpy(write, pIpcHdr, sizeof(stChtIpcHdr));
            write += sizeof(stChtIpcHdr);
            write_size += sizeof(stChtIpcHdr);
            memcpy(write, &rep, sizeof(stCamStatusByIdRep));
            write += sizeof(stCamStatusByIdRep);
            write_size += sizeof(stCamStatusByIdRep);

            *res_payload = response;
            if (res_len) *res_len = write_size;
        }
    }
}

int main(void)
{
    std::shared_ptr<nngipc::ResponseHandler> res_handler = nngipc::ResponseHandler::create(
        "system_service.ipc", 4, request_callback);
    if (!res_handler) return -1;
    if (!res_handler->start()) return -2;

    while(1) {
        sleep(2);
    }
    return 0;
}
