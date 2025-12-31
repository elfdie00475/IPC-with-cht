#include <stdlib.h>
#include <string.h>

#include <string>

#include "NngIpcRequestHandler.h"

#ifndef NNGIPC_DIR_PATH
#define NNGIPC_DIR_PATH "/tmp/nngipc"
#endif

namespace llt {
namespace nngipc {

std::shared_ptr<RequestHandler> RequestHandler::create(const char *ipc_name)
{
    if (!ipc_name || strlen(ipc_name) == 0) {
        return nullptr;
    }

    const auto& handler = std::shared_ptr<RequestHandler>(new RequestHandler(ipc_name));
    if (!handler) {
        return nullptr;
    }

    if (!handler->init()) {
        return nullptr;
    }

    return handler;
}

RequestHandler::RequestHandler(const char *ipc_name)
: m_ipcName{std::string(ipc_name)},
  m_msg{NULL},
  m_init{false}
{
    m_sock.id = 0;
}

RequestHandler::~RequestHandler()
{
    release();
}

bool RequestHandler::init(void)
{
    /*  Create the socket. */
    int rv = 0;
    if ((rv = nng_req0_open(&m_sock)) != 0) {
        fprintf(stderr, "%s: %s\n", "nng_req0_open", nng_strerror(rv));
        return false;
    }

    std::string url = std::string("ipc://") + std::string(NNGIPC_DIR_PATH) + "/" + m_ipcName;
    if ((rv = nng_dial(m_sock, url.c_str(), NULL, 0)) != 0) {
        fprintf(stderr, "%s: %s\n", "nng_dial", nng_strerror(rv));
        return false;
    }

    m_init = true;
    return true;
}

bool RequestHandler::append(const uint8_t *payload, size_t payload_len)
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

bool RequestHandler::send(void)
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

bool RequestHandler::recv(uint8_t **payload, size_t *payload_len)
{
    if (payload) *payload = NULL;
    if (payload_len) *payload_len = 0;

    int rv = 0;
	if ((rv = nng_recvmsg(m_sock, &m_msg, 0)) != 0) {
        fprintf(stderr, "%s: %s\n", "nng_recvmsg", nng_strerror(rv));
        return false;
	}

    size_t msglen = nng_msg_len(m_msg);
    uint8_t *pmsg = (uint8_t *)malloc(msglen);
    if (pmsg) {
        memcpy(pmsg, nng_msg_body(m_msg), msglen);
        if (payload) *payload = pmsg;
        if (payload_len) *payload_len = msglen;
    }

    if (m_msg) {
        nng_msg_free(m_msg);
        m_msg = NULL;
    }

    return true;
}

bool RequestHandler::release(void)
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
