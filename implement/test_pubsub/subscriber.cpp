#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#if 1
#include "NngIpcSubscribeHandler.h"

using namespace llt;

void subscribe_callback(const uint8_t *req_payload, size_t req_len, uint8_t **res_payload, size_t *res_len)
{
    (void) res_payload;
    (void) res_len;

    printf("req_payload %p, req_len %ld\n", req_payload, req_len);
    if (req_payload) printf("s %s\n", req_payload);
}

int main(void)
{
    std::shared_ptr<nngipc::SubscribeHandler> sub_handler =
            nngipc::SubscribeHandler::create("test_pubsub.ipc", subscribe_callback);
printf("%s %d\n", __func__, __LINE__);
    if (!sub_handler) return -1;
printf("%s %d\n", __func__, __LINE__);
    if (!sub_handler->subscribe("buffer")) return -2;
printf("%s %d\n", __func__, __LINE__);
    if (!sub_handler->start()) return -3;
printf("%s %d\n", __func__, __LINE__);
    while(1) {
        sleep(2);
    }
    return 0;
}
#else

#include <nng/nng.h>
#include <nng/protocol/pubsub0/sub.h>
#include <stdio.h>
#include <string.h>

int main() {
    nng_socket sock;
    int rv;

    if ((rv = nng_sub0_open(&sock)) != 0) {
        fprintf(stderr, "nng_sub0_open: %s\n", nng_strerror(rv));
        return 1;
    }

    if ((rv = nng_dial(sock, "tcp://127.0.0.1:22343", NULL, 0)) != 0) {
        fprintf(stderr, "nng_dial: %s\n", nng_strerror(rv));
        return 1;
    }

    if ((rv = nng_sub0_socket_subscribe(sock, "", 0)) != 0) {
        fprintf(stderr, "nng_setopt: %s\n", nng_strerror(rv));
        return 1;
    }

    while (1) {
        char buf[128];
        size_t sz = sizeof(buf);
        if ((rv = nng_recv(sock, buf, &sz, 0)) != 0) {
            fprintf(stderr, "nng_recv: %s\n", nng_strerror(rv));
            continue;
        }
        printf("Received: %s\n", buf);
    }

    nng_close(sock);
    return 0;
}
#endif
