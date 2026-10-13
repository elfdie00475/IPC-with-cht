/**
 * @file cht_p2p_camera_api.cpp
 * @brief CHT P2P Camera API實現
 * @date 2025/04/29
 */

#include <sys/stat.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <csignal>
#include <cstdlib> // std::strtol
#include <fstream>
#include <future>
#include <iomanip>
#include <iostream>
#include <map>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <regex>
#include <vector>

#include <rapidjson/document.h>
#include <rapidjson/error/en.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include "zwsystem_ipc_client.h"

#include "cht_p2p_camera_command_handler.h"
#include "cht_p2p_agent_payload_defined.h"
#include "camera_parameters_manager.h"

// 內部輔助函數 - 格式化時間戳
static std::string getFormattedTimestamp()
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

// 內部輔助函數 - 驗證JSON回應格式
static bool validateJsonResponse(const std::string &response, std::string &errorMsg)
{
    try
    {
        rapidjson::Document responseJson;
        rapidjson::ParseResult parseResult = responseJson.Parse(response.c_str());
        if (parseResult.IsError())
        {
            errorMsg = "JSON解析失敗: " + std::string(GetParseError_En(parseResult.Code()));
            return false;
        }

        if (!responseJson.HasMember("code") && !responseJson.HasMember(PAYLOAD_KEY_RESULT))
        {
            errorMsg = "回應格式錯誤：缺少code或result欄位";
            return false;
        }

        return true;
    }
    catch (const std::exception &e)
    {
        errorMsg = "驗證回應時發生異常: " + std::string(e.what());
        return false;
    }
}

// 內部輔助函數 - 處理初始化資訊錯誤
static void handleInitialInfoError(const std::string &errorMsg)
{
    std::cerr << "處理初始化資訊失敗: " << errorMsg << std::endl;

    // 記錄錯誤到參數管理器
    auto &paramsManager = CameraParametersManager::getInstance();
    paramsManager.addDebugLog("InitialInfo處理錯誤: " + errorMsg, true);

    // 設置錯誤狀態
    paramsManager.setParameter("lastInitError", errorMsg);
    paramsManager.setParameter("lastInitTime", std::to_string(std::time(nullptr)));
}

ChtP2PCameraCommandHandler &ChtP2PCameraCommandHandler::getInstance()
{
    static ChtP2PCameraCommandHandler instance;

    return instance;
}

ChtP2PCameraCommandHandler::ChtP2PCameraCommandHandler()
: m_initialized{false}
{
    initialize();
}

ChtP2PCameraCommandHandler::~ChtP2PCameraCommandHandler()
{
    deinitialize();
}

bool ChtP2PCameraCommandHandler::initialize(void)
{
    if (m_initialized) return true;

    m_initialized = true;
    return true;
}

void ChtP2PCameraCommandHandler::deinitialize()
{
    if (!m_initialized)
    {
        return;
    }

    m_initialized = false;
}

/*
規格要求：
    攝影機報到成功後從回應中提取publicIp
    後續的checkHiOSSstatus將使用正確的publicIp
    保持原有的錯誤處理和日誌輸出
*/
int ChtP2PCameraCommandHandler::bindCamera(const BindCameraConfig& config)
{
    if (!m_initialized)
    {
        std::cerr << "CHT P2P服務尚未初始化" << std::endl;
        return -1;
    }

    // change wifi first ==> system server
    uint32_t ssid_size = config.wifiSsid.size();
    uint32_t password_size = config.wifiPassword.size();
    if (ssid_size >= ZWSYSTEM_IPC_STRING_SIZE || password_size >= ZWSYSTEM_IPC_STRING_SIZE)
    {
        return -2;
    }

    stChangeWifiReq wifiReq;
    stChangeWifiRep wifiRep;

    snprintf(wifiReq.wifiSsid, ZWSYSTEM_IPC_STRING_SIZE, "%s", config.wifiSsid.c_str());
    snprintf(wifiReq.password, ZWSYSTEM_IPC_STRING_SIZE, "%s", config.wifiPassword.c_str());
    int rc = zwsystem_ipc_changeWifi(wifiReq, &wifiRep);
    if (rc < 0 || wifiRep.code < 0)
    {
        return -3;
    }

    std::string repWifiSsid = std::string(wifiRep.wifiSsid);
    if (config.wifiSsid != repWifiSsid)
    {
        return -4;
    }
    int wifiDdm = wifiRep.wifiDbm;

    // if scucess, call bindCameraReport
    stBindCameraReportReq bindReq;
    stBindCameraReportRep bindRep;
    rc = zwsystem_ipc_bindCameraReport(bindReq, &bindRep);
    if (rc < 0 || bindRep.code < 0)
    {
        return -5;
    }

    auto &paramsManager = CameraParametersManager::getInstance();
    const std::string& camId = paramsManager.getCameraId();
    const std::string& barcode = paramsManager.getCHTBarcode();
    const std::string& firmwareVer = paramsManager.getFirmwareVersion();

    bool bindRes = bindCameraReport(camId, config.userId,
                bindRep.name, config.netNo, firmwareVer,
                zwsystem_ipc_health_int2str(bindRep.externalStorageHealth),
                config.wifiSsid, wifiDdm,
                zwsystem_ipc_status_int2str(bindRep.status),
                bindRep.vsDomain, bindRep.vsToken, bindRep.macAddress,
                std::to_string(bindRep.activeStatus), std::to_string(bindRep.deviceStatus),
                CHT_P2P_AGENT_CAMERA_TYPE,
                bindRep.model,
                std::to_string(bindRep.isCheckHioss),
                bindRep.brand, barcode);
    if (!bindRes) {
        return -5;
    }

    return 0;
}

static void AddString(rapidjson::Document& doc, const char* key, const std::string& val)
{
    auto& alloc = doc.GetAllocator();
    rapidjson::Value k(key, alloc);
    rapidjson::Value v;
    v.SetString(val.c_str(), static_cast<rapidjson::SizeType>(val.size()), alloc);
    doc.AddMember(k, v, alloc);
}

static void AddString(rapidjson::Value& doc, const char* key, const std::string& val, rapidjson::Document::AllocatorType& alloc)
{
    rapidjson::Value k(key, alloc);
    rapidjson::Value v;
    v.SetString(val.c_str(), static_cast<rapidjson::SizeType>(val.size()), alloc);
    doc.AddMember(k, v, alloc);
}

static const rapidjson::Value& GetObjectMember(const rapidjson::Value& obj, const char* key)
{
    if (!obj.IsObject())
        throw std::runtime_error("Expected object when accessing member: " + std::string(key));

    auto it = obj.FindMember(key);
    if (it == obj.MemberEnd())
        throw std::runtime_error("Missing member: " + std::string(key));

    if (!it->value.IsObject())
        throw std::runtime_error("Member is not object: " + std::string(key));

    return it->value;
}

static int GetIntMember(const rapidjson::Value& obj, const char* key)
{
    if (!obj.IsObject())
        throw std::runtime_error("Expected object when accessing member: " + std::string(key));

    auto it = obj.FindMember(key);
    if (it == obj.MemberEnd() || !it->value.IsInt())
        throw std::runtime_error("Missing or not int: " + std::string(key));

    return it->value.GetInt();
}

static bool GetBoolMember(const rapidjson::Value& obj, const char* key)
{
    if (!obj.IsObject())
        throw std::runtime_error("Expected object when accessing member: " + std::string(key));

    auto it = obj.FindMember(key);
    if (it == obj.MemberEnd() || !it->value.IsBool())
        throw std::runtime_error("Missing or not string: " + std::string(key));

    return it->value.GetBool();
}

static std::string GetStringMember(const rapidjson::Value& obj, const char* key)
{
    if (!obj.IsObject())
        throw std::runtime_error("Expected object when accessing member: " + std::string(key));

    auto it = obj.FindMember(key);
    if (it == obj.MemberEnd() || !it->value.IsString())
        throw std::runtime_error("Missing or not string: " + std::string(key));

    // 直接複製出來，避免 dangling reference
    return std::string(it->value.GetString(), it->value.GetStringLength());
}

