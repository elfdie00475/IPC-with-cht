/**
 * @file cht_p2p_camera_example_enhanced_menu.cpp
 * @brief CHT P2P Camera API 增強版互動測試選單 - 包含所有控制函數與回報機制
 * @date 2025/04/29
 */

#include "cht_p2p_agent_c.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <random>
#include <csignal>
#include <sstream>
#include <iomanip>
#include <map>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <fstream>
#include <vector>
#include <memory>
#include <cstdlib>
#include <algorithm>
#include <regex>
#include "cht_p2p_camera_api.h"
#include "cht_p2p_camera_control_handler.h"
#include "camera_parameters_manager.h"
//#include "camera_driver.h"
//#include "stream_manager.h"
//#include "ReportManager.h"
//#include "cht_p2p_agent_payload_defined.h"
#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>
#include "timezone_utils.h"

// ===== RapidJSON 使用聲明 =====
using rapidjson::Document;
using rapidjson::kObjectType;
using rapidjson::StringRef;
using rapidjson::Value;

// ===== 全域變數 =====
std::atomic<bool> g_running(true);

// 線程計數與同步機制
std::atomic<int> g_activeThreads(0);
std::condition_variable g_allThreadsCompletedCV;
std::mutex g_allThreadsCompletedMutex;

// 訊號計數
std::atomic<int> g_signalCount(0);

// 定時回報管理器實例
std::unique_ptr<ReportManager> g_reportManager;

// 格式化並打印時間戳
std::string getFormattedTimestamp()
{
    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&now_time_t), "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

// 調試輔助函數
void printDebug(const std::string &message)
{
    std::cout << "[DEBUG] " << message << std::endl;
    std::cout.flush();
}

void printStepHeader(const std::string &step)
{
    std::cout << "\n===== " << step << " =====" << std::endl;
    std::cout.flush();
}

// ===== 測試模式IP管理 =====
static std::string g_testServerIP = "172.50.1.60";  // 預設測試伺服器IP

// IP地址格式驗證
bool isValidIPAddress(const std::string& ip) {
    std::regex ipRegex(
        "^((25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\\.){3}"
        "(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)$"
    );
    return std::regex_match(ip, ipRegex);
}

// 設定測試伺服器IP
void setTestServerIP() {
    std::cout << "\n===== 設定測試伺服器IP =====" << std::endl;
    std::cout << "目前測試伺服器IP: " << g_testServerIP << std::endl;
    std::cout << "請輸入新的IP地址 (Enter保持不變): ";
    
    std::string newIP;
    std::getline(std::cin, newIP);
    
    if (newIP.empty()) {
        std::cout << "IP地址未變更" << std::endl;
        return;
    }
    
    // 驗證IP格式
    if (!isValidIPAddress(newIP)) {
        std::cout << "✗ IP地址格式錯誤: " << newIP << std::endl;
        std::cout << "  請使用正確格式 (例如: 192.168.1.100)" << std::endl;
        return;
    }
    
    g_testServerIP = newIP;
    std::cout << "✓ 測試伺服器IP已更新為: " << g_testServerIP << std::endl;
    
    // 儲存到參數管理器供後續使用
    auto &paramsManager = CameraParametersManager::getInstance();
    paramsManager.setParameter("testServerIP", g_testServerIP);
}

// 取得測試伺服器IP
std::string getTestServerIP() {
    // 首次執行時從參數管理器讀取已儲存的設定
    static bool initialized = false;
    if (!initialized) {
        auto &paramsManager = CameraParametersManager::getInstance();
        std::string savedIP = paramsManager.getParameter("testServerIP", "");
        if (!savedIP.empty() && isValidIPAddress(savedIP)) {
            g_testServerIP = savedIP;
        }
        initialized = true;
    }
    return g_testServerIP;
}

void printConfig(const std::string &path)
{
    std::cout << "正在檢查配置文件 " << path << " 的內容..." << std::endl;
    std::ifstream configFile(path);
    if (configFile.is_open())
    {
        std::string line;
        while (std::getline(configFile, line))
        {
            std::cout << line << std::endl;
        }
        configFile.close();
    }
    else
    {
        std::cerr << "無法打開配置文件進行讀取檢查: " << path << std::endl;
    }
    std::cout.flush(); // 確保立即輸出
}

void startTimeoutWatchdog()
{
    // 創建一個守護線程監控程序執行時間
    std::thread([&]()
                {
        std::this_thread::sleep_for(std::chrono::minutes(5)); // 5分鐘超時
        if (g_running) {
            std::cerr << "程序執行超時，強制退出" << std::endl;
            exit(1);
        } })
        .detach();
}

void addDebugLog(const std::string &message)
{
    std::cout << "[" << getFormattedTimestamp() << "] DEBUG: " << message << std::endl;
    std::cout.flush(); // 確保立即輸出
    // 可以選擇將調試日誌寫入文件
    // std::ofstream logFile("/tmp/cht_debug.log", std::ios::app);
    // logFile << "[" << getFormattedTimestamp() << "] " << message << std::endl;
}

// 退出處理函數
void cleanupResources()
{
    std::cout << "執行資源清理..." << std::endl;
    if (g_reportManager)
    {
        g_reportManager->stop();
        g_reportManager.reset();
    }
}

// 信號處理函數
void signalHandler(int signal)
{
    int currentCount = ++g_signalCount;
    std::cout << "收到信號 " << signal << "，準備退出程序 (第 " << currentCount << " 次)" << std::endl;

    if (currentCount == 1)
    {
        g_running = false;
    }
    else if (currentCount >= 3)
    {
        std::cout << "多次收到退出信號，強制終止程序" << std::endl;
        exit(1);
    }
}

// 回調函數實現
void onInitialInfo(const std::string &hamiCamInfo,
                   const std::string &hamiSettings,
                   const std::string &hamiAiSettings,
                   const std::string &hamiSystemSettings)
{
    std::cout << "\n===== 收到 GetHamiCamInitialInfo 回調 =====" << std::endl;
    std::cout << "開始處理完整的初始化參數..." << std::endl;

    // 輸出接收到的JSON大小用於調試
    std::cout << "接收到的JSON參數大小:" << std::endl;
    std::cout << "  - hamiCamInfo: " << hamiCamInfo.length() << " 字元" << std::endl;
    std::cout << "  - hamiSettings: " << hamiSettings.length() << " 字元" << std::endl;
    std::cout << "  - hamiAiSettings: " << hamiAiSettings.length() << " 字元" << std::endl;
    std::cout << "  - hamiSystemSettings: " << hamiSystemSettings.length() << " 字元" << std::endl;

    // 可選：輸出完整JSON用於調試（僅在需要時開啟）
    bool enableVerboseLog = false; // 設為 true 可查看完整JSON內容
    if (enableVerboseLog)
    {
        std::cout << "\n--- 完整JSON內容 ---" << std::endl;
        std::cout << "hamiCamInfo: " << hamiCamInfo << std::endl;
        std::cout << "hamiSettings: " << hamiSettings << std::endl;
        std::cout << "hamiAiSettings: " << hamiAiSettings << std::endl;
        std::cout << "hamiSystemSettings: " << hamiSystemSettings << std::endl;
        std::cout << "--- JSON內容結束 ---\n"
                  << std::endl;
    }

    auto &paramsManager = CameraParametersManager::getInstance();

    try
    {
        // 使用新的完整解析方法
        bool parseResult = paramsManager.parseAndSaveInitialInfoWithSync(
            hamiCamInfo, hamiSettings, hamiAiSettings, hamiSystemSettings);

        if (parseResult)
        {
            std::cout << "✓ 完整初始化參數處理成功" << std::endl;

            // 顯示解析後的關鍵參數
            std::cout << "\n===== 解析後的關鍵參數 =====" << std::endl;

            // hamiCamInfo 參數
            std::cout << "[hamiCamInfo]" << std::endl;
            std::cout << "  - Camera ID: " << paramsManager.getCameraId() << std::endl;
            std::cout << "  - CHT Barcode: " << paramsManager.getCHTBarcode() << std::endl;
            std::cout << "  - Cam SID: " << paramsManager.getCamSid() << std::endl;
            std::cout << "  - Tenant ID: " << paramsManager.getTenantId() << std::endl;
            std::cout << "  - Net No: " << paramsManager.getNetNo() << std::endl;
            std::cout << "  - User ID: " << paramsManager.getUserId() << std::endl;

            // hamiSettings 關鍵參數
            std::cout << "\n[hamiSettings]" << std::endl;
            std::cout << "  - Night Mode: " << paramsManager.getNightMode() << std::endl;
            std::cout << "  - Auto Night Vision: " << paramsManager.getAutoNightVision() << std::endl;
            std::cout << "  - HD Mode: " << paramsManager.getIsHd() << std::endl;
            std::cout << "  - Image Quality: " << paramsManager.getImageQualityStr() << std::endl;
            std::cout << "  - Microphone: " << paramsManager.getIsMicrophone() << std::endl;
            std::cout << "  - Speaker: " << paramsManager.getIsSpeak() << std::endl;
            std::cout << "  - Storage Days: " << paramsManager.getStorageDay() << std::endl;
            std::cout << "  - Event Storage Days: " << paramsManager.getEventStorageDay() << std::endl;
            std::cout << "  - PTZ Status: " << paramsManager.getPtzStatus() << std::endl;
            std::cout << "  - Human Tracking: " << paramsManager.getHumanTracking() << std::endl;
            std::cout << "  - Pet Tracking: " << paramsManager.getPetTracking() << std::endl;

            // hamiAiSettings 關鍵參數
            std::cout << "\n[hamiAiSettings]" << std::endl;
            std::cout << "  - VMD Alert: " << paramsManager.getVmdAlert() << std::endl;
            std::cout << "  - Human Alert: " << paramsManager.getHumanAlert() << std::endl;
            std::cout << "  - Pet Alert: " << paramsManager.getPetAlert() << std::endl;
            std::cout << "  - Face Alert: " << paramsManager.getFaceAlert() << std::endl;
            std::cout << "  - VMD Sensitivity: " << paramsManager.getVmdSen() << std::endl;
            std::cout << "  - Human Sensitivity: " << paramsManager.getHumanSen() << std::endl;

            // 顯示人臉識別特徵數量
            auto features = paramsManager.getIdentificationFeatures();
            std::cout << "  - Identification Features: " << features.size() << " 筆資料" << std::endl;
            if (!features.empty())
            {
                std::cout << "    範例: ID=" << features[0].id << ", 姓名=" << features[0].name
                          << ", 驗證等級=" << features[0].verifyLevel << std::endl;
            }

            // 顯示電子圍籬座標
            auto pos1 = paramsManager.getFencePos1();
            auto pos2 = paramsManager.getFencePos2();
            std::cout << "  - Fence Pos1: (" << pos1.first << "," << pos1.second << ")" << std::endl;
            std::cout << "  - Fence Pos2: (" << pos2.first << "," << pos2.second << ")" << std::endl;
            std::cout << "  - Fence Direction: " << paramsManager.getFenceDir() << std::endl;

            // hamiSystemSettings 參數
            std::cout << "\n[hamiSystemSettings]" << std::endl;
            std::cout << "  - OTA Domain: " << paramsManager.getOtaDomainName() << std::endl;
            std::cout << "  - OTA Query Interval: " << paramsManager.getOtaQueryInterval() << " 秒" << std::endl;
            std::cout << "  - NTP Server: " << paramsManager.getNtpServer() << std::endl;
            std::cout << "  - Bucket Name: " << paramsManager.getBucketName() << std::endl;

            // 系統狀態
            std::cout << "\n[系統狀態]" << std::endl;
            std::cout << "  - Active Status: " << paramsManager.getActiveStatus() << std::endl;
            std::cout << "  - Device Status: " << paramsManager.getDeviceStatus() << std::endl;
            std::cout << "  - Time Zone: " << paramsManager.getTimeZone() << std::endl;
            std::cout << "  - Camera Name: " << paramsManager.getCameraName() << std::endl;

            std::cout << "\n===== 參數解析完成 =====" << std::endl;

            // 生成參數統計報告
            auto allParams = paramsManager.getAllParameters();
            std::cout << "\n總計儲存了 " << allParams.size() << " 個參數" << std::endl;

            // 可選：列出所有參數名稱（用於調試）
            bool showAllParamNames = false; // 設為 true 可查看所有參數名稱
            if (showAllParamNames)
            {
                std::cout << "\n--- 所有參數名稱列表 ---" << std::endl;
                int count = 0;
                for (const auto &param : allParams)
                {
                    std::cout << "  " << (++count) << ". " << param.first << " = " << param.second << std::endl;
                }
                std::cout << "--- 參數列表結束 ---" << std::endl;
            }
        }
        else
        {
            std::cerr << "✗ 初始化參數處理失敗" << std::endl;
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << "✗ 處理初始化參數時發生異常: " << e.what() << std::endl;
    }
    catch (...)
    {
        std::cerr << "✗ 處理初始化參數時發生未知異常" << std::endl;
    }

    std::cout << "===== GetHamiCamInitialInfo 回調處理完成 =====" << std::endl;
    std::cout.flush();
}

std::string onControl(CHTP2P_ControlType controlType, const std::string &payload)
{
    std::cout << "收到控制命令: controlType=" << controlType << ", payload=" << payload << std::endl;
    std::cout.flush();
#if 1
    // **直接使用現有的控制處理器（包含HiOSS檢查）**
    std::string result = ChtP2PCameraControlHandler::getInstance().handleControl(controlType, payload);

    // **如果是解綁指令，需要呼叫 chtp2p_send_control_done 回傳結果**
    if (controlType == _DeleteCameraInfo)
    {
        std::cout << "解綁指令處理完成，準備回傳結果給P2P Agent" << std::endl;
        // 注意：這裡需要適當的controlHandle，在實際實作中應該從回調參數取得
        // int chtp2p_result = chtp2p_send_control_done(_DeleteCameraInfo, controlHandle, result.c_str());
        // std::cout << "chtp2p_send_control_done 結果: " << chtp2p_result << std::endl;
    }

    return result;
#else
    // 使用控制處理器處理控制命令
    return ChtP2PCameraControlHandler::getInstance().handleControl(controlType, payload);
#endif
}

void onAudioData(const char *data, size_t dataSize, const std::string &metadata)
{
    std::cout << "收到音頻數據: dataSize=" << dataSize << ", metadata=" << metadata << std::endl;
    std::cout.flush();
}

// ===== 基本狀態與管理測試函數 =====

bool testGetCamStatusById(ChtP2PCameraAPI &cameraApi)
{
    std::cout << "\n===== 測試獲取攝影機狀態 =====" << std::endl;
    auto &paramsManager = CameraParametersManager::getInstance();

    std::string payload = "{\"tenantId\": \"" + paramsManager.getTenantId() +
                          "\", \"netNo\": \"" + paramsManager.getNetNo() +
                          "\", \"camSid\": " + (paramsManager.getCamSid().empty() ? "0" : paramsManager.getCamSid()) +
                          ", \"camId\": \"" + paramsManager.getCameraId() +
                          "\", \"userId\": \"" + paramsManager.getParameter("userId", "") + "\"}";

    std::string response = ChtP2PCameraControlHandler::getInstance().handleControl(_GetCamStatusById, payload);
    std::cout << "處理結果: " << response << std::endl;
    return true;
}

bool testDeleteCameraInfo(ChtP2PCameraAPI &cameraApi)
{
    std::cout << "\n===== 測試刪除攝影機資訊 =====" << std::endl;
    std::cout << "警告：此操作將解綁設備，確定要繼續嗎？(y/n): ";
    char confirm;
    std::cin >> confirm;
    if (confirm != 'y' && confirm != 'Y')
    {
        std::cout << "操作已取消" << std::endl;
        return false;
    }

    auto &paramsManager = CameraParametersManager::getInstance();
    std::string payload = "{\"camId\": \"" + paramsManager.getCameraId() + "\"}";

    std::string response = ChtP2PCameraControlHandler::getInstance().handleControl(_DeleteCameraInfo, payload);
    std::cout << "處理結果: " << response << std::endl;
    return true;
}

bool testSetTimeZone(ChtP2PCameraAPI &cameraApi, const std::string &tId = "")
{
    std::cout << "\n===== 測試設置時區 =====" << std::endl;

    std::string timeZoneId = tId;
    if (timeZoneId.empty())
    {
        std::cout << "請輸入時區ID (0-51, 預設51為台北): ";
        std::getline(std::cin, timeZoneId);
        if (timeZoneId.empty())
            timeZoneId = "51";
    }

    auto &paramsManager = CameraParametersManager::getInstance();
    std::string payload = "{\"camId\": \"" + paramsManager.getCameraId() + "\", \"tId\": \"" + timeZoneId + "\"}";

    std::string response = ChtP2PCameraControlHandler::getInstance().handleControl(_SetTimeZone, payload);
    std::cout << "處理結果: " << response << std::endl;
    
    std::time_t now = std::time(nullptr);
    std::tm lt = *std::localtime(&now);

    char buf[64];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &lt);
    std::cout << "local time: " << std::string(buf) << std::endl;

    return true;
}

bool testGetTimeZone(ChtP2PCameraAPI &cameraApi)
{
    std::cout << "\n===== 測試獲取時區 =====" << std::endl;
    auto &paramsManager = CameraParametersManager::getInstance();
    std::string payload = "{\"camId\": \"" + paramsManager.getCameraId() + "\"}";

    std::string response = ChtP2PCameraControlHandler::getInstance().handleControl(_GetTimeZone, payload);
    std::cout << "處理結果: " << response << std::endl;
    return true;
}

bool testUpdateCameraName(ChtP2PCameraAPI &cameraApi)
{
    std::cout << "\n===== 測試更新攝影機名稱 =====" << std::endl;
    std::cout << "請輸入新的攝影機名稱: ";
    std::string newName;
    std::getline(std::cin, newName);
    if (newName.empty())
        newName = "測試攝影機-" + std::to_string(time(nullptr));

    auto &paramsManager = CameraParametersManager::getInstance();
    std::string payload = "{\"camId\": \"" + paramsManager.getCameraId() +
                          "\", \"name\": \"" + newName + "\"}";

    std::string response = ChtP2PCameraControlHandler::getInstance().handleControl(_UpdateCameraName, payload);
    std::cout << "處理結果: " << response << std::endl;
    return true;
}

bool testGetHamiCamBindList(ChtP2PCameraAPI &cameraApi)
{
    std::cout << "\n===== 測試獲取WiFi綁定清單 =====" << std::endl;
    auto &paramsManager = CameraParametersManager::getInstance();
    std::string payload = "{\"camId\": \"" + paramsManager.getCameraId() + "\"}";

    std::string response = ChtP2PCameraControlHandler::getInstance().handleControl(_GetHamiCamBindList, payload);
    std::cout << "處理結果: " << response << std::endl;
    return true;
}

// ===== 影像與顯示設定測試函數 =====

bool testSetCameraOSD(ChtP2PCameraAPI &cameraApi)
{
    std::cout << "\n===== 測試設定攝影機OSD =====" << std::endl;
    auto &paramsManager = CameraParametersManager::getInstance();
#if 1
    // 測試案例：本地攝影機時間 (6個中文字符，應該被截取為前4個)
    std::string payload = "{\"camId\": \"" + paramsManager.getCameraId() +
                          "\", \"osdRule\": \"測試時間yyyy-MM-dd HH:mm:ss\"}";
    std::cout << "測試案例: \"測試時間\" (6個中文字符，應該截取為前4個)" << std::endl;
#else
    std::string payload = "{\"camId\": \"" + paramsManager.getCameraId() +
                          "\", \"osdFormat\": \"full\", \"dateFormat\": \"YYYY-MM-DD\", " +
                          "\"timeFormat\": \"HH:mm:ss\", \"showDate\": true, \"showTime\": true, " +
                          "\"showCameraName\": true, \"position\": 0}";
#endif

    std::string response = ChtP2PCameraControlHandler::getInstance().handleControl(_SetCameraOSD, payload);
    std::cout << "處理結果: " << response << std::endl;
    return true;
}

bool testSetCameraHD(ChtP2PCameraAPI &cameraApi)
{
    std::cout << "\n===== 測試設定攝影機HD =====" << std::endl;
    std::cout << "請選擇HD模式 (0=關閉, 1=開啟): ";
    std::string isHd;
    std::getline(std::cin, isHd);
    if (isHd.empty())
        isHd = "1";

    // 驗證輸入
    if (isHd != "0" && isHd != "1")
    {
        std::cout << "無效的輸入，使用預設值: 1" << std::endl;
        isHd = "1";
    }

    auto &paramsManager = CameraParametersManager::getInstance();

    // 生成符合格式的requestId: <UDP/Relay>_live_<userId>_<JWTToken>
    // std::string userId = "testUser123";
    std::string userId = paramsManager.getParameter("userId", "testUser123");
    std::string jwtToken = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9"; // 示例JWT Token
    std::string requestId = "UDP_live_" + userId + "_" + jwtToken;

    // 構建請求 JSON - 使用 rapidjson
    rapidjson::Document requestDoc;
    requestDoc.SetObject();
    rapidjson::Document::AllocatorType &allocator = requestDoc.GetAllocator();

    requestDoc.AddMember("camId", rapidjson::Value(paramsManager.getCameraId().c_str(), allocator).Move(), allocator);
    requestDoc.AddMember("requestId", rapidjson::Value(requestId.c_str(), allocator).Move(), allocator);
    requestDoc.AddMember("isHd", rapidjson::Value(isHd.c_str(), allocator).Move(), allocator);

    rapidjson::StringBuffer requestBuffer;
    rapidjson::Writer<rapidjson::StringBuffer> requestWriter(requestBuffer);
    requestDoc.Accept(requestWriter);

    std::string payload = requestBuffer.GetString();

    std::cout << "測試參數:" << std::endl;
    std::cout << "  - camId: " << paramsManager.getCameraId() << std::endl;
    std::cout << "  - requestId: " << requestId << std::endl;
    std::cout << "  - isHd: " << isHd << " (" << (isHd == "1" ? "開啟1080P" : "關閉720P") << ")" << std::endl;
    std::cout << "  - payload: " << payload << std::endl;

    std::string response = ChtP2PCameraControlHandler::getInstance().handleControl(_SetCameraHD, payload);
    std::cout << "處理結果: " << response << std::endl;

    // 解析回應驗證結果 - 使用 rapidjson
    try
    {
        rapidjson::Document responseJson;
        rapidjson::ParseResult parseResult = responseJson.Parse(response.c_str());
        if (!parseResult.IsError() && responseJson.HasMember(PAYLOAD_KEY_RESULT) && responseJson[PAYLOAD_KEY_RESULT].IsInt())
        {
            int result = responseJson[PAYLOAD_KEY_RESULT].GetInt();
            if (result == 1)
            {
                std::cout << "✓ HD設定成功" << std::endl;

                // 顯示回應中的詳細資訊
                if (responseJson.HasMember("requestId") && responseJson["requestId"].IsString())
                {
                    std::cout << "  - 回應requestId: " << responseJson["requestId"].GetString() << std::endl;
                }
                if (responseJson.HasMember("isHd") && responseJson["isHd"].IsString())
                {
                    std::string responseHd = responseJson["isHd"].GetString();
                    std::cout << "  - 確認HD模式: " << responseHd << " (" << (responseHd == "1" ? "開啟1080P" : "關閉720P") << ")" << std::endl;
                }
                return true;
            }
            else
            {
                std::cout << "✗ HD設定失敗，result=" << result << std::endl;
                return false;
            }
        }
        else
        {
            std::cout << "✗ 無法解析回應或缺少result欄位" << std::endl;
            return false;
        }
    }
    catch (const std::exception &e)
    {
        std::cout << "✗ 解析回應時發生異常: " << e.what() << std::endl;
        return false;
    }
}

bool testSetFlicker(ChtP2PCameraAPI &cameraApi)
{
    std::cout << "\n===== 測試設定閃爍率 =====" << std::endl;
    std::cout << "請選擇閃爍率 (0=50Hz, 1=60Hz, 2=戶外): ";
    std::string flicker;
    std::getline(std::cin, flicker);
    if (flicker.empty())
        flicker = "1";

    auto &paramsManager = CameraParametersManager::getInstance();
    std::string payload = "{\"camId\": \"" + paramsManager.getCameraId() +
                          "\", \"flicker\": \"" + flicker + "\"}";

    std::string response = ChtP2PCameraControlHandler::getInstance().handleControl(_SetFlicker, payload);
    std::cout << "處理結果: " << response << std::endl;
    return true;
}

bool testSetImageQuality(ChtP2PCameraAPI &cameraApi)
{
    std::cout << "\n===== 測試設定影像品質 =====" << std::endl;
    // 顯示當前運行模式
    bool isSimulationMode = CameraDriver::getInstance().isSimulationMode();
    std::cout << "當前運行模式: " << (isSimulationMode ? "模擬模式" : "真實模式") << std::endl;

    std::cout << "請選擇影像品質 (0=低, 1=中, 2=高): ";
    std::string imageQuality;
    std::getline(std::cin, imageQuality);
    if (imageQuality.empty())
        imageQuality = "2";

    // 驗證輸入
    if (imageQuality != "0" && imageQuality != "1" && imageQuality != "2")
    {
        std::cout << "無效的輸入，使用預設值: 2" << std::endl;
        imageQuality = "2";
    }

    auto &paramsManager = CameraParametersManager::getInstance();

    // 生成符合格式的requestId: <UDP/Relay>_live_<userId>_<JWTToken>
    // std::string userId = "testUser123";
    std::string userId = paramsManager.getParameter("userId", "testUser123");
    std::string jwtToken = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9"; // 示例JWT Token
    std::string requestId = "UDP_live_" + userId + "_" + jwtToken;

    // 構建請求 JSON - 使用 rapidjson
    rapidjson::Document requestDoc;
    requestDoc.SetObject();
    rapidjson::Document::AllocatorType &allocator = requestDoc.GetAllocator();

    requestDoc.AddMember("camId", rapidjson::Value(paramsManager.getCameraId().c_str(), allocator).Move(), allocator);
    requestDoc.AddMember("requestId", rapidjson::Value(requestId.c_str(), allocator).Move(), allocator);
    requestDoc.AddMember("imageQuality", rapidjson::Value(imageQuality.c_str(), allocator).Move(), allocator);

    rapidjson::StringBuffer requestBuffer;
    rapidjson::Writer<rapidjson::StringBuffer> requestWriter(requestBuffer);
    requestDoc.Accept(requestWriter);

    std::string payload = requestBuffer.GetString();

    std::cout << "測試參數:" << std::endl;
    std::cout << "  - camId: " << paramsManager.getCameraId() << std::endl;
    std::cout << "  - requestId: " << requestId << std::endl;

    std::string qualityDesc;
    if (imageQuality == "0")
        qualityDesc = "低品質";
    else if (imageQuality == "1")
        qualityDesc = "中品質";
    else if (imageQuality == "2")
        qualityDesc = "高品質";

    std::cout << "測試參數:" << std::endl;
    std::cout << "  - 運行模式: " << (isSimulationMode ? "模擬模式" : "真實模式") << std::endl;
    std::cout << "  - camId: " << paramsManager.getCameraId() << std::endl;
    std::cout << "  - requestId: " << requestId << std::endl;
    std::cout << "  - imageQuality: " << imageQuality << " (" << qualityDesc << ")" << std::endl;
    std::cout << "  - payload: " << payload << std::endl;

    std::string response = ChtP2PCameraControlHandler::getInstance().handleControl(_SetImageQuality, payload);
    std::cout << "處理結果: " << response << std::endl;

    // 解析回應驗證結果 - 使用 rapidjson
    try
    {
        rapidjson::Document responseJson;
        rapidjson::ParseResult parseResult = responseJson.Parse(response.c_str());
        if (!parseResult.IsError() && responseJson.HasMember(PAYLOAD_KEY_RESULT) && responseJson[PAYLOAD_KEY_RESULT].IsInt())
        {
            int result = responseJson[PAYLOAD_KEY_RESULT].GetInt();
            if (result == 1)
            {
                std::cout << "✓ 影像品質設定成功" << std::endl;

                // 顯示回應中的詳細資訊
                if (responseJson.HasMember("requestId") && responseJson["requestId"].IsString())
                {
                    std::cout << "  - 回應requestId: " << responseJson["requestId"].GetString() << std::endl;
                }
                if (responseJson.HasMember("imageQuality") && responseJson["imageQuality"].IsString())
                {
                    std::string responseQuality = responseJson["imageQuality"].GetString();
                    std::string responseDesc;
                    if (responseQuality == "0")
                        responseDesc = "低品質";
                    else if (responseQuality == "1")
                        responseDesc = "中品質";
                    else if (responseQuality == "2")
                        responseDesc = "高品質";

                    std::cout << "  - 確認影像品質: " << responseQuality << " (" << responseDesc << ")" << std::endl;
                }
                return true;
            }
            else
            {
                std::cout << "✗ 影像品質設定失敗，result=" << result << std::endl;
                return false;
            }
        }
        else
        {
            std::cout << "✗ 無法解析回應或缺少result欄位" << std::endl;
            return false;
        }
    }
    catch (const std::exception &e)
    {
        std::cout << "✗ 解析回應時發生異常: " << e.what() << std::endl;
        return false;
    }
}

bool testSetNightMode(ChtP2PCameraAPI &cameraApi)
{
    std::cout << "\n===== 測試設定夜間模式 =====" << std::endl;
    std::cout << "請選擇夜間模式 (0=關閉, 1=開啟): ";
    std::string nightMode;
    std::getline(std::cin, nightMode);
    if (nightMode.empty())
        nightMode = "0";

    auto &paramsManager = CameraParametersManager::getInstance();
    std::string payload = "{\"camId\": \"" + paramsManager.getCameraId() +
                          "\", \"nightMode\": \"" + nightMode + "\"}";

    std::string response = ChtP2PCameraControlHandler::getInstance().handleControl(_SetNightMode, payload);
    std::cout << "處理結果: " << response << std::endl;
    return true;
}

