#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <nngipc.h>

#include "zwsystem_ipc_defined.h"

using namespace llt;

#ifndef ZWSYSTEM_IPC_STRING_SIZE
#define ZWSYSTEM_IPC_STRING_SIZE 256
#endif

void request_callback(const uint8_t *req_payload, size_t req_len, uint8_t **res_payload, size_t *res_len)
{

}

int main(void)
{
    std::shared_ptr<nngipc::ResponseHandler> res_handler =
        nngipc::ResponseHandler::create(ZWSYSTEM_IPC_NAME, 4, request_callback);
    if (!res_handler) return -1;
    if (!res_handler->start()) return -2;

    while(1) {
        sleep(2);
    }

    return 0;
}
