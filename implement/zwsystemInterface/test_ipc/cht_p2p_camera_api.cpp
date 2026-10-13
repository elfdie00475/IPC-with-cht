/**
 * @file cht_p2p_camera_api.cpp
 * @brief CHT P2P Camera API實現
 * @date 2025/04/29
 */

#include <atomic>
#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <csignal>
#include <fstream>
#include <future>
#include <iomanip>
#include <iostream>
#include <map>
#include <mutex>
#include <sstream>
#include <thread>
#include <queue>
#include <vector>

#include "cht_p2p_camera_api.h"
#include "camera_parameters_manager.h"
#include "cht_p2p_camera_command_handler.h"
#include "cht_p2p_camera_control_handler.h"
#include "cht_p2p_camera_streaming_handler.h"

// 內部輔助函數 - 格式化時間戳
static std::string getFormattedTimestamp(void)
{
    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&now_time_t), "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

// 內部輔助函數 - 調試輸出
static void printApiDebug(const std::string &message)
{
    std::cout << "[API-DEBUG " << getFormattedTimestamp() << "] " << message << std::endl;
    std::cout.flush();
}

// 內部輔助函數 - 步驟標題輸出
static void printApiStepHeader(const std::string &step)
{
    std::cout << "\n===== API: " << step << " =====" << std::endl;
    std::cout.flush();
}

ChtP2PCameraAPI::ChtP2PCameraAPI()
: m_initialized{false}
{
    initialize();
}

ChtP2PCameraAPI::~ChtP2PCameraAPI()
{
    deinitialize();
}

static void commandDoneCallbackWrapper(CHTP2P_CommandType type, void *handle,
        const char *payload, void *userParam)
{
    if (!userParam) return;
    auto *self = static_cast<ChtP2PCameraAPI *>(userParam);
    self->commandDoneCallback(type, handle, payload, nullptr);
}

static void controlCallbackWrapper(CHTP2P_ControlType type, void *handle,
        const char *payload, void *userParam)
{
    if (!userParam) return;
    auto *self = static_cast<ChtP2PCameraAPI *>(userParam);
    self->controlCallback(type, handle, payload, nullptr);
}

static void audioCallbackWrapper(const char *data, size_t dataSize, const char *metadata, void *userParam)
{
    if (!userParam) return;
    auto *self = static_cast<ChtP2PCameraAPI *>(userParam);
    self->audioCallback(data, dataSize, metadata, nullptr);
}

static void systemEventCallbackWrapper(void *userParam,
        eZwsystemSubSystemEventType eventType, const uint8_t *data, size_t dataSize)
{
    if (!userParam) return;
    auto *self = static_cast<ChtP2PCameraAPI *>(userParam);

    // if event about snapshot, record, recognition, statusEvent
    uint64_t now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()
            ).count();
    self->addSystemEvent(eventType, data, dataSize, now_ms);
}

void ChtP2PCameraAPI::addSystemEvent(eZwsystemSubSystemEventType eventType,
        const uint8_t *data, size_t dataSize, uint64_t nextRetryMs)
{
    if (dataSize != 0 && data == nullptr) {
        printApiDebug("drop system event with null payload");
        return;
    }

    SystemEvent eventInfo;
    eventInfo.eventType = eventType;
    if (dataSize != 0) {
        eventInfo.data.assign(data, data + dataSize);
    }
    eventInfo.nextRetryMs = nextRetryMs;

    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        m_eventQueue.push_back(std::move(eventInfo));
    }
    m_queueCV.notify_one();
}