bool testSetAutoNightVision(ChtP2PCameraAPI &cameraApi)
{
    std::cout << "\n===== 測試設定自動夜視 =====" << std::endl;
    std::cout << "請選擇自動夜視 (0=關閉, 1=開啟): ";
    std::string autoNightVision;
    std::getline(std::cin, autoNightVision);
    if (autoNightVision.empty())
        autoNightVision = "1";

    auto &paramsManager = CameraParametersManager::getInstance();
    std::string payload = "{\"camId\": \"" + paramsManager.getCameraId() +
                          "\", \"autoNightVision\": \"" + autoNightVision + "\"}";

    std::string response = ChtP2PCameraControlHandler::getInstance().handleControl(_SetAutoNightVision, payload);
    std::cout << "處理結果: " << response << std::endl;
    return true;
}

bool testSetFlipUpDown(ChtP2PCameraAPI &cameraApi)
{
    std::cout << "\n===== 測試設定上下翻轉 =====" << std::endl;
    std::cout << "請選擇上下翻轉 (0=關閉, 1=開啟): ";
    std::string flipUpDown;
    std::getline(std::cin, flipUpDown);
    if (flipUpDown.empty())
        flipUpDown = "0";

    auto &paramsManager = CameraParametersManager::getInstance();
    std::string payload = "{\"camId\": \"" + paramsManager.getCameraId() +
                          "\", \"isFlipUpDown\": \"" + flipUpDown + "\"}";

    std::string response = ChtP2PCameraControlHandler::getInstance().handleControl(_SetFlipUpDown, payload);
    std::cout << "處理結果: " << response << std::endl;
    return true;
}

// ===== 音頻控制測試函數 =====

bool testSetMicrophone(ChtP2PCameraAPI &cameraApi)
{
    std::cout << "\n===== 測試設定麥克風 =====" << std::endl;
    std::cout << "請輸入麥克風靈敏度 (0-10): ";
    std::string sensitivity;
    std::getline(std::cin, sensitivity);
    if (sensitivity.empty())
        sensitivity = "5";

    auto &paramsManager = CameraParametersManager::getInstance();
    std::string payload = "{\"camId\": \"" + paramsManager.getCameraId() +
                          "\", \"microphoneSensitivity\": \"" + sensitivity + "\"}";

    std::string response = ChtP2PCameraControlHandler::getInstance().handleControl(_SetMicrophone, payload);
    std::cout << "處理結果: " << response << std::endl;
    return true;
}

bool testSetSpeak(ChtP2PCameraAPI &cameraApi)
{
    std::cout << "\n===== 測試設定揚聲器 =====" << std::endl;
    std::cout << "請輸入揚聲器音量 (0-10): ";
    std::string volume;
    std::getline(std::cin, volume);
    if (volume.empty())
        volume = "5";

    auto &paramsManager = CameraParametersManager::getInstance();
    std::string payload = "{\"camId\": \"" + paramsManager.getCameraId() +
                          "\", \"speakVolume\": \"" + volume + "\"}";

    std::string response = ChtP2PCameraControlHandler::getInstance().handleControl(_SetSpeak, payload);
    std::cout << "處理結果: " << response << std::endl;
    return true;
}

// ===== 系統控制測試函數 =====

bool testSetLED(ChtP2PCameraAPI &cameraApi)
{
    std::cout << "\n===== 測試設定LED指示燈 =====" << std::endl;
    std::cout << "請選擇LED指示燈 (0=關閉, 1=開啟): ";
    std::string led;
    std::getline(std::cin, led);
    if (led.empty())
        led = "1";

    auto &paramsManager = CameraParametersManager::getInstance();
    std::string payload = "{\"camId\": \"" + paramsManager.getCameraId() +
                          "\", \"statusIndicatorLight\": \"" + led + "\"}";

    std::string response = ChtP2PCameraControlHandler::getInstance().handleControl(_SetLED, payload);
    std::cout << "處理結果: " << response << std::endl;
    return true;
}

bool testSetCameraPower(ChtP2PCameraAPI &cameraApi)
{
    std::cout << "\n===== 測試設定攝影機電源 =====" << std::endl;
    std::cout << "請選擇攝影機電源 (0=關閉, 1=開啟): ";
    std::string power;
    std::getline(std::cin, power);
    if (power.empty())
        power = "1";

    auto &paramsManager = CameraParametersManager::getInstance();
    std::string payload = "{\"camId\": \"" + paramsManager.getCameraId() +
                          "\", \"camera\": \"" + power + "\"}";

    std::string response = ChtP2PCameraControlHandler::getInstance().handleControl(_SetCameraPower, payload);
    std::cout << "處理結果: " << response << std::endl;
    return true;
}
bool testGetSnapshotHamiCamDevice(ChtP2PCameraAPI &cameraApi)
{
    std::cout << "\n===== 測試取得快照 =====" << std::endl;

    // 顯示當前運行模式
    bool isSimulationMode = CameraDriver::getInstance().isSimulationMode();
    std::cout << "當前運行模式: " << (isSimulationMode ? "模擬模式" : "真實模式") << std::endl;
printf("%s %d\n", __func__, __LINE__);
    // 檢查SD卡狀態
    if (!isSimulationMode)
    {
        if (access("/mnt/sd", F_OK) != 0)
        {
            std::cout << "警告：/mnt/sd 不存在，可能無法正常截圖" << std::endl;
        }
        else
        {
            std::cout << "SD卡基礎路徑存在" << std::endl;

            // 顯示SD卡標籤資訊
            auto &driver = CameraDriver::getInstance();
            // 這裡可以調用 getSDCardLabelPath() 如果它是 public 的
            std::cout << "將動態檢測SD卡標籤名稱..." << std::endl;
        }
    }

    auto &paramsManager = CameraParametersManager::getInstance();
    // 生成事件ID（基於當前時間）
    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);
    std::tm *now_tm = std::localtime(&now_time_t);

    std::stringstream eventIdStream;
    eventIdStream << std::put_time(now_tm, "%Y%m%d%H%M%S");
    std::string eventId = eventIdStream.str();

    // 構建請求 JSON - 使用 rapidjson
    rapidjson::Document requestDoc;
    requestDoc.SetObject();
    rapidjson::Document::AllocatorType &allocator = requestDoc.GetAllocator();

    requestDoc.AddMember(PAYLOAD_KEY_EVENT_ID, rapidjson::Value(eventId.c_str(), allocator).Move(), allocator);
    requestDoc.AddMember(PAYLOAD_KEY_CAMID, rapidjson::Value(paramsManager.getCameraId().c_str(), allocator).Move(), allocator);

    rapidjson::StringBuffer requestBuffer;
    rapidjson::Writer<rapidjson::StringBuffer> requestWriter(requestBuffer);
    requestDoc.Accept(requestWriter);

    std::string payload = requestBuffer.GetString();

    std::cout << "測試參數:" << std::endl;
    std::cout << "  - 運行模式: " << (isSimulationMode ? "模擬模式" : "真實模式") << std::endl;
    std::cout << "  - eventId: " << eventId << std::endl;
    std::cout << "  - camId: " << paramsManager.getCameraId() << std::endl;
    std::cout << "  - payload: " << payload << std::endl;

    std::string response = ChtP2PCameraControlHandler::getInstance().handleControl(_GetSnapshotHamiCamDevice, payload);
    std::cout << "處理結果: " << response << std::endl;

    // 解析回應驗證結果 - 使用 rapidjson
    try
    {
        rapidjson::Document responseJson;
        rapidjson::ParseResult parseResult = responseJson.Parse(response.c_str());
        if (!parseResult.IsError() && responseJson.HasMember(PAYLOAD_KEY_RESULT) && responseJson[PAYLOAD_KEY_RESULT].IsInt())
        {
            int result = responseJson[PAYLOAD_KEY_RESULT].GetInt();
            if (result == 1)
            {
                std::cout << "✓ 截圖請求已接受" << std::endl;

                // 顯示回應中的詳細資訊
                if (responseJson.HasMember("description") && responseJson["description"].IsString())
                {
                    std::cout << "  - 狀態描述: " << responseJson["description"].GetString() << std::endl;
                }

                std::cout << "  - 注意: 截圖將在背景執行，完成後會有另一個回應" << std::endl;

                // 生成當前日期用於顯示預期路徑
                std::stringstream dateStream;
                dateStream << std::put_time(now_tm, "%Y-%m-%d");
                std::string dateStr = dateStream.str();

                if (isSimulationMode)
                {
                    std::cout << "  - 預期檔案路徑: /mnt/sd/SIM-LABEL/" << dateStr << "/" << eventId << "-" << paramsManager.getCameraId() << ".jpg" << std::endl;
                }
                else
                {
                    std::cout << "  - 預期檔案路徑: /mnt/sd/<動態標籤>/" << dateStr << "/" << eventId << "-" << paramsManager.getCameraId() << ".jpg" << std::endl;
                }

                return true;
            }
            else
            {
                std::cout << "✗ 截圖請求失敗，result=" << result << std::endl;
                return false;
            }
            printf("%s %d\n", __func__, __LINE__);
        }
        else
        {
            std::cout << "✗ 無法解析回應或缺少result欄位" << std::endl;
            return false;
        }
    }
    catch (const std::exception &e)
    {
    printf("%s %d\n", __func__, __LINE__);
        std::cout << "✗ 解析回應時發生異常: " << e.what() << std::endl;
        return false;
    }
}

bool testRestartHamiCamDevice(ChtP2PCameraAPI &cameraApi)
{
    std::cout << "\n===== 測試重啟設備 =====" << std::endl;
    std::cout << "警告：此操作將重啟設備，確定要繼續嗎？(y/n): ";
    char confirm;
    std::cin >> confirm;
    if (confirm != 'y' && confirm != 'Y')
    {
        std::cout << "操作已取消" << std::endl;
        return false;
    }

    auto &paramsManager = CameraParametersManager::getInstance();
    std::string payload = "{\"camId\": \"" + paramsManager.getCameraId() + "\"}";

    std::string response = ChtP2PCameraControlHandler::getInstance().handleControl(_RestartHamiCamDevice, payload);
    std::cout << "處理結果: " << response << std::endl;
    return true;
}

bool testHamiCamFormatSDCard(ChtP2PCameraAPI &cameraApi)
{
    std::cout << "\n===== 測試格式化SD卡 =====" << std::endl;
    std::cout << "警告：此操作將格式化SD卡，所有資料將被刪除，確定要繼續嗎？(y/n): ";
    char confirm;
    std::cin >> confirm;
    if (confirm != 'y' && confirm != 'Y')
    {
        std::cout << "操作已取消" << std::endl;
        return false;
    }

    auto &paramsManager = CameraParametersManager::getInstance();
    std::string payload = "{\"camId\": \"" + paramsManager.getCameraId() + "\"}";

    std::string response = ChtP2PCameraControlHandler::getInstance().handleControl(_HamiCamFormatSDCard, payload);
    std::cout << "處理結果: " << response << std::endl;
    return true;
}

bool testUpgradeHamiCamOTA(ChtP2PCameraAPI &cameraApi)
{
    std::cout << "\n===== 測試OTA升級 =====" << std::endl;
    std::cout << "請輸入韌體檔案路徑: ";
    std::string filePath;
    std::getline(std::cin, filePath);
    if (filePath.empty())
        filePath = "/tmp/firmware.bin";

    std::cout << "請選擇升級模式 (0=立即升級, 1=閒置時升級): ";
    std::string upgradeMode;
    std::getline(std::cin, upgradeMode);
    if (upgradeMode.empty())
        upgradeMode = "0";

    auto &paramsManager = CameraParametersManager::getInstance();
    std::string payload = "{\"camId\": \"" + paramsManager.getCameraId() +
                          "\", \"upgradeMode\": \"" + upgradeMode +
                          "\", \"filePath\": \"" + filePath + "\"}";

    std::string response = ChtP2PCameraControlHandler::getInstance().handleControl(_UpgradeHamiCamOTA, payload);
    std::cout << "處理結果: " << response << std::endl;
    return true;
}

// ===== 存儲管理測試函數 =====

bool testSetCamStorageDay(ChtP2PCameraAPI &cameraApi)
{
    std::cout << "\n===== 測試設定雲存天數 =====" << std::endl;
    std::cout << "請輸入雲存天數 (0-365): ";
    std::string storageDay;
    std::getline(std::cin, storageDay);
    if (storageDay.empty())
        storageDay = "7";

    auto &paramsManager = CameraParametersManager::getInstance();
    std::string payload = "{\"camId\": \"" + paramsManager.getCameraId() +
                          "\", \"storageDay\": \"" + storageDay + "\"}";

    std::string response = ChtP2PCameraControlHandler::getInstance().handleControl(_SetCamStorageDay, payload);
    std::cout << "處理結果: " << response << std::endl;
    return true;
}

bool testSetCamEventStorageDay(ChtP2PCameraAPI &cameraApi)
{
    std::cout << "\n===== 測試設定事件存儲天數 =====" << std::endl;
    std::cout << "請輸入事件存儲天數 (0-365): ";
    std::string eventStorageDay;
    std::getline(std::cin, eventStorageDay);
    if (eventStorageDay.empty())
        eventStorageDay = "30";

    auto &paramsManager = CameraParametersManager::getInstance();
    std::string payload = "{\"camId\": \"" + paramsManager.getCameraId() +
                          "\", \"eventStorageDay\": \"" + eventStorageDay + "\"}";

    std::string response = ChtP2PCameraControlHandler::getInstance().handleControl(_SetCamEventStorageDay, payload);
    std::cout << "處理結果: " << response << std::endl;
    return true;
}

// ===== PTZ控制測試函數 =====

bool testHamiCamPtzControlMove(ChtP2PCameraAPI &cameraApi)
{
    std::cout << "\n===== 測試PTZ移動控制 =====" << std::endl;
    std::cout << "請選擇PTZ命令 (left/right/up/down/stop/pan/home): ";
    std::string cmd;
    std::getline(std::cin, cmd);
    if (cmd.empty())
        cmd = "stop";

    auto &paramsManager = CameraParametersManager::getInstance();
    std::string payload = "{\"camId\": \"" + paramsManager.getCameraId() +
                          "\", \"cmd\": \"" + cmd + "\"}";

    std::string response = ChtP2PCameraControlHandler::getInstance().handleControl(_HamiCamPtzControlMove, payload);
    std::cout << "處理結果: " << response << std::endl;
    return true;
}

bool testHamiCamPtzControlConfigSpeed(ChtP2PCameraAPI &cameraApi)
{
    std::cout << "\n===== 測試PTZ速度設定 =====" << std::endl;
    std::cout << "請輸入PTZ速度 (0-2): ";
    std::string speed;
    std::getline(std::cin, speed);
    if (speed.empty())
        speed = "2";

    auto &paramsManager = CameraParametersManager::getInstance();
    std::string payload = "{\"camId\": \"" + paramsManager.getCameraId() +
                          "\", \"speed\": " + speed + "}";

    std::string response = ChtP2PCameraControlHandler::getInstance().handleControl(_HamiCamPtzControlConfigSpeed, payload);
    std::cout << "處理結果: " << response << std::endl;
    return true;
}

bool testHamiCamGetPtzControl(ChtP2PCameraAPI &cameraApi)
{
    std::cout << "\n===== 測試獲取PTZ控制資訊 =====" << std::endl;
    auto &paramsManager = CameraParametersManager::getInstance();
    std::string payload = "{\"camId\": \"" + paramsManager.getCameraId() + "\"}";

    std::string response = ChtP2PCameraControlHandler::getInstance().handleControl(_HamiCamGetPtzControl, payload);
    std::cout << "處理結果: " << response << std::endl;
    return true;
}

bool testHamiCamPtzControlTourGo(ChtP2PCameraAPI &cameraApi)
{
    std::cout << "\n===== 測試PTZ巡航模式 =====" << std::endl;
    std::cout << "請輸入巡航序列 (例如: 1,2,3,4): ";
    std::string indexSequence;
    std::getline(std::cin, indexSequence);
    if (indexSequence.empty())
        indexSequence = "1,2,3,4";

    auto &paramsManager = CameraParametersManager::getInstance();
    std::string payload = "{\"camId\": \"" + paramsManager.getCameraId() +
                          "\", \"indexSequence\": \"" + indexSequence + "\"}";

    std::string response = ChtP2PCameraControlHandler::getInstance().handleControl(_HamiCamPtzControlTourGo, payload);
    std::cout << "處理結果: " << response << std::endl;
    return true;
}

bool testHamiCamPtzControlGoPst(ChtP2PCameraAPI &cameraApi)
{
    std::cout << "\n===== 測試PTZ移動到預設點 =====" << std::endl;
    std::cout << "請輸入預設點編號: ";
    std::string pstIndex;
    std::getline(std::cin, pstIndex);
    if (pstIndex.empty())
        pstIndex = "1";

    auto &paramsManager = CameraParametersManager::getInstance();
    std::string payload = "{\"camId\": \"" + paramsManager.getCameraId() +
                          "\", \"index\": " + pstIndex + "}";

    std::string response = ChtP2PCameraControlHandler::getInstance().handleControl(_HamiCamPtzControlGoPst, payload);
    std::cout << "處理結果: " << response << std::endl;
    return true;
}

bool testHamiCamPtzControlConfigPst(ChtP2PCameraAPI &cameraApi)
{
    std::cout << "\n===== 測試PTZ設定預設點 =====" << std::endl;
    std::cout << "請輸入預設點編號: ";
    std::string pstIndex;
    std::getline(std::cin, pstIndex);
    if (pstIndex.empty())
        pstIndex = "1";

    std::cout << "清除預設點(1清除/0設定): ";
    std::string remove;
    std::getline(std::cin, remove);
    if (remove.empty())
        remove = "0";

    std::cout << "請輸入預設點名稱: ";
    std::string pstName;
    std::getline(std::cin, pstName);
    if (pstName.empty())
        pstName = "預設點" + pstIndex;

    auto &paramsManager = CameraParametersManager::getInstance();
    std::string payload = "{\"camId\": \"" + paramsManager.getCameraId() +
                          "\", \"index\": " + pstIndex +
                          ", \"remove\": \"" + remove +
                          "\", \"positionName\": \"" + pstName + "\"}";

    std::string response = ChtP2PCameraControlHandler::getInstance().handleControl(_HamiCamPtzControlConfigPst, payload);
    std::cout << "處理結果: " << response << std::endl;
    return true;
}

bool testHamiCamHumanTracking(ChtP2PCameraAPI &cameraApi)
{
    std::cout << "\n===== 測試人體追蹤 =====" << std::endl;
    std::cout << "請選擇人體追蹤模式 (0=關閉, 1=回到Home點, 2=停留原地): ";
    std::string tracking;
    std::getline(std::cin, tracking);
    if (tracking.empty())
        tracking = "1";

    auto &paramsManager = CameraParametersManager::getInstance();
    std::string payload = "{\"camId\": \"" + paramsManager.getCameraId() +
                          "\", \"val\": " + tracking + "}";

    std::string response = ChtP2PCameraControlHandler::getInstance().handleControl(_HamiCamHumanTracking, payload);
    std::cout << "處理結果: " << response << std::endl;
    return true;
}

bool testHamiCamPetTracking(ChtP2PCameraAPI &cameraApi)
{
    std::cout << "\n===== 測試寵物追蹤 =====" << std::endl;
    std::cout << "請選擇寵物追蹤模式 (0=關閉, 1=回到Home點, 2=停留原地): ";
    std::string tracking;
    std::getline(std::cin, tracking);
    if (tracking.empty())
        tracking = "1";

    auto &paramsManager = CameraParametersManager::getInstance();
    std::string payload = "{\"camId\": \"" + paramsManager.getCameraId() +
                          "\", \"val\": " + tracking + "}";

    std::string response = ChtP2PCameraControlHandler::getInstance().handleControl(_HamiCamPetTracking, payload);
    std::cout << "處理結果: " << response << std::endl;
    return true;
}

static std::string base64_encode(const std::vector<uint8_t>& in) {
    static const char* T =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve(((in.size() + 2) / 3) * 4);
    size_t i = 0;
    while (i + 3 <= in.size()) {
        uint32_t v = (in[i] << 16) | (in[i+1] << 8) | in[i+2];
        out.push_back(T[(v >> 18) & 63]);
        out.push_back(T[(v >> 12) & 63]);
        out.push_back(T[(v >> 6)  & 63]);
        out.push_back(T[v & 63]);
        i += 3;
    }
    if (i + 1 == in.size()) {
        uint32_t v = (in[i] << 16);
        out.push_back(T[(v >> 18) & 63]);
        out.push_back(T[(v >> 12) & 63]);
        out.push_back('=');
        out.push_back('=');
    } else if (i + 2 == in.size()) {
        uint32_t v = (in[i] << 16) | (in[i+1] << 8);
        out.push_back(T[(v >> 18) & 63]);
        out.push_back(T[(v >> 12) & 63]);
        out.push_back(T[(v >> 6)  & 63]);
        out.push_back('=');
    }
    return out;
}

static std::string generateIdFeatures(void)
{
    static const std::vector<std::string> namePool = {
        "Al**e","B*b","Car**","Dav**","Ev*","F***k","Grac*","Heid*","Iv**","Jud*",
        "莫○暘", "孫○儀", "馮○涵", "傅○堯", "鄭○恩", "房○樺", "陳○錡", "殷○潔", "黃○翔", "林○廷",
        "Birl*", "T*o", "Is***", "N**a", "Sadd**",
        "柯○", "小○", "小○妮", "柯○", "秋○"
    };
    
    srand(time(NULL));

    rapidjson::Document doc;
    doc.SetArray();
    rapidjson::Document::AllocatorType &alloc = doc.GetAllocator();
    
    for (int i = 0; i < 20; i++)
    {
        int id = rand()%10000;
        const std::string& name = namePool[(rand() % namePool.size())];
        int verifyLevel = (rand() % 2) + 1;
        uint32_t createTime = (rand() % (1798783260-1704088860+1)) + 1704088860;
        uint32_t updateTime = (rand() % (1798783260-createTime+1)) + createTime;
        auto epoch2Datetime = [](uint32_t timestamp) -> std::string {
            std::time_t tt = static_cast<std::time_t>(timestamp);
            std::tm tm{};
            localtime_r(&tt, &tm);
            
            char buf[32];
            snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d",
                            tm.tm_year+1900, tm.tm_mon+1, tm.tm_mday,
                            tm.tm_hour, tm.tm_min, tm.tm_sec);
            return std::string(buf);
        };
        std::vector<float> feats(512);
        for (size_t j = 0; j < feats.size(); j++) {
            feats[i] = (float)(rand() % 1000)/1000.0f;
        }
        std::vector<uint8_t> bytes(feats.size()*sizeof(float));
        memcpy(bytes.data(), feats.data(), bytes.size());
        
        std::string cts = epoch2Datetime(createTime);
        std::string uts = epoch2Datetime(updateTime);
        std::string b64 = base64_encode(bytes);
        
        rapidjson::Value obj(rapidjson::kObjectType);
        obj.AddMember("id", id, alloc);
        obj.AddMember("name", rapidjson::Value(name.c_str(), alloc), alloc);
        obj.AddMember("verifyLevel", verifyLevel, alloc);
        
        
        obj.AddMember("createTime", rapidjson::Value().SetString(cts.c_str(), cts.size(), alloc), alloc);
        obj.AddMember("updateTime", rapidjson::Value().SetString(uts.c_str(), uts.size(), alloc), alloc);
        obj.AddMember("faceFeatures", rapidjson::Value().SetString(b64.c_str(), b64.size(), alloc), alloc);

        doc.PushBack(obj, alloc);
    }
    
    rapidjson::StringBuffer sb;
    rapidjson::Writer<rapidjson::StringBuffer> writer(sb);
    doc.Accept(writer);

    printf("%s\n", sb.GetString());
    return sb.GetString();
}

