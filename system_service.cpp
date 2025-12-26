#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "NngIpcResponseHandler.h"

using namespace llt;

void request_callback(const uint8_t *req_payload, size_t req_len, uint8_t **res_payload, size_t *res_len)
{
}

int main(void)
{
    std::shared_ptr<nngipc::ResponseHandler> res_handler = nngipc::ResponseHandler::create(
        "system_service.ipc", 4, request_callback);

    while(1) {
        sleep(2);
    }
    return 0;
}
