/**
 * @file cht_p2p_camera_api.h
 * @brief CHT P2P Camera API - IP Camera與CHT P2P Agent SDK整合的API介面
 * @date 2025/04/29
 */

#ifndef CHT_P2P_CAMERA_API_H
#define CHT_P2P_CAMERA_API_H

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>

#include <zwsystem_ipc_client.h>

#include "cht_p2p_agent_c.h"
#include "cht_p2p_camera_command_handler.h"

/**
 * @brief CHT P2P Camera API類，IP Camera用此類調用CHT P2P Agent功能
 */
class ChtP2PCameraAPI
{
public:
    /**
     * @brief 建構函式
     */
    ChtP2PCameraAPI();

    /**
     * @brief 解構函式
     */
    ~ChtP2PCameraAPI();

    /**
     * @brief 初始化CHT P2P服務
     * @param camId 攝影機ID
     * @param chtBarcode CHT條碼(25~32碼)
     * @return 成功返回true，失敗返回false
     */
    bool initialize(void);

    /**
     * @brief 停止CHT P2P服務
     */
    void deinitialize(void);

    void commandDoneCallback(CHTP2P_CommandType type, void *handle,
            const char *payload, void *userParam);

    void controlCallback(CHTP2P_ControlType type, void *handle,
            const char *payload, void *userParam);

    void audioCallback(const char *data, size_t dataSize, const char *metadata, void *userParam);

public:
    int bindCamera(const BindCameraConfig& config);

    int cameraRegister(void);

    int checkHiOSSstatus(bool& hiOssStatus);

    int getHamiCameraInitialInfo(void);

    void addSystemEvent(eZwsystemSubSystemEventType eventType,
            const uint8_t *data, size_t dataSize);

private:
    struct SystemEvent {
        eZwsystemSubSystemEventType eventType;
        std::vector<uint8_t> data;
    };

    void eventWorkerThread(void);
    void processSystemEvent(const SystemEvent& event);
    bool isEventWorkerStopping(void);

private:
    bool m_initialized;

    std::queue<SystemEvent> m_eventQueue;
    std::mutex m_queueMutex;
    std::condition_variable m_queueCV;
    std::thread m_eventWorkerThread;
    std::atomic<bool> m_eventWorkerStopping;

};

#endif // CHT_P2P_CAMERA_API_H