static void assert_valid_json(const std::string& js)
{
    std::cout << js << std::endl;

    rapidjson::Document d;
    d.Parse(js.c_str());
    if (d.HasParseError()) {
        size_t off = d.GetErrorOffset();
        rapidjson::ParseErrorCode code = d.GetParseError();
        size_t from = (off > 40) ? off - 40 : 0;
        size_t len  = std::min<size_t>(80, js.size() - from);
        std::string ctx = js.substr(from, len);
        const char* msg = rapidjson::GetParseErrorFunc()(code);

        std:: cout <<
            std::string("JSON parse error: ") << msg <<
            " at offset " << std::to_string(off) << " near: " << ctx << std::endl;
    } else {
        std::cout << "ok" << std::endl;
    }
}
// ===== AI設定測試函數 =====
bool testUpdateCameraAISetting(ChtP2PCameraAPI &cameraApi)
{
    std::cout << "\n===== 測試更新AI設定 (互動模式) =====" << std::endl;
    auto &controlHandler = ChtP2PCameraControlHandler::getInstance();
    auto &paramsManager = CameraParametersManager::getInstance();

    // 先獲取當前的AI設定
    std::cout << "\n1. 獲取當前AI設定..." << std::endl;
    std::string getPayload = "{\"camId\": \"" + paramsManager.getCameraId() + "\"}";
    std::string currentSettings = controlHandler.handleControl(_GetCameraAISetting, getPayload);
    std::cout << "當前設定: " << currentSettings << std::endl;

    // 解析回應以顯示當前值
    Document currentDoc;
    std::map<std::string, std::string> currentValues;
    currentDoc.Parse(currentSettings.c_str());
    if (!currentDoc.HasParseError() &&
        currentDoc.HasMember("result") && currentDoc["result"].GetInt() == 1 &&
        currentDoc.HasMember("hamiAiSettings") && currentDoc["hamiAiSettings"].IsObject())
    {

        const Value &aiSettings = currentDoc["hamiAiSettings"];

        // 讀取所有當前值
        for (Value::ConstMemberIterator itr = aiSettings.MemberBegin(); itr != aiSettings.MemberEnd(); ++itr)
        {
            std::string key = itr->name.GetString();
            if (itr->value.IsString())
            {
                currentValues[key] = itr->value.GetString();
            }
            else if (itr->value.IsInt())
            {
                currentValues[key] = std::to_string(itr->value.GetInt());
            }
            else if (itr->value.IsObject() && key.find("fencePos") == 0)
            {
                // 處理圍籬座標
                if (itr->value.HasMember("x") && itr->value.HasMember("y"))
                {
                    currentValues[key + "_x"] = std::to_string(itr->value["x"].GetInt());
                    currentValues[key + "_y"] = std::to_string(itr->value["y"].GetInt());
                }
            }
        }
    }

    // 定義可用的AI參數
    struct AIParameter
    {
        std::string name;
        std::string key;
        std::string type; // "string", "int", "coord", "dir"
        std::string description;
        std::string range;    // 範圍說明
        std::string category; // 參數分類
    };

    std::vector<AIParameter> aiParams = {
        // 告警設定 (Alert)
        {"動態偵測告警", "vmdAlert", "string", "動態檢測告警", "0(關閉)/1(開啟)", "告警設定"},
        {"人形追蹤告警", "humanAlert", "string", "人形追蹤告警", "0(關閉)/1(開啟)", "告警設定"},
        {"寵物追蹤告警", "petAlert", "string", "寵物追蹤告警", "0(關閉)/1(開啟)", "告警設定"},
        {"聲音偵測告警", "adAlert", "string", "聲音偵測告警", "0(關閉)/1(開啟)", "告警設定"},
        {"電子圍籬告警", "fenceAlert", "string", "電子圍籬偵測告警", "0(關閉)/1(開啟)", "告警設定"},
        {"臉部偵測告警", "faceAlert", "string", "臉部偵測告警", "0(關閉)/1(開啟)", "告警設定"},
        {"跌倒偵測告警", "fallAlert", "string", "跌倒偵測告警", "0(關閉)/1(開啟)", "告警設定"},
        {"嬰兒哭泣告警", "adBabyCryAlert", "string", "嬰兒哭泣告警", "0(關閉)/1(開啟)", "告警設定"},
        {"人聲告警", "adSpeechAlert", "string", "人聲告警", "0(關閉)/1(開啟)", "告警設定"},
        {"警報聲告警", "adAlarmAlert", "string", "警報聲告警", "0(關閉)/1(開啟)", "告警設定"},
        {"狗叫聲告警", "adDogAlert", "string", "狗叫聲告警", "0(關閉)/1(開啟)", "告警設定"},
        {"貓叫聲告警", "adCatAlert", "string", "貓叫聲告警", "0(關閉)/1(開啟)", "告警設定"},

        // 靈敏度設定 (Sen)
        {"動態偵測靈敏度", "vmdSen", "int", "動態偵測靈敏度", "0(低)/1(中)/2(高)", "靈敏度設定"},
        {"聲音偵測靈敏度", "adSen", "int", "聲音偵測靈敏度", "0(低)/1(中)/2(高)", "靈敏度設定"},
        {"人形偵測靈敏度", "humanSen", "int", "人形偵測靈敏度", "0(低)/1(中)/2(高)", "靈敏度設定"},
        {"人臉偵測靈敏度", "faceSen", "int", "人臉偵測靈敏度", "0(低)/1(中)/2(高)", "靈敏度設定"},
        {"電子圍籬靈敏度", "fenceSen", "int", "電子圍離靈敏度", "0(低)/1(中)/2(高)", "靈敏度設定"},
        {"寵物偵測靈敏度", "petSen", "int", "寵物偵測靈敏度", "0(低)/1(中)/2(高)", "靈敏度設定"},
        {"哭泣偵測靈敏度", "adBabyCrySen", "int", "哭泣偵測靈敏度", "0(低)/1(中)/2(高)", "靈敏度設定"},
        {"人聲偵測靈敏度", "adSpeechSen", "int", "人聲偵測靈敏度", "0(低)/1(中)/2(高)", "靈敏度設定"},
        {"警報聲偵測靈敏度", "adAlarmSen", "int", "警報聲偵測靈敏度", "0(低)/1(中)/2(高)", "靈敏度設定"},
        {"狗叫聲偵測靈敏度", "adDogSen", "int", "狗叫聲偵測靈敏度", "0(低)/1(中)/2(高)", "靈敏度設定"},
        {"貓叫聲偵測靈敏度", "adCatSen", "int", "貓叫聲偵測靈敏度", "0(低)/1(中)/2(高)", "靈敏度設定"},
        {"跌倒偵測靈敏度", "fallSen", "int", "跌倒偵測靈敏度", "0(低)/1(中)/2(高)", "靈敏度設定"},

        // 電子圍籬設定
        {"電子圍籬座標1", "fencePos1", "coord", "電子圍籬座標1 (x,y)", "x,y (0-100)", "電子圍籬"},
        {"電子圍籬座標2", "fencePos2", "coord", "電子圍籬座標2 (x,y)", "x,y (0-100)", "電子圍籬"},
        {"電子圍籬座標3", "fencePos3", "coord", "電子圍籬座標3 (x,y)", "x,y (0-100)", "電子圍籬"},
        {"電子圍籬座標4", "fencePos4", "coord", "電子圍籬座標4 (x,y)", "x,y (0-100)", "電子圍籬"},
        {"圍籬進入方向", "fenceDir", "string", "電子圍籬進入方向", "0(進入)/1(離開)", "電子圍籬"},
        
        // 人臉特徵值欄位表
        {"人臉特徵值欄位表", "identificationFeatures", "face", "人臉特徵值", "隨機產生(滿20位)", "人臉特徵值"}
    };

    // 顯示參數選單（按分類顯示）
    std::cout << "\n2. 請選擇要修改的AI參數：" << std::endl;

    std::string lastCategory = "";
    int paramIndex = 1;

    for (const auto &param : aiParams)
    {
        // 如果是新分類，顯示分類標題
        if (param.category != lastCategory)
        {
            if (!lastCategory.empty())
            {
                std::cout << "╚══════╩════════════════════════╩══════════════════════════════╩═══════════════════╩════════╩══════════════╝" << std::endl;
                std::cout << std::endl;
            }
            std::cout << "\n【" << param.category << "】" << std::endl;
            std::cout << "╔══════╦════════════════════════╦══════════════════════════════╦═══════════════════╦════════╦══════════════╗" << std::endl;
            std::cout << "║ 編號 ║ 參數名稱               ║ 說明                         ║ 值範圍            ║ 當前值 ║ 參數鍵值     ║" << std::endl;
            std::cout << "╠══════╬════════════════════════╬══════════════════════════════╬═══════════════════╬════════╬══════════════╣" << std::endl;
            lastCategory = param.category;
        }

        // 獲取當前值
        std::string currentValue = "";
        if (param.type == "coord")
        {
            std::string x = currentValues[param.key + "_x"];
            std::string y = currentValues[param.key + "_y"];
            if (!x.empty() && !y.empty())
            {
                currentValue = "(" + x + "," + y + ")";
            }
            else
            {
                currentValue = "(10,10)"; // 預設值
            }
        }
        else
        {
            currentValue = currentValues[param.key];
            if (currentValue.empty())
            {
                // 設定預設值
                if (param.type == "string")
                {
                    currentValue = "0";
                }
                else if (param.type == "int")
                {
                    currentValue = "1";
                }
            }
        }

        std::cout << "║ " << std::setw(4) << std::right << paramIndex++ << " ║ "
                  << std::setw(28) << std::left << param.name << " ║ "
                  << std::setw(34) << std::left << param.description << " ║ "
                  << std::setw(21) << std::left << param.range << " ║ "
                  << std::setw(6) << std::left << currentValue << " ║ "
                  << std::setw(12) << std::left << param.key << " ║" << std::endl;
    }
    std::cout << "╚══════╩════════════════════════╩══════════════════════════════╩═══════════════════╩════════╩══════════════╝" << std::endl;

    // 讓用戶選擇要修改的參數
    std::vector<int> selectedIndices;
    std::cout << "\n請輸入要修改的參數編號(用空格分隔，如: 1 3 5)，或輸入0修改所有參數: ";
    std::string selection;
    std::getline(std::cin, selection);

    if (selection == "0")
    {
        // 選擇所有參數
        for (size_t i = 0; i < aiParams.size(); ++i)
        {
            selectedIndices.push_back(i);
        }
    }
    else
    {
        // 解析用戶選擇
        std::istringstream iss(selection);
        int index;
        while (iss >> index)
        {
            if (index >= 1 && index <= static_cast<int>(aiParams.size()))
            {
                selectedIndices.push_back(index - 1);
            }
        }
    }

    if (selectedIndices.empty())
    {
        std::cout << "未選擇任何參數，取消更新。" << std::endl;
        return false;
    }

    // 收集新的參數值
    std::cout << "\n3. 請為選擇的參數輸入新值：" << std::endl;
    std::map<std::string, std::string> newValues;
    std::map<std::string, std::pair<int, int>> newCoordValues; // 座標值

    for (int idx : selectedIndices)
    {
        const auto &param = aiParams[idx];

        // 顯示當前值
        std::string currentVal = "";
        if (param.type == "coord")
        {
            std::string x = currentValues[param.key + "_x"];
            std::string y = currentValues[param.key + "_y"];
            currentVal = "當前值: (" + (x.empty() ? "10" : x) + "," + (y.empty() ? "10" : y) + ")";
        }
        else
        {
            currentVal = "當前值: " + (currentValues[param.key].empty() ? (param.type == "string" ? "0" : "1") : currentValues[param.key]);
        }

        if (param.type == "face")
        {
            std::cout << "\n"
                      << param.name << " (" << param.description << "): ";
        }
        else
        {
            std::cout << "\n"
                      << param.name << " (" << param.description << ", 範圍: " << param.range << ", " << currentVal << "): ";
        }
        std::string value;
        std::getline(std::cin, value);

        // 如果用戶直接按 Enter，保留當前值
        if (value.empty())
        {
            std::cout << "  保留當前值" << std::endl;
            continue;
        }

        // 驗證輸入
        bool valid = false;

        if (param.type == "string")
        {
            // 告警開關和圍籬方向
            if (value == "0" || value == "1")
            {
                valid = true;
                newValues[param.key] = value;
            }
        }
        else if (param.type == "int")
        {
            try
            {
                int intValue = std::stoi(value);
#if 0
                // 特殊處理 adSen (0-255)
                if (param.key == "adSen")
                {
                    if (intValue >= 0 && intValue <= 255)
                    {
                        valid = true;
                        newValues[param.key] = value;
                    }
                }
                else
#endif
                {
                    // 其他靈敏度 (0-2)
                    if (intValue >= 0 && intValue <= 2)
                    {
                        valid = true;
                        newValues[param.key] = value;
                    }
                }
            }
            catch (...)
            {
            }
        }
        else if (param.type == "coord")
        {
            // 解析座標格式 x,y
            size_t commaPos = value.find(',');
            if (commaPos != std::string::npos)
            {
                try
                {
                    int x = std::stoi(value.substr(0, commaPos));
                    int y = std::stoi(value.substr(commaPos + 1));
                    if (x >= 0 && x <= 100 && y >= 0 && y <= 100)
                    {
                        valid = true;
                        newCoordValues[param.key] = {x, y};
                    }
                }
                catch (...)
                {
                }
            }
        }
        else if (param.type == "face")
        {
            // random generate new data
            try
            {
                newValues[param.key] = generateIdFeatures();
                valid = true;
            }
            catch (...)
            {
            }
        }

        if (valid)
        {
            std::cout << "  ✓ 已接受新值" << std::endl;
        }
        else
        {
            std::cout << "  ⚠️  無效的值，請檢查格式和範圍。跳過此參數。" << std::endl;
        }
    }

    if (newValues.empty() && newCoordValues.empty())
    {
        std::cout << "\n沒有有效的參數值，取消更新。" << std::endl;
        return false;
    }

    // 構建更新的JSON
    std::cout << "\n4. 準備更新以下參數：" << std::endl;
    std::string hamiAiSettings = "{";
    bool first = true;

    // 處理一般參數
    for (const auto &[key, value] : newValues)
    {
        if (!first)
            hamiAiSettings += ", ";

        // 找到參數以確定類型
        auto paramIt = std::find_if(aiParams.begin(), aiParams.end(),
                                    [&key](const AIParameter &p)
                                    { return p.key == key; });

        if (paramIt != aiParams.end())
        {
            if (paramIt->type == "string")
            {
                // 字串類型（告警開關等）
                hamiAiSettings += "\"" + key + "\": \"" + value + "\"";
            }
            else if (paramIt->type == "int")
            {
                // 整數類型（靈敏度）
                hamiAiSettings += "\"" + key + "\": " + value;
            }
            else if (paramIt->type == "face")
            {
                hamiAiSettings += "\"" + key + "\": " + value;
            }
            
            std::cout << "  • " << paramIt->name << " = " << value << std::endl;
        }
        first = false;
    }

    // 處理座標參數
    for (const auto &[key, coords] : newCoordValues)
    {
        if (!first)
            hamiAiSettings += ", ";

        hamiAiSettings += "\"" + key + "\": {\"x\": " + std::to_string(coords.first) +
                          ", \"y\": " + std::to_string(coords.second) + "}";

        // 顯示要更新的參數
        auto paramIt = std::find_if(aiParams.begin(), aiParams.end(),
                                    [&key](const AIParameter &p)
                                    { return p.key == key; });
        if (paramIt != aiParams.end())
        {
            std::cout << "  • " << paramIt->name << " = (" << coords.first << "," << coords.second << ")" << std::endl;
        }
        first = false;
    }

    hamiAiSettings += "}";

    assert_valid_json(hamiAiSettings);
    // 確認更新
    std::cout << "\n確定要更新這些設定嗎？(y/n): ";
    std::string confirm;
    std::getline(std::cin, confirm);

    if (confirm != "y" && confirm != "Y")
    {
        std::cout << "取消更新。" << std::endl;
        return false;
    }

    // 執行更新
    std::cout << "\n5. 執行更新..." << std::endl;
    std::string updatePayload = "{\"camId\": \"" + paramsManager.getCameraId() +
                                "\", \"hamiAiSettings\": " + hamiAiSettings + "}";
    std::cout << "發送請求: " << updatePayload << std::endl;

    std::string response = controlHandler.handleControl(_UpdateCameraAISetting, updatePayload);
    std::cout << "\n更新結果: " << response << std::endl;

    // 再次獲取設定以確認更新
    std::cout << "\n6. 確認更新後的設定..." << std::endl;
    std::string confirmGetPayload = "{\"camId\": \"" + paramsManager.getCameraId() + "\"}";
    std::string confirmSettings = controlHandler.handleControl(_GetCameraAISetting, confirmGetPayload);
    std::cout << "更新後設定: " << confirmSettings << std::endl;

    return true;
}

bool testGetCameraAISetting(ChtP2PCameraAPI &cameraApi)
{
    std::cout << "\n===== 測試獲取AI設定 (_GetCameraAISetting) =====" << std::endl;
    std::cout << "規格版本: 2.3.33 取得攝影機AI設定資訊" << std::endl;

    auto &controlHandler = ChtP2PCameraControlHandler::getInstance();
    auto &paramsManager = CameraParametersManager::getInstance();

    // 根據CHT規格，請求格式只需要包含camId
    std::string payload = "{\"camId\": \"" + paramsManager.getCameraId() + "\"}";

    std::cout << "\n發送請求：" << std::endl;
    std::cout << "控制類型: _GetCameraAISetting" << std::endl;
    std::cout << "請求內容: " << payload << std::endl;

    std::string response = controlHandler.handleControl(_GetCameraAISetting, payload);

    std::cout << "\n收到回應：" << std::endl;
    std::cout << "原始回應: " << response << std::endl;

    try {
        // 解析並格式化顯示回應
        rapidjson::Document responseDoc;
        responseDoc.Parse(response.c_str());
        
        if (responseDoc.HasParseError()) throw std::runtime_error("JSON解析失敗");

        std::cout << "\n===== AI設定詳細資訊 =====" << std::endl;
        
        std::string key = "result";
        auto it = responseDoc.FindMember(key.c_str());
        if (it == responseDoc.MemberEnd()) throw std::runtime_error(std::string("缺少欄位: ") + key);
        if (!it->value.IsInt()) throw std::runtime_error(std::string("欄位格式錯誤: ") + key);
        int result = it->value.GetInt();
        std::cout << "執行結果: " << (result == 1 ? "成功 ✓" : "失敗 ✗") << " (result: " << result << ")" << std::endl;

        if (result != 1) throw std::runtime_error("無法取得AI設定資訊或回應格式錯誤");
        key = "hamiAiSettings";
        it = responseDoc.FindMember(key.c_str());
        if (it == responseDoc.MemberEnd()) throw std::runtime_error(std::string("缺少欄位: ") + key);
        if (!it->value.IsObject()) throw std::runtime_error(std::string("欄位格式錯誤: ") + key);

        const rapidjson::Value &aiSettings = it->value;

        // === 告警設定 ===
        std::cout << "\n【告警設定】" << std::endl;
        std::cout << "┌─────────────────────┬────────┬─────────────┐" << std::endl;
        std::cout << "│ 告警類型             │ 狀態    │ 參數鍵值     │" << std::endl;
        std::cout << "├─────────────────────┼────────┼─────────────┤" << std::endl;

        struct AlertParam
        {
            std::string name;
            std::string key;
        };

        std::vector<AlertParam> alertParams = {
            {"動態檢測告警", "vmdAlert"},
            {"人形追蹤告警", "humanAlert"},
            {"寵物追蹤告警", "petAlert"},
            {"聲音偵測告警", "adAlert"},
            {"電子圍籬告警", "fenceAlert"},
            {"臉部偵測告警", "faceAlert"},
            {"跌倒偵測告警", "fallAlert"},
            {"嬰兒哭泣告警", "adBabyCryAlert"},
            {"人聲告警", "adSpeechAlert"},
            {"警報聲告警", "adAlarmAlert"},
            {"狗叫聲告警", "adDogAlert"},
            {"貓叫聲告警", "adCatAlert"}
        };

        for (const auto &param : alertParams)
        {
            auto it = aiSettings.FindMember(param.key.c_str());
            std::string value = "0";
            if (it == aiSettings.MemberEnd() || !it->value.IsString()) {
                value = "0";
            } else {
                value = it->value.GetString();
            }

            std::string status = (value == "1") ? "開啟 ✓" : "關閉 ✗";
            std::cout << "│ " << std::setw(18) << std::left << param.name << " │ "
                      << std::setw(6) << std::left << status << " │ "
                      << std::setw(11) << std::left << param.key << " │" << std::endl;
        }
        std::cout << "└─────────────────────┴────────┴─────────────┘" << std::endl;

        // === 靈敏度設定 ===
        std::cout << "\n【靈敏度設定】" << std::endl;
        std::cout << "┌─────────────────────┬────────┬─────────────┐" << std::endl;
        std::cout << "│ 靈敏度類型            │ 等級   │ 參數鍵值     │" << std::endl;
        std::cout << "├─────────────────────┼────────┼─────────────┤" << std::endl;

        std::vector<AlertParam> senParams = {
            {"動態偵測靈敏度", "vmdSen"},
            {"聲音偵測靈敏度", "adSen"},
            {"人形偵測靈敏度", "humanSen"},
            {"人臉偵測靈敏度", "faceSen"},
            {"電子圍籬靈敏度", "fenceSen"},
            {"寵物偵測靈敏度", "petSen"},
            {"哭泣偵測靈敏度", "adBabyCrySen"},
            {"人聲偵測靈敏度", "adSpeechSen"},
            {"警報聲偵測靈敏度", "adAlarmSen"},
            {"狗叫聲偵測靈敏度", "adDogSen"},
            {"貓叫聲偵測靈敏度", "adCatSen"},
            {"跌倒偵測靈敏度", "fallSen"}
        };

        for (const auto &param : senParams)
        {
            auto it = aiSettings.FindMember(param.key.c_str());
            int value = 1;
            if (it == aiSettings.MemberEnd() || !it->value.IsInt()) {
                value = 1;
            } else {
                value = it->value.GetInt();
            }

            std::string level;
#if 0
            if (param.key == "adSen")
            {
                level = std::to_string(value) + " (0-255)";
            }
            else
#endif
            {
                switch (value)
                {
                case 0:
                    level = "低 (0)";
                    break;
                case 1:
                    level = "中 (1)";
                    break;
                case 2:
                    level = "高 (2)";
                    break;
                default:
                    level = "中 (1)";
                    break;
                }
            }
            std::cout << "│ " << std::setw(18) << std::left << param.name << " │ "
                      << std::setw(6) << std::left << level << " │ "
                      << std::setw(11) << std::left << param.key << " │" << std::endl;
        }
        std::cout << "└─────────────────────┴────────┴─────────────┘" << std::endl;

        // === 電子圍籬座標 ===
        std::cout << "\n【電子圍籬設定】" << std::endl;
        std::cout << "┌─────────────┬────────────┬─────────────┐" << std::endl;
        std::cout << "│ 座標點       │ 位置 (X,Y)  │ 參數鍵值     │" << std::endl;
        std::cout << "├─────────────┼────────────┼─────────────┤" << std::endl;

        for (int i = 1; i <= 4; i++)
        {
            std::string posKey = "fencePos" + std::to_string(i);
            auto it = aiSettings.FindMember(posKey.c_str());
            int x = 0, y = 0;
            if (it == aiSettings.MemberEnd() || !it->value.IsObject()) {
                x = 0;
                y = 0;
            } else {
                const rapidjson::Value &pos = it->value;
                auto x_it = pos.FindMember("x");
                auto y_it = pos.FindMember("y");
                if ( (x_it != pos.MemberEnd() && x_it->value.IsInt()) &&
                     (y_it != pos.MemberEnd() && y_it->value.IsInt()) )
                {
                    x = x_it->value.GetInt();
                    y = y_it->value.GetInt();
                }
            }

            std::string coords = "(" + std::to_string(x) + "," + std::to_string(y) + ")";
            std::cout << "│ " << std::setw(11) << std::left << ("座標點" + std::to_string(i)) << " │ "
                      << std::setw(10) << std::left << coords << " │ "
                      << std::setw(11) << std::left << posKey << " │" << std::endl;
        }

        // === 電子圍籬方向 ===
        {
            auto it = aiSettings.FindMember("fenceDir");
            std::string dir = "0";
            if (it == aiSettings.MemberEnd() || !it->value.IsString()) {
                dir = "0";
            } else {
                dir = it->value.GetString();
            }
            
            std::string dirText = (dir == "0") ? "進入 (0)" : "離開 (1)";
            std::cout << "│ " << std::setw(11) << std::left << "圍籬方向" << " │ "
                      << std::setw(10) << std::left << dirText << " │ "
                      << std::setw(11) << std::left << "fenceDir" << " │" << std::endl;
        }
        std::cout << "└─────────────┴────────────┴─────────────┘" << std::endl;

        // === 人臉識別特徵 ===
        {
            auto it = aiSettings.FindMember("identificationFeatures");
            if (it == aiSettings.MemberEnd() || !it->value.IsArray()) {
                std::cout << "目前無人臉識別特徵資料" << std::endl;
            } else {
                const rapidjson::Value &features = it->value;
                std::cout << "\n【人臉識別特徵】(共 " << features.Size() << " 筆)" << std::endl;
                if (features.Size() > 0)
                {
                    std::cout << "┌────────────────┬──────────┬────────┬─────────────────┬─────────────────┐" << std::endl;
                    std::cout << "│ 人員ID         │ 姓名     │ 門檻值 │ 創建時間        │ 更新時間        │" << std::endl;
                    std::cout << "├────────────────┼──────────┼────────┼─────────────────┼─────────────────┤" << std::endl;
                    
                    
                    for (rapidjson::Value::ConstValueIterator itr = features.Begin(); itr != features.End(); ++itr)
                    {
                        std::string id = "N/A";
                        std::string name = "N/A";
                        std::string level = "N/A";
                        std::string createTime = "N/A";
                        std::string updateTime = "N/A";
                        if (itr->IsObject())
                        {
                            auto findItem = [&](const char *k) -> const rapidjson::Value * {
                                auto m = itr->FindMember(k);
                                return (m != itr->MemberEnd()) ? &m->value : nullptr;
                            };
                            const rapidjson::Value *vid = findItem("id");
                            const rapidjson::Value *vname = findItem("name");
                            const rapidjson::Value *vvl = findItem("verifyLevel");
                            const rapidjson::Value *vct = findItem("createTime");
                            const rapidjson::Value *vut = findItem("updateTime");
                            
                            id = (vid && vid->IsString()) ? vid->GetString() : "N/A";
                            name = (vname && vname->IsString()) ? vname->GetString() : "N/A";
                            level = (vvl && vvl->IsInt()) ? std::to_string(vvl->GetInt()) : "N/A";
                            createTime = (vct && vct->IsString()) ? vct->GetString() : "N/A";
                            updateTime = (vut && vut->IsString()) ? vut->GetString() : "N/A";
                        }
                        
                        std::cout << "│ " << std::setw(14) << std::left << id << " │ "
                                  << std::setw(8) << std::left << name << " │ "
                                  << std::setw(6) << std::left << level << " │ "
                                  << std::setw(15) << std::left << createTime << " │ "
                                  << std::setw(15) << std::left << updateTime << " │" << std::endl;
                    }
                    std::cout << "└────────────────┴──────────┴────────┴─────────────────┴─────────────────┘" << std::endl;
                }
                else
                {
                    std::cout << "目前無人臉識別特徵資料" << std::endl;
                }
            }
        }
    } catch (const std::exception& e) {
        std::cout << e.what() << std::endl;
    }

    std::cout << "\n===== 測試完成 =====" << std::endl;
    return true;
}

// ===== 串流控制測試函數 =====

bool testGetVideoLiveStream(ChtP2PCameraAPI &cameraApi)
{
    #if 1 // jeffery added 20250805
    using namespace rapidjson;
    #endif

    std::cout << "請輸入欲接收的Clinet端IP:";
    std::string IP;
    std::getline(std::cin, IP);
    if (IP.empty()){
        return false;
    }

    std::cout << "\n===== 測試開始即時影音串流 =====" << std::endl;
    auto &paramsManager = CameraParametersManager::getInstance();

    // 生成符合規格的requestId: <UDP/Relay>_live_<userId>_<JWTToken>
    // std::string userId = "testUser123";
    std::string userId = paramsManager.getParameter("userId", "testUser123");
    std::string jwtToken = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9"; // 示例JWT Token
    std::string requestId = "UDP_live_" + userId + "_" + jwtToken;

    // 設定frameType和imageQuality
    std::string frameType = "rtp";
    std::string imageQuality = "2"; // High bitrate

    std::string payload = "{\"camId\":\"" + paramsManager.getCameraId() +
                          "\", \"requestId\":\"" + requestId +
                          "\", \"frameType\":\"" + frameType +
                          "\", \"IP\":\"" + IP +
                          "\", \"imageQuality\":\"" + imageQuality + "\"}";

    std::cout << "測試參數:" << std::endl;
    std::cout << "  - camId: " << paramsManager.getCameraId() << std::endl;
    std::cout << "  - requestId: " << requestId << std::endl;
    std::cout << "  - frameType: " << frameType << std::endl;
    std::cout << "  - IP: " << IP << std::endl;
    std::cout << "  - imageQuality: " << imageQuality << std::endl;
#if 1 // jeffery added 20250805
    // format the .json file.
    Document doc;
    doc.SetObject();

    Document::AllocatorType &allocator = doc.GetAllocator();

    doc.AddMember("frameType", Value(frameType.c_str(), allocator), allocator);
    doc.AddMember("imageQuality", Value(imageQuality.c_str(), allocator), allocator);

    StringBuffer buffer;
    Writer<StringBuffer> writer(buffer);
    doc.Accept(writer);

    std::ofstream ofs("/mnt/sd/EXjson/rtp_live.json");
    if (ofs.is_open())
    {
        ofs << buffer.GetString();
        ofs.close();
        std::cout << "JSON saved to output.json\n";
    }
    else
    {
        std::cerr << "Failed to open output.json for writing.\n";
    }
#endif
    std::string response = ChtP2PCameraControlHandler::getInstance().handleControl(_GetVideoLiveStream, payload);
    std::cout << "處理結果: " << response << std::endl;

    // 保存requestId供後續停止使用
    paramsManager.setParameter("liveStreamRequestId", requestId);
    return true;
}

bool testStopVideoLiveStream(ChtP2PCameraAPI &cameraApi)
{
    std::cout << "\n===== 測試停止即時影音串流 =====" << std::endl;
    auto &paramsManager = CameraParametersManager::getInstance();
    std::string requestId = paramsManager.getParameter("liveStreamRequestId", "");

    if (requestId.empty())
    {
        // 如果沒有保存的requestId，生成一個測試用的
        // std::string userId = "testUser123";
        std::string userId = paramsManager.getParameter("userId", "testUser123");
        std::string jwtToken = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9";
        requestId = "UDP_live_" + userId + "_" + jwtToken;
        std::cout << "沒有活躍的即時串流requestId，使用測試requestId: " << requestId << std::endl;
    }

    std::string payload = "{\"camId\":\"" + paramsManager.getCameraId() +
                          "\", \"requestId\":\"" + requestId + "\"}";

    std::cout << "停止串流參數:" << std::endl;
    std::cout << "  - camId: " << paramsManager.getCameraId() << std::endl;
    std::cout << "  - requestId: " << requestId << std::endl;

    std::string response = ChtP2PCameraControlHandler::getInstance().handleControl(_StopVideoLiveStream, payload);
    std::cout << "處理結果: " << response << std::endl;

    // 清除保存的requestId
    paramsManager.setParameter("liveStreamRequestId", "");
    return true;
}

bool testGetVideoHistoryStream(ChtP2PCameraAPI &cameraApi)
{
#if 1 // jeffery added 20250805
    using namespace rapidjson;
#endif
    std::cout << "\n===== 測試開始歷史影音串流 =====" << std::endl;

    std::cout << "請輸入欲接收的Clinet端IP:";
    std::string IP;
    std::getline(std::cin, IP);
    if (IP.empty()){
        return false;
    }

    std::cout << "請輸入欲播放的檔案名稱 (需再sd卡root/manual底下):";
    std::string requestId;
    std::getline(std::cin, requestId);
    if (requestId.empty()){
        return false;
    }

    /*std::cout << "請輸入開始時間戳 (Unix時間戳，留空使用1小時前): ";
    std::string startTime;
    std::getline(std::cin, startTime);
    if (startTime.empty())
        startTime = std::to_string(time(nullptr) - 3600); // 1小時前*/
#if 1 // jeffery added 20250805
    std::string startTime = "19700101000000";
#endif
    auto &paramsManager = CameraParametersManager::getInstance();

    // 生成符合規格的requestId: <UDP/Relay>_history_<userId>_<JWTToken>
    // std::string userId = "testUser123";
    std::string userId = paramsManager.getParameter("userId", "testUser123");
    std::string jwtToken = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9"; // 示例JWT Token
    #if 1                                                              // jeffery added 20250805
    //std::string requestId = "Record_20250820094324_H264_NONE.mp4";
    #endif

    // 設定frameType和imageQuality
    std::string frameType = "rtp";
    std::string imageQuality = "2"; // High bitrate

    std::string payload = "{\"camId\":\"" + paramsManager.getCameraId() +
                          "\", \"requestId\":\"" + requestId +
                          "\", \"frameType\":\"" + frameType +
                          "\", \"imageQuality\":\"" + imageQuality +
                          "\", \"IP\":\"" + IP +
                          "\", \"startTime\":\"" + startTime + "\"}";

    std::cout << "測試參數:" << std::endl;
    std::cout << "  - camId: " << paramsManager.getCameraId() << std::endl;
    std::cout << "  - requestId: " << requestId << std::endl;
    std::cout << "  - frameType: " << frameType << std::endl;
    std::cout << "  - imageQuality: " << imageQuality << std::endl;
    std::cout << "  - IP: " << IP << std::endl;
    std::cout << "  - startTime: " << startTime << std::endl;
    #if 1 // jeffery added 20250805
    // format the .json file.
    Document doc;
    doc.SetObject();

    Document::AllocatorType &allocator = doc.GetAllocator();

    doc.AddMember("frameType", Value(frameType.c_str(), allocator), allocator);
    doc.AddMember("videoPath", Value(requestId.c_str(), allocator), allocator);

    StringBuffer buffer;
    Writer<StringBuffer> writer(buffer);
    doc.Accept(writer);

    std::ofstream ofs("/mnt/sd/EXjson/rtp_history.json");
    if (ofs.is_open())
    {
        ofs << buffer.GetString();
        ofs.close();
        std::cout << "JSON saved to rtp_history.json\n";
    }
    else
    {
        std::cerr << "Failed to open rtp_history.json for writing.\n";
    }
#endif
    std::string response = ChtP2PCameraControlHandler::getInstance().handleControl(_GetVideoHistoryStream, payload);
    std::cout << "處理結果: " << response << std::endl;

    // 保存requestId供後續停止使用
    paramsManager.setParameter("historyStreamRequestId", requestId);
    return true;
}

