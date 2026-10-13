/**
 * @file cht_p2p_agent_c_stub.cpp
 * @brief CHT P2P Agent C API 的存根實現
 * @date 2025/04/29
 */

#include <arpa/inet.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <csignal>
#include <cstring>   // 建議用 <cstring> 取代 <string.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <mutex>
#include <sstream>
#include <thread>

#include "cht_p2p_agent_c.h"

// 獲取 wlan0 網卡的 IP 地址
std::string getWlan0IpAddress()
{
    struct ifaddrs *ifaddr, *ifa;
    std::string ipAddress = "192.168.1"; // 預設值

    if (getifaddrs(&ifaddr) == -1)
    {
        std::cerr << "getifaddrs 失敗" << std::endl;
        return ipAddress;
    }

    for (ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next)
    {
        if (ifa->ifa_addr == nullptr)
            continue;

        // 檢查是否為 wlan0 接口且為 IPv4
        if (strcmp(ifa->ifa_name, "wlan0") == 0 && ifa->ifa_addr->sa_family == AF_INET)
        {
            struct sockaddr_in *pAddr = (struct sockaddr_in *)ifa->ifa_addr;
            std::string fullIp = inet_ntoa(pAddr->sin_addr);

            // 提取前三段 IP (例如 192.168.1.100 -> 192.168.1)
            size_t lastDot = fullIp.find_last_of('.');
            if (lastDot != std::string::npos)
            {
                ipAddress = fullIp.substr(0, lastDot);
                std::cout << "取得 wlan0 IP 前三: " << ipAddress << std::endl;
            }
            break;
        }
    }

    freeifaddrs(ifaddr);
    return ipAddress;
}

// 從 hami_uid 檔案讀取 userId
std::string getUserIdFromHamiUid()
{
    std::ifstream file("/etc/config/hami_uid");
    if (file.is_open())
    {
        std::string userId;
        if (std::getline(file, userId))
        {
            // 去除換行符和空白
            userId.erase(userId.find_last_not_of("\r\n \t") + 1);
            if (!userId.empty())
            {
                std::cout << "從 hami_uid 讀取到 userId: " << userId << std::endl;
                return userId;
            }
        }
        file.close();
    }

    std::cout << "無法從 hami_uid 讀取，使用預設值" << std::endl;
    return "SIM_USER1001"; // 預設值
}

// 儲存全域配置，以便在不同函數中使用
static CHTP2P_Config g_config;
static bool g_initialized = false;
static std::atomic<bool> g_isShuttingDown(false);

