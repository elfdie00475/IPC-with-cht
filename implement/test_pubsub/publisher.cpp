#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#if 1
#include "nngipc.h"

using namespace llt;

int main(void)
{
    std::shared_ptr<nngipc::PublishHandler> pub_handler =
            nngipc::PublishHandler::create("test_pubsub.ipc");

    if (!pub_handler) return -1;
    char buffer[128];
    int buffer_len = 128, msg_len = 0;

    while(1) {
        msg_len = snprintf(buffer, sizeof(buffer), "buffer test %d", rand());
        printf("msg_len %d %s\n", msg_len, buffer);
        pub_handler->append((uint8_t *)buffer, msg_len);
        printf("%s %d\n", __func__, __LINE__);
        pub_handler->send();
        printf("%s %d\n", __func__, __LINE__);
        sleep(2);
    }
    return 0;
}
#else

#include <nng/nng.h>
#include <nng/protocol/pubsub0/pub.h>
#include <stdio.h>
#include <string.h>

int main() {
    nng_socket sock;
    int rv;

    if ((rv = nng_pub0_open(&sock)) != 0) {
        fprintf(stderr, "nng_pub0_open: %s\n", nng_strerror(rv));
        return 1;
    }

    if ((rv = nng_listen(sock, "tcp://127.0.0.1:22343", NULL, 0)) != 0) {
        fprintf(stderr, "nng_listen: %s\n", nng_strerror(rv));
        return 1;
    }

    while (1) {
        char *msg = "Hello, world!";
        if ((rv = nng_send(sock, msg, strlen(msg) + 1, 0)) != 0) {
            fprintf(stderr, "nng_send: %s\n", nng_strerror(rv));
        }
        sleep(1);
    }

    nng_close(sock);
    return 0;
}
#endif
