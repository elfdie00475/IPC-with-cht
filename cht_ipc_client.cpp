#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "NngIpcRequestHandler.h"
#include "cht_ipc_client.h"

using namespace llt;

int cht_ipc_getCamStatusById(const stCamStatusByIdReq *pReq, stCamStatusByIdRep *pRep)
{
    if (!pReq || !pRep) return -1;

    memset((void *)pRep, 0, sizeof(stCamStatusByIdRep));

    std::shared_ptr<nngipc::RequestHandler> res_handler = nngipc::RequestHandler::create("system_service.ipc");
    if (!res_handler) return -2;

    bool res = res_handler->send((const uint8_t *)pReq, sizeof(stCamStatusByIdReq));
    if (!res) return -3;

    uint8_t *payload = NULL;
    size_t payload_len = 0;
    res = res_handler->recv(&payload, &payload_len);
    if (!res || !payload || payload_len != sizeof(stCamStatusByIdRep)) {
        if (payload) free(payload);
        return -4;
    }

    memcpy((void *)pRep, payload, payload_len);

    if (payload) free(payload);
    return 0;
}