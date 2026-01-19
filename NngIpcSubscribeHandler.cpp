#include <stdlib.h>
#include <string.h>

#include <string>

#include "NngIpcSubscribeHandler.h"
#include "utils.h"

#ifndef NNGIPC_DIR_PATH
#define NNGIPC_DIR_PATH "/tmp/nngipc"
#endif

namespace llt {
namespace nngipc {

static const uint32_t gc_maxWorkerNum = 1;

std::shared_ptr<SubscribeHandler> SubscribeHandler::create(const char *ipc_name, OutputCallback cb)
{
    if (!ipc_name || strlen(ipc_name) == 0) {
        return nullptr;
    }

    const auto& handler = std::shared_ptr<SubscribeHandler>(new SubscribeHandler(ipc_name, cb));
    if (!handler) {
        return nullptr;
    }

    if (!handler->init()) {
        return nullptr;
    }

    return handler;
}

SubscribeHandler::SubscribeHandler(const char *ipc_name, OutputCallback cb)
: m_ipcName{std::string(ipc_name)},
  m_init{false},
  m_workerNum{gc_maxWorkerNum},
  m_outputCB{cb}
{
    m_sock.id = 0;
}

SubscribeHandler::~SubscribeHandler()
{
    release();
}

bool SubscribeHandler::init(void)
{
    // create ipc folder
    const char *cmd[] = {"mkdir", "-p", NNGIPC_DIR_PATH, NULL};
    utils_runCmd(cmd);

    /*  Create the socket. */
    int rv = 0;
    if ((rv = nng_sub0_open(&m_sock)) != 0) {
        fprintf(stderr, "%s: %s\n", "nng_req0_open", nng_strerror(rv));
        return false;
    }

    for (uint32_t i = 0; i < m_workerNum; i++) {
        const auto& worker = AioWorker::create(m_sock, AioWorker::TYPE::Subscribe, m_outputCB);
        if (worker) m_workers.push_back(worker);
    }

    m_init = true;
    return true;
}

bool SubscribeHandler::subscribe(const std::string& subscribe_str)
{
    if (!m_init) return false;
    if (subscribe_str.empty()) return true;

    int rv = 0;
    if ((rv = nng_sub0_socket_subscribe(m_sock, subscribe_str.c_str(), subscribe_str.size())) != 0) {
        fprintf(stderr, "%s: %s\n", "nng_setopt NNG_OPT_SUB_SUBSCRIBE", nng_strerror(rv));
        return false;
    }

    for (const auto& worker : m_workers) {
        worker->subscribe(subscribe_str);
    }

    return true;
}

bool SubscribeHandler::unsubscribe(const std::string& subscribe_str)
{
    if (!m_init) return false;
    if (subscribe_str.empty()) return true;

    int rv = 0;
    if ((rv = nng_sub0_socket_unsubscribe(m_sock, subscribe_str.c_str(), subscribe_str.size())) != 0) {
        fprintf(stderr, "%s: %s\n", "nng_setopt NNG_OPT_SUB_SUBSCRIBE", nng_strerror(rv));
        return false;
    }

    for (const auto& worker : m_workers) {
        worker->unsubscribe(subscribe_str);
    }

    return true;
}

bool SubscribeHandler::start(void)
{
    if (!m_init) return false;

    std::string url = std::string("ipc://") + std::string(NNGIPC_DIR_PATH) + "/" + m_ipcName;
    url = "tcp://127.0.0.1:3328";
    int rv = 0;
    if ((rv = nng_dial(m_sock, url.c_str(), NULL, 0)) != 0) {
        fprintf(stderr, "%s: %s url %s\n", "nng_dial", nng_strerror(rv), url.c_str());
        return false;
    }

    for (const auto& worker : m_workers) {
        worker->start();
    }
    printf("%s %d url %s\n", __func__, __LINE__, url.c_str());

    return true;
}

bool SubscribeHandler::stop(void)
{
    for (const auto& worker : m_workers) {
        worker->stop();
    }

    return true;
}

bool SubscribeHandler::release(void)
{
    m_workers.clear();

    nng_close(m_sock);
    m_sock = NNG_SOCKET_INITIALIZER;

    m_init = false;
    return true;
}

} // namespace ipc
} // namespace llt
