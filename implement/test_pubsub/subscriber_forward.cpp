#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "nngipc.h"

using namespace llt;

void subscribe_callback(const uint8_t *req_payload, size_t req_len, uint8_t **res_payload, size_t *res_len)
{
    (void) res_payload;
    (void) res_len;

    printf("req_payload %p, req_len %ld\n", req_payload, req_len);
}

int main(void)
{
    std::shared_ptr<nngipc::SubscribeHandler> sub_handler =
            nngipc::SubscribeHandler::create("pubsub_proxy_back.sock", 1, subscribe_callback);

    if (!sub_handler) return -1;
    if (!sub_handler->subscribe("")) return -2;
    if (!sub_handler->start()) return -3;

    while(1) {
        sleep(2);
    }
    return 0;
}