bool ChtP2PCameraCommandHandler::bindCameraReport(const std::string& camId,
        const std::string &userId, const std::string &name, const std::string &netNo,
        const std::string &firmwareVer, const std::string &externalStorageHealth,
        const std::string &wifiSsid, int wifiDbm, const std::string &status,
        const std::string &vsDomain, const std::string &vsToken, const std::string &macAddress,
        const std::string &activeStatus, const std::string &deviceStatus,
        const std::string &cameraType, const std::string &model,
        const std::string &isCheckHioss, const std::string &brand, const std::string &chtBarcode)
{
    if (!m_initialized)
    {
        std::cerr << "CHT P2P服務尚未初始化" << std::endl;
        return false;
    }

    auto require_non_empty = [](const std::string& s) { return !s.empty(); };
    if (!require_non_empty(camId) || !require_non_empty(userId) || !require_non_empty(chtBarcode)) {
        return false;
    }
    if (wifiDbm < -150 || wifiDbm > 50) {
        return false;
    }

    // 更新參數管理器中的參數
    // 調試輸出
    printApiDebug("綁定攝影機使用參數:");
    printApiDebug("  camId: " + camId);
    printApiDebug("  chtBarcode: " + chtBarcode);

    try {
        // 構建JSON payload
        rapidjson::Document document;
        document.SetObject();
        auto& allocator = document.GetAllocator();

        AddString(document, PAYLOAD_KEY_CAMID, camId);
        AddString(document, PAYLOAD_KEY_UID, userId);
        AddString(document, PAYLOAD_KEY_NAME, name);
        AddString(document, PAYLOAD_KEY_NETNO, netNo);
        AddString(document, PAYLOAD_KEY_FIRMWARE_VER, firmwareVer);
        AddString(document, PAYLOAD_KEY_EXTERNAL_STORAGE_HEALTH, externalStorageHealth);
        AddString(document, PAYLOAD_KEY_WIFI_SSID, wifiSsid);

        document.AddMember(rapidjson::Value().SetString(PAYLOAD_KEY_WIFI_DBM, allocator), wifiDbm, allocator);

        AddString(document, PAYLOAD_KEY_STATUS, status);
        AddString(document, PAYLOAD_KEY_VSDOMAIN, vsDomain);
        AddString(document, PAYLOAD_KEY_VSTOKEN, vsToken);
        AddString(document, PAYLOAD_KEY_MAC_ADDRESS, macAddress);
        AddString(document, PAYLOAD_KEY_ACTIVE_STATUS, activeStatus);
        AddString(document, PAYLOAD_KEY_DEVICE_STATUS, deviceStatus);
        AddString(document, PAYLOAD_KEY_CAMERA_TYPE, cameraType);
        AddString(document, PAYLOAD_KEY_MODEL, model);
        AddString(document, PAYLOAD_KEY_IS_CHECK_HIOSS, isCheckHioss);
        AddString(document, PAYLOAD_KEY_BRAND, brand);
        AddString(document, PAYLOAD_KEY_CHT_BARCODE, chtBarcode);

        // 轉換為JSON字串
        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        if (!document.Accept(writer)) {
            return false;
        }

        // 調試輸出 JSON payload
        printApiDebug("bindCameraReport 發送 JSON payload: " + std::string(buffer.GetString()));

        // 發送命令
        std::string response;
        if (!sendCommand(_BindCameraReport, buffer.GetString(), response)) {
            return false;
        }

        // check response
        rapidjson::Document responseJson;
        rapidjson::ParseResult parseResult = responseJson.Parse(response.c_str());
        if (parseResult.IsError())
        {
            std::cerr << "解析回應JSON失敗: " << rapidjson::GetParseError_En(parseResult.Code()) << std::endl;
            return false;
        }

        const rapidjson::Value& rep_dataObj = GetObjectMember(responseJson, PAYLOAD_KEY_DATA);
        int rep_camSid = GetIntMember(rep_dataObj, PAYLOAD_KEY_CAMSID);
        const std::string& rep_camId = GetStringMember(rep_dataObj, PAYLOAD_KEY_CAMID);
        const std::string& rep_barcode = GetStringMember(rep_dataObj, PAYLOAD_KEY_CHT_BARCODE);
        const std::string& rep_tenantId = GetStringMember(rep_dataObj, PAYLOAD_KEY_TENANT_ID);
        const std::string& rep_netNo = GetStringMember(rep_dataObj, PAYLOAD_KEY_NETNO);
        const std::string& rep_userId = GetStringMember(rep_dataObj, PAYLOAD_KEY_UID);

        if (rep_camId != camId || rep_barcode != chtBarcode || rep_userId != userId || rep_netNo != netNo)
        {
            std::cerr << " response parameter is wrong!!!" << std::endl;
            return false;
        }

        // 使用參數管理器獲取參數
        auto &paramsManager = CameraParametersManager::getInstance();
        paramsManager.setCamSid(rep_camSid);
        paramsManager.setTenantId(rep_tenantId);
        paramsManager.setUserId(rep_userId);
        paramsManager.setNetNo(netNo);
        paramsManager.saveToFile();

        return true;

    } catch (const std::exception &e) {
        std::cerr << "bindCameraReport error msg=" << e.what() << std::endl;
        return false;
    }

    return false;
}

int ChtP2PCameraCommandHandler::cameraRegister(void)
{
    if (!m_initialized)
    {
        std::cerr << "CHT P2P服務尚未初始化" << std::endl;
        return -1;
    }

    auto &paramsManager = CameraParametersManager::getInstance();
    const std::string& camId = paramsManager.getCameraId();
    const std::string& chtBarcode = paramsManager.getCHTBarcode();

    bool regRes = cameraRegister(camId, chtBarcode);
    if (!regRes) {
        return -2;
    }

    return 0;
}

bool ChtP2PCameraCommandHandler::cameraRegister(const std::string& camId, const std::string& chtBarcode)
{
    if (!m_initialized)
    {
        std::cerr << "CHT P2P服務尚未初始化" << std::endl;
        return -1;
    }

    auto require_non_empty = [](const std::string& s) { return !s.empty(); };
    if (!require_non_empty(camId)) {
        return false;
    }

    try {
        // 構建JSON payload
        rapidjson::Document document;
        document.SetObject();
        const auto& allocator = document.GetAllocator();

        AddString(document, PAYLOAD_KEY_CAMID, camId);
        AddString(document, PAYLOAD_KEY_CHT_BARCODE, chtBarcode);

        // 轉換為JSON字串
        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        if (!document.Accept(writer)) {
            return false;
        }

        // 調試輸出 JSON payload
        printApiDebug("cameraRegister 發送 JSON payload: " + std::string(buffer.GetString()));

        // 發送命令
        std::string response;
        if (!sendCommand(_CameraRegister, buffer.GetString(), response)) {
            return false;
        }

        // check response
        rapidjson::Document responseJson;
        rapidjson::ParseResult parseResult = responseJson.Parse(response.c_str());
        if (parseResult.IsError())
        {
            std::cerr << "解析回應JSON失敗: " << rapidjson::GetParseError_En(parseResult.Code()) << std::endl;
            return false;
        }

        const rapidjson::Value& rep_dataObj = GetObjectMember(responseJson, PAYLOAD_KEY_DATA);
        const std::string& rep_publicIp = GetStringMember(rep_dataObj, PAYLOAD_KEY_PUBLIC_IP);

        // 使用參數管理器獲取參數
        auto &paramsManager = CameraParametersManager::getInstance();
        paramsManager.setPublicIp(rep_publicIp);
        paramsManager.saveToFile();

        return true;

    } catch (const std::exception &e) {
        std::cerr << "cameraRegister error msg=" << e.what() << std::endl;
        return false;
    }

    return false;
}

int ChtP2PCameraCommandHandler::checkHiOSSstatus(bool& hiOssStatus)
{
    if (!m_initialized)
    {
        std::cerr << "CHT P2P服務尚未初始化" << std::endl;
        return -1;
    }

    auto &paramsManager = CameraParametersManager::getInstance();
    const std::string& camId = paramsManager.getCameraId();
    const std::string& chtBarcode = paramsManager.getCHTBarcode();
    const std::string& publicIp = paramsManager.getPublicIp();

    bool checkRes = checkHiOSSstatus(camId, chtBarcode, publicIp);
    if(!checkRes) {
        return -2;
    }

    hiOssStatus = paramsManager.getHiOssStatus();

    return 0;
}

bool ChtP2PCameraCommandHandler::checkHiOSSstatus(const std::string& camId,
        const std::string& chtBarcode, const std::string& publicIp)
{
    if (!m_initialized)
    {
        std::cerr << "CHT P2P服務尚未初始化" << std::endl;
        return false;
    }

#ifdef SIMULATION_MODE
    // 模擬模式下可通過環境變數控制 HiOSS 狀態
    const char *simHiossStatus = getenv("SIM_HIOSS_STATUS");
    bool hiossResult = (simHiossStatus && strcmp(simHiossStatus, "false") == 0) ? false : true;

    printApiDebug("模擬模式 HiOSS 狀態: " + std::string(hiossResult ? "允許" : "受限"));

    auto &paramsManager = CameraParametersManager::getInstance();
    paramsManager.setHiOssStatus(simHiossStatus);
    paramsManager.setIsCheckHioss(hiossResult);
    paramsManager.saveToFile();

    return true;
#endif

    try {
        rapidjson::Document doc;
        doc.SetObject();
        const auto& allocator = doc.GetAllocator();

        AddString(doc, PAYLOAD_KEY_CAMID, camId);
        AddString(doc, PAYLOAD_KEY_CHT_BARCODE, chtBarcode);
        AddString(doc, PAYLOAD_KEY_PUBLIC_IP, publicIp);

        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        if (!doc.Accept(writer)) {
            return false;
        }

        // 發送命令
        std::string response;
        if (!sendCommand(_CheckHiOSSstatus, buffer.GetString(), response)) {
            return false;
        }

        // check response
        rapidjson::Document responseJson;
        rapidjson::ParseResult parseResult = responseJson.Parse(response.c_str());
        if (parseResult.IsError())
        {
            std::cerr << "解析回應JSON失敗: " << rapidjson::GetParseError_En(parseResult.Code()) << std::endl;
            return false;
        }

        const rapidjson::Value& rep_dataObj = GetObjectMember(responseJson, PAYLOAD_KEY_DATA);
        bool rep_status = GetBoolMember(rep_dataObj, PAYLOAD_KEY_STATUS);
        const std::string& rep_desc = GetStringMember(rep_dataObj, PAYLOAD_KEY_DESCRIPTION);

        // 使用參數管理器獲取參數
        auto &paramsManager = CameraParametersManager::getInstance();
        paramsManager.setHiOssStatus(rep_status);
        paramsManager.setIsCheckHioss(true);
        paramsManager.saveToFile();

        return true;

    } catch (const std::exception &e) {
        std::cerr << "checkHiOSSstatus error msg=" << e.what() << std::endl;
        return false;
    }

    return false;
}

int ChtP2PCameraCommandHandler::getHamiCameraInitialInfo(void)
{
    if (!m_initialized)
    {
        std::cerr << "CHT P2P服務尚未初始化" << std::endl;
        return -1;
    }

    auto &paramsManager = CameraParametersManager::getInstance();
    const std::string& camId = paramsManager.getCameraId();
    const std::string& chtBarcode = paramsManager.getCHTBarcode();
    const std::string& tenantId = paramsManager.getTenantId();
    const std::string& netNo = paramsManager.getNetNo();
    const std::string& userId = paramsManager.getUserId();

    bool regRes = getHamiCamInitialInfo(camId, chtBarcode,
            tenantId, netNo, userId);
    if (!regRes) {
        return -2;
    }

    return 0;
}

static int rewriteIntParam(int org, int minVal, int maxVal, bool hasDefault, int defaultVal)
{
    if (org < minVal) {
        if (hasDefault) {
            return defaultVal;
        }
        return minVal;
    }
    if (org > maxVal) {
        if (hasDefault) {
            return defaultVal;
        }
        return maxVal;
    }
    return org;
}

struct infoFaceFeaturesStruct {
    int id;
    std::string name;
    int verifyLevel;
    std::vector<uint8_t> faceFeatures;
    std::string createTime;
    std::string updateTime;
} ;