bool testStopVideoHistoryStream(ChtP2PCameraAPI &cameraApi)
{
    std::cout << "\n===== 測試停止歷史影音串流 =====" << std::endl;
    auto &paramsManager = CameraParametersManager::getInstance();
    std::string requestId = paramsManager.getParameter("historyStreamRequestId", "");

    if (requestId.empty())
    {
        // 如果沒有保存的requestId，生成一個測試用的
        // std::string userId = "testUser123";
        std::string userId = paramsManager.getParameter("userId", "testUser123");
        std::string jwtToken = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9";
        requestId = "UDP_history_" + userId + "_" + jwtToken;
        std::cout << "沒有活躍的歷史串流requestId，使用測試requestId: " << requestId << std::endl;
    }

    std::string payload = "{\"camId\":\"" + paramsManager.getCameraId() +
                          "\", \"requestId\":\"" + requestId + "\"}";

    std::cout << "停止歷史串流參數:" << std::endl;
    std::cout << "  - camId: " << paramsManager.getCameraId() << std::endl;
    std::cout << "  - requestId: " << requestId << std::endl;

    std::string response = ChtP2PCameraControlHandler::getInstance().handleControl(_StopVideoHistoryStream, payload);
    std::cout << "處理結果: " << response << std::endl;

    // 清除保存的requestId
    paramsManager.setParameter("historyStreamRequestId", "");
    return true;
}
#if 1 // jeffery added 20250805
bool testGetVideoScheduleStream(ChtP2PCameraAPI &cameraApi)
{
    using namespace rapidjson;

    std::cout << "\n===== 測試開始排程影音串流 =====" << std::endl;

    /*std::cout << "請輸入開始時間戳 (Unix時間戳，留空使用1小時前): ";
    std::string startTime;
    std::getline(std::cin, startTime);
    if (startTime.empty())
        startTime = std::to_string(time(nullptr) - 3600); // 1小時前*/

    auto &paramsManager = CameraParametersManager::getInstance();

    // 生成符合規格的requestId: <UDP/Relay>_history_<userId>_<JWTToken>
    // std::string userId = "testUser123";
    std::string userId = paramsManager.getParameter("userId", "testUser123");
    std::string jwtToken = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9"; // 示例JWT Token
    std::string requestId = "UDP_live_" + userId + "_" + jwtToken;
    std::string startTime = "19700101000000-19700101002000";

    // 設定frameType和imageQuality
    std::string frameType = "rtp";
    std::string imageQuality = "2"; // High bitrate

    std::string payload = "{\"camId\":\"" + paramsManager.getCameraId() +
                          "\", \"requestId\":\"" + requestId +
                          "\", \"frameType\":\"" + frameType +
                          "\", \"imageQuality\":\"" + imageQuality +
                          "\", \"startTime\":\"" + startTime + "\"}";

    std::cout << "測試參數:" << std::endl;
    std::cout << "  - camId: " << paramsManager.getCameraId() << std::endl;
    std::cout << "  - requestId: " << requestId << std::endl;
    std::cout << "  - frameType: " << frameType << std::endl;
    std::cout << "  - imageQuality: " << imageQuality << std::endl;
    std::cout << "  - startTime: " << startTime << std::endl;

    // format the .json file.
    Document doc;
    doc.SetObject();

    Document::AllocatorType &allocator = doc.GetAllocator();

    doc.AddMember("frameType", Value(frameType.c_str(), allocator), allocator);
    doc.AddMember("startTime", Value(startTime.c_str(), allocator), allocator);

    StringBuffer buffer;
    Writer<StringBuffer> writer(buffer);
    doc.Accept(writer);

    std::ofstream ofs("/mnt/sd/EXjson/rtp_schedule.json");
    if (ofs.is_open())
    {
        ofs << buffer.GetString();
        ofs.close();
        std::cout << "JSON saved to rtp_schedule.json\n";
    }
    else
    {
        std::cerr << "Failed to open rtp_schedule.json for writing.\n";
    }

    std::string response = ChtP2PCameraControlHandler::getInstance().handleControl(_GetVideoScheduleStream, payload);
    std::cout << "處理結果: " << response << std::endl;

    // 保存requestId供後續停止使用
    paramsManager.setParameter("scheduleStreamRequestId", requestId);
    return true;
}

bool testStopVideoScheduleStream(ChtP2PCameraAPI &cameraApi)
{
    std::cout << "\n===== 測試停止排程影音串流 =====" << std::endl;
    auto &paramsManager = CameraParametersManager::getInstance();
    std::string requestId = paramsManager.getParameter("scheduleStreamRequestId", "");

    if (requestId.empty())
    {
        // 如果沒有保存的requestId，生成一個測試用的
        // std::string userId = "testUser123";
        std::string userId = paramsManager.getParameter("userId", "testUser123");
        std::string jwtToken = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9";
        requestId = "UDP_history_" + userId + "_" + jwtToken;
        std::cout << "沒有活躍的排程串流requestId，使用測試requestId: " << requestId << std::endl;
    }

    std::string payload = "{\"camId\":\"" + paramsManager.getCameraId() +
                          "\", \"requestId\":\"" + requestId + "\"}";

    std::cout << "停止排程串流參數:" << std::endl;
    std::cout << "  - camId: " << paramsManager.getCameraId() << std::endl;
    std::cout << "  - requestId: " << requestId << std::endl;

    std::string response = ChtP2PCameraControlHandler::getInstance().handleControl(_StopVideoScheduleStream, payload);
    std::cout << "處理結果: " << response << std::endl;

    // 清除保存的requestId
    paramsManager.setParameter("scheduleStreamRequestId", "");
    return true;
}
#endif

bool testSendAudioStream(ChtP2PCameraAPI &cameraApi)
{
    std::cout << "\n===== 測試開始雙向語音串流 =====" << std::endl;
    std::cout << "請選擇音頻編碼 (8=PCMU, 11=G.711): ";
    std::string codec;
    std::getline(std::cin, codec);
    if (codec.empty())
        codec = "11";

    std::cout << "請輸入位元率 (64): ";
    std::string bitRate;
    std::getline(std::cin, bitRate);
    if (bitRate.empty())
        bitRate = "64";

    std::cout << "請輸入取樣率 (8): ";
    std::string sampleRate;
    std::getline(std::cin, sampleRate);
    if (sampleRate.empty())
        sampleRate = "8";

    std::string requestId = "audio_stream_" + std::to_string(time(nullptr));
    std::string payload = "{\"requestId\": \"" + requestId +
                          "\", \"codec\": " + codec +
                          ", \"bitRate\": " + bitRate +
                          ", \"sampleRate\": " + sampleRate + "}";

    std::string response = ChtP2PCameraControlHandler::getInstance().handleControl(_SendAudioStream, payload);
    std::cout << "處理結果: " << response << std::endl;

    // 保存requestId供後續停止使用
    auto &paramsManager = CameraParametersManager::getInstance();
    paramsManager.setParameter("audioStreamRequestId", requestId);
    return true;
}

bool testStopAudioStream(ChtP2PCameraAPI &cameraApi)
{
    std::cout << "\n===== 測試停止雙向語音串流 =====" << std::endl;
    auto &paramsManager = CameraParametersManager::getInstance();
    std::string requestId = paramsManager.getParameter("audioStreamRequestId", "");
    if (requestId.empty())
    {
        std::cout << "沒有活躍的音頻串流，請先開始音頻串流" << std::endl;
        return false;
    }

    std::string payload = "{\"requestId\": \"" + requestId + "\"}";

    std::string response = ChtP2PCameraControlHandler::getInstance().handleControl(_StopAudioStream, payload);
    std::cout << "處理結果: " << response << std::endl;

    // 清除保存的requestId
    paramsManager.setParameter("audioStreamRequestId", "");
    return true;
}

// ===== 新增：串流管理器直接測試函數 =====

bool testStreamManagerLiveVideo(ChtP2PCameraAPI &cameraApi)
{
    std::cout << "\n===== 測試串流管理器即時影音 =====" << std::endl;

    auto &streamManager = StreamManager::getInstance();

    std::string requestId = "sm_live_" + std::to_string(time(nullptr));

    // 設置影音參數
    VideoCodecParams videoParams;
    videoParams.codec = 2;    // H.264
    videoParams.width = 1920; // 1080p
    videoParams.height = 1080;
    videoParams.fps = 30;

    AudioCodecParams audioParams;
    audioParams.codec = 13; // 11:G711, 12:G729, 13:AAC
    audioParams.bitRate = 64;
    audioParams.sampleRate = 8;

    std::cout << "啟動即時串流 - RequestID: " << requestId << std::endl;
    std::cout << "使用測試伺服器IP: " << getTestServerIP() << std::endl;
    bool result = streamManager.startLiveVideoStream(requestId, videoParams, audioParams, getTestServerIP());

    if (result)
    {
        std::cout << "✓ 即時串流啟動成功" << std::endl;
        std::cout << "串流將運行10秒後自動停止..." << std::endl;

        // 等待10秒
        std::this_thread::sleep_for(std::chrono::seconds(10));

        bool stopResult = streamManager.stopLiveVideoStream(requestId);
        std::cout << (stopResult ? "✓ 即時串流已停止" : "✗ 停止即時串流失敗") << std::endl;
    }
    else
    {
        std::cout << "✗ 即時串流啟動失敗" << std::endl;
    }

    return result;
}

bool testStreamManagerHistoryVideo(ChtP2PCameraAPI &cameraApi)
{
    std::cout << "\n===== 測試串流管理器歷史影音 =====" << std::endl;

    auto &streamManager = StreamManager::getInstance();

    std::string requestId = "sm_history_" + std::to_string(time(nullptr));
    long startTime = time(nullptr) - 3600; // 1小時前

    // 設置影音參數
    VideoCodecParams videoParams;
    videoParams.codec = 2;    // H.264
    videoParams.width = 1280; // 720p
    videoParams.height = 720;
    videoParams.fps = 10;

    AudioCodecParams audioParams;
    audioParams.codec = 11; // G.711
    audioParams.bitRate = 64;
    audioParams.sampleRate = 8;

    std::cout << "啟動歷史串流 - RequestID: " << requestId
              << ", 開始時間: " << startTime << std::endl;
    std::cout << "使用測試伺服器IP: " << getTestServerIP() << std::endl;

    bool result = streamManager.startHistoryVideoStream(requestId, startTime, videoParams, audioParams, getTestServerIP());

    if (result)
    {
        std::cout << "✓ 歷史串流啟動成功" << std::endl;
        std::cout << "串流將運行15秒後自動停止..." << std::endl;

        // 等待15秒
        std::this_thread::sleep_for(std::chrono::seconds(15));

        bool stopResult = streamManager.stopHistoryVideoStream(requestId);
        std::cout << (stopResult ? "✓ 歷史串流已停止" : "✗ 停止歷史串流失敗") << std::endl;
    }
    else
    {
        std::cout << "✗ 歷史串流啟動失敗" << std::endl;
    }

    return result;
}

bool testStreamManagerAudio(ChtP2PCameraAPI &cameraApi)
{
    std::cout << "\n===== 測試串流管理器音頻串流 =====" << std::endl;

    auto &streamManager = StreamManager::getInstance();

    std::string requestId = "sm_audio_" + std::to_string(time(nullptr));

    // 設置音頻參數
    AudioCodecParams audioParams;
    audioParams.codec = 11; // G.711
    audioParams.bitRate = 64;
    audioParams.sampleRate = 8;

    std::cout << "啟動音頻串流 - RequestID: " << requestId << std::endl;
    bool result = streamManager.startAudioStream(requestId, audioParams);

    if (result)
    {
        std::cout << "✓ 音頻串流啟動成功" << std::endl;
        std::cout << "串流將運行8秒後自動停止..." << std::endl;

        // 等待8秒
        std::this_thread::sleep_for(std::chrono::seconds(8));

        bool stopResult = streamManager.stopAudioStream(requestId);
        std::cout << (stopResult ? "✓ 音頻串流已停止" : "✗ 停止音頻串流失敗") << std::endl;
    }
    else
    {
        std::cout << "✗ 音頻串流啟動失敗" << std::endl;
    }

    return result;
}

// ===== 新增：回報機制測試函數 =====

bool testReportSnapshot(ChtP2PCameraAPI &cameraApi)
{
    std::cout << "\n===== 測試截圖事件回報 =====" << std::endl;

    std::string eventId = "test_snap_" + std::to_string(time(nullptr));
    std::string snapshotTime = getFormattedTimestamp();
    std::string filePath = "/tmp/test_snapshot.jpg";

    std::cout << "回報截圖事件 - EventID: " << eventId << std::endl;
    std::cout << "截圖時間: " << snapshotTime << std::endl;
    std::cout << "檔案路徑: " << filePath << std::endl;

    bool result = cameraApi.reportSnapshot(eventId, snapshotTime, filePath);
    std::cout << (result ? "✓ 截圖事件回報成功" : "✗ 截圖事件回報失敗") << std::endl;

    return result;
}

bool testReportRecord(ChtP2PCameraAPI &cameraApi)
{
    std::cout << "\n===== 測試錄影事件回報 =====" << std::endl;

    std::string eventId = "test_rec_" + std::to_string(time(nullptr));

    // 模擬錄影時間範圍
    auto now = std::chrono::system_clock::now();
    auto fromTime = now - std::chrono::seconds(60);
    auto toTime = now;

    auto fromTime_t = std::chrono::system_clock::to_time_t(fromTime);
    auto toTime_t = std::chrono::system_clock::to_time_t(toTime);

    std::stringstream fromSS, toSS;
    fromSS << std::put_time(std::localtime(&fromTime_t), "%Y-%m-%d %H:%M:%S") << ".000";
    toSS << std::put_time(std::localtime(&toTime_t), "%Y-%m-%d %H:%M:%S") << ".000";

    std::string filePath = "/tmp/test_record.mp4";
    std::string thumbnailfilePath = "/tmp/test_record.jpg";

    std::cout << "回報錄影事件 - EventID: " << eventId << std::endl;
    std::cout << "錄影時間: " << fromSS.str() << " 到 " << toSS.str() << std::endl;
    std::cout << "檔案路徑: " << filePath << std::endl;

    bool result = cameraApi.reportRecord(eventId, fromSS.str(), toSS.str(), filePath, thumbnailfilePath);
    std::cout << (result ? "✓ 錄影事件回報成功" : "✗ 錄影事件回報失敗") << std::endl;

    return result;
}

bool testReportRecognition(ChtP2PCameraAPI &cameraApi)
{
    std::cout << "\n===== 測試辨識事件回報 =====" << std::endl;

    std::string eventId = "test_ai_" + std::to_string(time(nullptr));
    std::string eventTime = getFormattedTimestamp();

    std::cout << "請選擇事件類型 (1=EED, 2=FR, 3=FED, 4=BD): ";
    std::string choice;
    std::getline(std::cin, choice);

    std::string eventType = "EED";
    std::string eventClass = "person";

    if (choice == "2")
    {
        eventType = "FR";
        eventClass = "face";
    }
    else if (choice == "3")
    {
        eventType = "FED";
        eventClass = "person";
    }
    else if (choice == "4")
    {
        eventType = "BD";
        eventClass = "motion";
    }

    std::string videoFilePath = "/tmp/test_recognition.mp4";
    std::string snapshotFilePath = "/tmp/test_recognition.jpg";
    std::string audioFilePath = "/tmp/test_recognition.aac";
    std::string coordinate = "121.5654,25.0330"; // 台北座標
    std::string resultAttribute = "{\"confidence\":0.95,\"objectCount\":1}";

    std::cout << "回報辨識事件 - EventID: " << eventId << std::endl;
    std::cout << "事件類型: " << eventType << ", 事件類別: " << eventClass << std::endl;
    std::cout << "事件時間: " << eventTime << std::endl;

    bool result = cameraApi.reportRecognition(
        eventId, eventTime, eventType, eventClass,
        videoFilePath, snapshotFilePath, audioFilePath,
        coordinate, resultAttribute);

    std::cout << (result ? "✓ 辨識事件回報成功" : "✗ 辨識事件回報失敗") << std::endl;

    return result;
}

bool testReportStatus(ChtP2PCameraAPI &cameraApi)
{
    std::cout << "\n===== 測試狀態事件回報 =====" << std::endl;

    std::string eventId = "test_status_" + std::to_string(time(nullptr));

    std::cout << "請選擇狀態類型 (1=正常, 2=異常): ";
    std::string choice;
    std::getline(std::cin, choice);

    int type = (choice == "2") ? 4 : 2; // 2=正常, 4=異常
    std::string status = (choice == "2") ? "Abnormal" : "Normal";

    auto &paramsManager = CameraParametersManager::getInstance();
    std::string camId = paramsManager.getCameraId();

    std::cout << "回報狀態事件 - EventID: " << eventId << std::endl;
    std::cout << "攝影機ID: " << camId << ", 狀態: " << status << std::endl;

    bool result = cameraApi.reportStatusEvent(eventId, type, camId, status, true);
    std::cout << (result ? "✓ 狀態事件回報成功" : "✗ 狀態事件回報失敗") << std::endl;

    return result;
}

bool testReportManagerControl(ChtP2PCameraAPI &cameraApi)
{
    std::cout << "\n===== 測試回報管理器控制 =====" << std::endl;

    if (!g_reportManager)
    {
        g_reportManager = std::make_unique<ReportManager>(&cameraApi);
        std::cout << "回報管理器已初始化" << std::endl;
    }

    std::cout << "請選擇操作:" << std::endl;
    std::cout << "1 - 啟動回報機制" << std::endl;
    std::cout << "2 - 停止回報機制" << std::endl;
    std::cout << "3 - 顯示回報狀態" << std::endl;
    std::cout << "4 - 設定回報間隔" << std::endl;
    std::cout << "請輸入選擇: ";

    std::string choice;
    std::getline(std::cin, choice);

    if (choice == "1")
    {
        g_reportManager->start();
        std::cout << "回報機制已啟動" << std::endl;
    }
    else if (choice == "2")
    {
        g_reportManager->stop();
        std::cout << "回報機制已停止" << std::endl;
    }
    else if (choice == "3")
    {
        g_reportManager->printStatus();
    }
    else if (choice == "4")
    {
        std::cout << "請選擇回報類型 (snapshot/record/recognition/status): ";
        std::string type;
        std::getline(std::cin, type);

        std::cout << "請輸入間隔秒數 (5-300): ";
        std::string intervalStr;
        std::getline(std::cin, intervalStr);

        try
        {
            int interval = std::stoi(intervalStr);
            if (interval >= 5 && interval <= 300)
            {
                g_reportManager->setInterval(type, interval);
                std::cout << "間隔設定成功: " << type << " = " << interval << " 秒" << std::endl;
            }
            else
            {
                std::cout << "間隔必須在5-300秒之間" << std::endl;
            }
        }
        catch (...)
        {
            std::cout << "無效的間隔數值" << std::endl;
        }
    }
    else
    {
        std::cout << "無效的選擇" << std::endl;
        return false;
    }

    return true;
}
/**
 * 加入到 cht_p2p_camera_example_menu.cpp 的測試選單函數
 */

bool testDisplayTimezoneStatus(ChtP2PCameraAPI &cameraApi)
{
    std::cout << "\n===== 顯示時區狀態 =====" << std::endl;

    ChtP2PCameraControlHandler::displayCurrentTimezoneStatus();

    return true;
}

bool testReloadTimezone(ChtP2PCameraAPI &cameraApi)
{
    std::cout << "\n===== 重新載入時區設定 =====" << std::endl;

    bool reloadResult = ChtP2PCameraControlHandler::reloadSystemTimezone();

    if (reloadResult)
    {
        std::cout << "✓ 時區設定重新載入成功" << std::endl;
    }
    else
    {
        std::cout << "✗ 時區設定重新載入失敗" << std::endl;
    }

    return reloadResult;
}

bool initializeSystemTimezone()
{
    std::cout << "=========================" << std::endl;
    std::cout << "     初始化系統時區..." << std::endl;
    std::cout << "=========================" << std::endl;
    std::cout.flush();

    try
    {
        auto &paramsManager = CameraParametersManager::getInstance();

        // 使用新的整合方法
        bool result = paramsManager.initializeTimezoneWithNtpSync();

        if (result)
        {
            std::cout << "✓ 時區和 NTP 初始化成功" << std::endl;
        }
        else
        {
            std::cout << "✗ 時區和 NTP 初始化失敗" << std::endl;
        }

        return result;
    }
    catch (const std::exception &e)
    {
        std::cerr << "初始化時區時發生異常: " << e.what() << std::endl;
        return false;
    }
}

// 新增：NTP 同步測試函數
bool testNtpSync(ChtP2PCameraAPI &cameraApi)
{
    std::cout << "\n===== 測試 NTP 時間同步 =====" << std::endl;

    auto &paramsManager = CameraParametersManager::getInstance();

    // 顯示當前 NTP 設定
    std::string currentNtpServer = paramsManager.getNtpServer();
    std::cout << "當前 NTP 伺服器: " << currentNtpServer << std::endl;

    std::cout << "請選擇操作:" << std::endl;
    std::cout << "1 - 使用當前設定同步時間" << std::endl;
    std::cout << "2 - 手動輸入 NTP 伺服器並同步" << std::endl;
    std::cout << "3 - 重設為預設 NTP 伺服器" << std::endl;
    std::cout << "請輸入選擇: ";

    std::string choice;
    std::getline(std::cin, choice);

    if (choice == "1")
    {
        std::cout << "使用當前 NTP 伺服器同步時間..." << std::endl;
        bool result = paramsManager.updateSystemTimeFromNtp();
        std::cout << (result ? "✓ NTP 同步成功" : "✗ NTP 同步失敗") << std::endl;
        return result;
    }
    else if (choice == "2")
    {
        std::cout << "請輸入 NTP 伺服器地址: ";
        std::string ntpServer;
        std::getline(std::cin, ntpServer);

        if (ntpServer.empty())
        {
            std::cout << "NTP 伺服器地址不能為空" << std::endl;
            return false;
        }

        std::cout << "使用 " << ntpServer << " 同步時間..." << std::endl;
        bool result = paramsManager.syncTimeWithNtp(ntpServer);

        if (result)
        {
            std::cout << "✓ NTP 同步成功，是否要將此伺服器設為預設？(y/n): ";
            std::string saveChoice;
            std::getline(std::cin, saveChoice);

            if (saveChoice == "y" || saveChoice == "Y")
            {
                paramsManager.setNtpServer(ntpServer);
                paramsManager.saveToFile();
                std::cout << "✓ NTP 伺服器設定已保存" << std::endl;
            }
        }
        else
        {
            std::cout << "✗ NTP 同步失敗" << std::endl;
        }

        return result;
    }
    else if (choice == "3")
    {
        std::cout << "重設為預設 NTP 伺服器 (tock.stdtime.gov.tw)..." << std::endl;
        paramsManager.setNtpServer("tock.stdtime.gov.tw");
        paramsManager.saveToFile();

        bool result = paramsManager.updateSystemTimeFromNtp();
        std::cout << (result ? "✓ 重設並同步成功" : "✗ 重設成功但同步失敗") << std::endl;
        return true;
    }
    else
    {
        std::cout << "無效的選擇" << std::endl;
        return false;
    }
}

// 新增：顯示 NTP 狀態的函數
bool testDisplayNtpStatus(ChtP2PCameraAPI &cameraApi)
{
    std::cout << "\n===== NTP 狀態資訊 =====" << std::endl;

    auto &paramsManager = CameraParametersManager::getInstance();

    std::cout << "NTP 伺服器設定: " << paramsManager.getNtpServer() << std::endl;

    std::string lastSyncStr = paramsManager.getParameter("lastNtpSync", "0");
    std::string lastServer = paramsManager.getParameter("lastNtpServer", "未知");
    std::string lastError = paramsManager.getParameter("lastNtpError", "無");

    time_t lastSync = std::stol(lastSyncStr);

    if (lastSync > 0)
    {
        std::cout << "上次同步時間: " << std::ctime(&lastSync);
        std::cout << "上次使用伺服器: " << lastServer << std::endl;

        time_t now = std::time(nullptr);
        int minutesAgo = (now - lastSync) / 60;
        std::cout << "距離上次同步: " << minutesAgo << " 分鐘前" << std::endl;
    }
    else
    {
        std::cout << "從未進行過 NTP 同步" << std::endl;
    }

    if (lastError != "無")
    {
        std::cout << "最後錯誤: " << lastError << std::endl;
    }

    std::cout << "\n當前系統時間: ";
    if (system("date") != 0)
    {
        std::cout << "無法獲取系統時間" << std::endl;
    }

    return true;
}

bool testReinitializeTimezone(ChtP2PCameraAPI &cameraApi)
{
    std::cout << "\n===== 重新初始化時區 =====" << std::endl;

    bool initResult = initializeSystemTimezone();

    if (initResult)
    {
        std::cout << "✓ 時區重新初始化成功" << std::endl;
    }
    else
    {
        std::cout << "✗ 時區重新初始化失敗" << std::endl;
    }

    return initResult;
}

// ===== 特殊功能測試函數 =====

void displayCurrentStatus()
{
    std::cout << "\n===== 當前系統狀態 =====" << std::endl;
    auto &paramsManager = CameraParametersManager::getInstance();

    std::cout << "基本資訊:" << std::endl;
    std::cout << "  - Camera ID: " << paramsManager.getCameraId() << std::endl;
    std::cout << "  - Camera Name: " << paramsManager.getCameraName() << std::endl;
    std::cout << "  - Active Status: " << paramsManager.getActiveStatus() << std::endl;
    std::cout << "  - Device Status: " << paramsManager.getDeviceStatus() << std::endl;
    std::cout << "  - Time Zone: " << paramsManager.getTimeZone() << std::endl;

    std::cout << "\n網路資訊:" << std::endl;
    std::cout << "  - WiFi SSID: " << paramsManager.getWifiSsid() << std::endl;
    std::cout << "  - WiFi Signal: " << paramsManager.getWifiSignalStrength() << " dBm" << std::endl;
    std::cout << "  - IP Address: " << paramsManager.getParameter("ipAddress", "Unknown") << std::endl;
    std::cout << "  - MAC Address: " << paramsManager.getParameter("macAddress", "Unknown") << std::endl;

    std::cout << "\n硬體資訊:" << std::endl;
    std::cout << "  - Firmware Version: " << paramsManager.getFirmwareVersion() << std::endl;
    std::cout << "  - Storage Health: " << paramsManager.getStorageHealth() << std::endl;
    std::cout << "  - Storage Capacity: " << paramsManager.getStorageCapacity() << " MB" << std::endl;
    std::cout << "  - Storage Available: " << paramsManager.getStorageAvailable() << " MB" << std::endl;

    std::cout << "\n功能設定:" << std::endl;
    std::cout << "  - Image Quality: " << paramsManager.getImageQuality() << std::endl;
    std::cout << "  - Microphone Enabled: " << (paramsManager.getMicrophoneEnabled() ? "Yes" : "No") << std::endl;
    std::cout << "  - Speaker Volume: " << paramsManager.getSpeakerVolume() << std::endl;
}

void runCompleteTestSuite(ChtP2PCameraAPI &cameraApi)
{
    std::cout << "\n===== 執行完整測試流程 =====" << std::endl;

    // 基本狀態測試
    std::cout << "\n[1/9] 測試基本狀態功能..." << std::endl;
    testGetCamStatusById(cameraApi);
    testGetTimeZone(cameraApi);
    testSetTimeZone(cameraApi, "51"); // 設置為台北時區
    // std::string currentTzId = TimezoneUtils::getDefaultTimezoneId(); // 預設台北時區
    // testSetTimeZone(cameraApi, currentTzId); // 設置為台北時區

    // 影像設定測試
    std::cout << "\n[2/9] 測試影像設定功能..." << std::endl;
    testSetImageQuality(cameraApi);
    testSetCameraOSD(cameraApi);
    testSetNightMode(cameraApi);

    // 音頻測試
    std::cout << "\n[3/9] 測試音頻功能..." << std::endl;
    testSetMicrophone(cameraApi);
    testSetSpeak(cameraApi);

    // 系統控制測試
    std::cout << "\n[4/9] 測試系統控制功能..." << std::endl;
    testSetLED(cameraApi);
    testGetSnapshotHamiCamDevice(cameraApi);

    // 存儲管理測試
    std::cout << "\n[5/9] 測試存儲管理功能..." << std::endl;
    testSetCamStorageDay(cameraApi);
    testSetCamEventStorageDay(cameraApi);

    // PTZ控制測試
    std::cout << "\n[6/9] 測試PTZ控制功能..." << std::endl;
    testHamiCamGetPtzControl(cameraApi);
    testHamiCamPtzControlConfigSpeed(cameraApi);

    // AI設定測試
    std::cout << "\n[7/9] 測試AI設定功能..." << std::endl;
    testUpdateCameraAISetting(cameraApi);
    testGetCameraAISetting(cameraApi);

    // 串流控制測試
    std::cout << "\n[8/9] 測試串流控制功能..." << std::endl;
    testGetVideoLiveStream(cameraApi);
    std::this_thread::sleep_for(std::chrono::seconds(2));
    testStopVideoLiveStream(cameraApi);

    // 新增：NTP 相關測試
    std::cout << "\n[9/9] 測試 NTP 時間同步..." << std::endl;
    testDisplayNtpStatus(cameraApi);
    testNtpSync(cameraApi);

    std::cout << "\n===== 完整測試流程完成 =====" << std::endl;
}

