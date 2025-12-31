#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#include "NngIpcRequestHandler.h"
#include "cht_ipc_client.h"

using namespace llt;

static uint16_t g_u16Id = 0;
static pthread_mutex_t g_IdMutex = PTHREAD_MUTEX_INITIALIZER;

static int cht_ipc_client_getId(void)
{
    uint16_t u16TmpId = g_u16Id;

    pthread_mutex_lock(&g_IdMutex);
    g_u16Id++;
    if (g_u16Id == 0) g_u16Id++;
    u16TmpId = g_u16Id;
    pthread_mutex_unlock(&g_IdMutex);

    return u16TmpId;
}

int cht_ipc_getCamStatusById(const stCamStatusByIdReq *pReq, stCamStatusByIdRep *pRep)
{
    if (!pReq || !pRep) return -1;

    int rc = 0;
    stChtIpcHdr pIpcHdr;
    uint8_t *recv = NULL;
    bool res = false;
    do {
        cht_ipc_hdr_init(&pIpcHdr, ((cht_ipc_client_getId() << 1) | 0), _GetCamStatusById);
        pIpcHdr.u32PayloadSize = sizeof(stCamStatusByIdReq);

        std::shared_ptr<nngipc::RequestHandler> res_handler = nngipc::RequestHandler::create(CHT_IPC_NAME);
        if (!res_handler) { rc = -2; break; }

        res = res_handler->append((const uint8_t *)&pIpcHdr, sizeof(stChtIpcHdr));
        if (!res) { rc = -3; break; }
        res = res_handler->append((const uint8_t *)pReq, sizeof(stCamStatusByIdReq));
        if (!res) { rc = -3; break; }
        res = res_handler->send();
        if (!res) { rc = -4; break; }

        size_t recv_size = 0;
        res = res_handler->recv(&recv, &recv_size);
        if (!res || !recv || 
            recv_size != sizeof(stChtIpcHdr) + sizeof(stCamStatusByIdRep)) { 
            rc = -5; break;
        }
        uint32_t tmpFourCC = *((uint32_t *)recv);
        if (cht_ipc_hdr_checkFourCC(tmpFourCC) != 1) {
            rc = -5; break;
        }

        uint8_t *pread = recv;
        size_t read_size = 0;

        memcpy(&pIpcHdr, pread, sizeof(stChtIpcHdr));
        pread += sizeof(stChtIpcHdr);
        read_size += sizeof(stChtIpcHdr);
        printf("pIpcHdr u16CmdType %d %x %d\n", pIpcHdr.result, pIpcHdr.u16CmdType, pIpcHdr.u32PayloadSize);
        if (pIpcHdr.result != 0 ||
            pIpcHdr.u16CmdType != _GetCamStatusById ||
            pIpcHdr.u32PayloadSize != sizeof(stCamStatusByIdRep)) { rc = -6; break; }

        memcpy(pRep, pread, sizeof(stCamStatusByIdRep));

    } while (false);

    if (recv) {
        free(recv);
    }

    cht_ipc_hdr_free(&pIpcHdr);

    return rc;
}