bool ChtP2PCameraAPI::initialize(void)
{
    if (m_initialized)
    {
        std::cerr << "CHT P2P服務已經初始化" << std::endl;
        return false;
    }

    // 獲取參數管理器實例，但不再重新初始化
    auto &paramsManager = CameraParametersManager::getInstance();
    printApiDebug("使用已初始化的參數管理器");

    // 設置基本參數 - 可能重複設置，但不會重新初始化
    //paramsManager.setCameraId(camId);
    //paramsManager.setCHTBarcode(chtBarcode);
    const std::string& camId = paramsManager.getCameraId(); // from /proc/cameraId or random generate
    const std::string& barcode = paramsManager.getCHTBarcode(); // from /proc/chtBarcode

    paramsManager.setIsCheckHioss(false);
    paramsManager.setHiOssStatus(false);

    // 配置CHT P2P Agent
    CHTP2P_Config config{};
    config.camId = camId.c_str();
    config.chtBarcode = barcode.c_str();
    config.commandDoneCallback = commandDoneCallbackWrapper;
    config.controlCallback = controlCallbackWrapper;
    config.audioCallback = audioCallbackWrapper;
    config.userParam = this;

    // 初始化CHT P2P Agent
    int result = chtp2p_initialize(&config);
    if (result != 0)
    {
        std::cerr << "CHT P2P Agent初始化失敗，錯誤碼: " << result << std::endl;
        return false;
    }

    // subscribe system event
    result = zwsystem_sub_subscribeSystemEvent(systemEventCallbackWrapper, this);
    if (result != 0)
    {
        std::cerr << "Subscribe sytem event failed, error code: " << result << std::endl;
        chtp2p_deinitialize();
        return false;
    }

    // 訂閱完成後 callback 可能立刻送事件；必須先標示為可處理，再建立 worker。
    m_initialized = true;
    try {
        m_eventWorkerStopping = false;
        m_eventWorkerThread = std::thread(&ChtP2PCameraAPI::eventWorkerThread, this);
    } catch (const std::system_error& e) {
        std::cerr << "Create system event worker failed: " << e.what() << std::endl;
        m_initialized = false;
        zwsystem_sub_unsubscribeSystemEvent();
        chtp2p_deinitialize();
        return false;
    }

    std::cout << "CHT P2P Agent初始化成功" << std::endl;
    return true;
}

void ChtP2PCameraAPI::deinitialize(void)
{
    if (!m_initialized.exchange(false))
    {
        return;
    }

    // Unsubscribe first so no callback can enqueue an event after the worker
    // has exited and before CHT has been deinitialized.
    zwsystem_sub_unsubscribeSystemEvent();

    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        m_eventWorkerStopping = true;
    }
    m_queueCV.notify_all();

    if (m_eventWorkerThread.joinable()) {
        m_eventWorkerThread.join();
    }

    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        m_eventQueue.clear();
    }

    // 停止CHT P2P Agent
    chtp2p_deinitialize();

    std::cout << "CHT P2P Agent已停止" << std::endl;
}

int ChtP2PCameraAPI::bindCamera(const BindCameraConfig& config)
{
    if (!m_initialized) return -1;

    auto &cmdhandler = ChtP2PCameraCommandHandler::getInstance();
    return cmdhandler.bindCamera(config);
}

int ChtP2PCameraAPI::cameraRegister(void)
{
    if (!m_initialized) return -1;

    auto &cmdhandler = ChtP2PCameraCommandHandler::getInstance();
    return cmdhandler.cameraRegister();
}

int ChtP2PCameraAPI::checkHiOSSstatus(bool& hiOssStatus)
{
    if (!m_initialized) return -1;

    auto &cmdhandler = ChtP2PCameraCommandHandler::getInstance();
    return cmdhandler.checkHiOSSstatus(hiOssStatus);
}

int ChtP2PCameraAPI::getHamiCameraInitialInfo(void)
{
    if (!m_initialized) return -1;

    auto &cmdhandler = ChtP2PCameraCommandHandler::getInstance();
    return cmdhandler.getHamiCameraInitialInfo();
}

