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
    if (!pReq) return -1;

    int rc = 0;
    stChtIpcMsg pIpcRepMsg;
    uint8_t *recv = NULL;
    bool res = false;
    do {
        cht_ipc_msg_init(&pIpcRepMsg, ((cht_ipc_client_getMsgId() << 1) | 0), _GetCamStatusById);
        pIpcRepMsg.u32PayloadSize = sizeof(stCamStatusByIdReq);

        std::shared_ptr<nngipc::RequestHandler> rep_handler = 
                        nngipc::RequestHandler::create(CHT_IPC_NAME);
        if (!rep_handler) { rc = -2; break; }

        res = rep_handler->append((const uint8_t *)&pIpcRepMsg, sizeof(stChtIpcHdr));
        if (!res) { rc = -3; break; }
        res = rep_handler->append((const uint8_t *)pReq, sizeof(stCamStatusByIdReq));
        if (!res) { rc = -3; break; }
        res = rep_handler->send();
        if (!res) { rc = -4; break; }

        size_t recv_size = 0;
        res = rep_handler->recv(&recv, &recv_size);

        // check res and header;
        if (!res || !recv || recv_size < sizeof(stChtIpcHdr)) { 
            rc = -5; break;
        }
        stChtIpcHdr *pIpcRepHdr = (stChtIpcHdr *)recv;
        if ( cht_ipc_hdr_checkFourCC(pIpcRepHdr->u32FourCC) != 1 ||
             pIpcRepHdr->u32HdrSize < 3) {
            rc = -5; break;
        }
        int ipc_result = pIpcRepHdr->u16Headers[2];
        uint16_t u16CmdType = pIpcRepHdr->u16Headers[1];
        uint32_t u32PayloadSize = pIpcRepHdr->u32PayloadSize;
        printf("pIpcRepMsg ipc_result %d u16CmdType %x u32PayloadSize %d\n", ipc_result, u16CmdType, u32PayloadSize);
        if (ipc_result != 0 ||
            u16CmdType != _GetCamStatusById ||
            u32PayloadSize != sizeof(stCamStatusByIdRep)) { rc = -6; break; }

        if (pRep) {
            stChtIpcMsg *pIpcRepMsg = (stChtIpcMsg *)recv;
            memcpy(pRep, pIpcRepMsg->pu8Payload, sizeof(stCamStatusByIdRep));
        }
    } while (false);

    if (recv) {
        free(recv);
    }

    cht_ipc_msg_free(&pIpcRepMsg);

    return rc;
}
