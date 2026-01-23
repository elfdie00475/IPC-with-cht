/**
 * @file cht_p2p_camera_api.cpp
 * @brief CHT P2P Camera API實現
 * @date 2025/04/29
 */

#include <atomic>
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

// ==============================
// Custom / Project headers
// ==============================
#include "cht_p2p_camera_api.h"
#include "camera_parameters_manager.h"
#include "cht_p2p_camera_command_handler.h"
#include "cht_p2p_camera_control_handler.h"

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

    // 配置CHT P2P Agent
    CHTP2P_Config config;
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

    m_initialized = true;
    std::cout << "CHT P2P Agent初始化成功" << std::endl;
    return true;
}

void ChtP2PCameraAPI::deinitialize(void)
{
    if (!m_initialized)
    {
        return;
    }

    // 停止CHT P2P Agent
    chtp2p_deinitialize();

    m_initialized = false;
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

}

void ChtP2PCameraAPI::audioCallback(const char *data, size_t dataSize, const char *metadata, void *userParam)
{

}