struct positionStruct
{
    float x;
    float y;
} ;

bool ChtP2PCameraCommandHandler::getHamiCamInitialInfo(const std::string& camId, const std::string& chtBarcode,
        const std::string& tenantId, const std::string& netNo, const std::string& userId)
{
    if (!m_initialized)
    {
        std::cerr << "CHT P2P服務尚未初始化" << std::endl;
        return -1;
    }

    auto require_non_empty = [](const std::string& s) { return !s.empty(); };
    if (!require_non_empty(camId)) {
        return false;
    }

    printApiStepHeader("開始獲取攝影機初始資訊");

    try {
        // 構建JSON payload
        rapidjson::Document document;
        document.SetObject();
        const auto& allocator = document.GetAllocator();

        AddString(document, PAYLOAD_KEY_CAMID, camId);
        AddString(document, PAYLOAD_KEY_CHT_BARCODE, chtBarcode);

        // 轉換為JSON字串
        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        if (!document.Accept(writer)) {
            return false;
        }

        // 調試輸出 JSON payload
        printApiDebug("cameraRegister 發送 JSON payload: " + std::string(buffer.GetString()));

        // 發送命令
        std::string response;
        if (!sendCommand(_GetHamiCamInitialInfo, buffer.GetString(), response)) {
            return false;
        }

        // check response
        rapidjson::Document responseJson;
        rapidjson::ParseResult parseResult = responseJson.Parse(response.c_str());
        if (parseResult.IsError())
        {
            std::cerr << "解析回應JSON失敗: " << rapidjson::GetParseError_En(parseResult.Code()) << std::endl;
            return false;
        }

        // send to system service
        stSetHamiCamInitialInfoRep stRep;
        stSetHamiCamInitialInfoReq stReq;

        const rapidjson::Value& rep_dataObj = GetObjectMember(responseJson, PAYLOAD_KEY_DATA);
        const rapidjson::Value& rep_hamiCamInfoObj = GetObjectMember(rep_dataObj, PAYLOAD_KEY_HAMI_CAM_INFO);
        const rapidjson::Value& rep_hamiSettingObj = GetObjectMember(rep_dataObj, PAYLOAD_KEY_HAMI_SETTINGS);
        const rapidjson::Value& rep_hamiAiSettingObj = GetObjectMember(rep_dataObj, PAYLOAD_KEY_HAMI_AI_SETTINGS);
        const rapidjson::Value& rep_hamiSystemSettingObj = GetObjectMember(rep_dataObj, PAYLOAD_KEY_HAMI_SYSTEM_SETTINGS);

        // check camInfo
        int rep_camSid = GetIntMember(rep_hamiCamInfoObj, PAYLOAD_KEY_CAMSID);
        const std::string& rep_camId = GetStringMember(rep_hamiCamInfoObj, PAYLOAD_KEY_CAMID);
        const std::string& rep_barcode = GetStringMember(rep_hamiCamInfoObj, PAYLOAD_KEY_CHT_BARCODE);
        const std::string& rep_tenantId = GetStringMember(rep_hamiCamInfoObj, PAYLOAD_KEY_TENANT_ID);
        const std::string& rep_netNo = GetStringMember(rep_hamiCamInfoObj, PAYLOAD_KEY_NETNO);
        const std::string& rep_userId = GetStringMember(rep_hamiCamInfoObj, PAYLOAD_KEY_UID);
        if ( rep_camId != camId || rep_barcode != chtBarcode ||
             rep_tenantId != tenantId || rep_netNo != netNo ||
             rep_userId != userId) {
            std::cerr << " response parameter is wrong!!!" << std::endl;
            return false;
        }

        // check hami setting
        int rep_nightMode = GetIntMember(rep_hamiSettingObj, PAYLOAD_KEY_NIGHT_MODE);
        int rep_autoNightVision = GetIntMember(rep_hamiSettingObj, PAYLOAD_KEY_AUTO_NIGHT_VISION);
        int rep_statusIndicatorLight = GetIntMember(rep_hamiSettingObj, PAYLOAD_KEY_STATUS_INDICATOR_LIGHT);
        int rep_isFlipUpDown = GetIntMember(rep_hamiSettingObj, PAYLOAD_KEY_IS_FLIP_UP_DOWN);
        int rep_isHd = GetIntMember(rep_hamiSettingObj, PAYLOAD_KEY_IS_HD);
        int rep_flicker = GetIntMember(rep_hamiSettingObj, PAYLOAD_KEY_FLICKER);
        int rep_imageQuality = GetIntMember(rep_hamiSettingObj, PAYLOAD_KEY_IMAGE_QUALITY);
        int rep_isMic = GetIntMember(rep_hamiSettingObj, PAYLOAD_KEY_IS_MICROPHONE);
        int rep_micSen = GetIntMember(rep_hamiSettingObj, PAYLOAD_KEY_MICROPHONE_SENSITIVITY);
        int rep_isSpeaker = GetIntMember(rep_hamiSettingObj, PAYLOAD_KEY_IS_SPEAK);
        int rep_speakerVol = GetIntMember(rep_hamiSettingObj, PAYLOAD_KEY_SPEAK_VOLUME);
        int rep_storageDay = GetIntMember(rep_hamiSettingObj, PAYLOAD_KEY_STORAGE_DAY);
        int rep_eventStorageDay = GetIntMember(rep_hamiSettingObj, PAYLOAD_KEY_EVENT_STORAGE_DAY);
        stReq.hamiSetting.nightMode = rewriteIntParam(rep_nightMode, 0, 1, false, -1);
        stReq.hamiSetting.autoNightVision = rewriteIntParam(rep_autoNightVision, 0, 1, false, -1);
        stReq.hamiSetting.statusIndicatorLight = rewriteIntParam(rep_statusIndicatorLight, 0, 1, false, -1);
        stReq.hamiSetting.isFlipUpDown = rewriteIntParam(rep_isFlipUpDown, 0, 1, false, -1);
        stReq.hamiSetting.isHd = rewriteIntParam(rep_isHd, 0, 1, false, -1);
        stReq.hamiSetting.flicker = (eFlickerMode)rewriteIntParam(rep_flicker,
            (int)eFlicker_50Hz, (int)eFlicker_Outdoor, true, (int)eFlicker_60Hz);
        stReq.hamiSetting.imageQuality = (eImageQualityMode)rewriteIntParam(rep_imageQuality,
            (int)eImageQuality_Low, (int)eImageQuality_High, true, (int)eImageQuality_Middle);
        stReq.hamiSetting.isMicrophone = rewriteIntParam(rep_isMic, 0, 1, false, -1);
        stReq.hamiSetting.microphoneSensitivity = rewriteIntParam(rep_micSen, 0, 10, true, 3);
        stReq.hamiSetting.isSpeaker = rewriteIntParam(rep_isSpeaker, 0, 1, false, -1);
        stReq.hamiSetting.speakerVolume = rewriteIntParam(rep_speakerVol, 0, 10, true, 3);
        stReq.hamiSetting.storageDay = rewriteIntParam(rep_storageDay, 0, 30, true, 7);
        stReq.hamiSetting.eventStorageDay = rewriteIntParam(rep_eventStorageDay, 0, 30, true, 15);

        int rep_powerOn = GetIntMember(rep_hamiSettingObj, PAYLOAD_KEY_POWER_ON);
        int rep_alertOn = GetIntMember(rep_hamiSettingObj, PAYLOAD_KEY_ALERT_ON);
        int rep_vmd = GetIntMember(rep_hamiSettingObj, PAYLOAD_KEY_VMD);
        int rep_ad = GetIntMember(rep_hamiSettingObj, PAYLOAD_KEY_AD);
        int rep_power = GetIntMember(rep_hamiSettingObj, PAYLOAD_KEY_POWER);
        int rep_ptzStatus = GetIntMember(rep_hamiSettingObj, PAYLOAD_KEY_PTZ_STATUS);
        int rep_ptzPetStatus = GetIntMember(rep_hamiSettingObj, PAYLOAD_KEY_PTZ_PET_STATUS);
        int rep_ptzSpeed = GetIntMember(rep_hamiSettingObj, PAYLOAD_KEY_PTZ_SPEED);
        int rep_ptzTourStayTime = GetIntMember(rep_hamiSettingObj, PAYLOAD_KEY_PTZ_TOUR_STAY_TIME);
        int rep_humanTracking = GetIntMember(rep_hamiSettingObj, PAYLOAD_KEY_HUMAN_TRACKING);
        int rep_petTracking = GetIntMember(rep_hamiSettingObj, PAYLOAD_KEY_PET_TRACKING);
        stReq.hamiSetting.powerOn = rewriteIntParam(rep_powerOn, 0, 1, false, -1);
        stReq.hamiSetting.alertOn = rewriteIntParam(rep_alertOn, 0, 1, false, -1);
        stReq.hamiSetting.vmd = rewriteIntParam(rep_vmd, 0, 1, false, -1);
        stReq.hamiSetting.ad = rewriteIntParam(rep_ad, 0, 1, false, -1);
        stReq.hamiSetting.power = rewriteIntParam(rep_power, 0, 100, true, 50);
        stReq.hamiSetting.ptzStatus = (ePtzStatus)rewriteIntParam(rep_ptzStatus,
            (int)ePtzStatus_None, (int)ePtzStatus_Stay, true, (int)ePtzStatus_None);
        stReq.hamiSetting.ptzPetStatus = (ePtzStatus)rewriteIntParam(rep_ptzPetStatus,
            (int)ePtzStatus_None, (int)ePtzStatus_Stay, true, (int)ePtzStatus_None);
        stReq.hamiSetting.ptzSpeed = (float)rewriteIntParam(rep_ptzSpeed, 0, 2, true, 1);
        stReq.hamiSetting.ptzTourStayTime = rewriteIntParam(rep_ptzTourStayTime, 1, 5, true, 5);
        stReq.hamiSetting.humanTracking = (ePtzTrackingMode)rewriteIntParam(rep_humanTracking,
            (int)ePtzTracking_Off, (int)ePtzTracking_Stay, true, (int)ePtzTracking_Off);
        stReq.hamiSetting.petTracking = (ePtzTrackingMode)rewriteIntParam(rep_petTracking,
            (int)ePtzTracking_Off, (int)ePtzTracking_Stay, true, (int)ePtzTracking_Off);

        int rep_scheduleOn = GetIntMember(rep_hamiSettingObj, PAYLOAD_KEY_SCHEDULE_ON);
        const std::string& rep_scheduleSun = GetStringMember(rep_hamiSettingObj, PAYLOAD_KEY_SCHEDULE_SUN);
        const std::string& rep_scheduleMon = GetStringMember(rep_hamiSettingObj, PAYLOAD_KEY_SCHEDULE_MON);
        const std::string& rep_scheduleTue = GetStringMember(rep_hamiSettingObj, PAYLOAD_KEY_SCHEDULE_TUE);
        const std::string& rep_scheduleWed = GetStringMember(rep_hamiSettingObj, PAYLOAD_KEY_SCHEDULE_WED);
        const std::string& rep_scheduleThu = GetStringMember(rep_hamiSettingObj, PAYLOAD_KEY_SCHEDULE_THU);
        const std::string& rep_scheduleFri = GetStringMember(rep_hamiSettingObj, PAYLOAD_KEY_SCHEDULE_FRI);
        const std::string& rep_scheduleSat = GetStringMember(rep_hamiSettingObj, PAYLOAD_KEY_SCHEDULE_SAT);

        stReq.hamiSetting.scheduleOn = rewriteIntParam(rep_scheduleOn, 0, 1, false, -1);
        // check schedule string format
        auto validateScheduleString = [](const std::string& scheduleStr) -> bool {
            std::regex pattern("^([0-1][0-9]|2[0-3]):([0-5][0-9])-([0-1][0-9]|2[0-3]):([0-5][0-9])$");
            return std::regex_match(scheduleStr, pattern);
        };
        if (!validateScheduleString(rep_scheduleSun) ||
            !validateScheduleString(rep_scheduleMon) ||
            !validateScheduleString(rep_scheduleTue) ||
            !validateScheduleString(rep_scheduleWed) ||
            !validateScheduleString(rep_scheduleThu) ||
            !validateScheduleString(rep_scheduleFri) ||
            !validateScheduleString(rep_scheduleSat)) {
            throw std::runtime_error("Invalid schedule string format");
        }
        snprintf(stReq.hamiSetting.scheduleSun, sizeof(stReq.hamiSetting.scheduleSun),
                    "%s", rep_scheduleSun.c_str());
        snprintf(stReq.hamiSetting.scheduleMon, sizeof(stReq.hamiSetting.scheduleMon),
                    "%s", rep_scheduleMon.c_str());
        snprintf(stReq.hamiSetting.scheduleTue, sizeof(stReq.hamiSetting.scheduleTue),
                    "%s", rep_scheduleTue.c_str());
        snprintf(stReq.hamiSetting.scheduleWed, sizeof(stReq.hamiSetting.scheduleWed),
                    "%s", rep_scheduleWed.c_str());
        snprintf(stReq.hamiSetting.scheduleThu, sizeof(stReq.hamiSetting.scheduleThu),
                    "%s", rep_scheduleThu.c_str());
        snprintf(stReq.hamiSetting.scheduleFri, sizeof(stReq.hamiSetting.scheduleFri),
                    "%s", rep_scheduleFri.c_str());
        snprintf(stReq.hamiSetting.scheduleSat, sizeof(stReq.hamiSetting.scheduleSat),
                    "%s", rep_scheduleSat.c_str());

        // check aiSetting
        int rep_vmdAlert = GetIntMember(rep_hamiSettingObj, PAYLOAD_KEY_VMD_ALERT);
        int rep_humanAlert = GetIntMember(rep_hamiSettingObj, PAYLOAD_KEY_HUMAN_ALERT);
        int rep_petAlert = GetIntMember(rep_hamiSettingObj, PAYLOAD_KEY_PET_ALERT);
        int rep_adAlert = GetIntMember(rep_hamiSettingObj, PAYLOAD_KEY_AD_ALERT);
        int rep_fenceAlert = GetIntMember(rep_hamiSettingObj, PAYLOAD_KEY_FENCE_ALERT);
        int rep_faceAlert = GetIntMember(rep_hamiSettingObj, PAYLOAD_KEY_FACE_ALERT);
        int rep_fallAlert = GetIntMember(rep_hamiSettingObj, PAYLOAD_KEY_FALL_ALERT);
        int rep_adBabyCryAlert = GetIntMember(rep_hamiSettingObj, PAYLOAD_KEY_AD_BABY_CRY_ALERT);
        int rep_adSpeechAlert = GetIntMember(rep_hamiSettingObj, PAYLOAD_KEY_AD_SPEECH_ALERT);
        int rep_adAlarmAlert = GetIntMember(rep_hamiSettingObj, PAYLOAD_KEY_AD_ALARM_ALERT);
        int rep_adDogAlert = GetIntMember(rep_hamiSettingObj, PAYLOAD_KEY_AD_DOG_ALERT);
        int rep_adCatAlert = GetIntMember(rep_hamiSettingObj, PAYLOAD_KEY_AD_CAT_ALERT);
        int rep_vmdSen = GetIntMember(rep_hamiAiSettingObj, PAYLOAD_KEY_VMD_SEN);
        int rep_adSen = GetIntMember(rep_hamiAiSettingObj, PAYLOAD_KEY_AD_SEN);
        int rep_humanSen = GetIntMember(rep_hamiAiSettingObj, PAYLOAD_KEY_HUMAN_SEN);
        int rep_faceSen = GetIntMember(rep_hamiAiSettingObj, PAYLOAD_KEY_FACE_SEN);
        int rep_fenceSen = GetIntMember(rep_hamiAiSettingObj, PAYLOAD_KEY_FENCE_SEN);
        int rep_petSen = GetIntMember(rep_hamiAiSettingObj, PAYLOAD_KEY_PET_SEN);
        int rep_adBabyCrySen = GetIntMember(rep_hamiAiSettingObj, PAYLOAD_KEY_AD_BABY_CRY_SEN);
        int rep_adSpeechSen = GetIntMember(rep_hamiAiSettingObj, PAYLOAD_KEY_AD_SPEECH_SEN);
        int rep_adAlarmSen = GetIntMember(rep_hamiAiSettingObj, PAYLOAD_KEY_AD_ALARM_SEN);
        int rep_adDogSen = GetIntMember(rep_hamiAiSettingObj, PAYLOAD_KEY_AD_DOG_SEN);
        int rep_adCatSen = GetIntMember(rep_hamiAiSettingObj, PAYLOAD_KEY_AD_CAT_SEN);
        int rep_fallSen = GetIntMember(rep_hamiAiSettingObj, PAYLOAD_KEY_FALL_SEN);
        int rep_fallTime = GetIntMember(rep_hamiAiSettingObj, PAYLOAD_KEY_FALL_TIME);
        stReq.hamiAiSetting.vmdAlert = rewriteIntParam(rep_vmdAlert, 0, 1, false, -1);
        stReq.hamiAiSetting.humanAlert = rewriteIntParam(rep_humanAlert, 0, 1, false, -1);
        stReq.hamiAiSetting.petAlert = rewriteIntParam(rep_petAlert, 0, 1, false, -1);
        stReq.hamiAiSetting.adAlert = rewriteIntParam(rep_adAlert, 0, 1, false, -1);
        stReq.hamiAiSetting.fenceAlert = rewriteIntParam(rep_fenceAlert, 0, 1, false, -1);
        stReq.hamiAiSetting.faceAlert = rewriteIntParam(rep_faceAlert, 0, 1, false, -1);
        stReq.hamiAiSetting.fallAlert = rewriteIntParam(rep_fallAlert, 0, 1, false, -1);
        stReq.hamiAiSetting.adBabyCryAlert = rewriteIntParam(rep_adBabyCryAlert, 0, 1, false, -1);
        stReq.hamiAiSetting.adSpeechAlert = rewriteIntParam(rep_adSpeechAlert, 0, 1, false, -1);
        stReq.hamiAiSetting.adAlarmAlert = rewriteIntParam(rep_adAlarmAlert, 0, 1, false, -1);
        stReq.hamiAiSetting.adDogAlert = rewriteIntParam(rep_adDogAlert, 0, 1, false, -1);
        stReq.hamiAiSetting.adCatAlert = rewriteIntParam(rep_adCatAlert, 0, 1, false, -1);
        stReq.hamiAiSetting.vmdSen = (eSenMode)rewriteIntParam(rep_vmdSen,
            (int)eSenMode_Low, (int)eSenMode_High, true, (int)eSenMode_Middle);
        stReq.hamiAiSetting.adSen = (eSenMode)rewriteIntParam(rep_adSen,
            (int)eSenMode_Low, (int)eSenMode_High, true, (int)eSenMode_Middle);
        stReq.hamiAiSetting.humanSen = (eSenMode)rewriteIntParam(rep_humanSen,
            (int)eSenMode_Low, (int)eSenMode_High, true, (int)eSenMode_Middle);
        stReq.hamiAiSetting.faceSen = (eSenMode)rewriteIntParam(rep_faceSen,
            (int)eSenMode_Low, (int)eSenMode_High, true, (int)eSenMode_Middle);
        stReq.hamiAiSetting.fenceSen = (eSenMode)rewriteIntParam(rep_fenceSen,
            (int)eSenMode_Low, (int)eSenMode_High, true, (int)eSenMode_Middle);
        stReq.hamiAiSetting.petSen = (eSenMode)rewriteIntParam(rep_petSen,
            (int)eSenMode_Low, (int)eSenMode_High, true, (int)eSenMode_Middle);
        stReq.hamiAiSetting.adBabyCrySen = (eSenMode)rewriteIntParam(rep_adBabyCrySen,
            (int)eSenMode_Low, (int)eSenMode_High, true, (int)eSenMode_Middle);
        stReq.hamiAiSetting.adSpeechSen = (eSenMode)rewriteIntParam(rep_adSpeechSen,
            (int)eSenMode_Low, (int)eSenMode_High, true, (int)eSenMode_Middle);
        stReq.hamiAiSetting.adAlarmSen = (eSenMode)rewriteIntParam(rep_adAlarmSen,
            (int)eSenMode_Low, (int)eSenMode_High, true, (int)eSenMode_Middle);
        stReq.hamiAiSetting.adDogSen = (eSenMode)rewriteIntParam(rep_adDogSen,
            (int)eSenMode_Low, (int)eSenMode_High, true, (int)eSenMode_Middle);
        stReq.hamiAiSetting.adCatSen = (eSenMode)rewriteIntParam(rep_adCatSen,
            (int)eSenMode_Low, (int)eSenMode_High, true, (int)eSenMode_Middle);
        stReq.hamiAiSetting.fallSen = (eSenMode)rewriteIntParam(rep_fallSen,
            (int)eSenMode_Low, (int)eSenMode_High, true, (int)eSenMode_Middle);
        stReq.hamiAiSetting.fallTime = rewriteIntParam(rep_fallTime, 1, 5, true, 3);

        const rapidjson::Value& rep_identificationFeatures = GetObjectMember(rep_hamiAiSettingObj, PAYLOAD_KEY_IDENTIFICATION_FEATURES);

        if (rep_identificationFeatures.IsArray()) {
            rapidjson::SizeType i = 0;
            for (i = 0; i < rep_identificationFeatures.Size(); i++) {
                if (i >= ZWSYSTEM_FACE_FEATURES_ARRAY_SIZE) {
                    break;
                }
                const rapidjson::Value& featureObj = rep_identificationFeatures[i];
                stIdentificationFeature *feature = &stReq.hamiAiSetting.features[i];

                int rep_id = GetIntMember(featureObj, PAYLOAD_KEY_ID);
                const std::string& rep_name = GetStringMember(featureObj, PAYLOAD_KEY_NAME);
                int rep_verifyLevel = GetIntMember(featureObj, PAYLOAD_KEY_VERIFY_LEVEL);
                rep_verifyLevel = rewriteIntParam(rep_verifyLevel, 1, 2, true, 2);

                const rapidjson::Value& rep_faceFeaturesBlob = GetObjectMember(featureObj, PAYLOAD_KEY_FACE_FEATURES);
                if (rep_faceFeaturesBlob.IsArray()) {
                    const auto& blobData = rep_faceFeaturesBlob.GetArray();
                    rapidjson::SizeType blobSize = rep_faceFeaturesBlob.Size();
                    if (blobSize != ZWSYSTEM_FACE_FEATURES_SIZE) {
                        throw std::runtime_error("Invalid face features blob size");
                    }
                    rapidjson::SizeType j = 0;
                    for (j = 0; j < blobSize; j++) {
                        if (!blobData[j].IsUint() || blobData[j].GetUint() > 255) {
                            throw std::runtime_error("Invalid face features blob value");
                        }
                        feature->faceFeatures[j] = static_cast<uint8_t>(blobData[j].GetUint());
                    }
                } else {
                    throw std::runtime_error("Invalid face features blob type");
                }

                const std::string& rep_createTime = GetStringMember(featureObj, PAYLOAD_KEY_CREATE_TIME);
                const std::string& rep_updateTime = GetStringMember(featureObj, PAYLOAD_KEY_UPDATE_TIME);

                feature->id = rep_id;
                feature->verifyLevel = (eVerifyLevel)rewriteIntParam(rep_verifyLevel,
                    (int)eVerifyLevel_Low, (int)eVerifyLevel_High, true, (int)eVerifyLevel_High);
                snprintf(feature->name, sizeof(feature->name),
                            "%s", rep_name.c_str());
                snprintf(feature->createTime, sizeof(feature->createTime),
                            "%s", rep_createTime.c_str());
                snprintf(feature->updateTime, sizeof(feature->updateTime),
                            "%s", rep_updateTime.c_str());
            }
        }

        const rapidjson::Value& rep_fencePos1Obj = GetObjectMember(rep_hamiAiSettingObj, PAYLOAD_KEY_FENCE_POS1);
        positionStruct fencePos1;
        fencePos1.x = static_cast<float>(GetIntMember(rep_fencePos1Obj, PAYLOAD_KEY_X));
        fencePos1.y = static_cast<float>(GetIntMember(rep_fencePos1Obj, PAYLOAD_KEY_Y));
        const rapidjson::Value& rep_fencePos2Obj = GetObjectMember(rep_hamiAiSettingObj, PAYLOAD_KEY_FENCE_POS2);
        positionStruct fencePos2;
        fencePos2.x = static_cast<float>(GetIntMember(rep_fencePos2Obj, PAYLOAD_KEY_X));
        fencePos2.y = static_cast<float>(GetIntMember(rep_fencePos2Obj, PAYLOAD_KEY_Y));
        const rapidjson::Value& rep_fencePos3Obj = GetObjectMember(rep_hamiAiSettingObj, PAYLOAD_KEY_FENCE_POS3);
        positionStruct fencePos3;
        fencePos3.x = static_cast<float>(GetIntMember(rep_fencePos3Obj, PAYLOAD_KEY_X));
        fencePos3.y = static_cast<float>(GetIntMember(rep_fencePos3Obj, PAYLOAD_KEY_Y));
        const rapidjson::Value& rep_fencePos4Obj = GetObjectMember(rep_hamiAiSettingObj, PAYLOAD_KEY_FENCE_POS4);
        positionStruct fencePos4;
        fencePos4.x = static_cast<float>(GetIntMember(rep_fencePos4Obj, PAYLOAD_KEY_X));
        fencePos4.y = static_cast<float>(GetIntMember(rep_fencePos4Obj, PAYLOAD_KEY_Y));
        if ( (int)(fencePos1.x*100) < 0 || (int)(fencePos1.y*100) < 0 ||
             (int)(fencePos2.x*100) < 0 || (int)(fencePos2.y*100) < 0 ||
             (int)(fencePos3.x*100) < 0 || (int)(fencePos3.y*100) < 0 ||
             (int)(fencePos4.x*100) < 0 || (int)(fencePos4.y*100) < 0 ) {
            throw std::runtime_error("Invalid fence position values");
        }
        stReq.hamiAiSetting.fencePos[0].x = fencePos1.x;
        stReq.hamiAiSetting.fencePos[0].y = fencePos1.y;
        stReq.hamiAiSetting.fencePos[1].x = fencePos2.x;
        stReq.hamiAiSetting.fencePos[1].y = fencePos2.y;
        stReq.hamiAiSetting.fencePos[2].x = fencePos3.x;
        stReq.hamiAiSetting.fencePos[2].y = fencePos3.y;
        stReq.hamiAiSetting.fencePos[3].x = fencePos4.x;
        stReq.hamiAiSetting.fencePos[3].y = fencePos4.y;

        int rep_fenceDir = GetIntMember(rep_hamiAiSettingObj, PAYLOAD_KEY_FENCE_DIR);
        stReq.hamiAiSetting.fenceDir = (eFenceDirection)rewriteIntParam(rep_fenceDir,
            (int)eFenceDir_Out2In, (int)eFenceDir_In2Out, false, -1);

        stReq.hamiAiSetting.updateBit = eAiSettingUpdateMask_ALL;
        stReq.hamiAiSetting.fencePosUpdateBit = eFencePosUpdateMask_ALL;

        // check systemSetting
        const std::string& rep_otaDomainName = GetStringMember(rep_hamiSystemSettingObj, PAYLOAD_KEY_OTA_DOMAIN_NAME);
        int rep_otaQueryInterval = GetIntMember(rep_hamiSystemSettingObj, PAYLOAD_KEY_OTA_QUERY_INTERVAL);
        const std::string& rep_ntpServer = GetStringMember(rep_hamiSystemSettingObj, PAYLOAD_KEY_NTP_SERVER);
        const std::string& bucketName = GetStringMember(rep_hamiSystemSettingObj, PAYLOAD_KEY_BUCKET_NAME);
        stReq.hamiSystemSetting.otaQueryInterval = rep_otaQueryInterval;
        snprintf(stReq.hamiSystemSetting.otaDomainName, sizeof(stReq.hamiSystemSetting.otaDomainName),
                        "%s", rep_otaDomainName.c_str());
        snprintf(stReq.hamiSystemSetting.ntpServer, sizeof(stReq.hamiSystemSetting.ntpServer),
                        "%s", rep_ntpServer.c_str());
        snprintf(stReq.hamiSystemSetting.bucketName, sizeof(stReq.hamiSystemSetting.bucketName),
                        "%s", bucketName.c_str());

        int rc = zwsystem_ipc_setHamiCamInitialInfo(stReq, &stRep);
        if (rc != 0 || stRep.code != 0) {
            std::cerr << "zwsystem_ipc_setHamiCamInitialInfo failed, rc=" << rc
                      << ", code=" << stRep.code << std::endl;
            return false;
        }

        return true;

    } catch (const std::exception &e) {
        std::cerr << "getHamiCamInitialInfo error msg=" << e.what() << std::endl;
        return false;
    }

    return false;
}