void runTimeZoneBatchTest(ChtP2PCameraAPI &cameraApi)
{
    std::cout << "\n===== 時區批次測試 =====" << std::endl;
    // std::vector<std::string> testTimeZones = {"1", "9", "20", "51"}; // 倫敦、香港、紐約、台北
    std::vector<std::string> testTimeZones = {
        "1",                                  // 倫敦
        "9",                                  // 香港
        "20",                                 // 紐約
        TimezoneUtils::getDefaultTimezoneId() // 台北（預設）
    };
    for (const auto &tz : testTimeZones)
    {
        std::cout << "\n測試時區 " << tz << "..." << std::endl;
        testSetTimeZone(cameraApi, tz);
        std::this_thread::sleep_for(std::chrono::seconds(1));
        testGetTimeZone(cameraApi);
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    std::cout << "\n===== 時區批次測試完成 =====" << std::endl;
}

void runPTZBatchTest(ChtP2PCameraAPI &cameraApi)
{
    std::cout << "\n===== PTZ批次測試 =====" << std::endl;

    // 測試PTZ移動
    std::vector<std::string> ptzCommands = {"left", "right", "up", "down", "stop"};
    for (const auto &cmd : ptzCommands)
    {
        std::cout << "\n測試PTZ命令: " << cmd << std::endl;
        auto &paramsManager = CameraParametersManager::getInstance();
        std::string payload = "{\"camId\": \"" + paramsManager.getCameraId() +
                              "\", \"cmd\": \"" + cmd + "\"}";
        std::string response = ChtP2PCameraControlHandler::getInstance().handleControl(_HamiCamPtzControlMove, payload);
        std::cout << "結果: " << response << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    // 測試PTZ速度設定
    for (int speed = 0; speed <= 2; speed++)
    {
        std::cout << "\n測試PTZ速度: " << speed << std::endl;
        auto &paramsManager = CameraParametersManager::getInstance();
        std::string payload = "{\"camId\": \"" + paramsManager.getCameraId() +
                              "\", \"speed\": " + std::to_string(speed) + "}";
        std::string response = ChtP2PCameraControlHandler::getInstance().handleControl(_HamiCamPtzControlConfigSpeed, payload);
        std::cout << "結果: " << response << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    std::cout << "\n===== PTZ批次測試完成 =====" << std::endl;
}

void runStreamBatchTest(ChtP2PCameraAPI &cameraApi)
{
    std::cout << "\n===== 串流功能批次測試 =====" << std::endl;

    // 測試即時串流
    std::cout << "\n[1/3] 測試即時影音串流..." << std::endl;
    testStreamManagerLiveVideo(cameraApi);

    // 測試歷史串流
    std::cout << "\n[2/3] 測試歷史影音串流..." << std::endl;
    testStreamManagerHistoryVideo(cameraApi);

    // 測試音頻串流
    std::cout << "\n[3/3] 測試音頻串流..." << std::endl;
    testStreamManagerAudio(cameraApi);

    std::cout << "\n===== 串流功能批次測試完成 =====" << std::endl;
}

void runReportBatchTest(ChtP2PCameraAPI &cameraApi)
{
    std::cout << "\n===== 回報機制批次測試 =====" << std::endl;

    // 測試各種回報
    std::cout << "\n[1/4] 測試截圖事件回報..." << std::endl;
    testReportSnapshot(cameraApi);

    std::cout << "\n[2/4] 測試錄影事件回報..." << std::endl;
    testReportRecord(cameraApi);

    std::cout << "\n[3/4] 測試辨識事件回報..." << std::endl;
    // 自動選擇EED事件
    std::cout << "自動測試EED事件..." << std::endl;
    std::string eventId = "batch_ai_" + std::to_string(time(nullptr));
    std::string eventTime = getFormattedTimestamp();
    bool result = cameraApi.reportRecognition(
        eventId, eventTime, "EED", "person",
        "/tmp/batch_recognition.mp4", "/tmp/batch_recognition.jpg",
        "/tmp/batch_recognition.aac", "121.5654,25.0330",
        "{\"confidence\":0.88,\"objectCount\":1}");
    std::cout << (result ? "✓ 辨識事件回報成功" : "✗ 辨識事件回報失敗") << std::endl;

    std::cout << "\n[4/4] 測試狀態事件回報..." << std::endl;
    testReportStatus(cameraApi);

    std::cout << "\n===== 回報機制批次測試完成 =====" << std::endl;
}

bool testSetTimeZoneSimplified(ChtP2PCameraAPI &cameraApi, const std::string &tId = "")
{
    std::cout << "\n===== 簡化時區設置測試 =====" << std::endl;

    std::string timeZoneId = tId;
    if (timeZoneId.empty())
    {
        std::cout << "請輸入時區ID (0-51, 預設51為台北): ";
        std::getline(std::cin, timeZoneId);
        if (timeZoneId.empty())
            timeZoneId = "51";
    }

    // 顯示時區資訊
    if (TimezoneUtils::isValidTimezoneId(timeZoneId))
    {
        TimezoneInfo tzInfo = TimezoneUtils::getTimezoneInfo(timeZoneId);
        std::cout << "即將設置時區: " << tzInfo.displayName << std::endl;
        std::cout << "UTC偏移: " << tzInfo.baseUtcOffset << " 秒" << std::endl;
    }

    auto &paramsManager = CameraParametersManager::getInstance();
    std::string payload = "{\"camId\": \"" + paramsManager.getCameraId() + "\", \"tId\": \"" + timeZoneId + "\"}";

    std::string response = ChtP2PCameraControlHandler::getInstance().handleControl(_SetTimeZone, payload);
    std::cout << "處理結果: " << response << std::endl;

    return true;
}
/**
 * @brief 根據baseUtcOffset顯示時間
 */
std::string getTimeWithOffset(const std::string &baseUtcOffset)
{
    try
    {
        // 獲取當前UTC時間
        auto now = std::chrono::system_clock::now();
        auto utc_time = std::chrono::system_clock::to_time_t(now);

        // 加上時區偏移
        int offset_seconds = std::stoi(baseUtcOffset);
        time_t local_time = utc_time + offset_seconds;

        // 格式化時間
        std::tm *tm_local = std::gmtime(&local_time);
        char buffer[64];
        std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", tm_local);

        return std::string(buffer);
    }
    catch (const std::exception &e)
    {
        std::cerr << "計算時間偏移失敗: " << e.what() << std::endl;
        return "";
    }
}
/**
 * @brief 簡化的NTP同步函數
 */
bool performNtpSync()
{
    std::cout << "執行NTP時間同步..." << std::endl;

    auto &driver = CameraDriver::getInstance();
    if (driver.isSimulationMode())
    {
        std::cout << "模擬模式：模擬NTP同步完成" << std::endl;
        return true;
    }

    // 嘗試使用台灣的NTP服務器
    std::vector<std::string> ntpServers = {
        "tock.stdtime.gov.tw",
        "tick.stdtime.gov.tw",
        "time.stdtime.gov.tw"};

    for (const auto &server : ntpServers)
    {
        std::string ntpCmd = "ntpdate -b -u " + server + " 2>/dev/null";
        std::cout << "嘗試同步: " << server << std::endl;

        int result = system(ntpCmd.c_str());
        if (result == 0)
        {
            std::cout << "✓ NTP同步成功: " << server << std::endl;
            std::cout << "同步後時間: ";
            if (system("date") != 0)
            {
                std::cout << "無法獲取系統時間" << std::endl;
            }
            return true;
        }
    }

    std::cout << "✗ 所有NTP服務器同步失敗" << std::endl;
    return false;
}
bool testGetTimeZoneSimplified(ChtP2PCameraAPI &cameraApi)
{
    std::cout << "\n===== 簡化時區獲取測試 =====" << std::endl;

    auto &paramsManager = CameraParametersManager::getInstance();
    std::string payload = "{\"camId\": \"" + paramsManager.getCameraId() + "\"}";

    std::string response = ChtP2PCameraControlHandler::getInstance().handleControl(_GetTimeZone, payload);
    std::cout << "處理結果: " << response << std::endl;

    // 解析回應顯示詳細資訊
    try
    {
        rapidjson::Document responseJson;
        if (responseJson.Parse(response.c_str()).HasParseError())
        {
            std::cout << "回應解析失敗" << std::endl;
            return false;
        }

        if (responseJson.HasMember(PAYLOAD_KEY_RESULT) && responseJson[PAYLOAD_KEY_RESULT].GetInt() == 1)
        {
            if (responseJson.HasMember("timezone"))
            {
                std::string currentTzId = responseJson["timezone"].GetString();
                TimezoneInfo tzInfo = TimezoneUtils::getTimezoneInfo(currentTzId);

                std::cout << "\n當前時區詳細資訊:" << std::endl;
                std::cout << "  時區ID: " << currentTzId << std::endl;
                std::cout << "  描述: " << tzInfo.displayName << std::endl;
                std::cout << "  UTC偏移: " << tzInfo.baseUtcOffset << " 秒" << std::endl;

                // 顯示該時區的當前時間
                if (!tzInfo.baseUtcOffset.empty())
                {
                    std::string offsetTime = getTimeWithOffset(tzInfo.baseUtcOffset);
                    if (!offsetTime.empty())
                    {
                        std::cout << "  該時區時間: " << offsetTime << std::endl;
                    }
                }
            }
        }
    }
    catch (const std::exception &e)
    {
        std::cout << "解析回應時發生異常: " << e.what() << std::endl;
    }

    return true;
}

bool testNtpSyncSimplified(ChtP2PCameraAPI &cameraApi)
{
    std::cout << "\n===== 簡化NTP同步測試 =====" << std::endl;

    std::cout << "同步前系統時間: ";
    if (system("date") != 0)
    {
        std::cout << "無法獲取系統時間" << std::endl;
    }

    bool result = performNtpSync();

    if (result)
    {
        std::cout << "✓ NTP同步成功" << std::endl;
    }
    else
    {
        std::cout << "✗ NTP同步失敗" << std::endl;
    }

    return result;
}

bool testTimezoneWithNtpDemo(ChtP2PCameraAPI &cameraApi)
{
    std::cout << "\n===== 時區+NTP綜合演示 =====" << std::endl;

    // 步驟1: 先同步NTP時間
    std::cout << "\n[步驟1] 同步網路時間..." << std::endl;
    performNtpSync();

    // 步驟2: 設置時區
    std::cout << "\n[步驟2] 設置時區..." << std::endl;
    std::string tzId = "51"; // 台北時區
    testSetTimeZoneSimplified(cameraApi, tzId);

    // 步驟3: 顯示各時區的時間
    std::cout << "\n[步驟3] 顯示各時區當前時間..." << std::endl;

    std::vector<std::string> demoTimeZones = {"1", "9", "20", "51"}; // 倫敦、香港、紐約、台北

    for (const auto &id : demoTimeZones)
    {
        TimezoneInfo tzInfo = TimezoneUtils::getTimezoneInfo(id);
        if (!tzInfo.tId.empty() && !tzInfo.baseUtcOffset.empty())
        {
            std::string offsetTime = getTimeWithOffset(tzInfo.baseUtcOffset);
            std::cout << "  " << tzInfo.displayName << ": " << offsetTime << std::endl;
        }
    }

    return true;
}
// 顯示各時區當前時間的演示函數
bool testDisplayAllTimezoneCurrentTime(ChtP2PCameraAPI &cameraApi)
{
    std::cout << "\n===== 顯示各時區當前時間 =====" << std::endl;

    auto timezoneList = TimezoneUtils::getAllTimezoneInfo();

    std::cout << "╔══════════════════════════════════════════════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║                                   各時區當前時間                                           ║" << std::endl;
    std::cout << "╠══════════════════════════════════════════════════════════════════════════════════════════╣" << std::endl;

    for (const auto &tz : timezoneList)
    {
        if (!tz.baseUtcOffset.empty())
        {
            std::string offsetTime = getTimeWithOffset(tz.baseUtcOffset);
            if (!offsetTime.empty())
            {
                std::cout << "║ " << std::setw(2) << tz.tId << " │ "
                          << std::setw(35) << std::left << tz.displayName.substr(0, 35) << " │ "
                          << std::setw(19) << offsetTime << " ║" << std::endl;
            }
        }
    }

    std::cout << "╚════╧═════════════════════════════════════════╧═════════════════════╝" << std::endl;

    return true;
}
// 顯示所有支援的時區列表
bool testDisplayAllTimezones(ChtP2PCameraAPI &cameraApi)
{
    std::cout << "\n===== 顯示所有支援的時區列表 =====" << std::endl;

    try
    {
        TimezoneUtils::displayTimezoneList();
        return true;
    }
    catch (const std::exception &e)
    {
        std::cerr << "顯示時區列表時發生異常: " << e.what() << std::endl;

        // 提供調試資訊
        std::cout << "\n調試資訊:" << std::endl;
        TimezoneUtils::debugTimezoneData();
        return false;
    }
}

// 搜尋時區功能
bool testSearchTimezone(ChtP2PCameraAPI &cameraApi)
{
    std::cout << "\n===== 搜尋時區 =====" << std::endl;

    std::cout << "請輸入搜尋關鍵字: ";
    std::string searchTerm;
    std::getline(std::cin, searchTerm);

    if (searchTerm.empty())
    {
        std::cout << "搜尋關鍵字不能為空" << std::endl;
        return false;
    }

    try
    {
        auto results = TimezoneUtils::searchTimezoneByName(searchTerm);

        if (results.empty())
        {
            std::cout << "沒有找到包含 \"" << searchTerm << "\" 的時區" << std::endl;
            std::cout << "\n可嘗試的搜尋關鍵字:" << std::endl;
            std::cout << "  • taipei, taiwan (台灣)" << std::endl;
            std::cout << "  • beijing, hong kong, singapore (中國地區)" << std::endl;
            std::cout << "  • tokyo, seoul (日韓)" << std::endl;
            std::cout << "  • pacific, eastern, central (美國)" << std::endl;
            std::cout << "  • amsterdam, berlin, london (歐洲)" << std::endl;
            std::cout << "  • sydney, canberra (澳洲)" << std::endl;
            std::cout << "  • GMT, UTC (按時間偏移)" << std::endl;

            // 顯示調試資訊
            std::cout << "\n--- 調試資訊 ---" << std::endl;
            TimezoneUtils::debugTimezoneData();
            return false;
        }

        std::cout << "\n搜尋結果 (共找到 " << results.size() << " 個時區):" << std::endl;
        std::cout << "╔══════════════════════════════════════════════════════════════════════════════════════════╗" << std::endl;

        for (size_t i = 0; i < results.size(); ++i)
        {
            std::cout << "║ " << std::setw(2) << (i + 1) << ". "
                      << std::setw(82) << std::left << results[i] << " ║" << std::endl;
        }

        std::cout << "╚══════════════════════════════════════════════════════════════════════════════════════════╝" << std::endl;

        std::cout << "\n是否要直接設定其中一個時區？(y/n): ";
        std::string setChoice;
        std::getline(std::cin, setChoice);

        if (setChoice == "y" || setChoice == "Y")
        {
            std::cout << "請輸入要設定的時區編號 (1-" << results.size() << "): ";
            std::string indexStr;
            std::getline(std::cin, indexStr);

            try
            {
                int index = std::stoi(indexStr) - 1;
                if (index >= 0 && index < static_cast<int>(results.size()))
                {
                    // 從結果中提取時區ID
                    std::string result = results[index];
                    size_t idPos = result.find("ID: ");
                    size_t dashPos = result.find(" - ");

                    if (idPos != std::string::npos && dashPos != std::string::npos)
                    {
                        std::string timezoneId = result.substr(idPos + 4, dashPos - idPos - 4);

                        std::cout << "設定時區為: " << timezoneId << std::endl;
                        return testSetTimeZone(cameraApi, timezoneId);
                    }
                }

                std::cout << "無效的編號" << std::endl;
            }
            catch (...)
            {
                std::cout << "無效的輸入" << std::endl;
            }
        }

        return true;
    }
    catch (const std::exception &e)
    {
        std::cerr << "搜尋時區時發生異常: " << e.what() << std::endl;
        return false;
    }
}

// 顯示時區詳細資訊
bool testDisplayTimezoneDetails(ChtP2PCameraAPI &cameraApi)
{
    std::cout << "\n===== 顯示時區詳細資訊 =====" << std::endl;

    // 首先顯示可用的時區ID
    std::cout << "提示：可用的時區ID包括:" << std::endl;
    const auto &timezoneMap = TimezoneUtils::getTimezoneMap();
    std::cout << "  ";
    bool first = true;
    for (const auto &pair : timezoneMap)
    {
        if (!first)
            std::cout << ", ";
        std::cout << pair.first;
        first = false;
    }
    std::cout << std::endl;

    std::cout << "\n請輸入時區ID: ";
    std::string timezoneId;
    std::getline(std::cin, timezoneId);

    if (timezoneId.empty())
    {
        std::cout << "時區ID不能為空" << std::endl;
        return false;
    }

    try
    {
        // 檢查時區ID是否有效
        if (!TimezoneUtils::isValidTimezoneId(timezoneId))
        {
            std::cout << "時區ID \"" << timezoneId << "\" 無效！" << std::endl;

            // 顯示調試資訊
            std::cout << "\n--- 調試資訊 ---" << std::endl;
            TimezoneUtils::debugTimezoneData();
            return false;
        }

        std::string details = TimezoneUtils::getTimezoneDetails(timezoneId);

        std::cout << "\n╔══════════════════════════════════════════════════════════════════════════════════════════╗" << std::endl;
        std::cout << "║                                  時區詳細資訊                                             ║" << std::endl;
        std::cout << "╠══════════════════════════════════════════════════════════════════════════════════════════╣" << std::endl;

        // 分行顯示詳細資訊
        std::istringstream iss(details);
        std::string line;
        while (std::getline(iss, line))
        {
            std::cout << "║ " << std::setw(84) << std::left << line << " ║" << std::endl;
        }

        std::cout << "╚══════════════════════════════════════════════════════════════════════════════════════════╝" << std::endl;

        // 顯示模擬時間
        std::string tzString = TimezoneUtils::getTimezoneString(timezoneId);
        if (!tzString.empty())
        {
            std::cout << "\n如果設定為此時區，當前時間將顯示為:" << std::endl;

            // 臨時設定環境變數來顯示該時區的時間
            const char *originalTz = getenv("TZ");
            setenv("TZ", tzString.c_str(), 1);
            tzset();

            std::cout << "  ";
            if (system("date") != 0)
            {
                std::cout << "無法獲取系統時間" << std::endl;
            }

            // 恢復原始時區設定
            if (originalTz)
            {
                setenv("TZ", originalTz, 1);
            }
            else
            {
                unsetenv("TZ");
            }
            tzset();

            std::cout << "\n是否要設定為此時區？(y/n): ";
            std::string setChoice;
            std::getline(std::cin, setChoice);

            if (setChoice == "y" || setChoice == "Y")
            {
                return testSetTimeZone(cameraApi, timezoneId);
            }
        }

        return true;
    }
    catch (const std::exception &e)
    {
        std::cerr << "顯示時區詳細資訊時發生異常: " << e.what() << std::endl;
        return false;
    }
}

// 時區快速設定 (常用時區)
bool testQuickTimezoneSetup(ChtP2PCameraAPI &cameraApi)
{
    std::cout << "\n===== 快速時區設定 =====" << std::endl;

    std::cout << "常用時區快速選擇 (基於您系統中實際可用的時區ID):" << std::endl;
    std::cout << "╔══════════════════════════════════════════════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║  1. 台灣 (台北)           GMT+08:00  │  2. 中國 (北京)           GMT+08:00           ║" << std::endl;
    std::cout << "║  3. 日本 (東京)           GMT+09:00  │  4. 美國東部              GMT-05:00           ║" << std::endl;
    std::cout << "║  5. 美國西部              GMT-08:00  │  6. 歐洲中部              GMT+01:00           ║" << std::endl;
    std::cout << "║  7. 英國 (倫敦)           GMT+00:00  │  8. 澳洲 (雪梨)           GMT+10:00           ║" << std::endl;
    std::cout << "║  9. 阿聯酋 (杜拜)         GMT+04:00  │  0. 顯示所有可用時區                          ║" << std::endl;
    std::cout << "╚══════════════════════════════════════════════════════════════════════════════════════════╝" << std::endl;

    std::cout << "\n請選擇時區 (0-9): ";
    std::string choice;
    std::getline(std::cin, choice);

    if (choice == "0")
    {
        // 顯示調試資訊
        TimezoneUtils::debugTimezoneData();
        return true;
    }

    // 基於您實際可用的時區ID對應
    std::map<std::string, std::string> quickTimezones = {
        {"1", "51"}, // 台灣 - Taipei
        {"2", "9"},  // 中國 - Beijing, Hong Kong, Singapore
        {"3", "10"}, // 日本 - Seoul, Tokyo, Osaka
        {"4", "20"}, // 美國東部 - Eastern Time
        {"5", "17"}, // 美國西部 - Pacific Time
        {"6", "2"},  // 歐洲中部 - Amsterdam, Berlin, Rome, Vienna
        {"7", "1"},  // 英國 - Greenwich Mean Time: London
        {"8", "11"}, // 澳洲 - Canberra, Melbourne, Sydney
        {"9", "5"}   // 阿聯酋 - Abu Dhabi, Dubai, Muscat
    };

    auto it = quickTimezones.find(choice);
    if (it != quickTimezones.end())
    {
        std::string timezoneId = it->second;

        // 檢查時區ID是否有效
        if (!TimezoneUtils::isValidTimezoneId(timezoneId))
        {
            std::cout << "錯誤：時區ID " << timezoneId << " 在您的系統中不可用" << std::endl;

            // 顯示調試資訊
            std::cout << "\n--- 調試資訊 ---" << std::endl;
            TimezoneUtils::debugTimezoneData();
            return false;
        }

        std::string details = TimezoneUtils::getTimezoneDetails(timezoneId);

        std::cout << "\n選擇的時區資訊:" << std::endl;
        std::cout << details << std::endl;

        std::cout << "\n確定要設定此時區嗎？(y/n): ";
        std::string confirm;
        std::getline(std::cin, confirm);

        if (confirm == "y" || confirm == "Y")
        {
            return testSetTimeZone(cameraApi, timezoneId);
        }
        else
        {
            std::cout << "設定已取消" << std::endl;
            return true;
        }
    }
    else
    {
        std::cout << "無效的選擇" << std::endl;
        return false;
    }

    return true;
}

bool testDebugTimezoneData(ChtP2PCameraAPI &cameraApi)
{
    std::cout << "\n===== 調試時區資料 =====" << std::endl;

    try
    {
        TimezoneUtils::debugTimezoneData();
        return true;
    }
    catch (const std::exception &e)
    {
        std::cerr << "調試時區資料時發生異常: " << e.what() << std::endl;
        return false;
    }
}
bool testDisplayAllHamiCamParameters(ChtP2PCameraAPI &cameraApi)
{
    std::cout << "\n===== 顯示所有 HamiCam 參數 =====" << std::endl;

    auto &paramsManager = CameraParametersManager::getInstance();

    std::cout << "       ░░░░░░░░░███████╗██╗███╗░░██╗░██╗░░░░░░░██╗███████╗██╗░░░░░██╗░░░░░░░░░░░░  " << std::endl;
    std::cout << "       ░░░░░░░░░╚════██║██║████╗░██║░██║░░██╗░░██║██╔════╝██║░░░░░██║░░░░░░░░░░░░  " << std::endl;
    std::cout << "       ░░░░░░░░░░░███╔═╝██║██╔██╗██║░╚██╗████╗██╔╝█████╗░░██║░░░░░██║░░░░░░░░░░░░  " << std::endl;
    std::cout << "       ░░░░░░░░░██╔══╝░░██║██║╚████║░░████╔═████║░██╔══╝░░██║░░░░░██║░░░░░░░░░░░░  " << std::endl;
    std::cout << "       ░░░░░░░░░███████╗██║██║░╚███║░░╚██╔╝░╚██╔╝░███████╗███████╗███████╗░░░░░░░  " << std::endl;
    std::cout << "       ░░░░░░░░░╚══════╝╚═╝╚═╝░░╚══╝░░░╚═╝░░░╚═╝░░╚══════╝╚══════╝╚══════╝░░░░░░░  " << std::endl;
    std::cout << " " << std::endl;
    // std::cout << " ██████╗░██████╗░██████╗░  ░█████╗░░██████╗░███████╗███╗░░██╗████████╗" << std::endl;
    // std::cout << " ██╔══██╗╚════██╗██╔══██╗  ██╔══██╗██╔════╝░██╔════╝████╗░██║╚══██╔══╝" << std::endl;
    // std::cout << " ██████╔╝░░███╔═╝██████╔╝  ███████║██║░░██╗░█████╗░░██╔██╗██║░░░██║░░░" << std::endl;
    // std::cout << " ██╔═══╝░██╔══╝░░██╔═══╝░  ██╔══██║██║░░╚██╗██╔══╝░░██║╚████║░░░██║░░░" << std::endl;
    // std::cout << " ██║░░░░░███████╗██║░░░░░  ██║░░██║╚██████╔╝███████╗██║░╚███║░░░██║░░░" << std::endl;
    // std::cout << " ╚═╝░░░░░╚══════╝╚═╝░░░░░  ╚═╝░░╚═╝░╚═════╝░╚══════╝╚═╝░░╚══╝░░░╚═╝░░░" << std::endl;
    std::cout << "╔══════════════════════════════════════════════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║                                  HamiCam 完整參數資訊                                      ║" << std::endl;
    std::cout << "╠══════════════════════════════════════════════════════════════════════════════════════════╣" << std::endl;

    // ===== hamiCamInfo 參數 =====
    std::cout << "║ [hamiCamInfo] 基本資訊                                                                    ║" << std::endl;
    std::cout << "╠══════════════════════════════════════════════════════════════════════════════════════════╣" << std::endl;
    std::cout << "║ Camera ID        : " << std::setw(68) << std::left << paramsManager.getCameraId() << " ║" << std::endl;
    std::cout << "║ CHT Barcode      : " << std::setw(68) << std::left << paramsManager.getCHTBarcode() << " ║" << std::endl;
    std::cout << "║ Cam SID          : " << std::setw(68) << std::left << paramsManager.getCamSid() << " ║" << std::endl;
    std::cout << "║ Tenant ID        : " << std::setw(68) << std::left << paramsManager.getTenantId() << " ║" << std::endl;
    std::cout << "║ Net No           : " << std::setw(68) << std::left << paramsManager.getNetNo() << " ║" << std::endl;
    std::cout << "║ User ID          : " << std::setw(68) << std::left << paramsManager.getUserId() << " ║" << std::endl;

    // ===== hamiSettings 參數 =====
    std::cout << "╠══════════════════════════════════════════════════════════════════════════════════════════╣" << std::endl;
    std::cout << "║ [hamiSettings] 攝影機設定                                                                  ║" << std::endl;
    std::cout << "╠══════════════════════════════════════════════════════════════════════════════════════════╣" << std::endl;

    // 基本設定
    std::cout << "║ 夜間模式         : " << std::setw(10) << std::left << paramsManager.getNightMode()
              << " (1:開啟 0:關閉)                                              ║" << std::endl;
    std::cout << "║ 自動夜視         : " << std::setw(10) << std::left << paramsManager.getAutoNightVision()
              << " (1:開啟 0:關閉)                                              ║" << std::endl;
    std::cout << "║ 狀態指示燈       : " << std::setw(10) << std::left << paramsManager.getStatusIndicatorLight()
              << " (1:開啟 0:關閉)                                              ║" << std::endl;
    std::cout << "║ 上下翻轉         : " << std::setw(10) << std::left << paramsManager.getIsFlipUpDown()
              << " (1:開啟 0:關閉)                                              ║" << std::endl;
    std::cout << "║ HD模式           : " << std::setw(10) << std::left << paramsManager.getIsHd()
              << " (1:開啟 0:關閉)                                              ║" << std::endl;
    std::cout << "║ 閃爍抑制         : " << std::setw(10) << std::left << paramsManager.getFlicker()
              << " (0:50Hz 1:60Hz 2:戶外)                                       ║" << std::endl;
    std::cout << "║ 影像品質         : " << std::setw(10) << std::left << paramsManager.getImageQualityStr()
              << " (0:低 1:中 2:高)                                             ║" << std::endl;

    // 音頻設定
    std::cout << "║ 麥克風           : " << std::setw(10) << std::left << paramsManager.getIsMicrophone()
              << " (1:開啟 0:關閉)                                              ║" << std::endl;
    std::cout << "║ 麥克風靈敏度     : " << std::setw(10) << std::left << std::to_string(paramsManager.getMicrophoneSensitivity())
              << " (0~10)                                                       ║" << std::endl;
    std::cout << "║ 揚聲器           : " << std::setw(10) << std::left << paramsManager.getIsSpeak()
              << " (1:開啟 0:關閉)                                              ║" << std::endl;
    std::cout << "║ 揚聲器音量       : " << std::setw(10) << std::left << std::to_string(paramsManager.getSpeakVolume())
              << " (音量等級)                                                   ║" << std::endl;

    // 存儲設定
    std::cout << "║ 雲存天數         : " << std::setw(10) << std::left << std::to_string(paramsManager.getStorageDay())
              << " (0表雲端不儲存)                                              ║" << std::endl;
    std::cout << "║ 事件存儲天數     : " << std::setw(10) << std::left << std::to_string(paramsManager.getEventStorageDay())
              << " (一辨事件雲存天數)                                           ║" << std::endl;

    // 排程設定
    std::cout << "║ 排程模式         : " << std::setw(10) << std::left << paramsManager.getScheduleOn()
              << " (1:開啟 0:關閉)                                              ║" << std::endl;
    std::cout << "║ 星期日排程       : " << std::setw(68) << std::left << paramsManager.getScheduleSun() << " ║" << std::endl;
    std::cout << "║ 星期一排程       : " << std::setw(68) << std::left << paramsManager.getScheduleMon() << " ║" << std::endl;
    std::cout << "║ 星期二排程       : " << std::setw(68) << std::left << paramsManager.getScheduleTue() << " ║" << std::endl;
    std::cout << "║ 星期三排程       : " << std::setw(68) << std::left << paramsManager.getScheduleWed() << " ║" << std::endl;
    std::cout << "║ 星期四排程       : " << std::setw(68) << std::left << paramsManager.getScheduleThu() << " ║" << std::endl;
    std::cout << "║ 星期五排程       : " << std::setw(68) << std::left << paramsManager.getScheduleFri() << " ║" << std::endl;
    std::cout << "║ 星期六排程       : " << std::setw(68) << std::left << paramsManager.getScheduleSat() << " ║" << std::endl;

    // 系統狀態
    std::cout << "║ 攝影機開機狀態   : " << std::setw(10) << std::left << paramsManager.getPowerOn()
              << " (1:開啟 0:關閉)                                              ║" << std::endl;
    std::cout << "║ 攝影機警報       : " << std::setw(10) << std::left << paramsManager.getAlertOn()
              << " (1:開啟 0:關閉)                                              ║" << std::endl;
    std::cout << "║ 動態偵測         : " << std::setw(10) << std::left << paramsManager.getVmd()
              << " (1:開啟 0:關閉)                                              ║" << std::endl;
    std::cout << "║ 聲音偵測         : " << std::setw(10) << std::left << paramsManager.getAd()
              << " (1:開啟 0:關閉)                                              ║" << std::endl;
    std::cout << "║ 攝影機電量       : " << std::setw(10) << std::left << std::to_string(paramsManager.getPower())
              << " (使用電池時顯示)                                             ║" << std::endl;

    // PTZ 設定
    std::cout << "║ PTZ狀態          : " << std::setw(10) << std::left << paramsManager.getPtzStatus()
              << " (0:無 1:自動擺頭 2:巡航 3:回原點 4:停留)                     ║" << std::endl;
    std::cout << "║ PTZ速度          : " << std::setw(10) << std::left << paramsManager.getPtzSpeed()
              << " (0~2)                                                        ║" << std::endl;
    std::cout << "║ 巡航停留時間     : " << std::setw(10) << std::left << paramsManager.getPtzTourStayTime()
              << " (1~5 秒)                                                     ║" << std::endl;
    std::cout << "║ 人形追蹤         : " << std::setw(10) << std::left << paramsManager.getHumanTracking()
              << " (1:開啟 0:關閉)                                              ║" << std::endl;
    std::cout << "║ 寵物追蹤         : " << std::setw(10) << std::left << paramsManager.getPetTracking()
              << " (1:開啟 0:關閉)                                              ║" << std::endl;

    // ===== hamiAiSettings 參數 =====
    std::cout << "╠══════════════════════════════════════════════════════════════════════════════════════════╣" << std::endl;
    std::cout << "║ [hamiAiSettings] AI 設定                                                                   ║" << std::endl;
    std::cout << "╠══════════════════════════════════════════════════════════════════════════════════════════╣" << std::endl;

    // AI 告警設定
    std::cout << "║ 動態檢測告警     : " << std::setw(10) << std::left << paramsManager.getVmdAlert()
              << " (1:開啟 0:關閉)                                              ║" << std::endl;
    std::cout << "║ 人形追蹤告警     : " << std::setw(10) << std::left << paramsManager.getHumanAlert()
              << " (1:開啟 0:關閉)                                              ║" << std::endl;
    std::cout << "║ 寵物追蹤告警     : " << std::setw(10) << std::left << paramsManager.getPetAlert()
              << " (1:開啟 0:關閉)                                              ║" << std::endl;
    std::cout << "║ 聲音偵測告警     : " << std::setw(10) << std::left << paramsManager.getAdAlert()
              << " (1:開啟 0:關閉)                                              ║" << std::endl;
    std::cout << "║ 電子圍籬告警     : " << std::setw(10) << std::left << paramsManager.getFenceAlert()
              << " (1:開啟 0:關閉)                                              ║" << std::endl;
    std::cout << "║ 臉部偵測告警     : " << std::setw(10) << std::left << paramsManager.getFaceAlert()
              << " (1:開啟 0:關閉)                                              ║" << std::endl;
    std::cout << "║ 跌倒偵測告警     : " << std::setw(10) << std::left << paramsManager.getFallAlert()
              << " (1:開啟 0:關閉)                                              ║" << std::endl;
    std::cout << "║ 嬰兒哭泣告警     : " << std::setw(10) << std::left << paramsManager.getAdBabyCryAlert()
              << " (1:開啟 0:關閉)                                              ║" << std::endl;
    std::cout << "║ 人聲告警         : " << std::setw(10) << std::left << paramsManager.getAdSpeechAlert()
              << " (1:開啟 0:關閉)                                              ║" << std::endl;
    std::cout << "║ 警報聲告警       : " << std::setw(10) << std::left << paramsManager.getAdAlarmAlert()
              << " (1:開啟 0:關閉)                                              ║" << std::endl;
    std::cout << "║ 狗叫聲告警       : " << std::setw(10) << std::left << paramsManager.getAdDogAlert()
              << " (1:開啟 0:關閉)                                              ║" << std::endl;
    std::cout << "║ 貓叫聲告警       : " << std::setw(10) << std::left << paramsManager.getAdCatAlert()
              << " (1:開啟 0:關閉)                                              ║" << std::endl;

    // AI 靈敏度設定
    std::cout << "║ 動態偵測靈敏度   : " << std::setw(10) << std::left << std::to_string(paramsManager.getVmdSen())
              << " (0:低 1:中 2:高)                                             ║" << std::endl;
    std::cout << "║ 聲音偵測靈敏度   : " << std::setw(10) << std::left << std::to_string(paramsManager.getAdSen())
              << " (0:低 1:中 2:高)                                             ║" << std::endl;
    std::cout << "║ 人形偵測靈敏度   : " << std::setw(10) << std::left << std::to_string(paramsManager.getHumanSen())
              << " (0:低 1:中 2:高)                                             ║" << std::endl;
    std::cout << "║ 人臉偵測靈敏度   : " << std::setw(10) << std::left << std::to_string(paramsManager.getFaceSen())
              << " (0:低 1:中 2:高)                                             ║" << std::endl;
    std::cout << "║ 電子圍離靈敏度   : " << std::setw(10) << std::left << std::to_string(paramsManager.getFenceSen())
              << " (0:低 1:中 2:高)                                             ║" << std::endl;
    std::cout << "║ 寵物偵測靈敏度   : " << std::setw(10) << std::left << std::to_string(paramsManager.getPetSen())
              << " (0:低 1:中 2:高)                                             ║" << std::endl;
    std::cout << "║ 跌倒偵測靈敏度   : " << std::setw(10) << std::left << std::to_string(paramsManager.getFallSen())
              << " (0:低 1:中 2:高)                                             ║" << std::endl;
    std::cout << "║ 跌倒判定時間     : " << std::setw(10) << std::left << std::to_string(paramsManager.getFallTime())
              << " (1~5 秒)                                                     ║" << std::endl;

    // 電子圍籬座標
    auto pos1 = paramsManager.getFencePos1();
    auto pos2 = paramsManager.getFencePos2();
    auto pos3 = paramsManager.getFencePos3();
    auto pos4 = paramsManager.getFencePos4();
    std::cout << "║ 電子圍籬座標1    : (" << std::setw(3) << pos1.first << "," << std::setw(3) << pos1.second
              << ")                                                              ║" << std::endl;
    std::cout << "║ 電子圍籬座標2    : (" << std::setw(3) << pos2.first << "," << std::setw(3) << pos2.second
              << ")                                                              ║" << std::endl;
    std::cout << "║ 電子圍籬座標3    : (" << std::setw(3) << pos3.first << "," << std::setw(3) << pos3.second
              << ")                                                              ║" << std::endl;
    std::cout << "║ 電子圍籬座標4    : (" << std::setw(3) << pos4.first << "," << std::setw(3) << pos4.second
              << ")                                                              ║" << std::endl;
    std::cout << "║ 電子圍籬方向     : " << std::setw(10) << std::left << paramsManager.getFenceDir()
              << " (0:進入 1:離開)                                               ║" << std::endl;

    // 人臉識別特徵
    auto features = paramsManager.getIdentificationFeatures();
    std::cout << "║ 人臉識別特徵     : " << std::setw(10) << std::left << std::to_string(features.size())
              << " 筆資料 (最多20筆)                                            ║" << std::endl;

    if (!features.empty())
    {
        std::cout << "║   範例特徵:                                                                              ║" << std::endl;
        for (size_t i = 0; i < std::min(features.size(), size_t(3)); ++i)
        {
            const auto &feature = features[i];
            std::cout << "║     ID: " << std::setw(15) << std::left << feature.id
                      << " 姓名: " << std::setw(45) << std::left << feature.name << " ║" << std::endl;
            std::cout << "║     驗證等級: " << std::setw(10) << std::left << std::to_string(feature.verifyLevel)
                      << " 創建時間: " << std::setw(45) << std::left << feature.createTime << " ║" << std::endl;
        }
        if (features.size() > 3)
        {
            std::cout << "║     ... 還有 " << (features.size() - 3) << " 筆資料                                                           ║" << std::endl;
        }
    }

    // ===== hamiSystemSettings 參數 =====
    std::cout << "╠══════════════════════════════════════════════════════════════════════════════════════════╣" << std::endl;
    std::cout << "║ [hamiSystemSettings] 系統設定                                                              ║" << std::endl;
    std::cout << "╠══════════════════════════════════════════════════════════════════════════════════════════╣" << std::endl;
    std::cout << "║ OTA Domain       : " << std::setw(68) << std::left << paramsManager.getOtaDomainName() << " ║" << std::endl;
    std::cout << "║ OTA 查詢間隔     : " << std::setw(10) << std::left << std::to_string(paramsManager.getOtaQueryInterval())
              << " 秒                                                           ║" << std::endl;
    std::cout << "║ NTP Server       : " << std::setw(68) << std::left << paramsManager.getNtpServer() << " ║" << std::endl;
    std::cout << "║ Bucket Name      : " << std::setw(68) << std::left << paramsManager.getBucketName() << " ║" << std::endl;

    // ===== 系統狀態 =====
    std::cout << "╠══════════════════════════════════════════════════════════════════════════════════════════╣" << std::endl;
    std::cout << "║ [系統狀態] 當前狀態                                                                        ║" << std::endl;
    std::cout << "╠══════════════════════════════════════════════════════════════════════════════════════════╣" << std::endl;
    std::cout << "║ 綁定狀態         : " << std::setw(10) << std::left << paramsManager.getActiveStatus()
              << " (1:已綁定 0:未綁定)                                           ║" << std::endl;
    std::cout << "║ 設備狀態         : " << std::setw(10) << std::left << paramsManager.getDeviceStatus()
              << " (1:執行中 0:未執行)                                           ║" << std::endl;
    std::cout << "║ 時區             : " << std::setw(68) << std::left << paramsManager.getTimeZone() << " ║" << std::endl;
    std::cout << "║ 相機名稱         : " << std::setw(68) << std::left << paramsManager.getCameraName() << " ║" << std::endl;
    std::cout << "║ 韌體版本         : " << std::setw(68) << std::left << paramsManager.getFirmwareVersion() << " ║" << std::endl;
    std::cout << "║ WiFi SSID        : " << std::setw(68) << std::left << paramsManager.getWifiSsid() << " ║" << std::endl;
    std::cout << "║ 存儲健康狀態     : " << std::setw(68) << std::left << paramsManager.getStorageHealth() << " ║" << std::endl;

    std::cout << "╚══════════════════════════════════════════════════════════════════════════════════════════╝" << std::endl;

    // 顯示總計參數數量
    auto allParams = paramsManager.getAllParameters();
    std::cout << "\n總計儲存了 " << allParams.size() << " 個參數" << std::endl;

    return true;
}

bool testDisplayIdentificationFeatures(ChtP2PCameraAPI &cameraApi)
{
    std::cout << "\n===== 顯示人臉識別特徵詳細資訊 =====" << std::endl;

    auto &paramsManager = CameraParametersManager::getInstance();
    auto features = paramsManager.getIdentificationFeatures();

    if (features.empty())
    {
        std::cout << "目前沒有人臉識別特徵資料" << std::endl;
        return true;
    }

    std::cout << "人臉識別特徵總數: " << features.size() << " 筆 (最多20筆)" << std::endl;
    std::cout << "\n詳細資料:" << std::endl;

    for (size_t i = 0; i < features.size(); ++i)
    {
        const auto &feature = features[i];

        std::cout << "\n--- 特徵 " << (i + 1) << " ---" << std::endl;
        std::cout << "ID: " << feature.id << std::endl;
        std::cout << "姓名: " << feature.name << std::endl;
        std::cout << "驗證等級: " << feature.verifyLevel << " (1:低 2:高)" << std::endl;
        std::cout << "創建時間: " << feature.createTime << std::endl;
        std::cout << "更新時間: " << feature.updateTime << std::endl;

        std::cout << "特徵值長度: " << feature.faceFeatures.length() << " 字元" << std::endl;
        // 只顯示特徵值的開頭部分
        if (!feature.faceFeatures.empty())
        {
            std::string preview = feature.faceFeatures.substr(0, std::min(feature.faceFeatures.length(), size_t(50)));
            std::cout << "特徵值預覽: " << preview;
            if (feature.faceFeatures.length() > 50)
            {
                std::cout << "... (共 " << feature.faceFeatures.length() << " 字元)";
            }
            std::cout << std::endl;
        }
    }

    return true;
}

bool testManageIdentificationFeatures(ChtP2PCameraAPI &cameraApi)
{
    std::cout << "\n===== 人臉識別特徵管理 =====" << std::endl;

    auto &paramsManager = CameraParametersManager::getInstance();

    std::cout << "請選擇操作:" << std::endl;
    std::cout << "1 - 新增人臉特徵" << std::endl;
    std::cout << "2 - 移除人臉特徵" << std::endl;
    std::cout << "3 - 更新人臉特徵" << std::endl;
    std::cout << "4 - 顯示所有特徵" << std::endl;
    std::cout << "請輸入選擇 (1-4): ";

    std::string choice;
    std::getline(std::cin, choice);

    if (choice == "1")
    {
        // 新增人臉特徵
        CameraParametersManager::IdentificationFeature newFeature;

        std::cout << "請輸入人員ID: ";
        std::getline(std::cin, newFeature.id);

        std::cout << "請輸入姓名: ";
        std::getline(std::cin, newFeature.name);

        std::cout << "請輸入驗證等級 (1:低 2:高): ";
        std::string levelStr;
        std::getline(std::cin, levelStr);
        newFeature.verifyLevel = levelStr.empty() ? 1 : std::stoi(levelStr);

        std::cout << "請輸入特徵值 (可留空使用模擬資料): ";
        std::getline(std::cin, newFeature.faceFeatures);
        if (newFeature.faceFeatures.empty())
        {
            newFeature.faceFeatures = "SIMULATED_FACE_FEATURES_" + newFeature.id;
        }

        // 自動生成時間戳
        auto now = std::chrono::system_clock::now();
        auto now_time_t = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << std::put_time(std::localtime(&now_time_t), "%Y/%m/%d_%H%M%S");
        newFeature.createTime = ss.str();
        newFeature.updateTime = ss.str();

        bool result = paramsManager.addIdentificationFeature(newFeature);
        std::cout << (result ? "✓ 新增成功" : "✗ 新增失敗") << std::endl;

        if (result)
        {
            paramsManager.saveToFile();
        }
    }
    else if (choice == "2")
    {
        // 移除人臉特徵
        std::cout << "請輸入要移除的人員ID: ";
        std::string removeId;
        std::getline(std::cin, removeId);

        bool result = paramsManager.removeIdentificationFeature(removeId);
        std::cout << (result ? "✓ 移除成功" : "✗ 移除失敗") << std::endl;

        if (result)
        {
            paramsManager.saveToFile();
        }
    }
    else if (choice == "3")
    {
        // 更新人臉特徵
        std::cout << "請輸入要更新的人員ID: ";
        std::string updateId;
        std::getline(std::cin, updateId);

        // 先取得現有特徵
        auto features = paramsManager.getIdentificationFeatures();
        auto it = std::find_if(features.begin(), features.end(),
                               [&updateId](const CameraParametersManager::IdentificationFeature &f)
                               {
                                   return f.id == updateId;
                               });

        if (it != features.end())
        {
            CameraParametersManager::IdentificationFeature updatedFeature = *it;

            std::cout << "當前姓名: " << updatedFeature.name << std::endl;
            std::cout << "請輸入新姓名 (留空保持不變): ";
            std::string newName;
            std::getline(std::cin, newName);
            if (!newName.empty())
            {
                updatedFeature.name = newName;
            }

            std::cout << "當前驗證等級: " << updatedFeature.verifyLevel << std::endl;
            std::cout << "請輸入新驗證等級 (1:低 2:高, 留空保持不變): ";
            std::string newLevelStr;
            std::getline(std::cin, newLevelStr);
            if (!newLevelStr.empty())
            {
                updatedFeature.verifyLevel = std::stoi(newLevelStr);
            }

            // 更新時間戳
            auto now = std::chrono::system_clock::now();
            auto now_time_t = std::chrono::system_clock::to_time_t(now);
            std::stringstream ss;
            ss << std::put_time(std::localtime(&now_time_t), "%Y/%m/%d_%H%M%S");
            updatedFeature.updateTime = ss.str();

            bool result = paramsManager.updateIdentificationFeature(updateId, updatedFeature);
            std::cout << (result ? "✓ 更新成功" : "✗ 更新失敗") << std::endl;

            if (result)
            {
                paramsManager.saveToFile();
            }
        }
        else
        {
            std::cout << "✗ 找不到指定的人員ID" << std::endl;
        }
    }
    else if (choice == "4")
    {
        // 顯示所有特徵
        return testDisplayIdentificationFeatures(cameraApi);
    }
    else
    {
        std::cout << "無效的選擇" << std::endl;
        return false;
    }

    return true;
}

bool testExportHamiCamParameters(ChtP2PCameraAPI &cameraApi)
{
    std::cout << "\n===== 匯出 HamiCam 參數 =====" << std::endl;

    auto &paramsManager = CameraParametersManager::getInstance();

    std::cout << "請選擇匯出格式:" << std::endl;
    std::cout << "1 - JSON 格式" << std::endl;
    std::cout << "2 - CSV 格式" << std::endl;
    std::cout << "3 - 純文字格式" << std::endl;
    std::cout << "請輸入選擇 (1-3): ";

    std::string choice;
    std::getline(std::cin, choice);

    std::cout << "請輸入匯出檔案路徑 (留空使用預設): ";
    std::string exportPath;
    std::getline(std::cin, exportPath);

    if (exportPath.empty())
    {
        if (choice == "1")
            exportPath = "/tmp/hamicam_export.json";
        else if (choice == "2")
            exportPath = "/tmp/hamicam_export.csv";
        else
            exportPath = "/tmp/hamicam_export.txt";
    }

    try
    {
        std::ofstream exportFile(exportPath);
        if (!exportFile.is_open())
        {
            std::cerr << "無法建立匯出檔案: " << exportPath << std::endl;
            return false;
        }

        auto allParams = paramsManager.getAllParameters();

        if (choice == "1")
        {
            // JSON 格式
            exportFile << "{\n";
            bool first = true;
            for (const auto &param : allParams)
            {
                if (!first)
                    exportFile << ",\n";
                exportFile << "  \"" << param.first << "\": \"" << param.second << "\"";
                first = false;
            }
            exportFile << "\n}\n";
        }
        else if (choice == "2")
        {
            // CSV 格式
            exportFile << "Parameter,Value\n";
            for (const auto &param : allParams)
            {
                exportFile << "\"" << param.first << "\",\"" << param.second << "\"\n";
            }
        }
        else
        {
            // 純文字格式
            exportFile << "HamiCam 參數匯出\n";
            exportFile << "匯出時間: " << getFormattedTimestamp() << "\n\n";

            for (const auto &param : allParams)
            {
                exportFile << param.first << " = " << param.second << "\n";
            }
        }

        exportFile.close();

        std::cout << "✓ 參數已成功匯出到: " << exportPath << std::endl;
        std::cout << "總計匯出 " << allParams.size() << " 個參數" << std::endl;

        return true;
    }
    catch (const std::exception &e)
    {
        std::cerr << "✗ 匯出時發生錯誤: " << e.what() << std::endl;
        return false;
    }
}

bool testReparseInitialParameters(ChtP2PCameraAPI &cameraApi)
{
    std::cout << "\n===== 重新解析初始化參數 =====" << std::endl;

    auto &paramsManager = CameraParametersManager::getInstance();

    std::cout << "此功能將模擬重新接收 GetHamiCamInitialInfo 的完整參數" << std::endl;
    std::cout << "是否繼續？(y/n): ";

    std::string confirm;
    std::getline(std::cin, confirm);

    if (confirm != "y" && confirm != "Y")
    {
        std::cout << "操作已取消" << std::endl;
        return false;
    }

    // 模擬重新接收的JSON數據
    std::string mockHamiCamInfo = R"({
        "camSid": 13,
        "camId": ")" + paramsManager.getCameraId() +
                                  R"(",
        "chtBarcode": ")" + paramsManager.getCHTBarcode() +
                                  R"(",
        "tenantId": "updated_tenant",
        "netNo": "UPDATED_NET202405",
        "userId": "UPDATED_USER1001"
    })";

    std::string mockHamiSettings = R"({
        "nightMode": "1",
        "autoNightVision": "1",
        "statusIndicatorLight": "0",
        "isFlipUpDown": "0",
        "isHd": "1",
        "flicker": "1",
        "imageQuality": "2",
        "isMicrophone": "1",
        "microphoneSensitivity": 7,
        "isSpeak": "1",
        "speakVolume": 80,
        "storageDay": 14,
        "eventStorageDay": 30,
        "powerOn": "1",
        "alertOn": "1",
        "vmd": "1",
        "ad": "1",
        "ptzStatus": "2",
        "humanTracking": "1",
        "petTracking": "0"
    })";

    std::string mockHamiAiSettings = R"({
        "vmdAlert": "1",
        "humanAlert": "1",
        "petAlert": "0",
        "faceAlert": "1",
        "vmdSen": 7,
        "humanSen": 2,
        "petSen": 1,
        "faceSen": 2,
        "fencePos1": {"x": 20, "y": 20},
        "fencePos2": {"x": 20, "y": 80},
        "fencePos3": {"x": 80, "y": 80},
        "fencePos4": {"x": 80, "y": 20},
        "fenceDir": "0"
    })";

    std::string mockHamiSystemSettings = R"({
        "otaDomainName": "updated.ota.example.com",
        "otaQueryInterval": 7200,
        "ntpServer": "time.stdtime.gov.tw",
        "bucketName": "updated-cht-p2p"
    })";

    std::cout << "開始重新解析參數..." << std::endl;

    try
    {
        bool result = paramsManager.parseAndSaveInitialInfoWithSync(
            mockHamiCamInfo, mockHamiSettings, mockHamiAiSettings, mockHamiSystemSettings);

        if (result)
        {
            std::cout << "✓ 參數重新解析成功" << std::endl;
            std::cout << "更新的參數:" << std::endl;
            std::cout << "  - Tenant ID: " << paramsManager.getTenantId() << std::endl;
            std::cout << "  - Net No: " << paramsManager.getNetNo() << std::endl;
            std::cout << "  - User ID: " << paramsManager.getUserId() << std::endl;
            std::cout << "  - HD Mode: " << paramsManager.getIsHd() << std::endl;
            std::cout << "  - Speaker Volume: " << paramsManager.getSpeakVolume() << std::endl;
            std::cout << "  - Storage Days: " << paramsManager.getStorageDay() << std::endl;
            std::cout << "  - NTP Server: " << paramsManager.getNtpServer() << std::endl;
        }
        else
        {
            std::cout << "✗ 參數重新解析失敗" << std::endl;
        }

        return result;
    }
    catch (const std::exception &e)
    {
        std::cerr << "✗ 重新解析時發生異常: " << e.what() << std::endl;
        return false;
    }
}

