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

std::shared_ptr<SubscribeHandler> SubscribeHandler::create(
    const char *ipc_name, uint32_t worker_num, 
    OutputCallback cb, void *cb_param)
{
    if (!ipc_name || strlen(ipc_name) == 0) {
        return nullptr;
    }

    if (worker_num == 0) worker_num = 1;
    else if (worker_num > gc_maxWorkerNum) worker_num = gc_maxWorkerNum;

    const auto& handler = std::shared_ptr<SubscribeHandler>(
            new SubscribeHandler(ipc_name, worker_num, cb, cb_param));
    if (!handler) {
        return nullptr;
    }

    if (!handler->init()) {
        return nullptr;
    }

    return handler;
}

SubscribeHandler::SubscribeHandler(
    const char *ipc_name, uint32_t worker_num, 
    OutputCallback cb, void *cb_param)
: m_ipcName{std::string(ipc_name)},
  m_init{false},
  m_workerNum{worker_num},
  m_outputCB{cb},
  m_outputCBParam{cb_param},
  m_subscribeIdx{0}
{
    m_sock.id = 0;
    m_workers.reserve(worker_num);
}

SubscribeHandler::~SubscribeHandler()
{
    stop();
    release();
}

bool SubscribeHandler::init(void)
{
    // create ipc folder
    const char *cmd[] = {"mkdir", "-p", NNGIPC_DIR_PATH, NULL};
    utils_runCmd(cmd);

    std::lock_guard<std::mutex> lock(m_mutex);

    /*  Create the socket. */
    int rv = 0;
    if ((rv = nng_sub0_open(&m_sock)) != 0) {
        fprintf(stderr, "%s: %s\n", "nng_req0_open", nng_strerror(rv));
        return false;
    }

    for (uint32_t i = 0; i < m_workerNum; i++) {
        const auto& worker = AioWorker::create(
                m_sock, AioWorker::TYPE::Subscribe, 
                m_outputCB, m_outputCBParam);
        if (worker) m_workers.push_back(worker);
    }

    m_init = true;
    return true;
}

bool SubscribeHandler::subscribe(const std::string& subscribe_str)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_init) return false;

    int rv = 0;
    if ((rv = nng_sub0_socket_subscribe(m_sock, subscribe_str.c_str(), subscribe_str.size())) != 0) {
        fprintf(stderr, "%s: %s\n", "nng_setopt NNG_OPT_SUB_SUBSCRIBE", nng_strerror(rv));
        return false;
    }


    uint32_t idx = m_subscribeIdx;
    m_workers[idx]->subscribe(subscribe_str);
    m_subscribeIdx = (m_subscribeIdx+1) % m_workerNum;

    return true;
}

bool SubscribeHandler::unsubscribe(const std::string& subscribe_str)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_init) return false;

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
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_init) return false;

    std::string url = std::string("ipc://") + std::string(NNGIPC_DIR_PATH) + "/" + m_ipcName;
    int rv = 0;
    if ((rv = nng_dial(m_sock, url.c_str(), NULL, 0)) != 0) {
        fprintf(stderr, "%s: %s url %s\n", "nng_dial", nng_strerror(rv), url.c_str());
        return false;
    }

    for (const auto& worker : m_workers) {
        worker->start();
    }

    return true;
}

bool SubscribeHandler::stop(void)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    for (const auto& worker : m_workers) {
        worker->stop();
    }

    return true;
}

bool SubscribeHandler::release(void)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    m_workers.clear();

    nng_close(m_sock);
    m_sock = NNG_SOCKET_INITIALIZER;

    m_init = false;
    return true;
}

} // namespace ipc
} // namespace llt