bool ChtP2PCameraCommandHandler::checkHiOssStatus(void)
{
#ifndef SIMULATION_MODE
    auto &paramsManager = CameraParametersManager::getInstance();

    // 檢查HiOSS狀態 - 根據規格2.2要求
    bool isCheckHiOss = paramsManager.getIsCheckHioss();
    if (isCheckHiOss) {
        std::cerr << "Camera does not bind yet, drop control function" << std::endl;
        return false;
    }

    // 如果HiOSS狀態為false（"0"），則僅接收解綁攝影機指令(_DeleteCameraInfo)
    bool hiOssStatus = paramsManager.getHiOssStatus();
    return hiOssStatus;
#endif

    return true;
}

static bool is_leap_year(int y) {
    return (y % 4 == 0 && y % 100 != 0) || (y % 400 == 0);
}

static int days_in_month(int y, int m) {
    static const int d[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    if (m == 2) return d[1] + (is_leap_year(y) ? 1 : 0);
    return d[m - 1];
}

static bool is_valid_utc_ms(const std::string& s)
{
    // YYYY-MM-DDTHH:MM:SS.mmmZ
    static const std::regex re(
        R"(^(\d{4})-(\d{2})-(\d{2})T(\d{2}):(\d{2}):(\d{2})\.(\d{3})Z$)"
    );

    std::smatch m;
    if (!std::regex_match(s, m, re)) return false;

    auto to_int = [](const std::string& x) -> int {
        return static_cast<int>(std::strtol(x.c_str(), nullptr, 10));
    };

    const int year  = to_int(m[1]);
    const int mon   = to_int(m[2]);
    const int day   = to_int(m[3]);
    const int hour  = to_int(m[4]);
    const int min   = to_int(m[5]);
    const int sec   = to_int(m[6]);
    const int msec  = to_int(m[7]);

    if (mon < 1 || mon > 12) return false;
    const int dim = days_in_month(year, mon);
    if (day < 1 || day > dim) return false;

    if (hour < 0 || hour > 23) return false;
    if (min < 0 || min > 59) return false;
    if (sec < 0 || sec > 59) return false;
    if (msec < 0 || msec > 999) return false;

    return true;
}

static bool readable_regular_file(const std::string& p)
{
    struct stat st;
    if (::stat(p.c_str(), &st) != 0) return false;
    if (!S_ISREG(st.st_mode)) return false;
    return ::access(p.c_str(), R_OK) == 0;
}

int ChtP2PCameraCommandHandler::reportSnapshot(const uint8_t *data, size_t dataSize)
{
    if (!m_initialized)
    {
        std::cerr << "CHT P2P服務尚未初始化" << std::endl;
        return -1;
    }

    if (!checkHiOssStatus())
    {
        std::cerr << "Camera does not bind, drop event" << std::endl;
        return REPORT_EVENT_NOT_RETRY;
    }

    if (data == NULL || dataSize == 0 || dataSize != sizeof(stSnapshotEventSub))
    {
        std::cerr << "Invalid data!!!" << std::endl;
        return -2;
    }

    stSnapshotEventSub *pSub = (stSnapshotEventSub *)data;
    std::string eventId(pSub->eventId);
    std::string snapshotTime(pSub->snapshotTime);
    std::string filePath(pSub->filePath);
    if (eventId.empty() || snapshotTime.empty() || filePath.empty()) {
        std::cerr << "Invalid parameter in data!!!" << std::endl;
        return -2;
    }

    if (!is_valid_utc_ms(snapshotTime)) {
        std::cerr << "Invalid parameter in data!!!" << std::endl;
        return -2;
    }

    // check filePath have file
    if (!readable_regular_file(filePath)) {
        std::cerr << "The file does not exist or is not readable, drop this event!!! filePath=" << filePath << std::endl;
        return REPORT_EVENT_NOT_RETRY;
    }

    auto &paramsManager = CameraParametersManager::getInstance();
    const std::string& camId = paramsManager.getCameraId();
    const std::string& chtBarcode = paramsManager.getCHTBarcode();

    bool snapRes = reportSnapshot(camId, chtBarcode, eventId, snapshotTime, filePath);
    if (!snapRes) {
        std::cerr << "reportSnapshot failed!!!" << std::endl;
        return -3;
    }

    return 0;
}

bool ChtP2PCameraCommandHandler::reportSnapshot(const std::string& camId, const std::string& chtBarcode,
        const std::string& eventId, const std::string& snapshotTime, const std::string& filePath)
{
    if (!m_initialized)
    {
        std::cerr << "CHT P2P服務尚未初始化" << std::endl;
        return false;
    }

    if (!checkHiOssStatus())
    {
        std::cerr << "Camera does not bind, drop event" << std::endl;
        return false;
    }

    auto require_non_empty = [](const std::string& s) { return !s.empty(); };
    if (!require_non_empty(camId) || !require_non_empty(eventId) ||
        !require_non_empty(snapshotTime) || !require_non_empty(filePath)) {
        return false;
    }

    if (!is_valid_utc_ms(snapshotTime)) {
        return false;
    }

    // check filePath have file
    if (!readable_regular_file(filePath)) {
        return false;
    }

    try {
        // 構建JSON payload
        rapidjson::Document document;
        document.SetObject();
        rapidjson::Document::AllocatorType &allocator = document.GetAllocator();
        AddString(document, PAYLOAD_KEY_CAMID, camId);
        AddString(document, PAYLOAD_KEY_CHT_BARCODE, chtBarcode);
        AddString(document, PAYLOAD_KEY_EVENT_ID, eventId);
        AddString(document, PAYLOAD_KEY_SNAPSHOT_TIME, snapshotTime);
        AddString(document, PAYLOAD_KEY_FILE_PATH, filePath);

        // 轉換為JSON字符串
        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        document.Accept(writer);

        // 發送命令
        std::string response;
        if (!sendCommand(_Snapshot, buffer.GetString(), response)) {
            throw std::runtime_error("sendCommand failed");
        }

        // check response
        rapidjson::Document responseJson;
        rapidjson::ParseResult parseResult = responseJson.Parse(response.c_str());
        if (parseResult.IsError())
        {
            std::cerr << "解析回應JSON失敗: " << rapidjson::GetParseError_En(parseResult.Code()) << std::endl;
            return false;
        }

        int rep_result = GetIntMember(responseJson, PAYLOAD_KEY_RESULT);
        if (rep_result != 1) {
            throw std::runtime_error(std::string("reportSnapshot response result != 1, result=") + std::to_string(rep_result));
        }

        return true;
    } catch (const std::exception &e) {
        std::cerr << "reportSnapshot error msg=" << e.what() << std::endl;
        return false;
    }

    return false;
}

int ChtP2PCameraCommandHandler::reportRecord(const uint8_t *data, size_t dataSize)
{
    if (!m_initialized)
    {
        std::cerr << "CHT P2P服務尚未初始化" << std::endl;
        return -1;
    }

    if (!checkHiOssStatus())
    {
        std::cerr << "Camera does not bind, drop event" << std::endl;
        return REPORT_EVENT_NOT_RETRY;
    }

    if (data == NULL || dataSize == 0 || dataSize != sizeof(stRecordEventSub))
    {
        std::cerr << "Invalid data!!!" << std::endl;
        return -2;
    }

    stRecordEventSub *pSub = (stRecordEventSub *)data;
    std::string eventId(pSub->eventId);
    std::string fromTime(pSub->fromTime);
    std::string toTime(pSub->toTime);
    std::string filePath(pSub->filePath);
    std::string thumbnailfilePath(pSub->thumbnailfilePath);
    if (eventId.empty() || fromTime.empty() || toTime.empty() ||
        filePath.empty() || thumbnailfilePath.empty()) {
        std::cerr << "Invalid parameter in data!!!" << std::endl;
        return -2;
    }

    // check fromTime and toTime format is like "2024-09-19 00:00:30.000"
    if (!is_valid_utc_ms(fromTime) || !is_valid_utc_ms(toTime) ) {
        std::cerr << "Invalid parameter in data!!!" << std::endl;
        return -2;
    }

    // check filePath have file
    if (!readable_regular_file(filePath) || !readable_regular_file(thumbnailfilePath) ) {
        std::cerr << "The file does not exist or is not readable, drop this event!!!" <<
                " filePath=" << filePath <<
                " , thumbnailfilePath=" << thumbnailfilePath<< std::endl;
        return REPORT_EVENT_NOT_RETRY;
    }

    auto &paramsManager = CameraParametersManager::getInstance();
    const std::string& camId = paramsManager.getCameraId();
    const std::string& chtBarcode = paramsManager.getCHTBarcode();

    bool recRes = reportRecord(camId, chtBarcode, eventId,
            fromTime, toTime, filePath, thumbnailfilePath);
    if (!recRes) {
        std::cerr << "reportRecord failed!!!" << std::endl;
        return -3;
    }

    return 0;
}

bool ChtP2PCameraCommandHandler::reportRecord(const std::string& camId, const std::string& chtBarcode,
        const std::string &eventId,
        const std::string &fromTime, const std::string &toTime,
        const std::string &filePath, const std::string &thumbnailfilePath)
{
    if (!m_initialized)
    {
        std::cerr << "CHT P2P服務尚未初始化" << std::endl;
        return false;
    }

    if (!checkHiOssStatus())
    {
        std::cerr << "Camera does not bind, drop event" << std::endl;
        return false;
    }

    auto require_non_empty = [](const std::string& s) { return !s.empty(); };
    if (!require_non_empty(camId) || !require_non_empty(eventId) ||
        !require_non_empty(fromTime) || !require_non_empty(toTime) ||
        !require_non_empty(filePath) || !require_non_empty(thumbnailfilePath)) {
        return false;
    }
    if (!is_valid_utc_ms(fromTime) || !is_valid_utc_ms(toTime) ) {
        return false;
    }

    // check filePath have file
    if (!readable_regular_file(filePath) || !readable_regular_file(thumbnailfilePath) ) {
        return false;
    }

    try {
        // 構建JSON payload - 符合客戶規格要求
        rapidjson::Document document;
        document.SetObject();
        rapidjson::Document::AllocatorType &allocator = document.GetAllocator();

        AddString(document, PAYLOAD_KEY_CAMID, camId);
        AddString(document, PAYLOAD_KEY_EVENT_ID, eventId);
        AddString(document, PAYLOAD_KEY_FROM_TIME, fromTime);
        AddString(document, PAYLOAD_KEY_TO_TIME, toTime);
        AddString(document, PAYLOAD_KEY_FILE_PATH, filePath);
        AddString(document, PAYLOAD_KEY_THUMBNAIL_FILE_PATH, thumbnailfilePath);

        // 轉換為JSON字串
        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        document.Accept(writer);

        // 調試輸出JSON payload
        std::cout << "[API-DEBUG] reportRecord 發送 JSON payload: " << buffer.GetString() << std::endl;

        // 發送命令
        std::string response;
        if (!sendCommand(_Record, buffer.GetString(), response)) {
            throw std::runtime_error("sendCommand failed");
        }

        // check response
        rapidjson::Document responseJson;
        rapidjson::ParseResult parseResult = responseJson.Parse(response.c_str());
        if (parseResult.IsError())
        {
            std::cerr << "解析回應JSON失敗: " << rapidjson::GetParseError_En(parseResult.Code()) << std::endl;
            return false;
        }

        int rep_result = GetIntMember(responseJson, PAYLOAD_KEY_RESULT);
        if (rep_result != 1) {
            throw std::runtime_error(std::string("reportRecord response result != 1, result=") + std::to_string(rep_result));
        }

        return true;

    } catch (const std::exception &e) {
        std::cerr << "reportRecord error msg=" << e.what() << std::endl;
        return false;
    }

    return false;
}

int ChtP2PCameraCommandHandler::reportRecognition(const uint8_t *data, size_t dataSize)
{
    if (!m_initialized)
    {
        std::cerr << "CHT P2P服務尚未初始化" << std::endl;
        return -1;
    }

    if (!checkHiOssStatus())
    {
        std::cerr << "Camera does not bind, drop event" << std::endl;
        return REPORT_EVENT_NOT_RETRY;
    }

    if (data == NULL || dataSize == 0 || dataSize != sizeof(stRecognitionEventSub))
    {
        std::cerr << "Invalid data!!!" << std::endl;
        return -2;
    }

    stRecognitionEventSub *pSub = (stRecognitionEventSub *)data;
    std::string eventId(pSub->eventId);
    std::string eventTime(pSub->eventTime);
    eRecognitionType eventType = (eRecognitionType)pSub->eventType;
    eRecognitionEventClassType eventClass = (eRecognitionEventClassType)pSub->eventClass;
    std::string videoFilePath(pSub->videoFilePath);
    std::string snapshotFilePath(pSub->snapshotFilePath);
    std::string audioFilePath(pSub->audioFilePath);
    std::string coordinate(pSub->coordinate);
    std::string fidResult(pSub->fidResult);

    if (eventId.empty() || eventTime.empty() ||
        (videoFilePath.empty() && snapshotFilePath.empty() && audioFilePath.empty())) {
        std::cerr << "Invalid parameter in data!!!" << std::endl;
        return -2;
    }

    // check eventTime format is like "2024-09-19 00:00:30.000"
    if (!is_valid_utc_ms(eventTime)) {
        std::cerr << "Invalid parameter in data!!!" << std::endl;
        return -2;
    }

    // check filePath have file
    if ( (!videoFilePath.empty() && !readable_regular_file(videoFilePath)) ||
         (!snapshotFilePath.empty() && !readable_regular_file(snapshotFilePath)) ||
         (!audioFilePath.empty() && !readable_regular_file(audioFilePath)) ) {
        std::cerr << "The file does not exist or is not readable, drop this event!!!" <<
                " videoFilePath=" << (videoFilePath.empty()?"":videoFilePath) <<
                " , snapshotFilePath=" << (snapshotFilePath.empty()?"":snapshotFilePath) <<
                " , audioFilePath=" << (audioFilePath.empty()?"":audioFilePath) << std::endl;
        return REPORT_EVENT_NOT_RETRY;
    }

    std::string eventTypeStr = zwsystem_ipc_recognitionType_int2str(eventType);
    std::string eventClassStr = zwsystem_ipc_eventClass_int2str(eventClass);

    auto &paramsManager = CameraParametersManager::getInstance();
    const std::string& camId = paramsManager.getCameraId();
    const std::string& chtBarcode = paramsManager.getCHTBarcode();

    bool recognRes = reportRecognition(camId, chtBarcode, eventId,
            eventTime, eventTypeStr, eventClassStr,
            videoFilePath, snapshotFilePath, audioFilePath,
            coordinate, fidResult);
    if (!recognRes) {
        std::cerr << "reportRecognition failed!!!" << std::endl;
        return -3;
    }

    return 0;
}

bool ChtP2PCameraCommandHandler::reportRecognition(const std::string& camId, const std::string& chtBarcode,
    const std::string& eventId, const std::string& eventTime,
    const std::string& eventType, const std::string& eventClass,
    const std::string& videoFilePath, const std::string& snapshotFilePath, const std::string& audioFilePath,
    const std::string& coordinate, const std::string& fidResult)
{
    if (!m_initialized)
    {
        std::cerr << "CHT P2P服務尚未初始化" << std::endl;
        return false;
    }

    if (!checkHiOssStatus())
    {
        std::cerr << "Camera does not bind, drop event" << std::endl;
        return false;
    }

    auto require_non_empty = [](const std::string& s) { return !s.empty(); };
    if (!require_non_empty(camId) || !require_non_empty(eventId) ||
        !require_non_empty(eventTime) || !require_non_empty(eventType) || !require_non_empty(eventClass) ||
        (!require_non_empty(videoFilePath) && !require_non_empty(snapshotFilePath) && !require_non_empty(audioFilePath))) {
        return false;
    }

    // check eventTime format is like "2024-09-19 00:00:30.000"
    if (!is_valid_utc_ms(eventTime)) {
        return false;
    }

    // check filePath have file
    if ( (!videoFilePath.empty() && !readable_regular_file(videoFilePath)) ||
         (!snapshotFilePath.empty() && !readable_regular_file(snapshotFilePath)) ||
         (!audioFilePath.empty() && !readable_regular_file(audioFilePath)) ) {
        return false;
    }

    try
    {
        // 構建JSON payload
        rapidjson::Document document;
        document.SetObject();
        rapidjson::Document::AllocatorType &allocator = document.GetAllocator();

        AddString(document, PAYLOAD_KEY_CAMID, camId);
        AddString(document, PAYLOAD_KEY_EVENT_ID, eventId);
        AddString(document, PAYLOAD_KEY_EVENT_TIME, eventTime);
        AddString(document, PAYLOAD_KEY_EVENT_TYPE, eventType);
        AddString(document, PAYLOAD_KEY_EVENT_CLASS, eventClass);
        AddString(document, PAYLOAD_KEY_VIDEO_FILE_PATH, videoFilePath.empty() ? "" : videoFilePath);
        AddString(document, PAYLOAD_KEY_SNAPSHOT_FILE_PATH, snapshotFilePath.empty() ? "" : snapshotFilePath);
        AddString(document, PAYLOAD_KEY_AUDIO_FILE_PATH, audioFilePath.empty() ? "" : audioFilePath);
        AddString(document, PAYLOAD_KEY_COORDINATE, coordinate.empty() ? "" : coordinate);

        rapidjson::Value resultAttribute(rapidjson::kObjectType);
        AddString(resultAttribute, PAYLOAD_KEY_FID_RESULT, fidResult.empty() ? "" : fidResult, allocator);
        document.AddMember(PAYLOAD_KEY_RESULT_ATTRIBUTE, resultAttribute, allocator);

        // 轉換為JSON字符串
        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        document.Accept(writer);

        // 發送命令
        std::string response;
        if (!sendCommand(_Recognition, buffer.GetString(), response))
        {
            throw std::runtime_error("sendCommand failed");
        }

        // check response
        rapidjson::Document responseJson;
        rapidjson::ParseResult parseResult = responseJson.Parse(response.c_str());
        if (parseResult.IsError())
        {
            std::cerr << "解析回應JSON失敗: " << rapidjson::GetParseError_En(parseResult.Code()) << std::endl;
            return false;
        }

        int rep_result = GetIntMember(responseJson, PAYLOAD_KEY_RESULT);
        if (rep_result != 1)
        {
            throw std::runtime_error(std::string("reportRecognition response result != 1, result=") + std::to_string(rep_result));
        }

        return true;
    }
    catch (const std::exception &e)
    {
        std::cerr << "reportRecognition error msg=" << e.what() << std::endl;
        return false;
    }

    return false;
}

int ChtP2PCameraCommandHandler::reportStatusEvent(const uint8_t *data, size_t dataSize)
{
    if (!m_initialized)
    {
        std::cerr << "CHT P2P服務尚未初始化" << std::endl;
        return -1;
    }

    if (!checkHiOssStatus())
    {
        std::cerr << "Camera does not bind, drop event" << std::endl;
        return REPORT_EVENT_NOT_RETRY;
    }

    if (data == NULL || dataSize == 0 || dataSize != sizeof(stCameraStatusEventSub))
    {
        std::cerr << "Invalid data!!!" << std::endl;
        return -2;
    }

    stCameraStatusEventSub *pSub = (stCameraStatusEventSub *)data;
    std::string eventId(pSub->eventId);
    eCameraStatusEventType statusEventType = (eCameraStatusEventType)pSub->statusType;
    eCameraStatus status = (eCameraStatus)pSub->status;
    eExternalStorageHealth externalStorageHealth = (eExternalStorageHealth)pSub->externalStorageHealth;

    if (eventId.empty()) {
        std::cerr << "Invalid parameter in data!!!" << std::endl;
        return -2;
    }

    //std::string eventTypeStr = zwsystem_ipc_statusEventType_int2str(statusEventType);
    std::string statusStr = zwsystem_ipc_status_int2str(status);
    std::string externalStorageHealthStr = zwsystem_ipc_health_int2str(externalStorageHealth);

    auto &paramsManager = CameraParametersManager::getInstance();
    const std::string& camId = paramsManager.getCameraId();
    const std::string& chtBarcode = paramsManager.getCHTBarcode();

    bool statusRes = reportStatusEvent(camId, chtBarcode, eventId,
            (int)statusEventType, statusStr, externalStorageHealthStr);
    if (!statusRes) {
        std::cerr << "reportStatusEvent failed!!!" << std::endl;
        return -3;
    }

    return 0;
}


bool ChtP2PCameraCommandHandler::reportStatusEvent(const std::string& camId, const std::string& chtBarcode,
    const std::string& eventId, int type, const std::string &status, const std::string &storageHealth)
{
    if (!m_initialized)
    {
        std::cerr << "CHT P2P服務尚未初始化" << std::endl;
        return false;
    }

    if (!checkHiOssStatus())
    {
        std::cerr << "Camera does not bind, drop event" << std::endl;
        return false;
    }

    auto require_non_empty = [](const std::string& s) { return !s.empty(); };
    if (!require_non_empty(camId) || !require_non_empty(eventId)) {
        return false;
    }

    if (type != 2 || type != 4) {
        std::cerr << "Invalid type value!!!" << std::endl;
        return false;
    }

    try {
        // 構建JSON payload
        rapidjson::Document document;
        document.SetObject();
        rapidjson::Document::AllocatorType &allocator = document.GetAllocator();

        AddString(document, PAYLOAD_KEY_CAMID, camId);
        AddString(document, PAYLOAD_KEY_CHT_BARCODE, chtBarcode);
        AddString(document, PAYLOAD_KEY_EVENT_ID, eventId);
        document.AddMember(PAYLOAD_KEY_TYPE, type, allocator);

        rapidjson::Value recording(rapidjson::kObjectType);
        AddString(recording, PAYLOAD_KEY_EVENT_ID, eventId, allocator);
        AddString(recording, PAYLOAD_KEY_CAMID, camId, allocator);
        AddString(recording, PAYLOAD_KEY_STATUS, status, allocator);
        AddString(recording, PAYLOAD_KEY_EXTERNAL_STORAGE_HEALTH, storageHealth, allocator);
        document.AddMember(PAYLOAD_KEY_RECORDING, recording, allocator);

        // 轉換為JSON字符串
        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        document.Accept(writer);

        // 發送命令
        std::string response;
        if (!sendCommand(_StatusEvent, buffer.GetString(), response)) {
            throw std::runtime_error("sendCommand failed");
        }

        // check response
        rapidjson::Document responseJson;
        rapidjson::ParseResult parseResult = responseJson.Parse(response.c_str());
        if (parseResult.IsError())
        {
            std::cerr << "解析回應JSON失敗: " << rapidjson::GetParseError_En(parseResult.Code()) << std::endl;
            return false;
        }

        int rep_result = GetIntMember(responseJson, PAYLOAD_KEY_RESULT);
        if (rep_result != 1) {
            throw std::runtime_error(std::string("reportStatusEvent response result != 1, result=") + std::to_string(rep_result));
        }

        return true;
    } catch (const std::exception &e) {
        std::cerr << "reportStatusEvent error msg=" << e.what() << std::endl;
        return false;
    }

    return false;
}

// 添加定期同步狀態的函數
void ChtP2PCameraCommandHandler::scheduledSync()
{
    if (!m_initialized)
    {
        return;
    }

    // 獲取參數管理器
    auto &paramsManager = CameraParametersManager::getInstance();

    // 同步硬體參數
    //bool syncResult = paramsManager.syncWithHardware(false); // 只同步重要參數

    //if (syncResult)
    {
        // 如果同步成功，保存到文件
        paramsManager.saveToFile();
    }
}

// 添加一個獲取攝影機狀態的方法
bool ChtP2PCameraCommandHandler::getNetworkStatus()
{
    auto &paramsManager = CameraParametersManager::getInstance();

    // 檢查 WiFi 信號強度是否過期 (使用 10 秒的過期時間)
    if (paramsManager.isParameterStale("wifiSignalStrength", std::chrono::milliseconds(10000)))
    {
        // 同步硬體參數
        //paramsManager.syncWithHardware(false);
    }

    return true;
}

// 添加一個獲取儲存狀態的方法
bool ChtP2PCameraCommandHandler::getStorageStatus()
{
    auto &paramsManager = CameraParametersManager::getInstance();

    // 檢查存儲參數是否過期 (使用 30 秒的過期時間)
    if (paramsManager.isParameterStale("storageAvailable", std::chrono::milliseconds(30000)) ||
        paramsManager.isParameterStale("storageHealth", std::chrono::milliseconds(30000)))
    {
        // 同步硬體參數
        //paramsManager.syncWithHardware(false);
    }

    return true;
}

#if 0
void ChtP2PCameraCommandHandler::setInitialInfoCallback(ChtP2PCameraAPI::InitialInfoCallback callback)
{
    m_initialInfoCallback = callback;
}
#endif

// ===== CHT P2P Agent 回調函數實現 =====

void ChtP2PCameraCommandHandler::commandDoneCallback(CHTP2P_CommandType commandType, void *commandHandle,
                                            const char *payload, void *userParam)
{
    std::string payloadStr = payload ? payload : "";
    std::cout << "收到命令完成回調: commandType=" << commandType
              << ", payload=" << payloadStr
              << ", commandHandle=" << commandHandle << std::endl;

    // 打印 m_commandContexts 中的所有鍵
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        std::cout << "m_commandContexts 包含 " << m_commandContexts.size() << " 個項目:" << std::endl;
        for (const auto &pair : m_commandContexts)
        {
            std::cout << "  - 鍵: " << pair.first << std::endl;
        }
    }

    // 尋找對應的命令上下文
    std::shared_ptr<CommandContext> context;
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        auto it = m_commandContexts.find(commandHandle);
        if (it != m_commandContexts.end())
        {
            context = it->second;
            std::cout << "找到對應的命令上下文" << std::endl;
            // 處理完畢後移除
            m_commandContexts.erase(it);
        }
    }

    // 如果找到對應的命令上下文，則通知等待線程
    if (context)
    {
        {
            std::unique_lock<std::mutex> lock(context->mutex);
            context->response = payloadStr;
            context->done = true;
            std::cout << "設置命令完成標誌" << std::endl;
        }
        // 通知等待線程
        context->cv.notify_one();
        std::cout << "通知等待線程" << std::endl;
    }
    else
    {
        std::cerr << "commandDoneCallback 找不到對應的命令上下文" << std::endl;
    }
}