bool testValidateParameterIntegrity(ChtP2PCameraAPI &cameraApi)
{
    std::cout << "\n===== 驗證參數完整性 =====" << std::endl;

    auto &paramsManager = CameraParametersManager::getInstance();

    // 定義必要參數列表
    std::vector<std::string> requiredParams = {
        "camId", "chtBarcode", "camSid", "tenantId", "netNo", "userId",
        "nightMode", "autoNightVision", "isHd", "imageQuality",
        "isMicrophone", "isSpeak", "storageDay", "eventStorageDay",
        "vmdAlert", "humanAlert", "petAlert", "faceAlert",
        "otaDomainName", "ntpServer", "bucketName",
        "activeStatus", "deviceStatus", "timezone"};

    std::cout << "檢查必要參數..." << std::endl;

    int missingCount = 0;
    int validCount = 0;
    int invalidCount = 0;

    for (const auto &param : requiredParams)
    {
        if (!paramsManager.hasParameter(param))
        {
            std::cout << "✗ 缺少參數: " << param << std::endl;
            missingCount++;
        }
        else
        {
            std::string value = paramsManager.getParameter(param, "");
            bool isValid = paramsManager.validateParameter(param, value);

            if (isValid)
            {
                std::cout << "✓ " << param << ": " << value << std::endl;
                validCount++;
            }
            else
            {
                std::cout << "⚠ " << param << ": " << value << " (無效值)" << std::endl;
                invalidCount++;
            }
        }
    }

    std::cout << "\n=== 驗證結果 ===" << std::endl;
    std::cout << "總計檢查: " << requiredParams.size() << " 個必要參數" << std::endl;
    std::cout << "有效參數: " << validCount << " 個" << std::endl;
    std::cout << "無效參數: " << invalidCount << " 個" << std::endl;
    std::cout << "缺少參數: " << missingCount << " 個" << std::endl;

    // 檢查人臉識別特徵數量
    auto features = paramsManager.getIdentificationFeatures();
    std::cout << "人臉識別特徵: " << features.size() << " 筆 (上限20筆)" << std::endl;

    if (features.size() > 20)
    {
        std::cout << "⚠ 警告: 人臉識別特徵超過上限" << std::endl;
    }

    // 檢查電子圍籬座標
    auto pos1 = paramsManager.getFencePos1();
    auto pos2 = paramsManager.getFencePos2();
    auto pos3 = paramsManager.getFencePos3();
    auto pos4 = paramsManager.getFencePos4();

    bool fenceValid = (pos1.first >= 0 && pos1.first <= 100 && pos1.second >= 0 && pos1.second <= 100) &&
                      (pos2.first >= 0 && pos2.first <= 100 && pos2.second >= 0 && pos2.second <= 100) &&
                      (pos3.first >= 0 && pos3.first <= 100 && pos3.second >= 0 && pos3.second <= 100) &&
                      (pos4.first >= 0 && pos4.first <= 100 && pos4.second >= 0 && pos4.second <= 100);

    if (fenceValid)
    {
        std::cout << "✓ 電子圍籬座標有效" << std::endl;
    }
    else
    {
        std::cout << "⚠ 電子圍籬座標可能無效" << std::endl;
    }

    bool overallValid = (missingCount == 0 && invalidCount == 0 && features.size() <= 20 && fenceValid);

    std::cout << "\n總體評估: " << (overallValid ? "✓ 參數完整且有效" : "⚠ 發現問題，需要檢查") << std::endl;

    return overallValid;
}

