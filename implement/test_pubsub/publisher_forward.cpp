#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "nngipc.h"

using namespace llt;

int main(void)
{
    std::shared_ptr<nngipc::PublishHandler> pub_handler =
            nngipc::PublishHandler::create("pubsub_proxy_front.sock", true);

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

