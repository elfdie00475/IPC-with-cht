#include <stdlib.h>
#include <string.h>

#include <string>

#include "NngIpcPublishHandler.h"
#include "utils.h"

#ifndef NNGIPC_DIR_PATH
#define NNGIPC_DIR_PATH "/tmp/nngipc"
#endif

namespace llt {
namespace nngipc {

std::shared_ptr<PublishHandler> PublishHandler::create(const char *ipc_name, bool proxyMode)
{
    if (!ipc_name || strlen(ipc_name) == 0) {
        return nullptr;
    }

    const auto& handler = std::shared_ptr<PublishHandler>(new PublishHandler(ipc_name, proxyMode));
    if (!handler) {
        return nullptr;
    }

    if (!handler->init()) {
        return nullptr;
    }

    return handler;
}

PublishHandler::PublishHandler(const char *ipc_name, bool proxyMode)
: m_ipcName{std::string(ipc_name)},
  m_msg{NULL},
  m_init{false},
  m_proxyMode{proxyMode}
{
    m_sock.id = 0;
}

PublishHandler::~PublishHandler()
{
    release();
}

bool PublishHandler::init(void)
{
    // create ipc folder
    const char *cmd[] = {"mkdir", "-p", NNGIPC_DIR_PATH, NULL};
    utils_runCmd(cmd);

    /*  Create the socket. */
    int rv = 0;
    if ((rv = nng_pub0_open(&m_sock)) != 0) {
        fprintf(stderr, "%s: %s\n", "nng_pub0_open", nng_strerror(rv));
        return false;
    }

    std::string url = std::string("ipc://") + std::string(NNGIPC_DIR_PATH) + "/" + m_ipcName;
    url = "tcp://127.0.0.1:3327";
    if (m_proxyMode)
    {
        printf("%s %d nng_dial\n", __func__, __LINE__);
        if ((rv = nng_dial(m_sock, url.c_str(), NULL, 0)) != 0) {
            fprintf(stderr, "%s: %s\n", "nng_listen", nng_strerror(rv));
            return false;
        }
    }
    else
    {
        if ((rv = nng_listen(m_sock, url.c_str(), NULL, 0)) != 0) {
            fprintf(stderr, "%s: %s\n", "nng_listen", nng_strerror(rv));
            return false;
        }
    }
    printf("%s %d url %s\n", __func__, __LINE__, url.c_str());

    m_init = true;
    return true;
}

bool PublishHandler::append(const uint8_t *payload, size_t payload_len)
{
    if (!payload || payload_len == 0) return false;

    int rv = 0;
    if (!m_msg) {
        if ((rv = nng_msg_alloc(&m_msg, 0)) != 0) {
            fprintf(stderr, "%s: %s\n", "nng_msg_alloc", nng_strerror(rv));
            return false;
        }
    }
    if (!m_msg) return false;

    if ((rv = nng_msg_append(m_msg, payload, payload_len)) != 0) {
        fprintf(stderr, "%s: %s\n", "nng_msg_append", nng_strerror(rv));
        nng_msg_free(m_msg);
        m_msg = NULL;
        return false;
    }

    return true;
}

bool PublishHandler::send(void)
{
    if (!m_msg) return false;

    int rv = 0;
	if ((rv = nng_sendmsg(m_sock, m_msg, 0)) != 0) {
        fprintf(stderr, "%s: %s\n", "nng_sendmsg", nng_strerror(rv));
        nng_msg_free(m_msg);
        m_msg = NULL;
        return false;
	}

    m_msg = NULL;

    return true;
}

bool PublishHandler::release(void)
{
    if (m_msg) {
        nng_msg_free(m_msg);
        m_msg = NULL;
    }

    nng_close(m_sock);
    m_sock = NNG_SOCKET_INITIALIZER;

    m_init = false;
    return true;
}

} // namespace ipc
} // namespace llt