bool testSimulateParameterUpdates(ChtP2PCameraAPI &cameraApi)
{
    std::cout << "\n===== 模擬參數更新測試 =====" << std::endl;

    auto &paramsManager = CameraParametersManager::getInstance();

    std::cout << "此測試將模擬各種參數的更新操作" << std::endl;
    std::cout << "請選擇要測試的更新類型:" << std::endl;
    std::cout << "1 - 基本設定更新" << std::endl;
    std::cout << "2 - AI 設定更新" << std::endl;
    std::cout << "3 - 系統設定更新" << std::endl;
    std::cout << "4 - 批次更新測試" << std::endl;
    std::cout << "請輸入選擇 (1-4): ";

    std::string choice;
    std::getline(std::cin, choice);

    if (choice == "1")
    {
        // 基本設定更新
        std::cout << "模擬基本設定更新..." << std::endl;

        std::cout << "更新夜間模式..." << std::endl;
        paramsManager.setParameter("nightMode", paramsManager.getNightMode() == "1" ? "0" : "1");

        std::cout << "更新影像品質..." << std::endl;
        int currentQuality = std::stoi(paramsManager.getImageQualityStr());
        int newQuality = (currentQuality + 1) % 3;
        paramsManager.setParameter("imageQuality", std::to_string(newQuality));

        std::cout << "更新麥克風靈敏度..." << std::endl;
        int currentSens = paramsManager.getMicrophoneSensitivity();
        int newSens = std::min(10, currentSens + 1);
        paramsManager.setParameter("microphoneSensitivity", std::to_string(newSens));

        std::cout << "✓ 基本設定更新完成" << std::endl;
    }
    else if (choice == "2")
    {
        // AI 設定更新
        std::cout << "模擬 AI 設定更新..." << std::endl;

        std::cout << "更新人形偵測告警..." << std::endl;
        paramsManager.setParameter("humanAlert", paramsManager.getHumanAlert());

        std::cout << "更新動態偵測靈敏度..." << std::endl;
        int currentVmdSen = paramsManager.getVmdSen();
        int newVmdSen = (currentVmdSen + 1) % 3;
        paramsManager.setParameter("vmdSen", std::to_string(newVmdSen));

        std::cout << "更新電子圍籬座標..." << std::endl;
        auto pos1 = paramsManager.getFencePos1();
        paramsManager.setParameter("fencePos1_x", std::to_string((pos1.first + 5) % 100));
        paramsManager.setParameter("fencePos1_y", std::to_string((pos1.second + 5) % 100));

        std::cout << "✓ AI 設定更新完成" << std::endl;
    }
    else if (choice == "3")
    {
        // 系統設定更新
        std::cout << "模擬系統設定更新..." << std::endl;

        std::cout << "更新 OTA 查詢間隔..." << std::endl;
        int currentInterval = paramsManager.getOtaQueryInterval();
        int newInterval = currentInterval == 3600 ? 7200 : 3600;
        paramsManager.setParameter("otaQueryInterval", std::to_string(newInterval));

        std::cout << "更新相機名稱..." << std::endl;
        std::string currentName = paramsManager.getCameraName();
        std::string newName = currentName + "_UPDATED";
        paramsManager.setCameraName(newName);

        std::cout << "✓ 系統設定更新完成" << std::endl;
    }
    else if (choice == "4")
    {
        // 批次更新測試
        std::cout << "執行批次更新測試..." << std::endl;

        std::vector<std::pair<std::string, std::string>> updates = {
            {"nightMode", "1"},
            {"autoNightVision", "1"},
            {"isHd", "1"},
            {"imageQuality", "2"},
            {"speakVolume", "75"},
            {"storageDay", "30"},
            {"humanAlert", "1"},
            {"petAlert", "1"},
            {"vmdSen", "2"},
            {"humanSen", "2"}};

        for (const auto &update : updates)
        {
            std::cout << "更新 " << update.first << " = " << update.second << std::endl;
            paramsManager.setParameter(update.first, update.second);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        std::cout << "✓ 批次更新完成，共更新 " << updates.size() << " 個參數" << std::endl;
    }
    else
    {
        std::cout << "無效的選擇" << std::endl;
        return false;
    }

    // 保存更新的參數
    bool saveResult = paramsManager.saveToFile();
    std::cout << "參數保存: " << (saveResult ? "成功" : "失敗") << std::endl;

    return true;
}

bool testParameterChangeNotification(ChtP2PCameraAPI &cameraApi)
{
    std::cout << "\n===== 參數變更通知測試 =====" << std::endl;

    auto &paramsManager = CameraParametersManager::getInstance();

    // 註冊參數變更回調
    int callbackId1 = paramsManager.registerParameterChangeCallback("nightMode",
                                                                    [](const std::string &key, const std::string &value)
                                                                    {
                                                                        std::cout << "🔔 參數變更通知: " << key << " 已變更為 " << value << std::endl;
                                                                    });

    int callbackId2 = paramsManager.registerParameterChangeCallback("",
                                                                    [](const std::string &key, const std::string &value)
                                                                    {
                                                                        std::cout << "📢 全域參數變更: " << key << " = " << value << std::endl;
                                                                    });

    std::cout << "已註冊參數變更回調 (ID: " << callbackId1 << ", " << callbackId2 << ")" << std::endl;
    std::cout << "現在將模擬參數變更..." << std::endl;

    // 模擬參數變更
    std::cout << "\n1. 變更夜間模式..." << std::endl;
    paramsManager.setParameter("nightMode", paramsManager.getNightMode() == "1" ? "0" : "1");

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    std::cout << "\n2. 變更影像品質..." << std::endl;
    paramsManager.setParameter("imageQuality", "1");

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    std::cout << "\n3. 變更人形偵測告警..." << std::endl;
    paramsManager.setParameter("humanAlert", "1");

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    std::cout << "\n4. 變更相機名稱..." << std::endl;
    paramsManager.setCameraName("NOTIFICATION_TEST_CAMERA");

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // 取消註冊回調
    std::cout << "\n取消註冊回調..." << std::endl;
    bool unregResult1 = paramsManager.unregisterParameterChangeCallback(callbackId1);
    bool unregResult2 = paramsManager.unregisterParameterChangeCallback(callbackId2);

    std::cout << "回調取消結果: " << (unregResult1 ? "成功" : "失敗")
              << ", " << (unregResult2 ? "成功" : "失敗") << std::endl;

    // 再次變更參數確認回調已取消
    std::cout << "\n5. 再次變更參數（應該沒有通知）..." << std::endl;
    paramsManager.setParameter("nightMode", "0");

    std::cout << "✓ 參數變更通知測試完成" << std::endl;

    return true;
}

// ===== 主選單顯示函數 =====

// Helper function to format menu lines with proper padding
std::string formatMenuLine(const std::string &content)
{
    const int MENU_WIDTH = 110;               // Total width including borders
    const int CONTENT_WIDTH = MENU_WIDTH - 2; // Excluding left and right borders

    // Count actual display width (handle wide characters)
    int displayWidth = 0;
    for (size_t i = 0; i < content.length();)
    {
        unsigned char c = content[i];
        if ((c & 0x80) == 0)
        {
            // ASCII character
            displayWidth += 1;
            i += 1;
        }
        else if ((c & 0xE0) == 0xC0)
        {
            // 2-byte UTF-8
            displayWidth += 2;
            i += 2;
        }
        else if ((c & 0xF0) == 0xE0)
        {
            // 3-byte UTF-8 (Chinese characters, etc.)
            displayWidth += 2;
            i += 3;
        }
        else if ((c & 0xF8) == 0xF0)
        {
            // 4-byte UTF-8
            displayWidth += 2;
            i += 4;
        }
        else
        {
            // Invalid UTF-8 or continuation byte
            displayWidth += 1;
            i += 1;
        }
    }

    // Calculate padding needed
    int padding = CONTENT_WIDTH - displayWidth;
    if (padding < 0)
        padding = 0;

    return "║" + content + std::string(padding, ' ') + "║";
}

void displayMainMenu()
{
    std::cout << "           ░░░░░░░░░███████╗██╗███╗░░██╗░██╗░░░░░░░██╗███████╗██╗░░░░░██╗░░░░░░░░░░░░  " << std::endl;
    std::cout << "           ░░░░░░░░░╚════██║██║████╗░██║░██║░░██╗░░██║██╔════╝██║░░░░░██║░░░░░░░░░░░░  " << std::endl;
    std::cout << "           ░░░░░░░░░░░███╔═╝██║██╔██╗██║░╚██╗████╗██╔╝█████╗░░██║░░░░░██║░░░░░░░░░░░░  " << std::endl;
    std::cout << "           ░░░░░░░░░██╔══╝░░██║██║╚████║░░████╔═████║░██╔══╝░░██║░░░░░██║░░░░░░░░░░░░  " << std::endl;
    std::cout << "           ░░░░░░░░░███████╗██║██║░╚███║░░╚██╔╝░╚██╔╝░███████╗███████╗███████╗░░░░░░░  " << std::endl;
    std::cout << "           ░░░░░░░░░╚══════╝╚═╝╚═╝░░╚══╝░░░╚═╝░░░╚═╝░░╚══════╝╚══════╝╚══════╝░░░░░░░  " << std::endl;
    std::cout << "                                                             " << std::endl;
    std::cout << "\n"
              << std::endl;
    std::cout << "╔══════════════════════════════════════════════════════════════════════════════════════════════════════════╗" << std::endl;
    std::cout << formatMenuLine("                                Zinwell CHT P2P Camera 互動測試選單                                      ") << std::endl;
    std::cout << "╠══════════════════════════════════════════════════════════════════════════════════════════════════════════╣" << std::endl;
    std::cout << formatMenuLine("  基本狀態與管理類                                                                                       ") << std::endl;
    std::cout << formatMenuLine("    1[OK]  - 獲取攝影機狀態(_GetCamStatusById)        2[OK]  - 刪除攝影機資訊(_DeleteCameraInfo)        ") << std::endl;
    std::cout << formatMenuLine("    3[OK]  - 設置時區(_SetTimeZone)                   4[OK]  - 獲取時區(_GetTimeZone)                  ") << std::endl;
    std::cout << formatMenuLine("    5[OK]  - 更新攝影機名稱(_UpdateCameraName)        6[--]  - 獲取WiFi綁定清單(_GetHamiCamBindList)    ") << std::endl;
    std::cout << formatMenuLine("    7[OK]  - 顯示時區狀態                             8[OK]  - 重新載入時區設定                         ") << std::endl;
    std::cout << formatMenuLine("    9[OK]  - 重新初始化時區                           10[OK] - NTP 時間同步測試(快速)                   ") << std::endl;
    std::cout << "╠══════════════════════════════════════════════════════════════════════════════════════════════════════════╣" << std::endl;
    std::cout << formatMenuLine("  影像與顯示設定類                                                                                       ") << std::endl;
    std::cout << formatMenuLine("    11[OK] - 設定OSD(_SetCameraOSD)                   12[--] - 設定HD解析度(_SetCameraHD)               ") << std::endl;
    std::cout << formatMenuLine("    13[OK] - 設定閃爍率(_SetFlicker)                  14[OK] - 設定影像品質(_SetImageQuality)           ") << std::endl;
    std::cout << formatMenuLine("    15[OK] - 設定夜間模式(_SetNightMode)              16[--] - 設定自動夜視(_SetAutoNightVision)        ") << std::endl;
    std::cout << formatMenuLine("    17[OK] - 設定上下翻轉(_SetFlipUpDown)                                                                ") << std::endl;
    std::cout << "╠════════════════════════════════════════════════════════════════════════════════════════════════════════════╣" << std::endl;
    std::cout << formatMenuLine("  音頻控制類                                                                                             ") << std::endl;
    std::cout << formatMenuLine("    21[--] - 設定麥克風(_SetMicrophone)               22[--] - 設定揚聲器(_SetSpeak)                    ") << std::endl;
    std::cout << "╠════════════════════════════════════════════════════════════════════════════════════════════════════════════╣" << std::endl;
    std::cout << formatMenuLine("  系統控制類                                                                                             ") << std::endl;
    std::cout << formatMenuLine("    31[OK] - 設定LED指示燈(_SetLED)                   32[--] - 設定攝影機電源(_SetCameraPower)          ") << std::endl;
    std::cout << formatMenuLine("    33[--] - 取得快照(_GetSnapshotHamiCamDevice)      34[OK] - 重啟設備(_RestartHamiCamDevice)           ") << std::endl;
    std::cout << formatMenuLine("    35[OK] - 格式化SD卡(_HamiCamFormatSDCard)         36[--] - OTA升級(_UpgradeHamiCamOTA)               ") << std::endl;
    std::cout << "╠════════════════════════════════════════════════════════════════════════════════════════════════════════════╣" << std::endl;
    std::cout << formatMenuLine("  存儲管理類                                                                                             ") << std::endl;
    std::cout << formatMenuLine("    41[--] - 設定雲存天數(_SetCamStorageDay)          42[--] - 設定事件存儲天數(_SetCamEventStorageDay)  ") << std::endl;
    std::cout << "╠════════════════════════════════════════════════════════════════════════════════════════════════════════════╣" << std::endl;
    std::cout << formatMenuLine("  PTZ控制類                                                                                              ") << std::endl;
    std::cout << formatMenuLine("    51[OK] - PTZ移動控制(_HamiCamPtzControlMove)      52[OK] - PTZ速度設定(_HamiCamPtzControlConfigSpeed)") << std::endl;
    std::cout << formatMenuLine("    53[OK] - 獲取PTZ控制資訊(_HamiCamGetPtzControl)   54[OK] - PTZ巡航模式(_HamiCamPtzControlTourGo)     ") << std::endl;
    std::cout << formatMenuLine("    55[OK] - PTZ移動到預設點(_HamiCamPtzControlGoPst) 56[OK] - PTZ設定預設點(_HamiCamPtzControlConfigPst)") << std::endl;
    std::cout << formatMenuLine("    57[--] - 人體追蹤(_HamiCamHumanTracking)          58[--] - 寵物追蹤(_HamiCamPetTracking)             ") << std::endl;
    std::cout << "╠════════════════════════════════════════════════════════════════════════════════════════════════════════════╣" << std::endl;
    std::cout << formatMenuLine("  AI設定類                                                                                               ") << std::endl;
    std::cout << formatMenuLine("    61[OK] - 更新AI設定(_UpdateCameraAISetting)       62[OK] - 獲取AI設定(_GetCameraAISetting)           ") << std::endl;
    std::cout << "╠════════════════════════════════════════════════════════════════════════════════════════════════════════════╣" << std::endl;
    std::cout << formatMenuLine("  串流控制類                                                                                             ") << std::endl;
    std::cout << formatMenuLine("    71[--] - 開始即時影音串流(_GetVideoLiveStream)     72[--] - 停止即時影音串流(_StopVideoLiveStream)    ") << std::endl;
    std::cout << formatMenuLine("    73[--] - 開始歷史影音串流(_GetVideoHistoryStream) 74[--] - 停止歷史影音串流(_StopVideoHistoryStream)  ") << std::endl;

//    std::cout << "║     75[--] - 開始雙向語音串流(_SendAudioStream)         76[--] - 停止雙向語音串流(_StopAudioStream)        ║" << std::endl;
//    std::cout << "║     77[--] - 串流管理器即時影音測試                     78[--] - 串流管理器歷史影音測試                    ║" << std::endl;
//    std::cout << "║     79[--] - 串流管理器音頻測試                                                                            ║" << std::endl;
#if 1 // Jeffery added 20250805
//    std::cout << "║    171[--] - 開始排程影音串流(_GetVideoScheduleStream) 172[--] - 停止排程影音串流(_StopVideoScheduleStream)║" << std::endl;
#endif
    //    std::cout << "╠════════════════════════════════════════════════════════════════════════════════════════════════════════════╣" << std::endl;
    //    std::cout << "║  特殊功能與批次測試                                                                                        ║" << std::endl;
    //    std::cout << "║     81[--] - 執行完整測試流程                         82[--] - 顯示當前狀態                                ║" << std::endl;
    //    std::cout << "║     90[--] - 時區批次測試                             91[--] - PTZ批次測試                                 ║" << std::endl;
    //    std::cout << "║     92[--] - 串流功能批次測試                         93[--] - 回報機制批次測試                            ║" << std::endl;
    //    std::cout << "╠════════════════════════════════════════════════════════════════════════════════════════════════════════════╣" << std::endl;
    //    std::cout << "║  回報機制測試類                                                                                            ║" << std::endl;
    //    std::cout << "║    102[--] - 測試錄影事件回報                                                                              ║" << std::endl;
    //    std::cout << "║    103[--] - 測試辨識事件回報                        104[--] - 測試狀態事件回報                            ║" << std::endl;
    //    std::cout << "║    105[--] - 回報管理器控制                                                                                ║" << std::endl;
    //    std::cout << "╠════════════════════════════════════════════════════════════════════════════════════════════════════════════╣" << std::endl;
    //    std::cout << "║  時區與時間同步類                                                                                          ║" << std::endl;
    //    std::cout << "║    201[--] - NTP 時間同步測試(完整功能)              202[--] - 顯示 NTP 狀態和歷史                         ║" << std::endl;
    //    std::cout << "║    203[--] - 顯示所有支援的時區列表                  204[--] - 搜尋時區                   　               ║" << std::endl;
    //    std::cout << "║    205[--] - 顯示時區詳細資訊                        206[--] - 快速時區設定(常用時區)       　             ║" << std::endl;
    //    std::cout << "║    299 - 時區+NTP綜合演示                       　　 300 - 顯示各時區當前時間                 　   　　　　║" << std::endl;
    //    std::cout << "╠════════════════════════════════════════════════════════════════════════════════════════════════════════════╣" << std::endl;
    //    std::cout << "║  參數管理與顯示類                                                                           　   　　　    ║" << std::endl;
    //    std::cout << "║    301[--] - 顯示所有 HamiCam 參數                   302[--] - 顯示人臉識別特徵詳細資訊         　         ║" << std::endl;
    //    std::cout << "║    303[--] - 人臉識別特徵管理                        304[--] - 匯出 HamiCam 參數                  　　     ║" << std::endl;
    //    std::cout << "║    305[--] - 重新解析初始化參數                      306[--] - 驗證參數完整性                         　　 ║" << std::endl;
    //    std::cout << "║    307[--] - 模擬參數更新測試                        308[--] - 參數變更通知測試                          　║" << std::endl;
    std::cout << "╠════════════════════════════════════════════════════════════════════════════════════════════════════════════╣" << std::endl;
    std::cout << formatMenuLine("  測試工具                                                                                                  ") << std::endl;
    std::cout << formatMenuLine("    77 - 串流管理器即時影音測試            78 - 串流管理器歷史影音測試                                     ") << std::endl;
    std::cout << formatMenuLine("    ip - 設定測試伺服器IP (目前: " + getTestServerIP() + ")                                               ") << std::endl;
    std::cout << "╠════════════════════════════════════════════════════════════════════════════════════════════════════════════╣" << std::endl;
    std::cout << formatMenuLine("                                                                                                            ") << std::endl;
    std::cout << formatMenuLine("    h  - 顯示此選單                               q  - 退出程序                                           ") << std::endl;
    std::cout << "╚════════════════════════════════════════════════════════════════════════════════════════════════════════════╝" << std::endl;
}

// ===== 主要互動測試函數 =====

void runEnhancedInteractiveTests(ChtP2PCameraAPI &cameraApi)
{
    std::cout << "\n===== 進入增強版互動測試模式 =====" << std::endl;

    displayMainMenu();

    std::string input;
    while (g_running)
    {
        std::cout << "\n請輸入指令: ";
        std::getline(std::cin, input);

        if (input.empty())
            continue;

        try
        {
            int choice = 0;
            if (input == "h" || input == "H")
            {
                displayMainMenu();
                continue;
            }
            else if (input == "q" || input == "Q")
            {
                g_running = false;
                std::cout << "退出程序..." << std::endl;
                break;
            }
            else if (input == "ip" || input == "IP")
            {
                setTestServerIP();
                continue;
            }
            else
            {
                choice = std::stoi(input);
            }

            switch (choice)
            {
                // 基本狀態與管理類 (1-6)
            case 1:
                testGetCamStatusById(cameraApi);
                break;
            case 2:
                testDeleteCameraInfo(cameraApi);
                break;
            case 3:
                testSetTimeZone(cameraApi);
                break;
            case 4:
                testGetTimeZone(cameraApi);
                break;
            case 5:
                testUpdateCameraName(cameraApi);
                break;
            case 6:
                testGetHamiCamBindList(cameraApi);
                break;
            case 7: // 新增
                testDisplayTimezoneStatus(cameraApi);
                break;
            case 8: // 新增
                testReloadTimezone(cameraApi);
                break;
            case 9: // 新增
                testReinitializeTimezone(cameraApi);
                break;
            case 10: // NTP 時間同步測試
                testNtpSync(cameraApi);
                break;

            // 時區與時間同步類 (201-206)
            // case 201:
            //     testNtpSync(cameraApi);
            //     break;
            // case 202:
            //     testDisplayNtpStatus(cameraApi);
            //     break;
            // case 203:
            //     testDisplayAllTimezones(cameraApi);
            //     break;
            // case 204:
            //     testSearchTimezone(cameraApi);
            //     break;
            // case 205:
            //     testDisplayTimezoneDetails(cameraApi);
            //     break;
            // case 206:
            //     testQuickTimezoneSetup(cameraApi);
            //     break;
            // case 299:
            //     testTimezoneWithNtpDemo(cameraApi);
            //     break;
            // case 300:
            //     testDisplayAllTimezoneCurrentTime(cameraApi);
            //     break;

            // 影像與顯示設定類 (11-17)
            case 11:
                testSetCameraOSD(cameraApi);
                break;
            case 12:
                testSetCameraHD(cameraApi);
                break;
            case 13:
                testSetFlicker(cameraApi);
                break;
            case 14:
                testSetImageQuality(cameraApi);
                break;
            case 15:
                testSetNightMode(cameraApi);
                break;
            case 16:
                testSetAutoNightVision(cameraApi);
                break;
            case 17:
                testSetFlipUpDown(cameraApi);
                break;

            // 音頻控制類 (21-22)
            case 21:
                testSetMicrophone(cameraApi);
                break;
            case 22:
                testSetSpeak(cameraApi);
                break;

            // 系統控制類 (31-36)
            case 31:
                testSetLED(cameraApi);
                break;
            case 32:
                testSetCameraPower(cameraApi);
                break;
            case 33:
                testGetSnapshotHamiCamDevice(cameraApi);
                break;
            case 34:
                testRestartHamiCamDevice(cameraApi);
                break;
            case 35:
                testHamiCamFormatSDCard(cameraApi);
                break;
            case 36:
                testUpgradeHamiCamOTA(cameraApi);
                break;

            // 存儲管理類 (41-42)
            case 41:
                testSetCamStorageDay(cameraApi);
                break;
            case 42:
                testSetCamEventStorageDay(cameraApi);
                break;

            // PTZ控制類 (51-58)
            case 51:
                testHamiCamPtzControlMove(cameraApi);
                break;
            case 52:
                testHamiCamPtzControlConfigSpeed(cameraApi);
                break;
            case 53:
                testHamiCamGetPtzControl(cameraApi);
                break;
            case 54:
                testHamiCamPtzControlTourGo(cameraApi);
                break;
            case 55:
                testHamiCamPtzControlGoPst(cameraApi);
                break;
            case 56:
                testHamiCamPtzControlConfigPst(cameraApi);
                break;
            case 57:
                testHamiCamHumanTracking(cameraApi);
                break;
            case 58:
                testHamiCamPetTracking(cameraApi);
                break;

            // AI設定類 (61-62)
            case 61:
                testUpdateCameraAISetting(cameraApi);
                break;
            case 62:
                testGetCameraAISetting(cameraApi);
                break;

            // 串流控制類 (71-79)
            case 71:
                testGetVideoLiveStream(cameraApi);
                break;
            case 72:
                testStopVideoLiveStream(cameraApi);
                break;
            case 73:
                testGetVideoHistoryStream(cameraApi);
                break;
            case 74:
                testStopVideoHistoryStream(cameraApi);
                break;
            case 75:
                testSendAudioStream(cameraApi);
                break;
            case 76:
                testStopAudioStream(cameraApi);
                break;
            case 77:
                testStreamManagerLiveVideo(cameraApi);
                break;
            case 78:
                //testStreamManagerHistoryVideo(cameraApi);
                break;
            case 79:
                testStreamManagerAudio(cameraApi);
                break;
#if 1 // Jeffery added 20250805
      // EX stream:
            case 171:
                testGetVideoScheduleStream(cameraApi);
                break;
            case 172:
                testStopVideoScheduleStream(cameraApi);
                break;
#endif
            // 特殊功能 (81-93)
            case 81:
                runCompleteTestSuite(cameraApi);
                break;
            case 82:
                displayCurrentStatus();
                break;
            case 90:
                runTimeZoneBatchTest(cameraApi);
                break;
            case 91:
                runPTZBatchTest(cameraApi);
                break;
            case 92:
                runStreamBatchTest(cameraApi);
                break;
            case 93:
                runReportBatchTest(cameraApi);
                break;

            // 回報機制測試類 (101-105)
            case 101:
                // testReportSnapshot(cameraApi);
                break;
            case 102:
                testReportRecord(cameraApi);
                break;
            case 103:
                testReportRecognition(cameraApi);
                break;
            case 104:
                testReportStatus(cameraApi);
                break;
            case 105:
                testReportManagerControl(cameraApi);
                break;
            // 參數管理與顯示類 (301-308)
            // case 301:
            //     testDisplayAllHamiCamParameters(cameraApi);
            //     break;
            // case 302:
            //     testDisplayIdentificationFeatures(cameraApi);
            //     break;
            // case 303:
            //     testManageIdentificationFeatures(cameraApi);
            //     break;
            // case 304:
            //     testExportHamiCamParameters(cameraApi);
            //     break;
            // case 305:
            //     testReparseInitialParameters(cameraApi);
            //     break;
            // case 306:
            //     testValidateParameterIntegrity(cameraApi);
            //     break;
            // case 307:
            //     testSimulateParameterUpdates(cameraApi);
            //     break;
            // case 308:
            //     testParameterChangeNotification(cameraApi);
            //     break;
            default:
                std::cout << "無效指令，請輸入 'h' 查看選單" << std::endl;
                break;
            }
        }
        catch (const std::exception &e)
        {
            std::cout << "輸入錯誤，請重新輸入" << std::endl;
        }
    }
}

int main(int argc, char *argv[])
{
    // ===== 程序初始化 =====
    // 註冊退出處理函數
    atexit(cleanupResources);

    printDebug("增強版互動測試程式開始執行");

    std::cout << "開始初始化媒體組態管理器..." << std::endl;
    std::cout << "使用多路徑自動搜尋組態檔案..." << std::endl;
    // 初始化媒體組態管理器
    // if (!InitializeMediaConfig())
    // {
    //     std::cerr << "Media config init failed - 嘗試手動搜尋..." << std::endl;

    //     // 方法2：如果自動搜尋失敗，嘗試手動指定路徑
    //     MediaConfigManager &manager = MediaConfigManager::getInstance();

    //     // 顯示搜尋的路徑
    //     std::cout << "\n手動檢查 VRec 組態檔案路徑:" << std::endl;
    //     auto vrecPaths = manager.getDefaultVRecPaths();
    //     for (const auto &path : vrecPaths)
    //     {
    //         std::ifstream file(path);
    //         if (file.good())
    //         {
    //             std::cout << "  ✓ 找到: " << path << std::endl;
    //             file.close();
    //             break;
    //         }
    //         else
    //         {
    //             std::cout << "  ✗ 不存在: " << path << std::endl;
    //         }
    //     }

    //     std::cout << "\n手動檢查 Stream 組態檔案路徑:" << std::endl;
    //     auto streamPaths = manager.getDefaultStreamPaths();
    //     for (const auto &path : streamPaths)
    //     {
    //         std::ifstream file(path);
    //         if (file.good())
    //         {
    //             std::cout << "  ✓ 找到: " << path << std::endl;
    //             file.close();
    //             break;
    //         }
    //         else
    //         {
    //             std::cout << "  ✗ 不存在: " << path << std::endl;
    //         }
    //     }

    //     std::cerr << "\n錯誤：無法找到任何媒體組態檔案" << std::endl;
    //     std::cerr << "請確認以下檔案存在且可讀取:" << std::endl;
    //     std::cerr << "  - vrec_conf.ini" << std::endl;
    //     std::cerr << "  - stream_server_config.ini" << std::endl;

    //     return -1;
    // }

    // // 顯示實際使用的組態檔案路徑
    // MediaConfigManager &manager = MediaConfigManager::getInstance();
    // std::cout << "\n✓ 媒體組態管理器初始化成功" << std::endl;
    // std::cout << "實際使用的組態檔案:" << std::endl;
    // std::cout << "  VRec 組態: " << manager.getActualVRecPath() << std::endl;
    // std::cout << "  Stream 組態: " << manager.getActualStreamPath() << std::endl;

    // test: 獲取並顯示 RTSP 連接埠
    // int iRTSP_Port = MEDIA_CONFIG.getRTSPPort();
    // std::string port_str = std::to_string(iRTSP_Port);
    // std::cout << "### RTSP port = " << port_str << std::endl;

    /********************************************************** */
    // 檢查是否啟用模擬模式
    /********************************************************** */
    bool simulationMode = false;
    for (int i = 1; i < argc; i++)
    {
        if (std::string(argv[i]) == "--simulation" || std::string(argv[i]) == "-s")
        {
            simulationMode = true;
            CameraDriver::getInstance().setSimulationMode(true);
            std::cout << "模擬模式已啟用" << std::endl;
            break;
        }
    }

    // 顯示運行模式和基本資訊
    std::cout << "======================================================================" << std::endl;
    std::cout << "=           ZINWELL CHT P2P 攝影機函數單元測試互動選單程式啟動          =" << std::endl;
    std::cout << "= 運行模式: " << (simulationMode ? "模擬模式" : "真實模式") << std::endl;
    std::cout << "= 程序版本: 2025.07.24                                                =" << std::endl;
    std::cout << "======================================================================" << std::endl;
    std::cout.flush(); // 確保立即輸出

    printDebug("開始檢查目錄權限");

    // ===== 檢查目錄權限和組態檔案路徑 =====
    // 檢查目錄是否可寫
    std::string etcConfigPath = "/etc/config";
    std::string testDirCmd = "mkdir -p " + etcConfigPath + " 2>/dev/null && touch " + etcConfigPath + "/.test && rm " + etcConfigPath + "/.test";
    bool etcConfigWritable = (system(testDirCmd.c_str()) == 0);

    printDebug("目錄權限檢查完成: " + std::string(etcConfigWritable ? "可寫" : "不可寫"));

    // 根據目錄可寫性決定組態檔案路徑
    std::string configPath = etcConfigWritable ? "/etc/config/ipcam_config.json" : "./ipcam_config.json";
    std::string paramsPath = etcConfigWritable ? "/etc/config/ipcam_params.json" : "./ipcam_params.json";
    std::string barcodePath = etcConfigWritable ? "/etc/config/ipcam_barcode.json" : "./ipcam_barcode.json";

    // 檢查組態檔案是否存在
    printDebug("設置組態檔案路徑：");
    printDebug("  組態路徑: " + configPath);
    printDebug("  參數路徑: " + paramsPath);
    printDebug("  條碼路徑: " + barcodePath);

    // ===== 判斷是否為首次執行 =====
    // 檢查組態檔案是否存在 - 這決定是否為首次執行
    bool configFileExists = std::ifstream(paramsPath).good();
    bool isFirstBinding = !configFileExists;
    std::cout << "[DEBUG] 組態檔案是否存在: " << (configFileExists ? "存在" : "不存在") << std::endl;
    std::cout << "[DEBUG] 是否首次繫結: " << (isFirstBinding ? "是" : "否") << std::endl;
    std::cout.flush(); // 確保立即輸出

    // ===== 初始化參數管理器 =====
    printStepHeader("初始化參數管理器");
    printDebug("開始初始化參數管理器");

    // 使用參數管理器獲取單例
    auto &paramsManager = CameraParametersManager::getInstance();

    // 嘗試初始化參數管理器
    try
    {
        bool initResult = paramsManager.initialize(paramsPath, barcodePath);
        printDebug("參數管理器初始化結果: " + std::string(initResult ? "成功" : "失敗"));

        if (!initResult)
        {
            std::cerr << "參數管理器初始化失敗" << std::endl;
            return 1;
        }

        // 強制設置繫結狀態 - 若是首次繫結則標記為未繫結
        if (isFirstBinding)
        {
            paramsManager.setParameter("activeStatus", std::string("0"));
            std::cout << "[DEBUG] 由於是首次繫結，強制設置 activeStatus=0" << std::endl;
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << "參數管理器初始化異常: " << e.what() << std::endl;
        return 1;
    }

    // ===== 檢查和恢復設備狀態 =====
    // 顯示當前狀態
    std::cout << "[DEBUG] activeStatus: " << paramsManager.getParameter("activeStatus", "未找到") << std::endl;
    std::cout << "[DEBUG] deviceStatus: " << paramsManager.getParameter("deviceStatus", "未找到") << std::endl;
    std::cout.flush(); // 確保立即輸出

    // 設置裝置執行狀態為 1（執行中）
    paramsManager.setParameter("deviceStatus", std::string("1"));

    // ===== 檢查攝影機綁定狀態和狀態恢復邏輯 =====
    // 檢查是否已繫結 - 使用 activeStatus 參數，並處理重啟後的狀態恢復
    bool isBound = (paramsManager.getParameter("activeStatus", "0") == "1");

    // 檢查是否有綁定完成標記（處理重啟後的狀態恢復）
    std::string bindingCompleted = paramsManager.getParameter("bindingCompleted", "0");
    if (!isBound && bindingCompleted == "1")
    {
        std::cout << "檢測到綁定完成標記，這是重啟後的狀態恢復" << std::endl;
        std::cout << "根據規格2.1，綁定成功並重啟後，設置為已綁定狀態" << std::endl;

        // 設置為已綁定狀態
        paramsManager.setParameter("activeStatus", std::string("1"));
        paramsManager.setParameter("bindingCompleted", std::string("0")); // 清除標記
        paramsManager.saveToFile();

        isBound = true;
        std::cout << "狀態恢復完成：activeStatus 已設置為已綁定" << std::endl;
    }

    printDebug("攝影機繫結狀態: " + std::string(isBound ? "已繫結" : "未繫結"));
    std::cout << "[DEBUG] activeStatus: " << paramsManager.getParameter("activeStatus", "未找到") << std::endl;
    std::cout << "[DEBUG] bindingCompleted: " << bindingCompleted << std::endl;

    // ===== 獲取或設置攝影機基本參數 =====
    // 獲取或設置 camId 和 chtBarcode
    std::string camId;
    std::string chtBarcode;
    std::string chtMacAddr;

    if (!isBound)
    {
        // 未繫結狀態 - 使用固定 camId 和 barcode
        printDebug("檢測到未綁定狀態，使用固定 camId 和 barcode");

        // 使用固定值或從參數管理器獲取
        camId = paramsManager.getCameraId();
        chtBarcode = paramsManager.getCHTBarcode();
        chtMacAddr = paramsManager.getMacAddress();

        std::cout << "[DEBUG] !isBound camId:" << camId << " chtBarcode:" << chtBarcode << " chtMacAddr:" << chtMacAddr << std::endl;

        // 設置基本參數到參數管理器
        paramsManager.setCameraId(camId);
        paramsManager.setCHTBarcode(chtBarcode);
        paramsManager.setParameter("macAddress", chtMacAddr);

        // ===== 設置預設的用戶資訊和系統參數 =====
        // 從 /etc/config/hami_uid 讀取 userId
        std::string userId = paramsManager.loadUserIdFromHamiUidFile();
        if (userId.empty())
        {
            std::cerr << "錯誤: 無法讀取 /etc/config/hami_uid 檔案或檔案內容為空" << std::endl;
            std::cerr << "請確認檔案存在且包含有效的 userId，攝影機無法註冊" << std::endl;

            // 設置裝置執行狀態為 0（未執行）
            paramsManager.setParameter("deviceStatus", std::string("0"));
            paramsManager.saveToFile();

            return 1; // 退出程式
        }

        // 從 /etc/config/wpa_supplicant.conf 讀取 WiFi 資訊
        std::string wifiSsid, wifiPassword;
        if (!paramsManager.loadWifiInfoFromSupplicantFile(wifiSsid, wifiPassword))
        {
            std::cerr << "錯誤: 無法從 /etc/config/wpa_supplicant.conf 解析 WiFi 資訊" << std::endl;
            std::cerr << "請確認檔案存在且包含有效的網路設定，攝影機無法註冊" << std::endl;

            // 設置裝置執行狀態為 0（未執行）
            paramsManager.setParameter("deviceStatus", std::string("0"));
            paramsManager.saveToFile();

            return 1; // 退出程式
        }

        // 檢查 chtBarcode 有效性
        std::string chtBarcode = paramsManager.getCHTBarcode();
        if (chtBarcode.empty() || chtBarcode == "0000000000000000000")
        {
            std::cerr << "錯誤: 無法從 U-Boot 環境變數讀取有效的 chtBarcode" << std::endl;
            std::cerr << "請確認系統啟動腳本已正確執行並設置 chtBarcode，攝影機無法註冊" << std::endl;

            // 設置裝置執行狀態為 0（未執行）
            paramsManager.setParameter("deviceStatus", std::string("0"));
            paramsManager.saveToFile();

            return 1; // 退出程式
        }

        // 設置從檔案讀取的用戶資訊
        paramsManager.setParameter("userId", userId);
        paramsManager.setParameter("name", std::string("我的攝影機"));
        paramsManager.setParameter("netNo", std::string("NET202402"));
        paramsManager.setParameter("firmwareVer", std::string("1.0.5"));
        paramsManager.setParameter("wifiSsid", wifiSsid);
        paramsManager.setParameter("wifiPassword", wifiPassword);
        // 不再設置固定的 wifiDbm 值，將由參數管理器從硬體動態獲取
        // paramsManager.setParameter("wifiDbm", std::string("-75"));
        paramsManager.setParameter("status", std::string("Normal"));
        paramsManager.setParameter("vsDomain", std::string("videoserver.example.com"));
        paramsManager.setParameter("vsToken", std::string(""));
        paramsManager.setParameter("activeStatus", std::string("0")); // 確保為未綁定
        paramsManager.setParameter("deviceStatus", std::string("1")); // 程式執行中
        paramsManager.setParameter("cameraType", std::string("IPCAM"));
        paramsManager.setParameter("model", std::string("XYZ-1000"));
        paramsManager.setParameter("isCheckHioss", std::string("0")); // 攝影機是否卡控識別，「0」：不需要(stage預設)，「1」： 需要(production預設)
        paramsManager.setParameter("brand", std::string("ABC Security"));
        paramsManager.setCamSid(std::string(""));   // 初始為空，等待服務器分配
        paramsManager.setTenantId(std::string("")); // 初始為空，等待服務器分配

        printDebug("預設參數已設置");

        // 保存組態到檔案
        paramsManager.saveToFile();
        bool saveBarcodeResult = paramsManager.saveBarcodeToFile(barcodePath);
        printDebug("條碼保存結果: " + std::string(saveBarcodeResult ? "成功" : "失敗"));
    }
    else
    {
        // 已繫結狀態 - 從組態檔案讀取
        printDebug("從已綁定組態讀取 camId 和 barcode");
        camId = paramsManager.getCameraId();
        chtBarcode = paramsManager.getCHTBarcode();
        if (camId.empty() || chtBarcode.empty()) {
            if (camId.empty() && !chtBarcode.empty()) {
                camId = chtBarcode;
                paramsManager.setCameraId(camId);
            } else if (chtBarcode.empty() && !camId.empty()) {
                chtBarcode = camId;
                paramsManager.setCHTBarcode(chtBarcode);
            } else {
                camId = chtBarcode = "000000000000000000000000000000";
                paramsManager.setCameraId(camId);
                paramsManager.setCHTBarcode(chtBarcode);
            }
        }

        printDebug("讀取的 CamID: " + camId);
        printDebug("讀取的 CHT Barcode: " + chtBarcode);
        printDebug("讀取的 UserId: " + paramsManager.getParameter("userId", ""));
        printDebug("讀取的 NetNo: " + paramsManager.getParameter("netNo", ""));
        printDebug("讀取的 WiFi SSID: " + paramsManager.getParameter("wifiSsid", ""));

        std::cout << "初始化時區和 NTP 同步..." << std::endl;
        bool timezoneResult = initializeSystemTimezone();
        if (!timezoneResult)
        {
            std::cerr << "時區初始化失敗，繼續執行但時間可能不正確" << std::endl;
        }
    }

    // ===== 同步硬體參數 =====
    printDebug("開始同步硬體參數");

    try
    {
        // 保存同步前的 activeStatus 值
        std::string preActiveStatus = paramsManager.getParameter("activeStatus", "0");
        std::cout << "[DEBUG] 同步前 activeStatus: " << preActiveStatus << std::endl;
        std::cout.flush(); // 確保輸出被立即顯示

        bool syncResult = paramsManager.syncWithHardware(true);
        printDebug("硬體參數同步結果: " + std::string(syncResult ? "成功" : "失敗"));

        // 確保 activeStatus 不因同步而改變 - 這裡是額外保障
        std::string postActiveStatus = paramsManager.getParameter("activeStatus", "0");
        std::cout << "[DEBUG] 同步後 activeStatus: " << postActiveStatus << std::endl;

        // 如果同步導致 activeStatus 改變，恢復原值
        if (preActiveStatus != postActiveStatus)
        {
            std::cout << "[DEBUG] 同步改變了 activeStatus，恢復為: " << preActiveStatus << std::endl;
            paramsManager.setParameter("activeStatus", preActiveStatus);
        }

        // 再次確認繫結狀態
        isBound = (paramsManager.getParameter("activeStatus", "0") == "1");
        std::cout << "[DEBUG] 同步後綁定狀態: " << (isBound ? "已綁定" : "未綁定") << std::endl;
    }
    catch (const std::exception &e)
    {
        std::cerr << "同步硬體參數異常: " << e.what() << std::endl;
    }

    // ===== 保存參數到檔案 =====
    printDebug("開始保存參數到檔案");

    try
    {
        // 保存參數到檔案前
        std::cout << "[DEBUG-PRE-SAVE] activeStatus: " << paramsManager.getParameter("activeStatus", "未找到") << std::endl;

        bool saveResult = paramsManager.saveToFile();
        bool saveBarcodeResult = paramsManager.saveBarcodeToFile(barcodePath);

        // 保存參數到檔案後
        std::cout << "[DEBUG-POST-SAVE] activeStatus: " << paramsManager.getParameter("activeStatus", "未找到") << std::endl;

        printDebug("參數保存結果: " + std::string(saveResult ? "成功" : "失敗"));
        printDebug("條碼保存結果: " + std::string(saveBarcodeResult ? "成功" : "失敗"));
    }
    catch (const std::exception &e)
    {
        std::cerr << "保存參數異常: " << e.what() << std::endl;
    }

    // 檢查保存的配置文件內容
    printConfig(paramsPath);
    printConfig(barcodePath);

    printDebug("基本參數設置完成");

    // ===== 顯示攝影機基本資訊 =====
    std::cout << "CamID: " << camId << std::endl;
    std::cout << "Barcode: " << chtBarcode << std::endl;
    std::cout << "綁定狀態: " << (isBound ? "已綁定" : "等待綁定") << std::endl;
    std::cout.flush(); // 確保立即輸出

    /*************************************************************************************/
    // ===== 註冊信號處理函數 =====
    printDebug("註冊信號處理函數");
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    std::cout << "CHT P2P Camera 互動測試程序啟動..." << std::endl;

    // ===== 創建CHT P2P Camera API實例 =====
    printStepHeader("初始化 CHT P2P Camera API");
    printDebug("創建 ChtP2PCameraAPI 實例");

    ChtP2PCameraAPI cameraApi;

    // 設置回調函數
    printDebug("設置回調函數");
    cameraApi.setInitialInfoCallback(onInitialInfo);
    cameraApi.setControlCallback(onControl);
    cameraApi.setAudioDataCallback(onAudioData);

    // 初始化攝影機參數
    printDebug("獲取攝影機初始化參數");
    printDebug("CamID: " + camId);
    printDebug("CHT Barcode: " + chtBarcode);

    // ===== 初始化CHT P2P服務 =====
    printDebug("開始初始化 CHT P2P 服務");
    std::cout.flush(); // 確保輸出被立即顯示

    try
    {
        std::cout << "開始初始化 CHT P2P 服務 (CamID: " << camId << ", Barcode: " << chtBarcode << ")" << std::endl;
        std::cout.flush(); // 確保輸出被立即顯示

        bool initResult = cameraApi.initialize(camId, chtBarcode);
        if (!initResult)
        {
            std::cerr << "初始化CHT P2P服務失敗" << std::endl;
            return 1;
        }
        std::cout << "CHT P2P服務初始化成功" << std::endl;
        std::cout.flush(); // 確保輸出被立即顯示
    }
    catch (const std::exception &e)
    {
        std::cerr << "初始化CHT P2P服務時發生異常: " << e.what() << std::endl;
        return 1;
    }
    catch (...)
    {
        std::cerr << "初始化CHT P2P服務時發生未知異常" << std::endl;
        return 1;
    }

    // ===== 執行攝影機綁定流程（如果尚未綁定）=====
    // 如果未繫結，執行繫結攝影機流程
    if (!isBound)
    {
        printStepHeader("執行綁定攝影機流程");
        std::cout << "開始綁定攝影機..." << std::endl;
        std::cout.flush(); // 確保輸出被立即顯示

        try
        {
            // 獲取所需參數
            std::string userId = paramsManager.getParameter("userId", "USER7890");
            std::string name = paramsManager.getParameter("name", "我的攝影機");
            std::string netNo = paramsManager.getParameter("netNo", "NET202402");
            std::string firmwareVer = paramsManager.getParameter("firmwareVer", "1.0.5");
            std::string externalStorageHealth = paramsManager.getStorageHealth();
            std::string wifiSsid = paramsManager.getParameter("wifiSsid", "Home_WiFi");
            // 動態獲取當前 WiFi 信號強度，而非使用儲存的固定值
            int wifiDbm = paramsManager.getWifiSignalStrength();
            std::string status = paramsManager.getParameter("status", "Normal");
            std::string vsDomain = paramsManager.getParameter("vsDomain", "videoserver.example.com");
            std::string vsToken = paramsManager.getParameter("vsToken", "");
            std::string macAddress = paramsManager.getParameter("macAddress", "00:1A:2B:3C:4D:5E");
            std::string activeStatus = "0"; // 根據規格2.1，綁定時應為未啟用狀態
            std::string deviceStatus = "1"; // 程式執行中
            std::string cameraType = paramsManager.getParameter("cameraType", "IPCAM");
            std::string model = paramsManager.getParameter("model", "XYZ-1000");
            std::string isCheckHioss = paramsManager.getParameter("isCheckHioss", "0"); // 攝影機是否卡控識別，「0」：不需要(stage預設)，「1」： 需要(production預設)
            std::string brand = paramsManager.getParameter("brand", "ABC Security");

            // 確認所有參數值
            std::cout << "綁定攝影機使用參數:" << std::endl;
            std::cout << "  userId: " << userId << std::endl;
            std::cout << "  name: " << name << std::endl;
            std::cout << "  netNo: " << netNo << std::endl;
            std::cout << "  firmwareVer: " << firmwareVer << std::endl;
            std::cout << "  externalStorageHealth: " << externalStorageHealth << std::endl;
            std::cout << "  wifiSsid: " << wifiSsid << std::endl;
            std::cout << "  wifiDbm: " << wifiDbm << std::endl;
            std::cout << "  status: " << status << std::endl;
            std::cout << "  vsDomain: " << vsDomain << std::endl;
            std::cout << "  vsToken: " << vsToken << std::endl;
            std::cout << "  macAddress: " << macAddress << std::endl;
            std::cout << "  activeStatus: " << activeStatus << " (綁定時為未啟用)" << std::endl;
            std::cout << "  deviceStatus: " << deviceStatus << std::endl;
            std::cout << "  cameraType: " << cameraType << std::endl;
            std::cout << "  model: " << model << std::endl;
            std::cout << "  isCheckHioss: " << isCheckHioss << std::endl;
            std::cout << "  brand: " << brand << std::endl;
            std::cout << "  camId: " << camId << std::endl;
            std::cout << "  chtBarcode: " << chtBarcode << std::endl;
            std::cout.flush(); // 確保輸出被立即顯示

            std::cout << "準備執行綁定攝影機..." << std::endl;
            std::cout.flush(); // 確保輸出被立即顯示

            // 執行綁定攝影機 - 使用 _BindCameraReport 命令
            bool bindResult = cameraApi.bindCameraReport(
                userId, name, netNo, firmwareVer, externalStorageHealth, wifiSsid, wifiDbm,
                status, vsDomain, vsToken, macAddress, activeStatus, deviceStatus,
                cameraType, model, isCheckHioss, brand, chtBarcode);

            if (!bindResult)
            {
                std::cerr << "綁定攝影機失敗" << std::endl;

                // 設置裝置執行狀態為 0（未執行）
                paramsManager.setParameter("deviceStatus", std::string("0"));
                paramsManager.saveToFile();

                cameraApi.deinitialize();
                return 1;
            }

            std::cout << "綁定攝影機成功" << std::endl;
            std::cout.flush(); // 確保輸出被立即顯示

            // 根據規格2.1，綁定成功後Camera保存相關設定，然後進行reboot
            // 這裡先不要立即設置 activeStatus = "1"，應該等重啟後根據伺服器回應決定
            // 但為了程式邏輯正確，我們需要設置一個標記表示綁定已完成
            paramsManager.setParameter("bindingCompleted", std::string("1"));
            std::cout << "綁定完成標記已設置" << std::endl;

            // 保存更新後的參數
            paramsManager.saveToFile();
            std::cout << "綁定狀態保存成功" << std::endl;
            std::cout.flush(); // 確保輸出被立即顯示

            // 根據規格2.1，Camera保存相關設定後，進行reboot
            std::cout << "===================================================" << std::endl;
            std::cout << "=     綁定攝影機成功，依據規格2.1進行重新開機       =" << std::endl;
            std::cout << "= 運行模式: " << (simulationMode ? "模擬模式" : "真實模式") << std::endl;
            std::cout << "= 執行命令: _BindCameraReport 已成功                =" << std::endl;
            std::cout << "= 規格要求: Camera保存相關設定後，進行reboot        =" << std::endl;
            std::cout << "===================================================" << std::endl;
            std::cout.flush(); // 確保立即輸出

            if (simulationMode)
            {
                // 模擬模式下模擬重啟
                std::cout << "模擬模式：準備模擬設備重啟..." << std::endl;
                std::cout.flush(); // 確保輸出被立即顯示

                // 停止 P2P 服務
                std::cout << "停止 P2P 服務..." << std::endl;
                cameraApi.deinitialize();

                // 模擬重啟過程
                std::cout << "===================================================" << std::endl;
                std::cout << "=               模擬設備重啟中                     =" << std::endl;
                std::cout << "= 運行模式: 模擬模式                              =" << std::endl;
                std::cout << "= 執行操作: 關閉P2P服務並重新初始化                =" << std::endl;
                std::cout << "= 重啟原因: 綁定成功後依規格2.1要求重啟            =" << std::endl;
                std::cout << "===================================================" << std::endl;
                std::cout.flush(); // 確保立即輸出

                std::this_thread::sleep_for(std::chrono::seconds(3));

                std::cout << "模擬設備重啟完成，重新初始化..." << std::endl;
                std::cout.flush(); // 確保輸出被立即顯示

                std::cout << "===================================================" << std::endl;
                std::cout << "=               模擬設備重啟完成                   =" << std::endl;
                std::cout << "= 運行模式: 模擬模式                              =" << std::endl;
                std::cout << "= 後續流程: 重啟後依規格2.2執行報到流程            =" << std::endl;
                std::cout << "===================================================" << std::endl;
                std::cout.flush(); // 確保立即輸出

                // 重新讀取參數（模擬重啟後狀態恢復）
                paramsManager.loadFromFile();
                std::cout << "重新讀取參數成功" << std::endl;

                // 檢查綁定完成標記，設置正確狀態
                std::string bindingCompleted = paramsManager.getParameter("bindingCompleted", "0");
                if (bindingCompleted == "1")
                {
                    std::cout << "檢測到綁定完成標記，設置為已綁定狀態" << std::endl;
                    paramsManager.setParameter("activeStatus", std::string("1"));
                    paramsManager.setParameter("bindingCompleted", std::string("0")); // 清除標記
                    paramsManager.saveToFile();

                    // 更新綁定狀態變數
                    isBound = true;
                }

                // 重新獲取 camId 和 chtBarcode
                camId = paramsManager.getCameraId();
                chtBarcode = paramsManager.getCHTBarcode();

                // 重新初始化 P2P 服務
                std::cout << "重新初始化 P2P 服務..." << std::endl;
                std::cout.flush(); // 確保輸出被立即顯示

                if (!cameraApi.initialize(camId, chtBarcode))
                {
                    std::cerr << "重啟後重新初始化 P2P 服務失敗" << std::endl;
                    return 1;
                }

                std::cout << "P2P 服務重新初始化成功，準備執行規格2.2流程" << std::endl;
                std::cout.flush(); // 確保輸出被立即顯示
            }
            else
            {
                // 真實模式下，執行實際重啟
                // 設置裝置執行狀態為 0（未執行），因為要重啟了
                paramsManager.setParameter("deviceStatus", std::string("0"));
                paramsManager.saveToFile();

                std::cout << "真實模式：設備將重啟" << std::endl;
                int rebootResult = system("reboot");
                if (rebootResult != 0)
                {
                    std::cerr << "重啟命令執行失敗，錯誤碼: " << rebootResult << std::endl;
                }
                return 0; // 退出程序，等待重啟
            }
        }
        catch (const std::exception &e)
        {
            std::cerr << "繫結攝影機時發生異常: " << e.what() << std::endl;

            // 設置裝置執行狀態為 0（未執行）
            paramsManager.setParameter("deviceStatus", std::string("0"));
            paramsManager.saveToFile();

            cameraApi.deinitialize();
            return 1;
        }
    }

    // ===== 攝影機報到流程（規格2.2）=====
    // 攝影機報到 - 根據廠商說明檔案 2.2 步驟
    printDebug("開始攝影機報到");
    std::cout.flush(); // 確保輸出被立即顯示

    try
    {
        std::cout << "開始執行攝影機報到..." << std::endl;
        std::cout.flush(); // 確保輸出被立即顯示

        bool registerResult = cameraApi.cameraRegister();
        if (!registerResult)
        {
            std::cerr << "攝影機報到失敗" << std::endl;

            // 設置裝置執行狀態為 0（未執行）
            paramsManager.setParameter("deviceStatus", std::string("0"));
            paramsManager.saveToFile();

            cameraApi.deinitialize();
            return 1;
        }
        else
        {
            std::cout << "===================================================" << std::endl;
            std::cout << "=               攝影機報到成功                     =" << std::endl;
            std::cout << "= 運行模式: " << (simulationMode ? "模擬模式" : "真實模式") << std::endl;
            std::cout << "= 執行命令: _CameraRegister                       =" << std::endl;
            std::cout << "= 後續流程: 檢查HiOSS狀態                         =" << std::endl;
            std::cout << "===================================================" << std::endl;
            std::cout.flush(); // 確保立即輸出
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << "攝影機報到時發生異常: " << e.what() << std::endl;

        // 設置裝置執行狀態為 0（未執行）
        paramsManager.setParameter("deviceStatus", std::string("0"));
        paramsManager.saveToFile();

        cameraApi.deinitialize();
        return 1;
    }

    /*
    # 測試 HiOSS 受限模式
    export SIM_HIOSS_STATUS=false
    ./cht_p2p_camera_example --simulation

    # 這樣可以測試：
    # 1. 受限模式下是否跳過 _GetHamiCamInitialInfo
    # 2. 控制指令是否只接收 _DeleteCameraInfo
    # 3. 其他控制指令是否被正確拒絕

    HiOSS狀態檢查的兩種結果 :
    正常模式 (data.status = true)
        繼續執行 _GetHamiCamInitialInfo
        啟動完整功能
        接受所有遠端控制指令

    受限模式 (data.status = false)

        跳過 _GetHamiCamInitialInfo
        不啟動 定時回報機制
        僅接受 _DeleteCameraInfo 遠端控制指令
        拒絕 所有其他控制指令

    */
    // ===== 檢查HiOSS狀態流程（規格2.2）=====
    // 檢查HiOSS狀態 - 根據廠商說明檔案 2.2 步驟
    printDebug("開始檢查 HiOSS 狀態");
    std::cout.flush(); // 確保輸出被立即顯示

    bool hiossStatusAllowed = false; // 用來記錄HiOSS狀態檢查結果

    try
    {
        std::cout << "開始檢查 HiOSS 狀態..." << std::endl;
        std::cout.flush(); // 確保輸出被立即顯示

        std::string actualIp = paramsManager.getPublicIp();
        std::cout << "使用公網 IP: " << actualIp << " 檢查 HiOSS 狀態" << std::endl;

        hiossStatusAllowed = cameraApi.checkHiOSSstatus(actualIp);

        if (hiossStatusAllowed)
        {
            std::cout << "===================================================" << std::endl;
            std::cout << "=             HiOSS狀態檢查成功                   =" << std::endl;
            std::cout << "= 運行模式: " << (simulationMode ? "模擬模式" : "真實模式") << std::endl;
            std::cout << "= 執行命令: _CheckHiOSSstatus                     =" << std::endl;
            std::cout << "= 檢查結果: status=true，允許所有控制指令          =" << std::endl;
            std::cout << "= 後續流程: 取得攝影機初始值                       =" << std::endl;
            std::cout << "===================================================" << std::endl;
            paramsManager.setParameter("hiossStatus", "1");
            std::cout.flush(); // 確保立即輸出
        }
        else
        {
            std::cout << "===================================================" << std::endl;
            std::cout << "=             HiOSS狀態檢查受限                   =" << std::endl;
            std::cout << "= 運行模式: " << (simulationMode ? "模擬模式" : "真實模式") << std::endl;
            std::cout << "= 執行命令: _CheckHiOSSstatus                     =" << std::endl;
            std::cout << "= 檢查結果: status=false，僅接收解綁指令          =" << std::endl;
            std::cout << "= 限制說明: 後續僅接收_DeleteCameraInfo控制指令   =" << std::endl;
            std::cout << "===================================================" << std::endl;
            paramsManager.setParameter("hiossStatus", "0");
            std::cout.flush(); // 確保立即輸出

            std::cout << "\n警告：HiOSS狀態檢查顯示設備處於受限模式" << std::endl;
            std::cout << "根據規格要求，設備將僅接收解綁攝影機的遠端控制指令" << std::endl;
            std::cout << "其他控制指令將被拒絕執行" << std::endl;
            std::cout << "設備將跳過初始化資訊獲取，直接進入待機狀態" << std::endl;
            std::cout.flush(); // 確保輸出被立即顯示
        }
        // 保存狀態
        paramsManager.saveToFile();
    }
    catch (const std::exception &e)
    {
        std::cerr << "檢查HiOSS狀態時發生異常: " << e.what() << std::endl;
        hiossStatusAllowed = false;

        // 設置裝置執行狀態為 0（未執行）
        paramsManager.setParameter("deviceStatus", std::string("0"));
        paramsManager.setParameter("hiossStatus", std::string("0"));
        paramsManager.saveToFile();

        cameraApi.deinitialize();
        return 1;
    }
    catch (...)
    {
        std::cerr << "檢查HiOSS狀態時發生未知異常" << std::endl;
        hiossStatusAllowed = false;

        // 設置裝置執行狀態為 0（未執行）
        paramsManager.setParameter("deviceStatus", std::string("0"));
        paramsManager.setParameter("hiossStatus", std::string("0"));
        paramsManager.saveToFile();

        cameraApi.deinitialize();
        return 1;
    }

    // ===== 根據HiOSS狀態決定後續流程 =====
    // 只有當HiOSS狀態為允許(true)時，才執行取得攝影機初始值
    if (hiossStatusAllowed)
    {
        // 取得攝影機初始值 - 根據廠商說明檔案 2.2 步驟
        printDebug("開始獲取攝影機初始值");
        std::cout.flush(); // 確保輸出被立即顯示

        try
        {
            std::cout << "開始獲取攝影機初始值..." << std::endl;
            std::cout.flush(); // 確保輸出被立即顯示

            bool initInfoResult = cameraApi.getHamiCamInitialInfo();
            if (!initInfoResult)
            {
                std::cerr << "取得攝影機初始值失敗" << std::endl;

                // 設置裝置執行狀態為 0（未執行）
                paramsManager.setParameter("deviceStatus", std::string("0"));
                paramsManager.saveToFile();

                cameraApi.deinitialize();
                return 1;
            }
            else
            {
                std::cout << "===================================================" << std::endl;
                std::cout << "=             獲取攝影機初始值成功                 =" << std::endl;
                std::cout << "= 運行模式: " << (simulationMode ? "模擬模式" : "真實模式") << std::endl;
                std::cout << "= 執行命令: _GetHamiCamInitialInfo                =" << std::endl;
                std::cout << "= 流程狀態: 所有初始化流程已完成，進入互動測試模式 =" << std::endl;
                std::cout << "===================================================" << std::endl;
                std::cout.flush();

                // 確認參數處理和硬體同步狀態
                std::cout << "\n===== 初始化後參數狀態確認 =====" << std::endl;
                std::cout << "設備狀態: " << paramsManager.getDeviceStatus() << std::endl;
                std::cout << "綁定狀態: " << paramsManager.getActiveStatus() << std::endl;
                std::cout << "HiOSS狀態: " << paramsManager.getParameter("hiossStatus", "未設置") << std::endl;
                std::cout << "韌體版本: " << paramsManager.getFirmwareVersion() << std::endl;
                std::cout << "WiFi SSID: " << paramsManager.getWifiSsid() << std::endl;
                std::cout << "存儲健康: " << paramsManager.getStorageHealth() << std::endl;
                std::cout << "===== 參數狀態確認完成 =====" << std::endl;
            }

            // 輸出提示，表示完成所有初始化流程
            std::cout << "\n\n===== 所有初始化流程已完成，進入互動測試模式 =====" << std::endl;
            std::cout.flush(); // 確保輸出被立即顯示
        }
        catch (const std::exception &e)
        {
            std::cerr << "取得攝影機初始值時發生異常: " << e.what() << std::endl;

            // 設置裝置執行狀態為 0（未執行）
            paramsManager.setParameter("deviceStatus", std::string("0"));
            paramsManager.saveToFile();

            cameraApi.deinitialize();
            return 1;
        }
        catch (...)
        {
            std::cerr << "取得攝影機初始值時發生未知異常" << std::endl;

            // 設置裝置執行狀態為 0（未執行）
            paramsManager.setParameter("deviceStatus", std::string("0"));
            paramsManager.saveToFile();

            cameraApi.deinitialize();
            return 1;
        }
    }
    else
    {
        // HiOSS狀態不允許，跳過初始化資訊獲取
        std::cout << "\n===== HiOSS狀態受限，跳過初始化資訊獲取 =====" << std::endl;
        std::cout << "由於HiOSS狀態檢查結果為受限模式(status=false)" << std::endl;
        std::cout << "根據規格2.2要求，跳過_GetHamiCamInitialInfo步驟" << std::endl;
        std::cout << "設備將直接進入互動測試模式，但功能受限" << std::endl;
        std::cout << "===== 受限模式互動測試狀態 =====" << std::endl;
        std::cout.flush(); // 確保輸出被立即顯示
    }

    // ===== 初始化成功，自動啟動定時回報機制 =====
    std::cout << "\n===== 攝影機初始化完成，啟動定時回報機制 =====" << std::endl;
    // 顯示環境變數設定說明
    std::cout << "\n環境變數組態說明：" << std::endl;
    std::cout << "  SNAPSHOT_INTERVAL=秒數      - 設定截圖事件間隔 (預設45秒)" << std::endl;
    std::cout << "  RECORD_INTERVAL=秒數         - 設定錄影事件間隔 (預設60秒)" << std::endl;
    std::cout << "  RECOGNITION_INTERVAL=秒數    - 設定辨識事件間隔 (預設35秒)" << std::endl;
    std::cout << "  STATUS_INTERVAL=秒數         - 設定狀態事件間隔 (預設30秒)" << std::endl;
    std::cout << "  範圍: 5-300秒，例如: export SNAPSHOT_INTERVAL=60" << std::endl;

    g_reportManager = std::make_unique<ReportManager>(&cameraApi);
    g_reportManager->stop();

    std::cout << "\n系統初始化完成，進入增強版互動測試模式" << std::endl;
    std::cout << "運行模式: " << (simulationMode ? "模擬模式" : "真實模式") << std::endl;

    // ===== 執行增強版互動測試 =====
    runEnhancedInteractiveTests(cameraApi);

    // ===== 程序退出處理 =====
    // 清理回報管理器
    if (g_reportManager)
    {
        std::cout << "正在清理資源..." << std::endl;
        g_reportManager->stop();
        g_reportManager.reset();
    }

    // 設置裝置執行狀態為 0（未執行）
    paramsManager.setParameter("deviceStatus", std::string("0"));
    paramsManager.saveToFile();

    // 停止CHT P2P服務
    printDebug("開始停止 CHT P2P 服務");
    try
    {
        std::cout << "正在停止 CHT P2P 服務..." << std::endl;
        cameraApi.deinitialize();
        std::cout << "CHT P2P 服務已停止" << std::endl;
    }
    catch (const std::exception &e)
    {
        std::cerr << "停止 CHT P2P 服務時發生錯誤: " << e.what() << std::endl;
    }

    printDebug("程序正常結束");
    std::cout << "程序已退出" << std::endl;
    return 0;
}
