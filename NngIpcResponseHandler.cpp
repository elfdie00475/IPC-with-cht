#include <stdlib.h>
#include <string.h>

#include <string>

#include "NngIpcResponseHandler.h"
#include "NngIpcPath.h"
#include "utils.h"

#ifndef NNGIPC_DIR_PATH
#define NNGIPC_DIR_PATH "/tmp/nngipc/"
#endif

namespace llt {
namespace nngipc {

static const uint32_t gc_maxWorkerNum = 8;

std::shared_ptr<ResponseHandler> ResponseHandler::create(
    const char *ipc_name, uint32_t worker_num, 
    OutputCallback cb, void *cb_param)
{
    if (!ipc_name || strlen(ipc_name) == 0) {
        return nullptr;
    }

    if (worker_num == 0) worker_num = 1;
    else if (worker_num > gc_maxWorkerNum) worker_num = gc_maxWorkerNum;

    const auto& handler = std::shared_ptr<ResponseHandler>(
            new ResponseHandler(ipc_name, worker_num, cb, cb_param));
    if (!handler) {
        return nullptr;
    }

    if (!handler->init()) {
        return nullptr;
    }

    return handler;
}

ResponseHandler::ResponseHandler(
    const char *ipc_name, uint32_t worker_num, 
    OutputCallback cb, void *cb_param)
: m_ipcName{std::string(ipc_name)},
  m_init{false},
  m_workerNum{worker_num},
  m_outputCB{cb},
  m_outputCBParam{cb_param}
{
    m_sock.id = 0;
    m_workers.reserve(worker_num);
}

ResponseHandler::~ResponseHandler()
{
    stop();
    release();
}

bool ResponseHandler::init(void)
{
    std::string ipcDirPath;
    if (!normalizeIpcDirectoryPath(NNGIPC_DIR_PATH, &ipcDirPath)) {
        fprintf(stderr, "NNGIPC_DIR_PATH must not be empty\n");
        return false;
    }

    // create ipc folder
    const char *cmd[] = {"mkdir", "-p", ipcDirPath.c_str(), NULL};
    utils_runCmd(cmd);

    std::lock_guard<std::mutex> lock(m_mutex);

    /*  Create the socket. */
    int rv = 0;
    if ((rv = nng_rep0_open(&m_sock)) != 0) {
        fprintf(stderr, "%s: %s\n", "nng_rep0_open", nng_strerror(rv));
        return false;
    }

    for (uint32_t i = 0; i < m_workerNum; i++) {
        const auto& worker = AioWorker::create(
                m_sock, AioWorker::TYPE::Response, 
                m_outputCB, m_outputCBParam);
        if (worker) m_workers.push_back(worker);
    }

    if (m_workers.empty()) {
        // 至少需要一個 receive context；部分 worker 建立成功仍可服務。
        m_workers.clear();
        nng_close(m_sock);
        m_sock = NNG_SOCKET_INITIALIZER;
        return false;
    }

    m_init = true;
    return true;
}

bool ResponseHandler::start(void)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_init) return false;

    // start() is a separate use site, so normalize again before URL creation.
    std::string ipcDirPath;
    if (!normalizeIpcDirectoryPath(NNGIPC_DIR_PATH, &ipcDirPath)) {
        fprintf(stderr, "NNGIPC_DIR_PATH must not be empty\n");
        return false;
    }
    std::string url = std::string("ipc://") + ipcDirPath + m_ipcName;
    int rv = 0;
    if ((rv = nng_listen(m_sock, url.c_str(), NULL, 0)) != 0) {
        fprintf(stderr, "%s: %s url %s\n", "nng_listen", nng_strerror(rv), url.c_str());
        return false;
    }

    for (const auto& worker : m_workers) {
        worker->start();
    }

    return true;
}

bool ResponseHandler::stop(void)
{
    std::vector<std::shared_ptr<AioWorker>> workers;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        workers = m_workers;
    }
    for (const auto& worker : workers) {
        worker->stop();
    }

    return true;
}

bool ResponseHandler::release(void)
{
    std::vector<std::shared_ptr<AioWorker>> workers;
    nng_socket socket = NNG_SOCKET_INITIALIZER;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        workers = std::move(m_workers);
        socket = m_sock;
        m_sock = NNG_SOCKET_INITIALIZER;
        m_init = false;
    }
    // Worker joins and socket shutdown can block; keep them outside m_mutex.
    workers.clear();
    if (socket.id != 0) nng_close(socket);
    return true;
}

} // namespace ipc
} // namespace llt
