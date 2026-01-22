#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#include "zwsystem_ipc_client.h"

int main(void)
{
    return 0;
}

#if 0
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
    stChtIpcMsg pIpcReqMsg;
    uint8_t *recv = NULL;
    bool res = false;
    do {
        cht_ipc_msg_init(&pIpcReqMsg, ((cht_ipc_client_getMsgId() << 1) | 0), _GetCamStatusById);
        pIpcReqMsg.stHdr.u32PayloadSize = sizeof(stCamStatusByIdReq);

        std::shared_ptr<nngipc::RequestHandler> rep_handler =
                        nngipc::RequestHandler::create(CHT_IPC_NAME);
        if (!rep_handler) { rc = -2; break; }

        res = rep_handler->append((const uint8_t *)&pIpcReqMsg, sizeof(stChtIpcHdr));
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
        if ( cht_ipc_msg_checkFourCC(pIpcRepHdr->u32FourCC) != 1 ||
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

    cht_ipc_msg_free(&pIpcReqMsg);

    return rc;
}

int cht_ipc_setPtzControlMove(const stPtzMoveReq *pReq, stPtzMoveRep *pRep)
{
    if (!pReq) return -1;

    int rc = 0;
    stChtIpcMsg pIpcReqMsg;
    uint8_t *recv = NULL;
    bool res = false;
    do {
        cht_ipc_msg_init(&pIpcReqMsg, ((cht_ipc_client_getMsgId() << 1) | 0), _HamiCamPtzControlMove);
        pIpcReqMsg.stHdr.u32PayloadSize = sizeof(stPtzMoveReq);

        std::shared_ptr<nngipc::RequestHandler> rep_handler =
                        nngipc::RequestHandler::create(CHT_IPC_NAME);
        if (!rep_handler) { rc = -2; break; }

        res = rep_handler->append((const uint8_t *)&pIpcReqMsg, sizeof(stChtIpcHdr));
        if (!res) { rc = -3; break; }
        res = rep_handler->append((const uint8_t *)pReq, sizeof(stPtzMoveReq));
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
        if ( cht_ipc_msg_checkFourCC(pIpcRepHdr->u32FourCC) != 1 ||
             pIpcRepHdr->u32HdrSize < 3) {
            rc = -5; break;
        }
        int ipc_result = pIpcRepHdr->u16Headers[2];
        uint16_t u16CmdType = pIpcRepHdr->u16Headers[1];
        uint32_t u32PayloadSize = pIpcRepHdr->u32PayloadSize;
        if (ipc_result != 0 ||
            u16CmdType != _HamiCamPtzControlMove ||
            u32PayloadSize != sizeof(stPtzMoveRep)) { rc = -6; break; }

        if (pRep) {
            stChtIpcMsg *pIpcRepMsg = (stChtIpcMsg *)recv;
            memcpy(pRep, pIpcRepMsg->pu8Payload, sizeof(stPtzMoveRep));
        }
    } while (false);

    if (recv) {
        free(recv);
    }

    cht_ipc_msg_free(&pIpcReqMsg);

    return rc;
}

int cht_ipc_setPtzAbsoluteMove(const stPtzMoveReq *pReq, stPtzMoveRep *pRep)
{
    if (!pReq) return -1;

    int rc = 0;
    stChtIpcMsg pIpcReqMsg;
    uint8_t *recv = NULL;
    bool res = false;
    do {
        cht_ipc_msg_init(&pIpcReqMsg, ((cht_ipc_client_getMsgId() << 1) | 0), _PtzAbsoluteMove);
        pIpcReqMsg.stHdr.u32PayloadSize = sizeof(stPtzMoveReq);

        std::shared_ptr<nngipc::RequestHandler> rep_handler =
                        nngipc::RequestHandler::create(CHT_IPC_NAME);
        if (!rep_handler) { rc = -2; break; }

        res = rep_handler->append((const uint8_t *)&pIpcReqMsg, sizeof(stChtIpcHdr));
        if (!res) { rc = -3; break; }
        res = rep_handler->append((const uint8_t *)pReq, sizeof(stPtzMoveReq));
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
        if ( cht_ipc_msg_checkFourCC(pIpcRepHdr->u32FourCC) != 1 ||
             pIpcRepHdr->u32HdrSize < 3) {
            rc = -5; break;
        }
        int ipc_result = pIpcRepHdr->u16Headers[2];
        uint16_t u16CmdType = pIpcRepHdr->u16Headers[1];
        uint32_t u32PayloadSize = pIpcRepHdr->u32PayloadSize;
        if (ipc_result != 0 ||
            u16CmdType != _PtzAbsoluteMove ||
            u32PayloadSize != sizeof(stPtzMoveRep)) { rc = -6; break; }

        if (pRep) {
            stChtIpcMsg *pIpcRepMsg = (stChtIpcMsg *)recv;
            memcpy(pRep, pIpcRepMsg->pu8Payload, sizeof(stPtzMoveRep));
        }
    } while (false);

    if (recv) {
        free(recv);
    }

    cht_ipc_msg_free(&pIpcReqMsg);

    return rc;
}

int cht_ipc_setPtzRelativeMove(const stPtzMoveReq *pReq, stPtzMoveRep *pRep)
{
    if (!pReq) return -1;

    int rc = 0;
    stChtIpcMsg pIpcReqMsg;
    uint8_t *recv = NULL;
    bool res = false;
    do {
        cht_ipc_msg_init(&pIpcReqMsg, ((cht_ipc_client_getMsgId() << 1) | 0), _PtzRelativeMove);
        pIpcReqMsg.stHdr.u32PayloadSize = sizeof(stPtzMoveReq);

        std::shared_ptr<nngipc::RequestHandler> rep_handler =
                        nngipc::RequestHandler::create(CHT_IPC_NAME);
        if (!rep_handler) { rc = -2; break; }

        res = rep_handler->append((const uint8_t *)&pIpcReqMsg, sizeof(stChtIpcHdr));
        if (!res) { rc = -3; break; }
        res = rep_handler->append((const uint8_t *)pReq, sizeof(stPtzMoveReq));
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
        if ( cht_ipc_msg_checkFourCC(pIpcRepHdr->u32FourCC) != 1 ||
             pIpcRepHdr->u32HdrSize < 3) {
            rc = -5; break;
        }
        int ipc_result = pIpcRepHdr->u16Headers[2];
        uint16_t u16CmdType = pIpcRepHdr->u16Headers[1];
        uint32_t u32PayloadSize = pIpcRepHdr->u32PayloadSize;
        if (ipc_result != 0 ||
            u16CmdType != _PtzRelativeMove ||
            u32PayloadSize != sizeof(stPtzMoveRep)) { rc = -6; break; }

        if (pRep) {
            stChtIpcMsg *pIpcRepMsg = (stChtIpcMsg *)recv;
            memcpy(pRep, pIpcRepMsg->pu8Payload, sizeof(stPtzMoveRep));
        }
    } while (false);

    if (recv) {
        free(recv);
    }

    cht_ipc_msg_free(&pIpcReqMsg);

    return rc;
}

int cht_ipc_setPtzContinuousMove(const stPtzMoveReq *pReq, stPtzMoveRep *pRep)
{
    if (!pReq) return -1;

    int rc = 0;
    stChtIpcMsg pIpcReqMsg;
    uint8_t *recv = NULL;
    bool res = false;
    do {
        cht_ipc_msg_init(&pIpcReqMsg, ((cht_ipc_client_getMsgId() << 1) | 0), _PtzContinuousMove);
        pIpcReqMsg.stHdr.u32PayloadSize = sizeof(stPtzMoveReq);

        std::shared_ptr<nngipc::RequestHandler> rep_handler =
                        nngipc::RequestHandler::create(CHT_IPC_NAME);
        if (!rep_handler) { rc = -2; break; }

        res = rep_handler->append((const uint8_t *)&pIpcReqMsg, sizeof(stChtIpcHdr));
        if (!res) { rc = -3; break; }
        res = rep_handler->append((const uint8_t *)pReq, sizeof(stPtzMoveReq));
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
        if ( cht_ipc_msg_checkFourCC(pIpcRepHdr->u32FourCC) != 1 ||
             pIpcRepHdr->u32HdrSize < 3) {
            rc = -5; break;
        }
        int ipc_result = pIpcRepHdr->u16Headers[2];
        uint16_t u16CmdType = pIpcRepHdr->u16Headers[1];
        uint32_t u32PayloadSize = pIpcRepHdr->u32PayloadSize;
        if (ipc_result != 0 ||
            u16CmdType != _PtzContinuousMove ||
            u32PayloadSize != sizeof(stPtzMoveRep)) { rc = -6; break; }

        if (pRep) {
            stChtIpcMsg *pIpcRepMsg = (stChtIpcMsg *)recv;
            memcpy(pRep, pIpcRepMsg->pu8Payload, sizeof(stPtzMoveRep));
        }
    } while (false);

    if (recv) {
        free(recv);
    }

    cht_ipc_msg_free(&pIpcReqMsg);

    return rc;
}

int cht_ipc_setNightMode(const stNightModeReq *pReq, stNightModeRep *pRep)
{
    if (!pReq) return -1;

    int rc = 0;
    stChtIpcMsg pIpcReqMsg;
    uint8_t *recv = NULL;
    bool res = false;
    do {
        cht_ipc_msg_init(&pIpcReqMsg, ((cht_ipc_client_getMsgId() << 1) | 0), _SetNightMode);
        pIpcReqMsg.stHdr.u32PayloadSize = sizeof(stNightModeReq);

        std::shared_ptr<nngipc::RequestHandler> rep_handler =
                        nngipc::RequestHandler::create(CHT_IPC_NAME);
        if (!rep_handler) { rc = -2; break; }

        res = rep_handler->append((const uint8_t *)&pIpcReqMsg, sizeof(stChtIpcHdr));
        if (!res) { rc = -3; break; }
        res = rep_handler->append((const uint8_t *)pReq, sizeof(stNightModeReq));
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
        if ( cht_ipc_msg_checkFourCC(pIpcRepHdr->u32FourCC) != 1 ||
             pIpcRepHdr->u32HdrSize < 3) {
            rc = -5; break;
        }
        int ipc_result = pIpcRepHdr->u16Headers[2];
        uint16_t u16CmdType = pIpcRepHdr->u16Headers[1];
        uint32_t u32PayloadSize = pIpcRepHdr->u32PayloadSize;
        if (ipc_result != 0 ||
            u16CmdType != _SetNightMode ||
            u32PayloadSize != sizeof(stNightModeRep)) { rc = -6; break; }

        if (pRep) {
            stChtIpcMsg *pIpcRepMsg = (stChtIpcMsg *)recv;
            memcpy(pRep, pIpcRepMsg->pu8Payload, sizeof(stNightModeRep));
        }
    } while (false);

    if (recv) {
        free(recv);
    }

    cht_ipc_msg_free(&pIpcReqMsg);

    return rc;
}

int cht_ipc_setAutoNightMode(const stNightModeReq *pReq, stNightModeRep *pRep)
{
    if (!pReq) return -1;

    int rc = 0;
    stChtIpcMsg pIpcReqMsg;
    uint8_t *recv = NULL;
    bool res = false;
    do {
        cht_ipc_msg_init(&pIpcReqMsg, ((cht_ipc_client_getMsgId() << 1) | 0), _SetAutoNightVision);
        pIpcReqMsg.stHdr.u32PayloadSize = sizeof(stNightModeReq);

        std::shared_ptr<nngipc::RequestHandler> rep_handler =
                        nngipc::RequestHandler::create(CHT_IPC_NAME);
        if (!rep_handler) { rc = -2; break; }

        res = rep_handler->append((const uint8_t *)&pIpcReqMsg, sizeof(stChtIpcHdr));
        if (!res) { rc = -3; break; }
        res = rep_handler->append((const uint8_t *)pReq, sizeof(stNightModeReq));
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
        if ( cht_ipc_msg_checkFourCC(pIpcRepHdr->u32FourCC) != 1 ||
             pIpcRepHdr->u32HdrSize < 3) {
            rc = -5; break;
        }
        int ipc_result = pIpcRepHdr->u16Headers[2];
        uint16_t u16CmdType = pIpcRepHdr->u16Headers[1];
        uint32_t u32PayloadSize = pIpcRepHdr->u32PayloadSize;
        if (ipc_result != 0 ||
            u16CmdType != _SetAutoNightVision ||
            u32PayloadSize != sizeof(stNightModeRep)) { rc = -6; break; }

        if (pRep) {
            stChtIpcMsg *pIpcRepMsg = (stChtIpcMsg *)recv;
            memcpy(pRep, pIpcRepMsg->pu8Payload, sizeof(stNightModeRep));
        }
    } while (false);

    if (recv) {
        free(recv);
    }

    cht_ipc_msg_free(&pIpcReqMsg);

    return rc;
}

int cht_ipc_setPtzHome(const stSetPtzHomeReq *pReq, stSetPtzHomeRep *pRep)
{
    if (!pReq) return -1;

    int rc = 0;
    stChtIpcMsg pIpcReqMsg;
    uint8_t *recv = NULL;
    bool res = false;
    do {
        cht_ipc_msg_init(&pIpcReqMsg, ((cht_ipc_client_getMsgId() << 1) | 0), _SetPtzHome);
        pIpcReqMsg.stHdr.u32PayloadSize = sizeof(stSetPtzHomeReq);

        std::shared_ptr<nngipc::RequestHandler> rep_handler =
                        nngipc::RequestHandler::create(CHT_IPC_NAME);
        if (!rep_handler) { rc = -2; break; }

        res = rep_handler->append((const uint8_t *)&pIpcReqMsg, sizeof(stChtIpcHdr));
        if (!res) { rc = -3; break; }
        res = rep_handler->append((const uint8_t *)pReq, sizeof(stSetPtzHomeReq));
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
        if ( cht_ipc_msg_checkFourCC(pIpcRepHdr->u32FourCC) != 1 ||
             pIpcRepHdr->u32HdrSize < 3) {
            rc = -5; break;
        }
        int ipc_result = pIpcRepHdr->u16Headers[2];
        uint16_t u16CmdType = pIpcRepHdr->u16Headers[1];
        uint32_t u32PayloadSize = pIpcRepHdr->u32PayloadSize;
        if (ipc_result != 0 ||
            u16CmdType != _SetPtzHome ||
            u32PayloadSize != sizeof(stSetPtzHomeRep)) { rc = -6; break; }

        if (pRep) {
            stChtIpcMsg *pIpcRepMsg = (stChtIpcMsg *)recv;
            memcpy(pRep, pIpcRepMsg->pu8Payload, sizeof(stSetPtzHomeRep));
        }
    } while (false);

    if (recv) {
        free(recv);
    }

    cht_ipc_msg_free(&pIpcReqMsg);

    return rc;
}

int cht_ipc_gotoPtzHome(const stPtzMoveReq *pReq, stPtzMoveRep *pRep)
{
    if (!pReq) return -1;

    int rc = 0;
    stChtIpcMsg pIpcReqMsg;
    uint8_t *recv = NULL;
    bool res = false;
    do {
        cht_ipc_msg_init(&pIpcReqMsg, ((cht_ipc_client_getMsgId() << 1) | 0), _GotoPtzHome);
        pIpcReqMsg.stHdr.u32PayloadSize = sizeof(stPtzMoveReq);

        std::shared_ptr<nngipc::RequestHandler> rep_handler =
                        nngipc::RequestHandler::create(CHT_IPC_NAME);
        if (!rep_handler) { rc = -2; break; }

        res = rep_handler->append((const uint8_t *)&pIpcReqMsg, sizeof(stChtIpcHdr));
        if (!res) { rc = -3; break; }
        res = rep_handler->append((const uint8_t *)pReq, sizeof(stPtzMoveReq));
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
        if ( cht_ipc_msg_checkFourCC(pIpcRepHdr->u32FourCC) != 1 ||
             pIpcRepHdr->u32HdrSize < 3) {
            rc = -5; break;
        }
        int ipc_result = pIpcRepHdr->u16Headers[2];
        uint16_t u16CmdType = pIpcRepHdr->u16Headers[1];
        uint32_t u32PayloadSize = pIpcRepHdr->u32PayloadSize;
        if (ipc_result != 0 ||
            u16CmdType != _GotoPtzHome ||
            u32PayloadSize != sizeof(stPtzMoveRep)) { rc = -6; break; }

        if (pRep) {
            stChtIpcMsg *pIpcRepMsg = (stChtIpcMsg *)recv;
            memcpy(pRep, pIpcRepMsg->pu8Payload, sizeof(stPtzMoveRep));
        }
    } while (false);

    if (recv) {
        free(recv);
    }

    cht_ipc_msg_free(&pIpcReqMsg);

    return rc;
}

int cht_ipc_getDateTimeInfo(const stDateTimeInfoReq *pReq, stDateTimeInfoRep *pRep)
{
    if (!pReq) return -1;

    int rc = 0;
    stChtIpcMsg pIpcReqMsg;
    uint8_t *recv = NULL;
    bool res = false;
    do {
        cht_ipc_msg_init(&pIpcReqMsg, ((cht_ipc_client_getMsgId() << 1) | 0), _GetTimeZone);
        pIpcReqMsg.stHdr.u32PayloadSize = sizeof(stDateTimeInfoReq);

        std::shared_ptr<nngipc::RequestHandler> rep_handler =
                        nngipc::RequestHandler::create(CHT_IPC_NAME);
        if (!rep_handler) { rc = -2; break; }

        res = rep_handler->append((const uint8_t *)&pIpcReqMsg, sizeof(stChtIpcHdr));
        if (!res) { rc = -3; break; }
        res = rep_handler->append((const uint8_t *)pReq, sizeof(stDateTimeInfoReq));
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
        if ( cht_ipc_msg_checkFourCC(pIpcRepHdr->u32FourCC) != 1 ||
             pIpcRepHdr->u32HdrSize < 3) {
            rc = -5; break;
        }
        int ipc_result = pIpcRepHdr->u16Headers[2];
        uint16_t u16CmdType = pIpcRepHdr->u16Headers[1];
        uint32_t u32PayloadSize = pIpcRepHdr->u32PayloadSize;
        if (ipc_result != 0 ||
            u16CmdType != _GetTimeZone ||
            u32PayloadSize != sizeof(stDateTimeInfoRep)) { rc = -6; break; }

        if (pRep) {
            stChtIpcMsg *pIpcRepMsg = (stChtIpcMsg *)recv;
            memcpy(pRep, pIpcRepMsg->pu8Payload, sizeof(stDateTimeInfoRep));
        }
    } while (false);

    if (recv) {
        free(recv);
    }

    cht_ipc_msg_free(&pIpcReqMsg);

    return rc;
}

int cht_ipc_setDateTimeInfo(const stDateTimeInfoReq *pReq, stDateTimeInfoRep *pRep)
{
    if (!pReq) return -1;

    int rc = 0;
    stChtIpcMsg pIpcReqMsg;
    uint8_t *recv = NULL;
    bool res = false;
    do {
        cht_ipc_msg_init(&pIpcReqMsg, ((cht_ipc_client_getMsgId() << 1) | 0), _SetTimeZone);
        pIpcReqMsg.stHdr.u32PayloadSize = sizeof(stDateTimeInfoReq);

        std::shared_ptr<nngipc::RequestHandler> rep_handler =
                        nngipc::RequestHandler::create(CHT_IPC_NAME);
        if (!rep_handler) { rc = -2; break; }

        res = rep_handler->append((const uint8_t *)&pIpcReqMsg, sizeof(stChtIpcHdr));
        if (!res) { rc = -3; break; }
        res = rep_handler->append((const uint8_t *)pReq, sizeof(stDateTimeInfoReq));
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
        if ( cht_ipc_msg_checkFourCC(pIpcRepHdr->u32FourCC) != 1 ||
             pIpcRepHdr->u32HdrSize < 3) {
            rc = -5; break;
        }
        int ipc_result = pIpcRepHdr->u16Headers[2];
        uint16_t u16CmdType = pIpcRepHdr->u16Headers[1];
        uint32_t u32PayloadSize = pIpcRepHdr->u32PayloadSize;
        if (ipc_result != 0 ||
            u16CmdType != _SetTimeZone ||
            u32PayloadSize != sizeof(stDateTimeInfoRep)) { rc = -6; break; }

        if (pRep) {
            stChtIpcMsg *pIpcRepMsg = (stChtIpcMsg *)recv;
            memcpy(pRep, pIpcRepMsg->pu8Payload, sizeof(stDateTimeInfoRep));
        }
    } while (false);

    if (recv) {
        free(recv);
    }

    cht_ipc_msg_free(&pIpcReqMsg);

    return rc;
}
#endif