void ChtP2PCameraAPI::commandDoneCallback(CHTP2P_CommandType type, void *handle,
        const char *payload, void *userParam)
{
    auto &cmdhandler = ChtP2PCameraCommandHandler::getInstance();
    cmdhandler.commandDoneCallback(type, handle, payload, nullptr);
}

void ChtP2PCameraAPI::controlCallback(CHTP2P_ControlType type, void *handle,
        const char *payload, void *userParam)
{
    auto &ctrlhandler = ChtP2PCameraControlHandler::getInstance();
    ctrlhandler.controlCallback(type, handle, payload, nullptr);
}

void ChtP2PCameraAPI::audioCallback(const char *data, size_t dataSize, const char *metadata, void *userParam)
{
    auto &streaminghandler = ChtP2PCameraStreamingHandler::getInstance();
    streaminghandler.audioCallback(data, dataSize, metadata, userParam);
}

bool ChtP2PCameraAPI::isEventWorkerStopping(void)
{
    std::lock_guard<std::mutex> lock(m_queueMutex);
    return m_eventWorkerStopping;
}

void ChtP2PCameraAPI::eventWorkerThread(void)
{
    printApiDebug("eventWorkerThread is started");

    for (;;) {
        std::unique_lock<std::mutex> lock(m_queueMutex);

        m_queueCV.wait(lock, [this] {
            return m_eventWorkerStopping || !m_eventQueue.empty();
        });

        if (m_eventWorkerStopping) {
            break;
        }

        const uint64_t nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
        const auto due = std::find_if(m_eventQueue.begin(), m_eventQueue.end(),
                [nowMs](const SystemEvent& event) { return event.nextRetryMs <= nowMs; });
        if (due == m_eventQueue.end()) {
            const auto next = std::min_element(m_eventQueue.begin(), m_eventQueue.end(),
                    [](const SystemEvent& lhs, const SystemEvent& rhs) {
                        return lhs.nextRetryMs < rhs.nextRetryMs;
                    });
            m_queueCV.wait_for(lock,
                    std::chrono::milliseconds(next->nextRetryMs - nowMs));
            continue;
        }

        SystemEvent event = std::move(*due);
        m_eventQueue.erase(due);

        // update queue in local file
        // remove this event local file
        // add all event in queue into local

        lock.unlock();

        // 在隊列鎖外處理事件
        processSystemEvent(event);
    }

    printApiDebug("eventWorkerThread is stopped");
}

static int dispatchEvent(ChtP2PCameraCommandHandler& handler,
        eZwsystemSubSystemEventType t, const uint8_t *data, size_t dataSize)
{
    switch (t) {
        case eSystemEventType_Snapshot:    return handler.reportSnapshot(data, dataSize);
        case eSystemEventType_Record:      return handler.reportRecord(data, dataSize);
        case eSystemEventType_Recognition: return handler.reportRecognition(data, dataSize);
        case eSystemEventType_StatusEvent: return handler.reportStatusEvent(data, dataSize);
        default:
            printApiDebug("Unknown system event type received");
            break;
    }

    return 0;
}

void ChtP2PCameraAPI::processSystemEvent(const SystemEvent& event)
{
    if (!m_initialized.load()) return ;

    eZwsystemSubSystemEventType eventType = event.eventType;
    const uint8_t *data = event.data.data();
    size_t dataSize = event.data.size();

    // if event about snapshot/record/recognition/statusEvent, call command handler to process by function
    auto &cmdhandler = ChtP2PCameraCommandHandler::getInstance();
    int rc = dispatchEvent(cmdhandler, eventType, data, dataSize);
    if (rc == 0 || rc == REPORT_EVENT_NOT_RETRY) return ;

    printApiDebug("dispatchEvent failed, rc=" + std::to_string(rc));

    // maybe let this event retry later?
    // and save to local storage?
    // next time when camera boots up, resend these failed events
    uint64_t next_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();
    next_ms += 300000ull;
    addSystemEvent(eventType, data, dataSize, next_ms);
}
