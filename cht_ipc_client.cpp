#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#include "NngIpcRequestHandler.h"
#include "cht_ipc_client.h"

using namespace llt;

static uint16_t g_u16MsgId = 0;
static pthread_mutex_t g_IdMutex = PTHREAD_MUTEX_INITIALIZER;

static int cht_ipc_client_getMsgId(void)
{
    uint16_t u16TmpId = g_u16MsgId;

    pthread_mutex_lock(&g_IdMutex);
    g_u16MsgId++;
    if (g_u16MsgId == 0) g_u16MsgId++;
    u16TmpId = g_u16MsgId;
    pthread_mutex_unlock(&g_IdMutex);

    return u16TmpId;
}

int cht_ipc_getCamStatusById(const stCamStatusByIdReq *pReq, stCamStatusByIdRep *pRep)
{
    if (!pReq || !pRep) return -1;

    int rc = 0;
    stChtIpcMsg pIpcMsg;
    uint8_t *recv = NULL;
    bool res = false;
    do {
        cht_ipc_msg_init(&pIpcMsg, ((cht_ipc_client_getMsgId() << 1) | 0), _GetCamStatusById);
        pIpcMsg.u32PayloadSize = sizeof(stCamStatusByIdReq);

        std::shared_ptr<nngipc::RequestHandler> res_handler = nngipc::RequestHandler::create(CHT_IPC_NAME);
        if (!res_handler) { rc = -2; break; }

        res = res_handler->append((const uint8_t *)&pIpcMsg, sizeof(stChtIpcHdr));
        if (!res) { rc = -3; break; }
        res = res_handler->append((const uint8_t *)pReq, sizeof(stCamStatusByIdReq));
        if (!res) { rc = -3; break; }
        res = res_handler->send();
        if (!res) { rc = -4; break; }

        size_t recv_size = 0;
        res = res_handler->recv(&recv, &recv_size);

        // check res and header;
        if (!res || !recv || 
            recv_size != (sizeof(stChtIpcHdr) + sizeof(stCamStatusByIdRep))) { 
            rc = -5; break;
        }

        stChtIpcMsg *pIpcMsg = (stChtIpcMsg *)recv;
        if (cht_ipc_hdr_checkFourCC(pIpcMsg->u32FourCC) != 1) {
            rc = -5; break;
        }

        uint8_t *pread = recv;
        size_t read_size = 0;

        memcpy(&pIpcMsg, pread, sizeof(stChtIpcHdr));
        pread += sizeof(stChtIpcHdr);
        read_size += sizeof(stChtIpcHdr);
        printf("pIpcMsg u16CmdType %d %x %d\n", pIpcMsg.result, pIpcMsg.u16CmdType, pIpcMsg.u32PayloadSize);
        if (pIpcMsg.result != 0 ||
            pIpcMsg.u16CmdType != _GetCamStatusById ||
            pIpcMsg.u32PayloadSize != sizeof(stCamStatusByIdRep)) { rc = -6; break; }

        memcpy(pRep, pread, sizeof(stCamStatusByIdRep));

    } while (false);

    if (recv) {
        free(recv);
    }

    cht_ipc_msg_free(&pIpcMsg);

    return rc;
}