// ===== 幫助函數實現 =====
bool ChtP2PCameraCommandHandler::sendCommand(CHTP2P_CommandType commandType,
        const std::string &payload, std::string &response)
{
    if (!m_initialized)
    {
        std::cerr << "CHT P2P服務尚未初始化" << std::endl;
        return false;
    }

    // 創建命令上下文
    auto context = std::make_shared<CommandContext>();
    context->done = false;

    // 創建一個有效的指針作為 commandHandle
    static int dummy_handle;
    void *commandHandle = &dummy_handle; // 使用靜態變量的地址作為有效的指針

    // 在發送命令前先存儲上下文
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_commandContexts[commandHandle] = context;
        std::cout << "儲存命令上下文，commandHandle: " << commandHandle << std::endl;
    }

    // 發送命令
    int result = chtp2p_send_command(commandType, &commandHandle, payload.c_str());
    if (result != 0)
    {
        std::cerr << "發送命令失敗，錯誤碼: " << result << std::endl;

        // 移除命令上下文
        std::unique_lock<std::mutex> lock(m_mutex);
        m_commandContexts.erase(commandHandle);

        return false;
    }

    // 等待命令完成
    {
        std::unique_lock<std::mutex> lock(context->mutex);
        std::cout << "等待命令完成，commandHandle: " << commandHandle << std::endl;
        if (!context->cv.wait_for(lock, std::chrono::seconds(10), [&context]()
                                  { return context->done; }))
        {
            // 超時
            std::cerr << "命令執行超時" << std::endl;

            // 移除命令上下文
            std::unique_lock<std::mutex> globalLock(m_mutex);
            m_commandContexts.erase(commandHandle);

            return false;
        }
        std::cout << "命令已完成！" << std::endl;
    }

    // 命令執行完成，獲取回應
    response = context->response;

    // 檢查回應是否成功
    try
    {
        rapidjson::Document responseJson;
        rapidjson::ParseResult parseResult = responseJson.Parse(response.c_str());
        if (parseResult.IsError())
        {
            std::cerr << "解析回應JSON失敗: " << rapidjson::GetParseError_En(parseResult.Code()) << std::endl;
            return false;
        }

        auto it = responseJson.FindMember(PAYLOAD_KEY_CODE);
        if (it != responseJson.MemberEnd() && it->value.IsInt()) {
            int code = it->value.GetInt();
            if (code != 0)
            {
                std::cerr << "命令執行失敗，錯誤碼: " << code << std::endl;
                return false;
            }

            return true;
        }

        it = responseJson.FindMember(PAYLOAD_KEY_RESULT);
        if (it != responseJson.MemberEnd() && it->value.IsInt()) {
            int result = it->value.GetInt();
            if (result != 1)
            {
                std::cerr << "命令執行失敗，錯誤碼: " << result << std::endl;
                return false;
            }

            return true;
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << "解析回應JSON失敗: " << e.what() << std::endl;
        return false;
    }

    return false;
}