// 模擬 CHT P2P Agent 函數的簡單實現
extern "C"
{

    int chtp2p_initialize(const CHTP2P_Config *config)
    {
        std::cout << "[CHT P2P Agent Stub] 初始化 P2P Agent，camId: "
                  << (config ? config->camId : "(null)") << std::endl;

        // 保存配置資訊到全域變數
        if (config)
        {
            g_config.camId = strdup(config->camId);
            g_config.chtBarcode = strdup(config->chtBarcode);
            g_config.commandDoneCallback = config->commandDoneCallback;
            g_config.controlCallback = config->controlCallback;
            g_config.audioCallback = config->audioCallback;
            g_config.userParam = config->userParam;
            g_initialized = true;
            std::cout << "[CHT P2P Agent Stub] 配置資訊已保存，回調函數: "
                      << (g_config.commandDoneCallback ? "已設置" : "未設置") << std::endl;
        }
        else
        {
            std::cerr << "[CHT P2P Agent Stub] 錯誤: 配置為空" << std::endl;
            return -1; // 配置為空，初始化失敗
        }

        return 0; // 成功
    }

    void chtp2p_deinitialize(void)
    {
        std::cout << "[CHT P2P Agent Stub] 停止 P2P Agent" << std::endl;

        // 設置關閉標志，這樣其他正在執行的操作會知道系統正在關閉
        g_isShuttingDown = true;

        // 等待一小段時間，讓進行中的操作有機會完成
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        // 釋放內存
        if (g_initialized)
        {
            free((void *)g_config.camId);
            free((void *)g_config.chtBarcode);
            g_initialized = false;
        }
    }

    int chtp2p_send_command(CHTP2P_CommandType commandType, void **commandHandle, const char *payload)
    {
        // 注意：這裡需要使用傳入的 commandHandle，不要生成新的
        // static int dummy_handle = 1;
        // *commandHandle = &dummy_handle;

        // 直接印傳入的 commandHandle
        std::cout << "[CHT P2P Agent Stub] 發送命令，類型: " << commandType
                  << ", 負載: " << (payload ? payload : "(null)")
                  << ", commandHandle: " << *commandHandle << std::endl;

        // 創建一個模擬的回應
        std::string responseStr;

        switch (commandType)
        {
        case _BindCameraReport:
            responseStr = "{"
                          "\"code\":0,"
                          "\"data\":{"
                          "\"camSid\":13,"
                          "\"camId\":\"" +
                          std::string(g_config.camId ? g_config.camId : "1234567890123456789012345") + "\","
                                                                                                       "\"chtBarcode\":\"" +
                          std::string(g_config.chtBarcode ? g_config.chtBarcode : "1234567890123456789012345") + "\","
                                                                                                                 "\"tenantId\":\"sim_tenant\","
                                                                                                                 "\"netNo\":\"SIM_NET202405\","
                                                                                                                 "\"userId\":\"" +
                          getUserIdFromHamiUid() + "\""
                                                                                                                 "},"
                                                                                                                 "\"description\":\"成功\""
                                                                                                                 "}";
            break;

        case _CameraRegister:
        {
            std::string baseIp = getWlan0IpAddress();
            responseStr = "{"
                          "\"code\":0,"
                          "\"data\":{"
                          "\"publicIp\":\"" +
                          baseIp + ".100\""
                          "},"
                          "\"description\":\"成功\""
                          "}";
        }
            break;

        case _CheckHiOSSstatus:
            responseStr = "{"
                          "\"code\":0,"
                          "\"data\":{"
                          "\"status\":true,"
                          "\"description\":\"HiOSS狀態正常\""
                          "},"
                          "\"description\":\"成功\""
                          "}";
            break;

        case _GetHamiCamInitialInfo:
            responseStr = "{"
                          "\"code\":0,"
                          "\"data\":{"
                          "\"hamiCamInfo\":{"
                          "\"camSid\":13,"
                          "\"camId\":\"" +
                          std::string(g_config.camId ? g_config.camId : "27E13A0931001004734") + "\","
                                                                                                 "\"chtBarcode\":\"" +
                          std::string(g_config.chtBarcode ? g_config.chtBarcode : "27E13A0931001004734") + "\","
                                                                                                           "\"tenantId\":\"sim_tenant\","
                                                                                                           "\"netNo\":\"SIM_NET202405\","
                                                                                                           "\"userId\":\"" +
                          getUserIdFromHamiUid() + "\""
                                                                                                           "},"
                                                                                                           "\"hamiSettings\":{"
                                                                                                           "\"nightMode\":\"1\","
                                                                                                           "\"autoNightVision\":\"1\","
                                                                                                           "\"statusIndicatorLight\":\"0\","
                                                                                                           "\"isFlipUpDown\":\"0\","
                                                                                                           "\"isHd\":\"0\","
                                                                                                           "\"flicker\":\"0\","
                                                                                                           "\"imageQuality\":\"2\","
                                                                                                           "\"isMicrophone\":\"1\","
                                                                                                           "\"microphoneSensitivity\":50,"
                                                                                                           "\"isSpeak\":\"1\","
                                                                                                           "\"speakVolume\":70,"
                                                                                                           "\"storageDay\":7,"
                                                                                                           "\"scheduleOn\":\"1\","
                                                                                                           "\"ScheduleSun\":\"0000-2359\","
                                                                                                           "\"scheduleMon\":\"0840-1730\","
                                                                                                           "\"scheduleTue\":\"0840-1730\","
                                                                                                           "\"scheduleWed\":\"0840-1730\","
                                                                                                           "\"scheduleThu\":\"0840-1730\","
                                                                                                           "\"scheduleFri\":\"0840-1730\","
                                                                                                           "\"scheduleSat\":\"0000-2359\","
                                                                                                           "\"eventStorageDay\":14,"
                                                                                                           "\"powerOn\":\"1\","
                                                                                                           "\"alertOn\":\"1\","
                                                                                                           "\"vmd\":\"1\","
                                                                                                           "\"ad\":\"1\","
                                                                                                           "\"power\":100,"
                                                                                                           "\"ptzStatus\":\"1\","
                                                                                                           "\"ptzSpeed\":\"5\","
                                                                                                           "\"ptzTourStayTime\":\"5\","
                                                                                                           "\"humanTracking\":\"1\","
                                                                                                           "\"petTracking\":\"1\""
                                                                                                           "},"
                                                                                                           "\"hamiAiSettings\":{"
                                                                                                           "\"vmdAlert\":\"1\","
                                                                                                           "\"humanAlert\":\"1\","
                                                                                                           "\"petAlert\":\"1\","
                                                                                                           "\"adAlert\":\"1\","
                                                                                                           "\"fenceAlert\":\"0\","
                                                                                                           "\"faceAlert\":\"1\","
                                                                                                           "\"fallAlert\":\"1\","
                                                                                                           "\"adBabyCryAlert\":\"1\","
                                                                                                           "\"adSpeechAlert\":\"0\","
                                                                                                           "\"adAlarmAlert\":\"1\","
                                                                                                           "\"adDogAlert\":\"1\","
                                                                                                           "\"adCatAlert\":\"1\","
                                                                                                           "\"vmdSen\":5,"
                                                                                                           "\"adSen\":200,"
                                                                                                           "\"humanSen\":1,"
                                                                                                           "\"faceSen\":1,"
                                                                                                           "\"fenceSen\":1,"
                                                                                                           "\"petSen\":2,"
                                                                                                           "\"adBabyCrySen\":1,"
                                                                                                           "\"adSpeechSen\":1,"
                                                                                                           "\"adAlarmSen\":1,"
                                                                                                           "\"adDogSen\":1,"
                                                                                                           "\"adCatSen\":1,"
                                                                                                           "\"fallSen\":1,"
                                                                                                           "\"fallTime\":1,"
                                                                                                           "\"identificationFeatures\":["
                                                                                                           "{"
                                                                                                           "\"id\":\"20250519123456\","
                                                                                                           "\"name\":\"模擬使用者\","
                                                                                                           "\"verifyLevel\":1,"
                                                                                                           "\"faceFeatures\":\"SIMULATED_BLOB_DATA\","
                                                                                                           "\"createTime\":\"2025/05/19_123456\","
                                                                                                           "\"updateTime\":\"2025/05/19_123456\""
                                                                                                           "}"
                                                                                                           "],"
                                                                                                           "\"fencePos1\":{\"x\":10,\"y\":10},"
                                                                                                           "\"fencePos2\":{\"x\":10,\"y\":90},"
                                                                                                           "\"fencePos3\":{\"x\":90,\"y\":90},"
                                                                                                           "\"fencePos4\":{\"x\":90,\"y\":10},"
                                                                                                           "\"fenceDir\":\"1\""
                                                                                                           "},"
                                                                                                           "\"hamiSystemSettings\":{"
                                                                                                           "\"otaDomainName\":\"ota.sim.example.com\","
                                                                                                           "\"otaQueryInterval\":3600,"
                                                                                                           "\"ntpServer\":\"tock.stdtime.gov.tw\","
                                                                                                           "\"bucketName\":\"sim-cht-p2p\""
                                                                                                           "}"
                                                                                                           "},"
                                                                                                           "\"description\":\"成功\""
                                                                                                           "}";
            break;

        case _Snapshot:
            responseStr = "{\"code\":0,\"description\":\"截圖事件回報成功\"}";
            break;

        case _Record:
            responseStr = "{\"code\":0,\"description\":\"錄影事件回報成功\"}";
            break;

        case _Recognition:
            responseStr = "{\"code\":0,\"description\":\"辨識事件回報成功\"}";
            break;

        case _StatusEvent:
            responseStr = "{\"code\":0,\"description\":\"辨識事件回報成功\"}";
            break;

        default:
            responseStr = "{\"code\":0,\"description\":\"命令執行成功\"}";
            break;
        }

        std::cout << "[CHT P2P Agent Stub] 準備回調，回應: " << responseStr << std::endl;

        // 模擬非同步調用命令完成回調
        // 在回調中使用相同的 commandHandle
        if (g_initialized && g_config.commandDoneCallback)
        {
            std::cout << "[CHT P2P Agent Stub] 啟動異步回調，使用 commandHandle: " << *commandHandle << std::endl;

            // 創建臨時變數用於回調
            char *responseCopy = strdup(responseStr.c_str());

            // 立即調用回調（在真實系統中可能是異步的）
            g_config.commandDoneCallback(commandType, *commandHandle, responseCopy, g_config.userParam);

            // 釋放複製的回應字串
            free(responseCopy);

            std::cout << "[CHT P2P Agent Stub] 回調已執行" << std::endl;
        }
        else
        {
            std::cerr << "[CHT P2P Agent Stub] 警告: 回調未執行，原因: "
                      << (!g_initialized ? "未初始化" : "回調函數未設置") << std::endl;
        }

        return 0; // 成功
    }

    int chtp2p_send_control_done(CHTP2P_ControlType controlType, void *controlHandle, const char *payload)
    {
        std::cout << "[CHT P2P Agent Stub] 發送控制完成，類型: " << controlType
                  << ", 負載: " << (payload ? payload : "(null)") << std::endl;
        return 0; // 成功
    }

    // note: new version by CHT spec 2025/07/05 remove datasize.
    int chtp2p_send_stream_data(const void *data, const char *metadata)
    {
        std::cout << "[CHT P2P Agent Stub] 發送串流數據, metadata 元數據: " << (metadata ? metadata : "(null)") << std::endl;
        return 0; // 成功
    }

} // extern "C"