// 實現內部輔助函數
void ChtP2PCameraCommandHandler::handleInitialInfoReceived(const std::string &hamiCamInfo,
                                                  const std::string &hamiSettings,
                                                  const std::string &hamiAiSettings,
                                                  const std::string &hamiSystemSettings)
{
    std::cout << "ChtP2PCameraCommandHandler: 處理初始化資訊..." << std::endl;

    try
    {
        // 獲取參數管理器
        auto &paramsManager = CameraParametersManager::getInstance();

        // 解析並儲存參數
        bool parseResult = paramsManager.parseAndSaveInitialInfo(
            hamiCamInfo, hamiSettings, hamiAiSettings, hamiSystemSettings);

        if (parseResult)
        {
            // 同步到硬體
            if (syncParametersToHardware())
            {
                std::cout << "ChtP2PCameraCommandHandler: 初始化參數處理完成" << std::endl;
            }
            else
            {
                std::cerr << "ChtP2PCameraCommandHandler: 硬體參數同步失敗" << std::endl;
            }
        }
        else
        {
            std::cerr << "ChtP2PCameraCommandHandler: 參數解析失敗" << std::endl;
        }
#if 0
        // 調用用戶設定的回調函數
        if (m_initialInfoCallback)
        {
            m_initialInfoCallback(hamiCamInfo, hamiSettings, hamiAiSettings, hamiSystemSettings);
        }
#endif
    }
    catch (const std::exception &e)
    {
        std::cerr << "ChtP2PCameraCommandHandler: 處理初始化資訊時發生異常: " << e.what() << std::endl;
    }
}

bool ChtP2PCameraCommandHandler::syncParametersToHardware()
{
    try
    {
        auto &paramsManager = CameraParametersManager::getInstance();
        //auto &driver = CameraDriver::getInstance();

        // 使用攝影機驅動同步硬體參數
        //return driver.syncHardwareFromParameters(paramsManager);
    }
    catch (const std::exception &e)
    {
        std::cerr << "ChtP2PCameraCommandHandler: 同步硬體參數時發生異常: " << e.what() << std::endl;
        return false;
    }
    return false;
}
