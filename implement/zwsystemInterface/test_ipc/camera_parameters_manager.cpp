// CameraParametersManager.cpp

#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <random>
#include <sstream>
#include <string>

#include <rapidjson/document.h>
#include <rapidjson/error/en.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include "camera_parameters_manager.h"

#include "cht_p2p_agent_payload_defined.h"
#include "timezone_utils.h"
// #include "camera_driver.h"

// 獲取單例實例
CameraParametersManager &CameraParametersManager::getInstance()
{
    static CameraParametersManager instance;
    return instance;
}

// 構造函數
CameraParametersManager::CameraParametersManager()
    : m_configFilePath("/etc/config/ipcam_params.json"),
      m_barcodeConfigPath("/etc/config/ipcam_barcode.json"),
      m_initialized(false),
      m_nextCallbackId(1)
{
    // 初始默認參數在 initializeDefaultParameters 中設置
}

// 初始化參數管理器
bool CameraParametersManager::initialize(const std::string &configFilePath, const std::string &barcodeConfigPath)
{
    std::cout << "CameraParametersManager::initialize - 開始初始化 (configPath: "
              << configFilePath << ", barcodePath: " << barcodeConfigPath << ")" << std::endl;
    std::cout << "CameraParametersManager::initialize - 開始初始化流程" << std::endl;
    std::cout << "CameraParametersManager::initializeBarcode:" << __LINE__ << std::endl;

    // 保存路徑
    if (!barcodeConfigPath.empty())
    {
        m_barcodeConfigPath = barcodeConfigPath;
    }

    // 只初始化基本參數
    bool result = initialize(configFilePath);
    if (!result)
    {
        std::cout << "CameraParametersManager::initialize - 基本參數初始化失敗" << std::endl;
        return false;
    }
    // std::cout << "CameraParametersManager::initializeBarcode:" << __LINE__ << std::endl;
    std::cout << "CameraParametersManager::initialize - 基本參數初始化成功" << std::endl;
    // 手動創建條碼文件，跳過initializeBarcode調用
    std::string barcode = getCHTBarcode();
    if (barcode.empty())
    {
        barcode = "CHT123456789DEFAULTCODE0000";
        setCHTBarcode(barcode);
    }
    std::cout << "CameraParametersManager::initializeBarcode:" << __LINE__ << std::endl;
    // 創建條碼文件
    std::cout << "CameraParametersManager::initialize - 手動創建條碼文件: " << m_barcodeConfigPath << std::endl;
    FILE *file = fopen(m_barcodeConfigPath.c_str(), "w");
    if (file)
    {
        std::string jsonContent = "{\"chtBarcode\": \"" + barcode + "\"}";
        fputs(jsonContent.c_str(), file);
        fclose(file);
        std::cout << "CameraParametersManager::initialize - 條碼文件創建成功" << std::endl;
    }
    else
    {
        std::cerr << "CameraParametersManager::initialize - 條碼文件創建失敗，嘗試備用路徑" << std::endl;
        std::string backupPath = "./ipcam_barcode.json";
        file = fopen(backupPath.c_str(), "w");
        if (file)
        {
            std::string jsonContent = "{\"chtBarcode\": \"" + barcode + "\"}";
            fputs(jsonContent.c_str(), file);
            fclose(file);
            std::cout << "CameraParametersManager::initialize - 備用條碼文件創建成功" << std::endl;
        }
    }

    std::cout << "CameraParametersManager::initialize - 完成初始化" << std::endl;
    m_initialized = true;
    return true;
}

bool CameraParametersManager::initialize(const std::string &configFilePath)
{
    // 添加調試輸出
    std::cout << "CameraParametersManager::initialize(single) - 開始初始化 (configPath: "
              << configFilePath << ")" << std::endl;
    std::cout << "CameraParametersManager::initialize - 開始初始化流程" << std::endl;
    std::cout << "CameraParametersManager::initializeBarcode:" << __LINE__ << std::endl;

    // 設置配置文件路徑
    if (!configFilePath.empty())
    {
        m_configFilePath = configFilePath;
    }
    std::cout << "CameraParametersManager::initializeBarcode:" << __LINE__ << std::endl;
    std::cout << "CameraParametersManager::initialize(single) - 使用配置路徑: "
              << m_configFilePath << std::endl;

    // 確保配置目錄存在
    std::string dirPath = m_configFilePath.substr(0, m_configFilePath.find_last_of('/'));
    std::string mkdirCmd = "mkdir -p " + dirPath;
    std::cout << "CameraParametersManager::initialize(single) - 執行命令: " << mkdirCmd << std::endl;
    std::cout << "CameraParametersManager::initializeBarcode:" << __LINE__ << std::endl;
    int result = system(mkdirCmd.c_str());
    if (result != 0)
    {
        std::cerr << "警告: 無法建立目錄 " << dirPath << std::endl;
        // 使用備用目錄
        m_configFilePath = "./ipcam_params.json";
        std::cout << "使用備用配置路徑: " << m_configFilePath << std::endl;
    }

    std::cout << "CameraParametersManager::initialize(single) - 嘗試從文件加載配置" << std::endl;

    // 嘗試從文件加載配置
    bool loaded = loadFromFile(m_configFilePath);

    std::cout << "CameraParametersManager::initialize(single) - 加載配置結果: "
              << (loaded ? "成功" : "失敗") << std::endl;

    // 如果加載失敗或文件不存在，初始化預設參數並同步硬體
    if (!loaded)
    {
        std::cout << "配置檔案不存在或讀取失敗，將使用預設值並同步硬體參數" << std::endl;
        // 設置基本預設值
        // 初始化默認參數後立即輸出
        initializeDefaultParameters();
        std::cout << "DEFAULT - activeStatus: " << m_parameters["activeStatus"] << std::endl;

        std::cout << "CameraParametersManager::initialize(single) - 儲存配置到檔案" << std::endl;

        // 儲存到檔案
        saveToFile(m_configFilePath);
    }

    std::cout << "CameraParametersManager::initialize(single) - 完成初始化" << std::endl;
    m_initialized = true;
    return true;
}

bool CameraParametersManager::initializeBarcode(const std::string &barcodeConfigPath)
{
    std::cout << "CameraParametersManager::initializeBarcode - 開始條碼初始化" << std::endl;
    std::cout << "CameraParametersManager::initialize - 開始初始化流程" << std::endl;
    std::cout << "CameraParametersManager::initializeBarcode:" << __LINE__ << std::endl;
    try
    {
        // 設置配置路徑
        if (!barcodeConfigPath.empty())
        {
            m_barcodeConfigPath = barcodeConfigPath;
        }

        // 使用固定條碼或從參數獲取
        std::string barcode = getCHTBarcode();
        if (barcode.empty() || barcode == "DEFAULT_BARCODE")
        {
            barcode = "CHT123456789DEFAULTCODE0000";
            setCHTBarcode(barcode);
        }

        std::cout << "CameraParametersManager::initializeBarcode - 使用條碼: " << barcode << std::endl;

        // 創建條碼文件 - 使用C文件API避免可能的C++流問題
        FILE *file = fopen(m_barcodeConfigPath.c_str(), "w");
        if (file)
        {
            std::string jsonContent = "{\"chtBarcode\": \"" + barcode + "\"}";
            fputs(jsonContent.c_str(), file);
            fclose(file);
            std::cout << "CameraParametersManager::initializeBarcode - 條碼文件已創建: " << m_barcodeConfigPath << std::endl;
        }
        else
        {
            std::cerr << "CameraParametersManager::initializeBarcode - 無法創建條碼文件: " << m_barcodeConfigPath << std::endl;

            // 嘗試使用備用路徑
            std::string backupPath = "./ipcam_barcode.json";
            std::cout << "CameraParametersManager::initializeBarcode - 嘗試備用路徑: " << backupPath << std::endl;

            FILE *backupFile = fopen(backupPath.c_str(), "w");
            if (backupFile)
            {
                std::string jsonContent = "{\"chtBarcode\": \"" + barcode + "\"}";
                fputs(jsonContent.c_str(), backupFile);
                fclose(backupFile);
                std::cout << "CameraParametersManager::initializeBarcode - 條碼文件已創建(備用): " << backupPath << std::endl;
            }
            else
            {
                std::cerr << "CameraParametersManager::initializeBarcode - 備用路徑也創建失敗，但繼續執行" << std::endl;
            }
        }

        std::cout << "CameraParametersManager::initializeBarcode - 完成初始化" << std::endl;
        return true;
    }
    catch (const std::exception &e)
    {
        std::cerr << "CameraParametersManager::initializeBarcode - 發生異常: " << e.what() << std::endl;
        std::cerr << "CameraParametersManager::initializeBarcode - 將繼續執行" << std::endl;
        return true; // 即使出錯也返回true以避免阻塞
    }
    catch (...)
    {
        std::cerr << "CameraParametersManager::initializeBarcode - 發生未知異常" << std::endl;
        std::cerr << "CameraParametersManager::initializeBarcode - 將繼續執行" << std::endl;
        return true; // 即使出錯也返回true以避免阻塞
    }
}

bool CameraParametersManager::saveBarcodeToFile(const std::string &path)
{
    std::cout << "CameraParametersManager::saveBarcodeToFile - 開始 (path: " << path << ")" << std::endl;

    try
    {
        // 使用提供的路徑或默認路徑
        std::string effectivePath = path.empty() ? m_barcodeConfigPath : path;
        std::cout << "CameraParametersManager::saveBarcodeToFile - 使用路徑: " << effectivePath << std::endl;

        // 獲取條碼
        std::string chtBarcode = getCHTBarcode();
        std::cout << "CameraParametersManager::saveBarcodeToFile - 保存條碼: " << chtBarcode << std::endl;

        if (chtBarcode.empty())
        {
            std::cerr << "CameraParametersManager::saveBarcodeToFile - 條碼為空，使用默認值" << std::endl;
            chtBarcode = "CHT123456789DEFAULTCODE0000";
            setCHTBarcode(chtBarcode);
        }

        // 創建JSON內容
        std::string jsonContent = "{\"chtBarcode\": \"" + chtBarcode + "\"}";
        std::cout << "CameraParametersManager::saveBarcodeToFile - JSON內容: " << jsonContent << std::endl;

        // 使用C文件API寫入
        FILE *file = fopen(effectivePath.c_str(), "w");
        if (file)
        {
            fputs(jsonContent.c_str(), file);
            fclose(file);
            std::cout << "CameraParametersManager::saveBarcodeToFile - 文件寫入成功: " << effectivePath << std::endl;
            return true;
        }
        else
        {
            std::cerr << "CameraParametersManager::saveBarcodeToFile - 無法創建文件: " << effectivePath << std::endl;

            // 嘗試備用路徑
            std::string backupPath = "./ipcam_barcode.json";
            std::cout << "CameraParametersManager::saveBarcodeToFile - 嘗試備用路徑: " << backupPath << std::endl;

            FILE *backupFile = fopen(backupPath.c_str(), "w");
            if (backupFile)
            {
                fputs(jsonContent.c_str(), backupFile);
                fclose(backupFile);
                std::cout << "CameraParametersManager::saveBarcodeToFile - 備用文件寫入成功" << std::endl;
                return true;
            }
            else
            {
                std::cerr << "CameraParametersManager::saveBarcodeToFile - 備用路徑也創建失敗，但繼續執行" << std::endl;
                return true; // 仍然返回成功以避免阻塞
            }
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << "CameraParametersManager::saveBarcodeToFile - 異常: " << e.what() << std::endl;
        return true; // 返回true避免中斷流程
    }
}

// 獲取默認條碼配置
// 返回一個包含 camId 和 chtBarcode 的 pair
std::string CameraParametersManager::generateDefaultBarcode() const
{
    // 使用基本的隨機生成方法產生 barcode
    std::string barcode = "CHT";

    // 加上時間戳，確保唯一性
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                         now.time_since_epoch())
                         .count();

    // 添加隨機數據
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dist(0, 9);

    for (int i = 0; i < 16; i++)
    {
        barcode += std::to_string(dist(gen));
    }

    // 加上時間戳最後6位
    barcode += std::to_string(timestamp % 1000000);

    // 確保長度正確 (25位)
    if (barcode.length() > 25)
    {
        barcode = barcode.substr(0, 25);
    }
    else
    {
        while (barcode.length() < 25)
        {
            barcode += std::to_string(dist(gen));
        }
    }

    return barcode;
}

// 檢查配置文件是否存在
bool CameraParametersManager::configFilesExist() const
{
    std::ifstream configFile(m_configFilePath);
    std::ifstream barcodeFile(m_barcodeConfigPath);

    bool configExists = configFile.good();
    bool barcodeExists = barcodeFile.good();

    configFile.close();
    barcodeFile.close();

    return configExists && barcodeExists;
}

// 檢查是否已完成綁定
bool CameraParametersManager::isBound() const
{
#if 1
    // 首先檢查組態檔案是否存在
    std::ifstream configFile(m_configFilePath);
    if (!configFile.good())
    {
        // 組態檔案不存在，一定是未繫結
        return false;
    }
    configFile.close();

    // 使用 activeStatus 判斷繫結狀態
    std::string activeStatus = getParameter("activeStatus", "0");
    return activeStatus == "1";
#else
    // 檢查是否有配置文件
    if (!configFilesExist())
    {
        return false;
    }

    // 檢查camId和chtBarcode是否為有效值
    std::string camId = getCameraId();
    std::string chtBarcode = getCHTBarcode();

    return !camId.empty() && !chtBarcode.empty() &&
           camId != "DEFAULT_CAM_ID" && chtBarcode != "DEFAULT_BARCODE";
#endif
}
// 加入 isFirstBinding 方法
bool CameraParametersManager::isFirstBinding() const
{
    // 檢查組態檔案是否存在，不存在表示首次繫結
    std::ifstream configFile(m_configFilePath);
    bool firstBinding = !configFile.good();
    if (configFile.is_open())
    {
        configFile.close();
    }
    return firstBinding;
}
// 生成隨機camId和barcode
std::pair<std::string, std::string> CameraParametersManager::generateRandomCamIdAndBarcode()
{
    // 使用現有的信息生成隨機ID
    std::string baseString = getMacAddress();
    if (baseString == "00:00:00:00:00:00")
    {
        // 使用隨機數生成器
        std::random_device rd;
        std::mt19937 gen(rd());

        // 生成16位隨機數字字符串作為基礎
        std::uniform_int_distribution<> dist(0, 9);
        baseString = "";
        for (int i = 0; i < 12; i++)
        {
            baseString += std::to_string(dist(gen));
        }
    }
    else
    {
        // 移除MAC地址中的冒號
        baseString.erase(std::remove(baseString.begin(), baseString.end(), ':'), baseString.end());
    }

    // 為了確保唯一性，加上時間戳
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                         now.time_since_epoch())
                         .count();

    // 組合生成camId (25位)
    std::string prefix = "CHT";
    std::string suffix = std::to_string(timestamp % 1000000);
    std::string camId = prefix + baseString + suffix;

    // 保證camId長度為25位
    if (camId.length() > 25)
    {
        camId = camId.substr(0, 25);
    }
    else
    {
        // 如果長度不足，使用隨機數填充
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dist(0, 9);

        while (camId.length() < 25)
        {
            camId += std::to_string(dist(gen));
        }
    }

    // 條碼與camId相同
    std::string barcode = camId;

    return std::make_pair(camId, barcode);
}

std::string CameraParametersManager::generateCameraNameFromMac()
{
    std::string iface;
    for (const std::string &candidate : {"eth0", "eth1"})
    {
        std::ifstream stateFile("/sys/class/net/" + candidate + "/operstate");
        std::string state;
        if (stateFile.is_open())
        {
            std::getline(stateFile, state);
            stateFile.close();
            if (state == "up")
            {
                iface = candidate;
                break;
            }
        }
    }

    std::string macAddress;
    if (!iface.empty())
    {
        std::ifstream macFile("/sys/class/net/" + iface + "/address");
        if (macFile.is_open())
        {
            std::getline(macFile, macAddress);
            macFile.close();
            macAddress.erase(std::remove(macAddress.begin(), macAddress.end(), '\n'), macAddress.end());
            m_parameters["macAddress"] = macAddress; // 更新參數表
        }
    }

    // 擷取 MAC 後四碼
    std::string macSuffix;
    if (!macAddress.empty())
    {
        size_t lastColon = macAddress.rfind(':');
        if (lastColon != std::string::npos)
        {
            size_t secondLastColon = macAddress.rfind(':', lastColon - 1);
            if (secondLastColon != std::string::npos)
            {
                macSuffix = macAddress.substr(secondLastColon + 1, 2) + macAddress.substr(lastColon + 1, 2);
            }
        }
    }

    if (macSuffix.empty() || macSuffix.length() != 4)
    {
        macSuffix = "4455";
    }

    std::transform(macSuffix.begin(), macSuffix.end(), macSuffix.begin(), ::toupper);
    return "HAMICAM-ZAI730-" + macSuffix;
}

// 初始化默認參數
void CameraParametersManager::initializeDefaultParameters()
{
    // 原有的默認參數...
    m_parameters[PAYLOAD_KEY_CAMID] = "27E13A0931001004734"; //"DEFAULT_CAM_ID";
    // m_parameters["chtBarcode"] = "27E13A0931001004734"; //"DEFAULT_BARCODE";
    m_parameters["publicIp"] = "192.168.1.100";
    m_parameters["wifiSsid"] = "DefaultWiFi";
    m_parameters["wifiSignalStrength"] = "-65";
    // m_parameters["firmwareVersion"] = "1.0.0";
    m_parameters["cameraStatus"] = "Normal";
    m_parameters["storageCapacity"] = "1024"; // 1GB
    m_parameters["storageAvailable"] = "512"; // 500MB
    m_parameters["storageHealth"] = "Normal";
    m_parameters["microphoneEnabled"] = "1"; // 啟用
    m_parameters["speakerVolume"] = "50";    // 50%
    m_parameters["imageQuality"] = "2";      // 中等品質
    // m_parameters["macAddress"] = "00:11:22:33:44:55"; // 預設MAC地址
    m_parameters["activeStatus"] = "0"; // 改為未繫結狀態 (綁定後為 "1")
    m_parameters["deviceStatus"] = "1"; // 程式執行中
    m_parameters["timezone"] = "51";    // 台北時區

    // 新增原本 dataStorage 中的默認參數
    m_parameters["netNo"] = "DEFAULT_NET";
    m_parameters["vsDomain"] = "vs.default.com";
    m_parameters["vsToken"] = "default_token";
    m_parameters["cameraType"] = "IPCAM";
    m_parameters["model"] = "DefaultModel";
    m_parameters["isCheckHioss"] = "0"; // 不需要卡控 ,攝影機是否卡控識別，「0」：不需要(stage預設)，「1」： 需要(production預設)
    m_parameters["brand"] = "DefaultBrand";
    m_parameters["camSid"] = "DEFAULT_SID";         // 加入 camSid 預設值
    m_parameters["tenantId"] = "DEFAULT_TENANT_ID"; // 加入 tenantId 預設值

    std::cout << "initializeDefaultParameters" << std::endl;
    // 設置攝影機名稱
    m_parameters["cameraName"] = generateCameraNameFromMac();

    // check chtBarcode from /etc/init.d/S99zwp2pagent start ,
    std::string chtBarcode = getChtBarcodeFromUbootExport();
    std::cout << "## initializeDefaultParameters chtBarcode:" << chtBarcode << std::endl;
    if (!chtBarcode.empty() && chtBarcode != "0000000000000000000")
    {
        m_parameters["chtBarcode"] = chtBarcode;
        // 根據 spec 規定，chtBarcode 等同於 camId
        m_parameters["camId"] = chtBarcode;
        std::cout << "## 設置 chtBarcode 和 camId 為: " << chtBarcode << std::endl;
    }
    else
    {
        std::cerr << "錯誤: 無法從 U-Boot 環境變數讀取有效的 chtBarcode" << std::endl;
        std::cerr << "IPCAM 無法啟用，因為無法對 CHT P2P Agent 註冊與綁定" << std::endl;
        // 使用預設值暫時允許程式繼續，但應該要在上層處理這個錯誤
        m_parameters["chtBarcode"] = ""; // 設置為空字串表示無效
        m_parameters["camId"] = "";
    }

    std::string macFromExport = getethaddrFromUbootExport();
    std::cout << "## initializeDefaultParameters macFromExport:" << macFromExport << std::endl;
    if (!macFromExport.empty())
    {
        m_parameters["macAddress"] = macFromExport;
    }

    // read  data from /
    std::string firmwareVersionExport = getFirmwareDefVersion();
    std::cout << "## initializeDefaultParameters firmwareVersionExport:" << firmwareVersionExport << std::endl;
    if (!firmwareVersionExport.empty())
    {
        m_parameters["firmwareVersion"] = firmwareVersionExport;
    }

    // 記錄更新時間
    auto now = std::chrono::system_clock::now();
    for (const auto &param : m_parameters)
    {
        m_parameterUpdateTimes[param.first] = now;
    }
}

// 獲取 NTP 伺服器設定
std::string CameraParametersManager::getNtpServer() const
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto it = m_parameters.find("ntpServer");
    return (it != m_parameters.end()) ? it->second : "tock.stdtime.gov.tw"; // 預設值
}

// 設定 NTP 伺服器
void CameraParametersManager::setNtpServer(const std::string &ntpServer)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_parameters["ntpServer"] = ntpServer;
    m_parameterUpdateTimes["ntpServer"] = std::chrono::system_clock::now();

    std::cout << "NTP 伺服器已更新為: " << ntpServer << std::endl;

    // 通知參數變更
    notifyParameterChanged("ntpServer", ntpServer);
}

// 使用指定的 NTP 伺服器同步時間
bool CameraParametersManager::syncTimeWithNtp(const std::string &customNtpServer)
{
    std::string ntpServer = customNtpServer.empty() ? getNtpServer() : customNtpServer;

    std::cout << "開始使用 NTP 伺服器同步時間: " << ntpServer << std::endl;

    try
    {
        // 方法 1: 使用 ntpdate
        std::string ntpCmd = "ntpdate -b -u " + ntpServer + " 2>/dev/null";
        std::cout << "## [DEBUG] Execute NTP Command: " << ntpCmd << std::endl;

        int result = system(ntpCmd.c_str());

        if (result == 0)
        {
            std::cout << "✓ NTP 時間同步成功 (使用 ntpdate)" << std::endl;

            // 記錄最後成功同步的時間和伺服器
            setParameter("lastNtpSync", std::to_string(std::time(nullptr)));
            setParameter("lastNtpServer", ntpServer);

            return true;
        }

        // 方法 2: 如果 ntpdate 失敗，嘗試使用 sntp
        std::string sntpCmd = "sntp -s " + ntpServer + " 2>/dev/null";
        std::cout << "## [DEBUG] Fallback SNTP Command: " << sntpCmd << std::endl;

        result = system(sntpCmd.c_str());

        if (result == 0)
        {
            std::cout << "✓ NTP 時間同步成功 (使用 sntp)" << std::endl;

            setParameter("lastNtpSync", std::to_string(std::time(nullptr)));
            setParameter("lastNtpServer", ntpServer);

            return true;
        }

        // 方法 3: 嘗試使用 chrony (如果可用)
        std::string chronyCmd = "chronyd -q 'server " + ntpServer + " iburst' 2>/dev/null";
        result = system(chronyCmd.c_str());

        if (result == 0)
        {
            std::cout << "✓ NTP 時間同步成功 (使用 chrony)" << std::endl;

            setParameter("lastNtpSync", std::to_string(std::time(nullptr)));
            setParameter("lastNtpServer", ntpServer);

            return true;
        }

        std::cerr << "✗ 所有 NTP 同步方法都失敗" << std::endl;
        setParameter("lastNtpError", "All NTP sync methods failed");

        return false;
    }
    catch (const std::exception &e)
    {
        std::cerr << "NTP 同步時發生異常: " << e.what() << std::endl;
        setParameter("lastNtpError", e.what());
        return false;
    }
}

// 使用當前設定的 NTP 伺服器更新系統時間
bool CameraParametersManager::updateSystemTimeFromNtp()
{
    return syncTimeWithNtp("");
}

// 完整的時區初始化與 NTP 同步
bool CameraParametersManager::initializeTimezoneWithNtpSync()
{
    std::cout << "=========================" << std::endl;
    std::cout << "   初始化時區並同步 NTP 時間" << std::endl;
    std::cout << "=========================" << std::endl;

    try
    {
        // 步驟 1: 初始化時區（使用現有邏輯）
        std::string savedTzId = getTimeZone();
        std::cout << "當前時區設定: " << (savedTzId.empty() ? "(空)" : savedTzId) << std::endl;

        std::string targetTzId = savedTzId.empty() ? "51" : savedTzId; // 預設台北時區

        // 步驟 2: 設定時區
        std::string tzString = TimezoneUtils::getTimezoneString(targetTzId);
        if (tzString.empty())
        {
            std::cerr << "無法獲取時區字串，時區ID: " << targetTzId << std::endl;
            return false;
        }

        std::cout << "設定時區: " << tzString << std::endl;

        // 應用時區設定
        setenv("TZ", tzString.c_str(), 1);
        tzset();

        // 寫入 /etc/TZ 檔案
        std::ofstream tzFile("/etc/TZ");
        if (tzFile.is_open())
        {
            tzFile << tzString << std::endl;
            tzFile.close();
            std::cout << "時區已寫入 /etc/TZ" << std::endl;
        }

        // 更新參數管理器
        setTimeZone(targetTzId);

        // 步驟 3: NTP 時間同步
        std::cout << "\n開始 NTP 時間同步..." << std::endl;

        // 從參數中獲取 NTP 伺服器設定
        std::string configuredNtpServer = getNtpServer();
        std::cout << "使用 NTP 伺服器: " << configuredNtpServer << std::endl;

        bool ntpSuccess = syncTimeWithNtp(configuredNtpServer);

        if (ntpSuccess)
        {
            std::cout << "✓ 時區設定和 NTP 同步完成" << std::endl;
        }
        else
        {
            std::cout << "⚠ 時區設定完成，但 NTP 同步失敗（這是正常的，可能是網路問題）" << std::endl;
        }

        // 步驟 4: 顯示當前時間
        std::cout << "\n當前系統時間: ";
        if (system("date") != 0)
        {
            std::cout << "無法獲取系統時間" << std::endl;
        }

        // 步驟 5: 保存設定
        bool saveResult = saveToFile();
        std::cout << "參數保存: " << (saveResult ? "成功" : "失敗") << std::endl;

        std::cout << "\n===== 時區和時間初始化完成 =====" << std::endl;
        return true;
    }
    catch (const std::exception &e)
    {
        std::cerr << "初始化時區和 NTP 同步時發生異常: " << e.what() << std::endl;
        return false;
    }
}

std::string CameraParametersManager::getChtBarcodeFromUbootExport()
{
    std::cout << "getChtBarcodeFromUbootExport" << std::endl;
    const std::string filePath = "/tmp/tmp_chtBarcode";

    struct stat buffer;
    if (stat(filePath.c_str(), &buffer) != 0)
    {
        std::cerr << "[ERROR] File not found: " << filePath << std::endl;
        return "";
    }

    std::ifstream file(filePath);
    if (!file)
    {
        std::cerr << "[ERROR] Failed to open file: " << filePath << std::endl;
        return "";
    }

    std::string barcode;
    std::getline(file, barcode);

    // 移除結尾換行符號與空白
    barcode.erase(barcode.find_last_not_of(" \n\r\t") + 1);
    //  empty_chtBarcode_mac  is  from /etc/init.d/S99zwp2pagent export chtBarcode from U-boot
    if (barcode.empty() || barcode == "empty_chtBarcode_mac")
    {
        std::cerr << "[WARNING] Invalid chtBarcode: " << barcode << std::endl;
        return "";
    }

    return barcode;
}

std::string CameraParametersManager::getethaddrFromUbootExport()
{
    std::cout << "getChtBarcodeFromUbootExport" << std::endl;
    const std::string filePath = "/tmp/tmp_ethaddr";
    std::ifstream file(filePath);

    if (!file.is_open())
    {
        std::cerr << "Error: " << filePath << " not found or cannot be opened." << std::endl;
        return "";
    }

    std::string ethaddr;
    std::getline(file, ethaddr);
    file.close();

    // 可以加強驗證格式是否為合法的 MAC 位址（選擇性）
    return ethaddr;
}

std::string CameraParametersManager::getFirmwareDefVersion()
{
    const std::string versionFile = "/etc/sysinfo/.version";
    std::ifstream file(versionFile);
    if (!file.is_open())
    {
        return "unknown"; // 無法開啟檔案時回傳預設值
    }

    std::string line;
    while (std::getline(file, line))
    {
        if (line.rfind("SW_VERSION=", 0) == 0) // 檢查行是否以 SW_VERSION= 開頭
        {
            return line.substr(std::string("SW_VERSION=").length());
        }
    }

    return "unknown"; // 找不到 SW_VERSION 行時回傳預設值
}

// 參數讀取函數
std::string CameraParametersManager::getCameraId() const
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto it = m_parameters.find(PAYLOAD_KEY_CAMID);
    return (it != m_parameters.end()) ? it->second : "";
}

std::string CameraParametersManager::getCHTBarcode() const
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto it = m_parameters.find(PAYLOAD_KEY_CHT_BARCODE);
    return (it != m_parameters.end()) ? it->second : "";
}
// 實現 getter 和 setter 方法
std::string CameraParametersManager::getCamSid() const
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto it = m_parameters.find("camSid");
    return (it != m_parameters.end()) ? it->second : "";
}

std::string CameraParametersManager::getTenantId() const
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto it = m_parameters.find("tenantId");
    return (it != m_parameters.end()) ? it->second : "";
}

std::string CameraParametersManager::getPublicIp() const
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto it = m_parameters.find("publicIp");
    return (it != m_parameters.end()) ? it->second : "";
}

std::string CameraParametersManager::getCameraName() const
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto it = m_parameters.find("cameraName");
    if (it != m_parameters.end() && !it->second.empty())
    {
        return it->second;
    }

    return "Unknown Camera";
}

static const char defaultOsdRule[] = "yyyy-MM-dd HH:mm:ss"; // follow cht format
std::string CameraParametersManager::getOsdRule() const
{
    return getParameter("osdRule", defaultOsdRule);
}

std::string CameraParametersManager::getWifiSsid() const
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto it = m_parameters.find("wifiSsid");
    return (it != m_parameters.end()) ? it->second : "";
}

std::string CameraParametersManager::getFirmwareVersion() const
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto it = m_parameters.find("firmwareVersion");
    return (it != m_parameters.end()) ? it->second : "";
}

std::string CameraParametersManager::getCameraStatus() const
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto it = m_parameters.find("cameraStatus");
    return (it != m_parameters.end()) ? it->second : "offline";
}

long CameraParametersManager::getStorageCapacity() const
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto it = m_parameters.find("storageCapacity");
    return (it != m_parameters.end()) ? std::stol(it->second) : 0;
}

long CameraParametersManager::getStorageAvailable() const
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto it = m_parameters.find("storageAvailable");
    return (it != m_parameters.end()) ? std::stol(it->second) : 0;
}

std::string CameraParametersManager::getStorageHealth() const
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto it = m_parameters.find("storageHealth");
    return (it != m_parameters.end()) ? it->second : "unknown";
}

bool CameraParametersManager::getMicrophoneEnabled() const
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto it = m_parameters.find("microphoneEnabled");
    return (it != m_parameters.end()) ? (it->second == "1") : false;
}

int CameraParametersManager::getSpeakerVolume() const
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto it = m_parameters.find("speakerVolume");
    return (it != m_parameters.end()) ? std::stoi(it->second) : 50;
}

std::string CameraParametersManager::getActiveStatus() const
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto it = m_parameters.find("activeStatus");
    return (it != m_parameters.end()) ? it->second : "0";
}

std::string CameraParametersManager::getDeviceStatus() const
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto it = m_parameters.find("deviceStatus");
    return (it != m_parameters.end()) ? it->second : "offline";
}

std::string CameraParametersManager::getAISettings() const
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto it = m_parameters.find("aiSettings");
    return (it != m_parameters.end()) ? it->second : "{}";
}

std::string CameraParametersManager::getMacAddress() const
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto it = m_parameters.find("macAddress");
    return (it != m_parameters.end()) ? it->second : "00:00:00:00:00:00";
}

std::string CameraParametersManager::getTimeZone() const
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto it = m_parameters.find("timezone");
    return (it != m_parameters.end()) ? it->second : "51"; // 預設台北時區
}

// 新增 dataStorage 中的 getter 方法
const std::string &CameraParametersManager::getNetNo() const
{
    static std::string defaultValue = "";
    auto it = m_parameters.find("netNo");
    return (it != m_parameters.end()) ? it->second : defaultValue;
}

const std::string &CameraParametersManager::getVsDomain() const
{
    static std::string defaultValue = "";
    auto it = m_parameters.find("vsDomain");
    return (it != m_parameters.end()) ? it->second : defaultValue;
}

const std::string &CameraParametersManager::getVsToken() const
{
    static std::string defaultValue = "";
    auto it = m_parameters.find("vsToken");
    return (it != m_parameters.end()) ? it->second : defaultValue;
}

const std::string &CameraParametersManager::getCameraType() const
{
    static std::string defaultValue = "IPCAM";
    auto it = m_parameters.find("cameraType");
    return (it != m_parameters.end()) ? it->second : defaultValue;
}

const std::string &CameraParametersManager::getModel() const
{
    static std::string defaultValue = "DefaultModel";
    auto it = m_parameters.find("model");
    return (it != m_parameters.end()) ? it->second : defaultValue;
}

const std::string &CameraParametersManager::getIsCheckHioss() const
{
    static std::string defaultValue = "0";
    auto it = m_parameters.find("isCheckHioss");
    return (it != m_parameters.end()) ? it->second : defaultValue;
}

const std::string &CameraParametersManager::getBrand() const
{
    static std::string defaultValue = "DefaultBrand";
    auto it = m_parameters.find("brand");
    return (it != m_parameters.end()) ? it->second : defaultValue;
}

// 參數設置函數
void CameraParametersManager::setCameraId(const std::string &cameraId)
{
    setParameter(PAYLOAD_KEY_CAMID, cameraId);
}

void CameraParametersManager::setCHTBarcode(const std::string &barcode)
{

    setParameter("chtBarcode", barcode);
}

void CameraParametersManager::setPublicIp(const std::string &ip)
{
    setParameter("publicIp", ip);
}

void CameraParametersManager::setCamSid(const std::string &camSid)
{
    setParameter("camSid", camSid);
}

void CameraParametersManager::setTenantId(const std::string &tenantId)
{
    setParameter("tenantId", tenantId);
}

void CameraParametersManager::setCameraName(const std::string &name)
{
    setParameter("cameraName", name);
}

void CameraParametersManager::setOsdRule(const std::string &osdRule)
{
    setParameter("osdRule", osdRule);
}

void CameraParametersManager::setAISettings(const std::string &aiSettings)
{
    setParameter("aiSettings", aiSettings);
}

void CameraParametersManager::setTimeZone(const std::string &timezone)
{
    setParameter("timezone", timezone);
}

// 新增 dataStorage 中的 setter 方法
void CameraParametersManager::setNetNo(const std::string &netNo)
{
    setParameter("netNo", netNo);
}

void CameraParametersManager::setVsDomain(const std::string &domain)
{
    setParameter("vsDomain", domain);
}

void CameraParametersManager::setVsToken(const std::string &token)
{
    setParameter("vsToken", token);
}

void CameraParametersManager::setActiveStatus(const std::string &status)
{
    setParameter("activeStatus", status);
}

void CameraParametersManager::setDeviceStatus(const std::string &status)
{
    setParameter("deviceStatus", status);
}

void CameraParametersManager::setCameraType(const std::string &type)
{
    setParameter("cameraType", type);
}

void CameraParametersManager::setModel(const std::string &model)
{
    setParameter("model", model);
}

void CameraParametersManager::setIsCheckHioss(bool value)
{
    setIsCheckHioss(value?"1":"0");
}

void CameraParametersManager::setIsCheckHioss(const std::string &value)
{
    setParameter("isCheckHioss", value);
}

void CameraParametersManager::setBrand(const std::string &brand)
{
    setParameter("brand", brand);
}

// 泛型參數操作
template <typename T>
T CameraParametersManager::getParameter(const std::string &key, const T &defaultValue) const
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto it = m_parameters.find(key);
    if (it == m_parameters.end())
    {
        return defaultValue;
    }

    std::istringstream iss(it->second);
    T value;
    if (iss >> value)
    {
        return value;
    }
    return defaultValue;
}

template <typename T>
void CameraParametersManager::setParameter(const std::string &key, const T &value)
{

    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    // 轉換值為字符串
    std::ostringstream oss;
    oss << value;
    std::string strValue = oss.str();

    // 檢查參數是否已存在且值相同
    auto it = m_parameters.find(key);
    bool changed = (it == m_parameters.end() || it->second != strValue);

    // 更新參數
    m_parameters[key] = strValue;
    m_parameterUpdateTimes[key] = std::chrono::system_clock::now();

    if (changed)
    {

        // 通知參數變更
        notifyParameterChanged(key, strValue);
    }
}

// 專用於字符串參數的getter
std::string CameraParametersManager::getParameter(const std::string &key, const std::string &defaultValue) const
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto it = m_parameters.find(key);
    return (it != m_parameters.end()) ? it->second : defaultValue;
}

// 實例化常用模板函數
// 實例化常用範本函數
template std::string CameraParametersManager::getParameter<std::string>(const std::string &, const std::string &) const;
template int CameraParametersManager::getParameter<int>(const std::string &, const int &) const;
template long CameraParametersManager::getParameter<long>(const std::string &, const long &) const;
template double CameraParametersManager::getParameter<double>(const std::string &, const double &) const;
template bool CameraParametersManager::getParameter<bool>(const std::string &, const bool &) const;

template void CameraParametersManager::setParameter<std::string>(const std::string &, const std::string &);
template void CameraParametersManager::setParameter<int>(const std::string &, const int &);
template void CameraParametersManager::setParameter<long>(const std::string &, const long &);
template void CameraParametersManager::setParameter<double>(const std::string &, const double &);
template void CameraParametersManager::setParameter<bool>(const std::string &, const bool &);

// Add char array type template instantiations
template void CameraParametersManager::setParameter<char[1]>(const std::string &, const char (&)[1]);
template void CameraParametersManager::setParameter<char[2]>(const std::string &, const char (&)[2]);
template void CameraParametersManager::setParameter<char[3]>(const std::string &, const char (&)[3]);
template void CameraParametersManager::setParameter<char[4]>(const std::string &, const char (&)[4]);
template void CameraParametersManager::setParameter<char[5]>(const std::string &, const char (&)[5]);
template void CameraParametersManager::setParameter<char[6]>(const std::string &, const char (&)[6]);
template void CameraParametersManager::setParameter<char[7]>(const std::string &, const char (&)[7]);
template void CameraParametersManager::setParameter<char[8]>(const std::string &, const char (&)[8]);
template void CameraParametersManager::setParameter<char[9]>(const std::string &, const char (&)[9]);
template void CameraParametersManager::setParameter<char[10]>(const std::string &, const char (&)[10]);
template void CameraParametersManager::setParameter<char[11]>(const std::string &, const char (&)[11]);
template void CameraParametersManager::setParameter<char[12]>(const std::string &, const char (&)[12]);
template void CameraParametersManager::setParameter<char[13]>(const std::string &, const char (&)[13]);
template void CameraParametersManager::setParameter<char[14]>(const std::string &, const char (&)[14]);
template void CameraParametersManager::setParameter<char[15]>(const std::string &, const char (&)[15]);
template void CameraParametersManager::setParameter<char[16]>(const std::string &, const char (&)[16]);
template void CameraParametersManager::setParameter<char[17]>(const std::string &, const char (&)[17]);
template void CameraParametersManager::setParameter<char[18]>(const std::string &, const char (&)[18]);
template void CameraParametersManager::setParameter<char[19]>(const std::string &, const char (&)[19]);
template void CameraParametersManager::setParameter<char[20]>(const std::string &, const char (&)[20]);
template void CameraParametersManager::setParameter<char[21]>(const std::string &, const char (&)[21]);
template void CameraParametersManager::setParameter<char[22]>(const std::string &, const char (&)[22]);
template void CameraParametersManager::setParameter<char[23]>(const std::string &, const char (&)[23]);
template void CameraParametersManager::setParameter<char[24]>(const std::string &, const char (&)[24]);

// Add const char* type template instantiation
template void CameraParametersManager::setParameter<const char *>(const std::string &, const char *const &);
// Check if parameter exists
bool CameraParametersManager::hasParameter(const std::string &key) const
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return m_parameters.find(key) != m_parameters.end();
}

// 刪除參數
bool CameraParametersManager::removeParameter(const std::string &key)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto it = m_parameters.find(key);
    if (it != m_parameters.end())
    {
        m_parameters.erase(it);
        auto timeIt = m_parameterUpdateTimes.find(key);
        if (timeIt != m_parameterUpdateTimes.end())
        {
            m_parameterUpdateTimes.erase(timeIt);
        }
        return true;
    }
    return false;
}

// 獲取全部參數
std::map<std::string, std::string> CameraParametersManager::getAllParameters() const
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return m_parameters;
}

// 參數變更回調
int CameraParametersManager::registerParameterChangeCallback(const std::string &key,
                                                             std::function<void(const std::string &, const std::string &)> callback)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    int callbackId = m_nextCallbackId++;
    CallbackInfo info = {callbackId, key, callback};
    m_callbacks.push_back(info);
    return callbackId;
}

bool CameraParametersManager::unregisterParameterChangeCallback(int callbackId)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    for (auto it = m_callbacks.begin(); it != m_callbacks.end(); ++it)
    {
        if (it->id == callbackId)
        {
            m_callbacks.erase(it);
            return true;
        }
    }
    return false;
}
#if 1
void CameraParametersManager::notifyParameterChanged(const std::string &key, const std::string &value)
{
#if 0
    //std::cout << "[DEBUG]notifyParameterChanged  LINE : " << __LINE__ << std::endl;
    // 暫時停用通知，避免死鎖
    std::cout << "notifyParameterChanged 參數變更通知已暫時停用，參數: " << key << " = " << value << std::endl;
    return;
#else
    // 複製回調列表，避免在回調過程中修改列表
    std::vector<CallbackInfo> callbacks;
    {

        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        for (const auto &callback : m_callbacks)
        {

            if (callback.key.empty() || callback.key == key)
            {

                callbacks.push_back(callback);
            }
        }
    } // 互斥鎖在這裡被釋放

    // 執行回調 - 此時沒有持有互斥鎖
    for (const auto &callback : callbacks)
    {

        try
        {

            callback.callback(key, value);
        }
        catch (const std::exception &e)
        {

            std::cerr << "執行參數變更回調異常: " << e.what() << std::endl;
        }
    }

#endif
}
#else
void CameraParametersManager::notifyParameterChanged(const std::string &key, const std::string &value)
{

    // 複製回調列表，避免在回調過程中修改列表
    std::vector<CallbackInfo> callbacks;
    {

        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        for (const auto &callback : m_callbacks)
        {

            if (callback.key.empty() || callback.key == key)
            {

                callbacks.push_back(callback);
            }
        }
    }

    // 執行回調
    for (const auto &callback : callbacks)
    {

        try
        {

            callback.callback(key, value);
        }
        catch (const std::exception &e)
        {
            std::cerr << "執行參數變更回調異常: " << e.what() << std::endl;
        }
    }
}
#endif
// 參數過期管理
std::chrono::system_clock::time_point CameraParametersManager::getParameterUpdateTime(const std::string &key) const
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto it = m_parameterUpdateTimes.find(key);
    if (it != m_parameterUpdateTimes.end())
    {
        return it->second;
    }
    return std::chrono::system_clock::time_point{}; // 返回epoch時間
}

bool CameraParametersManager::isParameterStale(const std::string &key, std::chrono::milliseconds maxAge) const
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto it = m_parameterUpdateTimes.find(key);
    if (it == m_parameterUpdateTimes.end())
    {
        return true; // 參數不存在，視為過期
    }

    auto now = std::chrono::system_clock::now();
    auto age = std::chrono::duration_cast<std::chrono::milliseconds>(now - it->second);

    return age > maxAge;
}

// 文件操作
bool CameraParametersManager::saveToFile(const std::string &configFilePath)
{
    std::string filePath = configFilePath.empty() ? m_configFilePath : configFilePath;

    // 確保目錄存在
    std::string dirPath = filePath.substr(0, filePath.find_last_of('/'));
    std::string mkdirCmd = "mkdir -p " + dirPath;
    int result = system(mkdirCmd.c_str());
    (void)result; // 忽略返回值

    rapidjson::Document document;
    document.SetObject();
    rapidjson::Document::AllocatorType &allocator = document.GetAllocator();

    // 鎖定互斥鎖，保護參數訪問
    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    // 添加所有參數
    for (const auto &param : m_parameters)
    {
        document.AddMember(rapidjson::Value(param.first.c_str(), allocator).Move(),
                           rapidjson::Value(param.second.c_str(), allocator).Move(),
                           allocator);
    }

    // 寫入文件
    std::ofstream ofs(filePath);
    if (!ofs.is_open())
    {
        std::cerr << "無法打開配置文件進行寫入: " << filePath << std::endl;
        // 嘗試使用備用路徑
        std::string backupPath = "./ipcam_params.json";
        std::ofstream backupOfs(backupPath);
        if (backupOfs.is_open())
        {
            rapidjson::StringBuffer buffer;
            rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
            document.Accept(writer);

            backupOfs << buffer.GetString();
            backupOfs.close();
            std::cout << "配置已保存到備用路徑: " << backupPath << std::endl;
            return true;
        }
        return false;
    }

    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    document.Accept(writer);

    ofs << buffer.GetString();
    ofs.close();

    std::cout << "配置已保存到: " << filePath << std::endl;
    return true;
}

bool CameraParametersManager::loadFromFile(const std::string &configFilePath)
{
    std::string filePath = configFilePath.empty() ? m_configFilePath : configFilePath;

    std::ifstream ifs(filePath);
    if (!ifs.is_open())
    {
        std::cerr << "無法打開配置文件進行讀取: " << filePath << std::endl;
        return false;
    }

    std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    ifs.close();

    rapidjson::Document document;
    rapidjson::ParseResult parseResult = document.Parse(content.c_str());
    if (parseResult.IsError())
    {
        std::cerr << "解析配置文件失敗: " << rapidjson::GetParseError_En(parseResult.Code()) << std::endl;
        return false;
    }

    // 鎖定互斥鎖，保護參數訪問
    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    // 讀取所有參數
    for (rapidjson::Value::ConstMemberIterator it = document.MemberBegin(); it != document.MemberEnd(); ++it)
    {
        if (it->name.IsString() && it->value.IsString())
        {
            std::string key = it->name.GetString();
            std::string value = it->value.GetString();
            m_parameters[key] = value;
        }
    }

    // 更新所有參數的更新時間
    auto now = std::chrono::system_clock::now();
    for (const auto &param : m_parameters)
    {
        m_parameterUpdateTimes[param.first] = now;
    }

    return true;
}
bool CameraParametersManager::parseAndSaveInitialInfo(const std::string &hamiCamInfo,
                                                      const std::string &hamiSettings,
                                                      const std::string &hamiAiSettings,
                                                      const std::string &hamiSystemSettings)
{
    std::cout << "CameraParametersManager: 開始解析完整初始化參數..." << std::endl;

    bool allSuccess = true;

    try
    {
        // 鎖定互斥鎖
        std::lock_guard<std::recursive_mutex> lock(m_mutex);

        // 解析各個部分
        if (!parseHamiCamInfo(hamiCamInfo))
        {
            std::cerr << "解析 hamiCamInfo 失敗" << std::endl;
            allSuccess = false;
        }

        if (!parseHamiSettings(hamiSettings))
        {
            std::cerr << "解析 hamiSettings 失敗" << std::endl;
            allSuccess = false;
        }

        if (!parseHamiAiSettings(hamiAiSettings))
        {
            std::cerr << "解析 hamiAiSettings 失敗" << std::endl;
            allSuccess = false;
        }

        if (!parseHamiSystemSettings(hamiSystemSettings))
        {
            std::cerr << "解析 hamiSystemSettings 失敗" << std::endl;
            allSuccess = false;
        }

        // 更新所有參數的時間戳
        auto now = std::chrono::system_clock::now();
        for (auto &pair : m_parameterUpdateTimes)
        {
            pair.second = now;
        }

        // 保存到檔案
        if (allSuccess)
        {
            saveToFile();
            std::cout << "CameraParametersManager: 完整初始化參數解析完成並已保存" << std::endl;
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << "CameraParametersManager: 解析初始化參數時發生異常: " << e.what() << std::endl;
        allSuccess = false;
    }

    return allSuccess;
}

bool CameraParametersManager::parseHamiCamInfo(const std::string &jsonStr)
{
    if (jsonStr.empty() || jsonStr == "{}")
    {
        std::cout << "hamiCamInfo 為空，跳過解析" << std::endl;
        return true;
    }

    rapidjson::Document doc;
    rapidjson::ParseResult parseResult = doc.Parse(jsonStr.c_str());
    if (parseResult.IsError())
    {
        std::cerr << "解析 hamiCamInfo JSON 失敗: " << rapidjson::GetParseError_En(parseResult.Code()) << std::endl;
        return false;
    }

    std::cout << "開始解析 hamiCamInfo 參數..." << std::endl;

    // 解析 camSid (int)
    if (doc.HasMember("camSid") && doc["camSid"].IsInt())
    {
        setCamSid(std::to_string(doc["camSid"].GetInt()));
        std::cout << "設定 camSid: " << doc["camSid"].GetInt() << std::endl;
    }

    // 解析 camId (string)
    if (doc.HasMember("camId") && doc["camId"].IsString())
    {
        setCameraId(doc["camId"].GetString());
        std::cout << "設定 camId: " << doc["camId"].GetString() << std::endl;
    }

    // 解析 chtBarcode (string)
    if (doc.HasMember("chtBarcode") && doc["chtBarcode"].IsString())
    {
        setCHTBarcode(doc["chtBarcode"].GetString());
        std::cout << "設定 chtBarcode: " << doc["chtBarcode"].GetString() << std::endl;
    }

    // 解析 tenantId (string)
    if (doc.HasMember("tenantId") && doc["tenantId"].IsString())
    {
        setTenantId(doc["tenantId"].GetString());
        std::cout << "設定 tenantId: " << doc["tenantId"].GetString() << std::endl;
    }

    // 解析 netNo (string)
    if (doc.HasMember("netNo") && doc["netNo"].IsString())
    {
        setNetNo(doc["netNo"].GetString());
        std::cout << "設定 netNo: " << doc["netNo"].GetString() << std::endl;
    }

    // 解析 userId (string)
    if (doc.HasMember("userId") && doc["userId"].IsString())
    {
        setUserId(doc["userId"].GetString());
        std::cout << "設定 userId: " << doc["userId"].GetString() << std::endl;
    }

    std::cout << "hamiCamInfo 解析完成" << std::endl;
    return true;
}

bool CameraParametersManager::parseHamiSettings(const std::string &jsonStr)
{
    if (jsonStr.empty() || jsonStr == "{}")
    {
        std::cout << "hamiSettings 為空，跳過解析" << std::endl;
        return true;
    }

    rapidjson::Document doc;
    rapidjson::ParseResult parseResult = doc.Parse(jsonStr.c_str());
    if (parseResult.IsError())
    {
        std::cerr << "解析 hamiSettings JSON 失敗: " << rapidjson::GetParseError_En(parseResult.Code()) << std::endl;
        return false;
    }

    std::cout << "開始解析 hamiSettings 參數..." << std::endl;

    // 字串參數
    const std::vector<std::string> stringParams = {
        "nightMode", "autoNightVision", "statusIndicatorLight", "isFlipUpDown",
        "isHd", "flicker", "imageQuality", "isMicrophone", "isSpeak",
        "scheduleOn", "ScheduleSun", "scheduleMon", "scheduleTue", "scheduleWed",
        "scheduleThu", "scheduleFri", "scheduleSat", "powerOn", "alertOn",
        "vmd", "ad", "lastPtzCommand", "ptzStatus", "ptzSpeed", "ptzTourStayTime",
        "humanTracking", "petTracking", "ptzTourSequence", "positionName1", "positionName2", "positionName3", "positionName4"};

    for (const auto &param : stringParams)
    {
        if (doc.HasMember(param.c_str()) && doc[param.c_str()].IsString())
        {
            setParameter(param, doc[param.c_str()].GetString());
            std::cout << "設定 " << param << ": " << doc[param.c_str()].GetString() << std::endl;
        }
    }

    // 整數參數
    const std::vector<std::string> intParams = {
        "microphoneSensitivity", "speakVolume", "storageDay", "eventStorageDay", "power"};

    for (const auto &param : intParams)
    {
        if (doc.HasMember(param.c_str()) && doc[param.c_str()].IsInt())
        {
            setParameter(param, std::to_string(doc[param.c_str()].GetInt()));
            std::cout << "設定 " << param << ": " << doc[param.c_str()].GetInt() << std::endl;
        }
    }

    std::cout << "hamiSettings 解析完成" << std::endl;
    return true;
}

static int char2int(int ch)
{
    // 空白與可被忽略的字元
    if (ch==' ' || ch=='\t' || ch=='\r' || ch=='\n') return -2;

    // 標準 Base64
    if (ch >= 'A' && ch <= 'Z') return ch - 'A';
    if (ch >= 'a' && ch <= 'z') return ch - 'a' + 26;
    if (ch >= '0' && ch <= '9') return ch - '0' + 52;
    if (ch == '+') return 62;
    if (ch == '/') return 63;

    // URL-safe 變體
    if (ch == '-') return 62;
    if (ch == '_') return 63;

    // padding 在外層處理
    return -1;
}

static bool decodeBase64(const std::string &s, std::vector<uint8_t> &out) {
    std::vector<uint8_t> tmp;
    tmp.reserve((s.size() * 3) / 4 + 4);

    int quartet[4];
    int qn = 0;
    bool seen_pad = false;

    auto push_quartet = [&](int q0, int q1, int q2, int q3, int pad_count)->bool {
        // q0..q3 is 0..63；pad_count is 0,1,2 for counting '='
        uint32_t v = (uint32_t(q0) << 18) | (uint32_t(q1) << 12)
                   | (uint32_t(q2) << 6)  |  uint32_t(q3);
        if (pad_count == 0) {
            tmp.push_back(uint8_t((v >> 16) & 0xFF));
            tmp.push_back(uint8_t((v >> 8)  & 0xFF));
            tmp.push_back(uint8_t(v & 0xFF));
        } else if (pad_count == 1) {
            tmp.push_back(uint8_t((v >> 16) & 0xFF));
            tmp.push_back(uint8_t((v >> 8)  & 0xFF));
        } else if (pad_count == 2) {
            tmp.push_back(uint8_t((v >> 16) & 0xFF));
        } else {
            return false;
        }
        return true;
    };

    for (uint32_t i = 0; i < s.size(); ++i)
    {
        uint8_t ch = static_cast<uint8_t>(s[i]);

        if (ch == '=')
        {
            if (qn < 2) return false;
            int pad_count = 1;
            uint32_t j = i + 1;
            bool second_eq = false;
            while (j < s.size()) {
                if (s[j] == '=') { second_eq = true; ++j; break; }
                int m = char2int(static_cast<uint8_t>(s[j]));
                if (m == -2) { ++j; continue; }
                break;
            }
            if (qn == 2) {
                if (!second_eq) return false;
                quartet[2] = 0;
                quartet[3] = 0;
                if (!push_quartet(quartet[0], quartet[1], quartet[2], quartet[3], 2)) return false;
            } else if (qn == 3) {
                quartet[3] = 0;
                if (!push_quartet(quartet[0], quartet[1], quartet[2], quartet[3], 1)) return false;
            } else {
                return false;
            }
            seen_pad = true;

            for (uint32_t k = j; k < s.size(); ++k) {
                uint8_t c2 = static_cast<uint8_t>(s[k]);
                if (c2==' ' || c2=='\t' || c2=='\r' || c2=='\n') continue;
                return false;
            }
            out.swap(tmp);
            return true;
        }

        int m = char2int(ch);
        if (m == -2) continue;   // 跳過空白
        if (m < 0) return false; // 非法字元
        if (seen_pad) return false; // padding 後不允許再有有效資料

        quartet[qn++] = m;
        if (qn == 4) {
            if (!push_quartet(quartet[0], quartet[1], quartet[2], quartet[3], 0)) return false;
            qn = 0;
        }
    }

    if (qn == 0) {
        out.swap(tmp);
        return true;
    }

    return false;
}

bool CameraParametersManager::parseHamiAiSettings(const std::string &jsonStr)
{
    if (jsonStr.empty() || jsonStr == "{}")
    {
        std::cout << "hamiAiSettings 為空，跳過解析" << std::endl;
        return true;
    }

    try {
        rapidjson::Document doc;
        rapidjson::ParseResult parseResult = doc.Parse(jsonStr.c_str());
        if (parseResult.IsError() || !doc.IsObject())
        {
            std::cerr << "解析 hamiAiSettings JSON 失敗: " << rapidjson::GetParseError_En(parseResult.Code()) << std::endl;
            throw std::runtime_error("JSON格式錯誤");
        }

        std::cout << "開始解析 hamiAiSettings 參數..." << std::endl;

        // 將完整的 AI 設定儲存
        setAISettings(jsonStr);

        // 字串參數
        static const std::vector<std::string> stringParams = {
            "vmdAlert", "humanAlert", "petAlert", "adAlert", "fenceAlert",
            "faceAlert", "fallAlert", "adBabyCryAlert", "adSpeechAlert",
            "adAlarmAlert", "adDogAlert", "adCatAlert", "fenceDir"
        };
        for (const auto &param : stringParams)
        {
            std::string key = param;
            auto it = doc.FindMember(key.c_str());
            if (it == doc.MemberEnd() || !it->value.IsString()) continue;

            setParameter(key, it->value.GetString());
            std::cout << "設定 " << key << ": " << it->value.GetString() << std::endl;
        }

        // 整數參數
        static const std::vector<std::string> intParams = {
            "vmdSen", "adSen", "humanSen", "faceSen", "fenceSen", "petSen",
            "adBabyCrySen", "adSpeechSen", "adAlarmSen", "adDogSen",
            "adCatSen", "fallSen", "fallTime"
        };
        for (const auto &param : intParams)
        {
            std::string key = param;
            auto it = doc.FindMember(key.c_str());
            if (it == doc.MemberEnd() || !it->value.IsInt()) continue;

            setParameter(key, it->value.GetInt());
            std::cout << "設定 " << key << ": " << it->value.GetInt() << std::endl;
        }

        // 解析電子圍籬座標
        static const std::vector<std::string> fencePositions = {
            "fencePos1", "fencePos2", "fencePos3", "fencePos4"
        };
        for (const auto &param : fencePositions)
        {
            std::string key = param;
            auto it = doc.FindMember(key.c_str());
            if (it == doc.MemberEnd() || !it->value.IsObject()) continue;

            const rapidjson::Value &posObj = it->value;
            auto x_it = posObj.FindMember("x");
            auto y_it = posObj.FindMember("y");
            if ( (x_it == posObj.MemberEnd() || !x_it->value.IsInt()) ||
                 (y_it == posObj.MemberEnd() || !y_it->value.IsInt()) ) continue;

            setParameter(param + "_x", std::to_string(x_it->value.GetInt()));
            setParameter(param + "_y", std::to_string(y_it->value.GetInt()));
            std::cout << "設定 " << param << ": x=" << x_it->value.GetInt()
                                         << ", y=" << y_it->value.GetInt() << std::endl;
        }

        // 解析人臉識別特徵陣列
        updateIdentificationFeature(jsonStr);

        std::cout << "hamiAiSettings 解析完成" << std::endl;
        return true;
    }
    catch (const std::exception &e)
    {
        std::cerr << "更新AI設定時發生異常: " << e.what() << std::endl;
        return false;
    }
}

bool CameraParametersManager::parseHamiSystemSettings(const std::string &jsonStr)
{
    if (jsonStr.empty() || jsonStr == "{}")
    {
        std::cout << "hamiSystemSettings 為空，跳過解析" << std::endl;
        return true;
    }

    rapidjson::Document doc;
    rapidjson::ParseResult parseResult = doc.Parse(jsonStr.c_str());
    if (parseResult.IsError())
    {
        std::cerr << "解析 hamiSystemSettings JSON 失敗: " << rapidjson::GetParseError_En(parseResult.Code()) << std::endl;
        return false;
    }

    std::cout << "開始解析 hamiSystemSettings 參數..." << std::endl;

    // 字串參數
    const std::vector<std::string> stringParams = {
        "otaDomainName", "ntpServer", "bucketName"};

    for (const auto &param : stringParams)
    {
        if (doc.HasMember(param.c_str()) && doc[param.c_str()].IsString())
        {
            setParameter(param, doc[param.c_str()].GetString());
            std::cout << "設定 " << param << ": " << doc[param.c_str()].GetString() << std::endl;
        }
    }

    // 整數參數
    if (doc.HasMember("otaQueryInterval") && doc["otaQueryInterval"].IsInt())
    {
        setParameter("otaQueryInterval", std::to_string(doc["otaQueryInterval"].GetInt()));
        std::cout << "設定 otaQueryInterval: " << doc["otaQueryInterval"].GetInt() << std::endl;
    }

    // ===== 重點：處理 NTP 伺服器設定 =====
    if (doc.HasMember("ntpServer") && doc["ntpServer"].IsString())
    {
        std::string newNtpServer = doc["ntpServer"].GetString();
        std::string currentNtpServer = getNtpServer();

        std::cout << "從 hamiSystemSettings 獲取 NTP 伺服器: " << newNtpServer << std::endl;
        std::cout << "當前 NTP 伺服器: " << currentNtpServer << std::endl;

        // 更新 NTP 伺服器設定
        setNtpServer(newNtpServer);

        // 如果 NTP 伺服器有變化，立即嘗試同步時間
        if (newNtpServer != currentNtpServer && !newNtpServer.empty())
        {
            std::cout << "NTP 伺服器已變更，嘗試立即同步時間..." << std::endl;

            bool syncResult = syncTimeWithNtp(newNtpServer);
            if (syncResult)
            {
                std::cout << "✓ NTP 時間同步成功" << std::endl;
            }
            else
            {
                std::cout << "⚠ NTP 時間同步失敗（網路問題或伺服器不可達）" << std::endl;
            }
        }
    }

    std::cout << "hamiSystemSettings 解析完成" << std::endl;
    return true;
}
// ===== hamiSettings 相關 getter 函數實現 =====

std::string CameraParametersManager::getNightMode() const
{
    return getParameter("nightMode", "0");
}

std::string CameraParametersManager::getAutoNightVision() const
{
    return getParameter("autoNightVision", "0");
}

std::string CameraParametersManager::getStatusIndicatorLight() const
{
    return getParameter("statusIndicatorLight", "1");
}

std::string CameraParametersManager::getIsFlipUpDown() const
{
    return getParameter("isFlipUpDown", "0");
}

std::string CameraParametersManager::getFlicker() const
{
    return getParameter("flicker", "1");
}

std::string CameraParametersManager::getImageQualityStr() const
{
    return getParameter("imageQuality", "2");
}

std::string CameraParametersManager::getIsMicrophone() const
{
    return getParameter("isMicrophone", "1");
}

int CameraParametersManager::getMicrophoneSensitivity() const
{
    return std::stoi(getParameter("microphoneSensitivity", "5"));
}

std::string CameraParametersManager::getIsSpeak() const
{
    return getParameter("isSpeak", "1");
}

int CameraParametersManager::getSpeakVolume() const
{
    return std::stoi(getParameter("speakVolume", "50"));
}

int CameraParametersManager::getStorageDay() const
{
    return std::stoi(getParameter("storageDay", "7"));
}

std::string CameraParametersManager::getScheduleOn() const
{
    return getParameter("scheduleOn", "0");
}

std::string CameraParametersManager::getScheduleSun() const
{
    return getParameter("ScheduleSun", "0000-2359");
}

std::string CameraParametersManager::getScheduleMon() const
{
    return getParameter("scheduleMon", "0840-1730");
}

std::string CameraParametersManager::getScheduleTue() const
{
    return getParameter("scheduleTue", "0840-1730");
}

std::string CameraParametersManager::getScheduleWed() const
{
    return getParameter("scheduleWed", "0840-1730");
}

std::string CameraParametersManager::getScheduleThu() const
{
    return getParameter("scheduleThu", "0840-1730");
}

std::string CameraParametersManager::getScheduleFri() const
{
    return getParameter("scheduleFri", "0840-1730");
}

std::string CameraParametersManager::getScheduleSat() const
{
    return getParameter("scheduleSat", "0000-2359");
}

int CameraParametersManager::getEventStorageDay() const
{
    return std::stoi(getParameter("eventStorageDay", "14"));
}

std::string CameraParametersManager::getPowerOn() const
{
    return getParameter("powerOn", "1");
}

std::string CameraParametersManager::getAlertOn() const
{
    return getParameter("alertOn", "1");
}

std::string CameraParametersManager::getVmd() const
{
    return getParameter("vmd", "1");
}

std::string CameraParametersManager::getAd() const
{
    return getParameter("ad", "1");
}

int CameraParametersManager::getPower() const
{
    return std::stoi(getParameter("power", "100"));
}

std::string CameraParametersManager::getLastPtzCommand() const
{
    return getParameter("lastPtzCommand", "stop");
}

std::string CameraParametersManager::getPtzStatus() const
{
    return getParameter("ptzStatus", "0");
}

std::string CameraParametersManager::getPtzSpeed() const
{
    return getParameter("ptzSpeed", "1");
}

std::string CameraParametersManager::getPtzTourStayTime() const
{
    return getParameter("ptzTourStayTime", "3");
}

int CameraParametersManager::getHumanTracking() const
{
    return std::stoi(getParameter("humanTracking", "0")); // 0: close, 1: back to home, 2: stay
}

int CameraParametersManager::getPetTracking() const
{
    return std::stoi(getParameter("petTracking", "0")); // 0: close, 1: back to home, 2: stay
}

std::string CameraParametersManager::getPtzTourSequence() const
{
    return getParameter("ptzTourSequence", "1,2,3,4");
}

std::string CameraParametersManager::getPositionName1() const
{
    return getParameter("positionName1", "測試點1");
}
std::string CameraParametersManager::getPositionName2() const
{
    return getParameter("positionName2", "測試點2");
}
std::string CameraParametersManager::getPositionName3() const
{
    return getParameter("positionName3", "測試點3");
}
std::string CameraParametersManager::getPositionName4() const
{
    return getParameter("positionName4", "測試點4");
}

// ===== hamiAiSettings 相關 getter 函數實現 =====
bool CameraParametersManager::getVmdAlert() const
{
    int value = std::stoi(getParameter("vmdAlert", "1"));
    return (value != 0);
}

bool CameraParametersManager::getHumanAlert() const
{
    int value = std::stoi(getParameter("humanAlert", "1"));
    return (value != 0);
}

bool CameraParametersManager::getPetAlert() const
{
    int value = std::stoi(getParameter("petAlert", "1"));
    return (value != 0);
}

bool CameraParametersManager::getAdAlert() const
{
    int value = std::stoi(getParameter("adAlert", "1"));
    return (value != 0);
}

bool CameraParametersManager::getFenceAlert() const
{
    int value = std::stoi(getParameter("fenceAlert", "1"));
    return (value != 0);
}

bool CameraParametersManager::getFaceAlert() const
{
    int value = std::stoi(getParameter("faceAlert", "1"));
    return (value != 0);
}

bool CameraParametersManager::getFallAlert() const
{
    int value = std::stoi(getParameter("fallAlert", "1"));
    return (value != 0);
}

bool CameraParametersManager::getAdBabyCryAlert() const
{
    int value = std::stoi(getParameter("adBabyCryAlert", "1"));
    return (value != 0);
}

bool CameraParametersManager::getAdSpeechAlert() const
{
    int value = std::stoi(getParameter("adSpeechAlert", "1"));
    return (value != 0);
}

bool CameraParametersManager::getAdAlarmAlert() const
{
    int value = std::stoi(getParameter("adAlarmAlert", "1"));
    return (value != 0);
}

bool CameraParametersManager::getAdDogAlert() const
{
    int value = std::stoi(getParameter("adDogAlert", "1"));
    return (value != 0);
}

bool CameraParametersManager::getAdCatAlert() const
{
    int value = std::stoi(getParameter("adCatAlert", "1"));
    return (value != 0);
}

int CameraParametersManager::getVmdSen() const
{
    return std::stoi(getParameter("vmdSen", "1"));
}

int CameraParametersManager::getAdSen() const
{
    return std::stoi(getParameter("adSen", "1"));
}

int CameraParametersManager::getHumanSen() const
{
    return std::stoi(getParameter("humanSen", "1"));
}

int CameraParametersManager::getFaceSen() const
{
    return std::stoi(getParameter("faceSen", "1"));
}

int CameraParametersManager::getFenceSen() const
{
    return std::stoi(getParameter("fenceSen", "1"));
}

int CameraParametersManager::getPetSen() const
{
    return std::stoi(getParameter("petSen", "1"));
}

int CameraParametersManager::getAdBabyCrySen() const
{
    return std::stoi(getParameter("adBabyCrySen", "1"));
}

int CameraParametersManager::getAdSpeechSen() const
{
    return std::stoi(getParameter("adSpeechSen", "1"));
}

int CameraParametersManager::getAdAlarmSen() const
{
    return std::stoi(getParameter("adAlarmSen", "1"));
}

int CameraParametersManager::getAdDogSen() const
{
    return std::stoi(getParameter("adDogSen", "1"));
}

int CameraParametersManager::getAdCatSen() const
{
    return std::stoi(getParameter("adCatSen", "1"));
}

int CameraParametersManager::getFallSen() const
{
    return std::stoi(getParameter("fallSen", "1"));
}

int CameraParametersManager::getFallTime() const
{
    return std::stoi(getParameter("fallTime", "1"));
}

std::string CameraParametersManager::getFenceDir() const
{
    return getParameter("fenceDir", "1");
}

// ===== 電子圍籬座標相關函數實現 =====

std::pair<int, int> CameraParametersManager::getFencePos1() const
{
    int x = std::stoi(getParameter("fencePos1_x", "10"));
    int y = std::stoi(getParameter("fencePos1_y", "10"));
    return std::make_pair(x, y);
}

std::pair<int, int> CameraParametersManager::getFencePos2() const
{
    int x = std::stoi(getParameter("fencePos2_x", "10"));
    int y = std::stoi(getParameter("fencePos2_y", "90"));
    return std::make_pair(x, y);
}

std::pair<int, int> CameraParametersManager::getFencePos3() const
{
    int x = std::stoi(getParameter("fencePos3_x", "90"));
    int y = std::stoi(getParameter("fencePos3_y", "90"));
    return std::make_pair(x, y);
}

std::pair<int, int> CameraParametersManager::getFencePos4() const
{
    int x = std::stoi(getParameter("fencePos4_x", "90"));
    int y = std::stoi(getParameter("fencePos4_y", "10"));
    return std::make_pair(x, y);
}

// ===== hamiSystemSettings 相關 getter 函數實現 =====

std::string CameraParametersManager::getOtaDomainName() const
{
    return getParameter("otaDomainName", "ota.example.com");
}

int CameraParametersManager::getOtaQueryInterval() const
{
    return std::stoi(getParameter("otaQueryInterval", "3600"));
}

std::string CameraParametersManager::getBucketName() const
{
    return getParameter("bucketName", "default-bucket");
}

// ===== 人臉識別特徵相關函數實現 =====

std::vector<CameraParametersManager::IdentificationFeature> CameraParametersManager::getIdentificationFeatures() const
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return m_identificationFeatures;
}

bool CameraParametersManager::addIdentificationFeature(const IdentificationFeature &feature)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    // 檢查ID是否已存在
    for (const auto &existing : m_identificationFeatures)
    {
        if (existing.id == feature.id)
        {
            std::cerr << "人臉特徵ID已存在: " << feature.id << std::endl;
            return false;
        }
    }

    // 檢查是否超過最大數量限制（20筆）
    if (m_identificationFeatures.size() >= 20)
    {
        std::cerr << "人臉特徵數量已達上限（20筆）" << std::endl;
        return false;
    }

    m_identificationFeatures.push_back(feature);
    std::cout << "新增人臉特徵成功: ID=" << feature.id << ", 姓名=" << feature.name << std::endl;

    // 通知參數變更
    notifyParameterChanged("identificationFeatures", "added:" + feature.id);

    return true;
}

bool CameraParametersManager::removeIdentificationFeature(const std::string &id)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    auto it = std::find_if(m_identificationFeatures.begin(), m_identificationFeatures.end(),
                           [&id](const IdentificationFeature &feature)
                           {
                               return feature.id == id;
                           });

    if (it != m_identificationFeatures.end())
    {
        std::cout << "移除人臉特徵: ID=" << it->id << ", 姓名=" << it->name << std::endl;
        m_identificationFeatures.erase(it);

        // 通知參數變更
        notifyParameterChanged("identificationFeatures", "removed:" + id);

        return true;
    }

    std::cerr << "找不到指定的人臉特徵ID: " << id << std::endl;
    return false;
}

bool CameraParametersManager::updateIdentificationFeature(const std::string &id, const IdentificationFeature &feature)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    auto it = std::find_if(m_identificationFeatures.begin(), m_identificationFeatures.end(),
                           [&id](const IdentificationFeature &existing)
                           {
                               return existing.id == id;
                           });

    if (it != m_identificationFeatures.end())
    {
        std::cout << "更新人臉特徵: ID=" << id << std::endl;
        *it = feature;

        // 通知參數變更
        notifyParameterChanged("identificationFeatures", "updated:" + id);

        return true;
    }

    std::cerr << "找不到指定的人臉特徵ID: " << id << std::endl;
    return false;
}

static bool ensureDir(const std::string &folder)
{
    pid_t pid = fork();
    if (pid < 0) return false;
    if (pid == 0) {
        // child: exec mkdir -p -- path
        execlp("mkdir", "mkdir", "-p", "--", folder.c_str(), (char *)nullptr);
        _exit(127); // exec fail
    }

    int status = 0;
    if (waitpid(pid, &status, 0) < 0) return false;
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

static bool moveSaveDir(const std::string &dstFolder, const std::string &srcFolder)
{
    pid_t pid = fork();
    if (pid < 0) return false;
    if (pid == 0) {
        // child: exec mv -- folder1 folder 2
        execlp("mv", "mv", "--", srcFolder.c_str(), dstFolder.c_str(), (char *)nullptr);
        _exit(127); // exec fail
    }

    int status = 0;
    if (waitpid(pid, &status, 0) < 0) return false;
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

static bool removeTmpDir(const std::string &folder)
{
    pid_t pid = fork();
    if (pid < 0) return false;
    if (pid == 0) {
        // child: exec rm -rf -- path
        execlp("rm", "rm", "-rf", "--", folder.c_str(), (char *)nullptr);
        _exit(127); // exec fail
    }

    int status = 0;
    if (waitpid(pid, &status, 0) < 0) return false;
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

bool CameraParametersManager::updateIdentificationFeature(const std::string& aiSettingJson)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    bool result = true;

    static constexpr char kSaveDir[] = "/mnt/model/matrixs";
    static constexpr char kTmpSaveDir[] = "/tmp/matrixs";
    ensureDir(kTmpSaveDir);
    ensureDir(kSaveDir);

    try
    {
        rapidjson::Document doc;
        rapidjson::ParseResult pr = doc.Parse(aiSettingJson.c_str());
        if (pr.IsError() || !doc.IsObject())
        {
            std::cerr << "Parse json string failed: " << rapidjson::GetParseError_En(pr.Code()) << std::endl;
            throw std::runtime_error("The string \"aiSettingJson\" is not JSON format");
        }

        // update identificationFeatures
        auto idFeatures_it = doc.FindMember(PAYLOAD_KEY_IDENTIFICATION_FEATURES);
        if (idFeatures_it == doc.MemberEnd() || !idFeatures_it->value.IsArray()) {
            throw std::runtime_error(std::string("Lost the item: ") + std::string(PAYLOAD_KEY_IDENTIFICATION_FEATURES));
        }
        const rapidjson::Value& featureObjs = idFeatures_it->value;
        std::cout << "解析人臉識別特徵，共 " << featureObjs.Size() << " 筆資料" << std::endl;

        std::vector<IdentificationFeature> newFeatures;
        newFeatures.reserve(20);

        for (rapidjson::SizeType i = 0; i < featureObjs.Size(); i++)
        {
            const rapidjson::Value &feature = featureObjs[i];
            if (!feature.IsObject()) continue;

            IdentificationFeature idFeature;
            // find all member. if not, don't push into vector
            // id
            auto member_it = feature.FindMember(PAYLOAD_KEY_ID);
            if (member_it == feature.MemberEnd() || !member_it->value.IsInt()) continue;
            idFeature.id = std::to_string(member_it->value.GetInt());

            // name
            member_it = feature.FindMember(PAYLOAD_KEY_NAME);
            if (member_it == feature.MemberEnd() || !member_it->value.IsString()) continue;
            idFeature.name = member_it->value.GetString();

            // verifyLevel
            member_it = feature.FindMember(PAYLOAD_KEY_VERIFY_LEVEL);
            if (member_it == feature.MemberEnd() || !member_it->value.IsInt()) continue;
            idFeature.verifyLevel = member_it->value.GetInt();

            // createTime
            member_it = feature.FindMember(PAYLOAD_KEY_CREATE_TIME);
            if (member_it == feature.MemberEnd() || !member_it->value.IsString()) continue;
            idFeature.createTime = member_it->value.GetString();

            // updateTime
            member_it = feature.FindMember(PAYLOAD_KEY_UPDATE_TIME);
            if (member_it == feature.MemberEnd() || !member_it->value.IsString()) continue;
            idFeature.updateTime = member_it->value.GetString();

            // faceFeatures (assume format is base64, 512 float, 2048 bytes)
            member_it = feature.FindMember(PAYLOAD_KEY_FACE_FEATURES);
            if (member_it == feature.MemberEnd() || !member_it->value.IsString()) continue;
            idFeature.faceFeatures = member_it->value.GetString();

            // decode base64 into 512 float
            std::vector<uint8_t> bytes;
            bool hasFeatures = decodeBase64(idFeature.faceFeatures, bytes);
            if (hasFeatures && bytes.size() != 2048) continue;
            do {
                // sanitize filename
                auto sanitize = [](std::string s) {
                    for (char& c : s) {
                        if (c == '/' || c == '\\') c = '_';
                    }
                    for (std::size_t pos = 0; (pos = s.find("..", pos)) != std::string::npos; ) {
                        s.replace(pos, 2, "_");
                        pos += 1;
                    }
                    if (!s.empty() && s.front() == '.') s.front() = '_';
                    if (!s.empty() && s.back() == '.') s.back() = '_';
                    if (s.empty()) s = "unnamed";
                    return s;
                };
                idFeature.id = sanitize(idFeature.id);
                idFeature.name = sanitize(idFeature.name);

                // save feature
                std::string filename = idFeature.id + "_" + idFeature.name + "_" +
                                    std::to_string(idFeature.verifyLevel) + ".fea";
                std::string path = std::string(kTmpSaveDir) + "/" + filename;
                std::cout << path << " " << bytes.size() << std::endl;
                std::ofstream f(path.c_str(), std::ios::binary | std::ios::trunc);
                if (!f) { hasFeatures = false; break; }

                f.write(reinterpret_cast<const char *>(bytes.data()),
                        static_cast<std::streamsize>(bytes.size()));
                if (!f) { hasFeatures = false; break; }
            } while (false);

            if (!hasFeatures) continue;

            newFeatures.push_back(idFeature);
            std::cout << "新增人臉特徵 ID: " << idFeature.id <<
                         ", 姓名: " << idFeature.name << std::endl;
        }

        if (newFeatures.size())
        {
            // remove old face feature
            removeTmpDir(kSaveDir);
            moveSaveDir(kSaveDir, kTmpSaveDir);
            m_identificationFeatures.swap(newFeatures);
        }

        result = true;
    }
    catch (const std::exception &e)
    {
        std::cerr << "updateIdentificationFeature error: " << e.what() << std::endl;
        result = false;
    }


    // finally remove tmp folder
    removeTmpDir(kTmpSaveDir);
    return result;
}

// ===== 新增 userId 相關方法 =====

void CameraParametersManager::setUserId(const std::string &userId)
{
    setParameter("userId", userId);
}

std::string CameraParametersManager::getUserId() const
{
    return getParameter("userId", "");
}

void CameraParametersManager::setRequestId(const std::string& requestId)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    setParameter("requestId", requestId);
}

std::string CameraParametersManager::getRequestId() const
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    return getParameter("requestId", "");
}

void CameraParametersManager::setIsHd(const std::string& isHd)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    setParameter("isHd", isHd);
}

std::string CameraParametersManager::getIsHd() const
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    return getParameter("isHd", "0");
}

void CameraParametersManager::setImageQuality(const std::string& imageQuality)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    setParameter("imageQuality", imageQuality);
}

std::string CameraParametersManager::getImageQuality() const
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    return getParameter("imageQuality", "0");
}

// 實現解析並儲存初始化資訊後同步硬體
bool CameraParametersManager::parseAndSaveInitialInfoWithSync(const std::string &hamiCamInfo,
                                                              const std::string &hamiSettings,
                                                              const std::string &hamiAiSettings,
                                                              const std::string &hamiSystemSettings)
{
    std::cout << "CameraParametersManager::parseAndSaveInitialInfoWithSync - 開始處理" << std::endl;

    try
    {
        // 1. 先解析並儲存所有參數
        bool parseResult = parseAndSaveInitialInfo(hamiCamInfo, hamiSettings, hamiAiSettings, hamiSystemSettings);
        if (!parseResult)
        {
            std::cerr << "解析初始化資訊失敗" << std::endl;
            return false;
        }

        std::cout << "初始化資訊解析成功" << std::endl;

        // 2. 驗證關鍵參數
        if (!validateParameter("camId", getCameraId()) ||
            !validateParameter("activeStatus", getActiveStatus()))
        {
            std::cerr << "關鍵參數驗證失敗" << std::endl;
            return false;
        }

        // 3. 儲存到檔案
        bool saveResult = saveToFile();
        if (!saveResult)
        {
            std::cerr << "儲存參數到檔案失敗" << std::endl;
            return false;
        }

        std::cout << "參數儲存成功" << std::endl;

        // 5. 再次儲存（包含同步後的硬體參數）
        saveToFile();

        addDebugLog("GetHamiCamInitialInfo 參數處理完成，硬體已同步");

        return true;
    }
    catch (const std::exception &e)
    {
        std::cerr << "parseAndSaveInitialInfoWithSync 異常: " << e.what() << std::endl;
        return false;
    }
}

bool CameraParametersManager::validateParameter(const std::string &key, const std::string &value)
{
    if (key == "camId")
    {
        return !value.empty() && value.length() >= 10;
    }
    else if (key == "activeStatus")
    {
        return value == "0" || value == "1";
    }
    else if (key == "deviceStatus")
    {
        return value == "0" || value == "1";
    }
    else if (key == "timezone")
    {
        try
        {
            int tz = std::stoi(value);
            return tz >= 0 && tz <= 51;
        }
        catch (...)
        {
            return false;
        }
    }

    return true; // 其他參數預設為有效
}

void CameraParametersManager::addDebugLog(const std::string &message, bool logToFile)
{
    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&now_time_t), "%Y-%m-%d %H:%M:%S");

    std::string logEntry = "[" + ss.str() + "] PARAMS: " + message;
    std::cout << logEntry << std::endl;

    if (logToFile)
    {
        try
        {
            std::ofstream logFile("/tmp/cht_params_debug.log", std::ios::app);
            if (logFile.is_open())
            {
                logFile << logEntry << std::endl;
                logFile.close();
            }
        }
        catch (...)
        {
            // 忽略日誌寫入錯誤
        }
    }
}

// ===== 系統檔案讀取方法實作 =====

// 從 /etc/config/hami_uid 讀取 userId
std::string CameraParametersManager::loadUserIdFromHamiUidFile()
{
    const std::string hamiUidPath = "/etc/config/hami_uid";

    std::cout << "嘗試從 " << hamiUidPath << " 讀取 userId..." << std::endl;

    std::ifstream file(hamiUidPath);
    if (!file.is_open())
    {
        std::cerr << "錯誤: 無法開啟 " << hamiUidPath << " 檔案" << std::endl;
        std::cerr << "請確認檔案存在且有讀取權限" << std::endl;
        return "";
    }

    std::string userId;
    if (std::getline(file, userId))
    {
        // 移除字串前後的空白字元
        userId.erase(0, userId.find_first_not_of(" \t\r\n"));
        userId.erase(userId.find_last_not_of(" \t\r\n") + 1);

        if (!userId.empty())
        {
            std::cout << "成功從 hami_uid 讀取到 userId: " << userId << std::endl;
            file.close();
            return userId;
        }
        else
        {
            std::cerr << "錯誤: " << hamiUidPath << " 檔案內容為空" << std::endl;
        }
    }
    else
    {
        std::cerr << "錯誤: 無法從 " << hamiUidPath << " 讀取內容" << std::endl;
    }

    file.close();
    return "";
}

// 從 /etc/config/wpa_supplicant.conf 讀取 WiFi 資訊
bool CameraParametersManager::loadWifiInfoFromSupplicantFile(std::string &wifiSsid, std::string &wifiPassword)
{
    const std::string supplicantPath = "/etc/config/wpa_supplicant.conf";

    std::cout << "嘗試從 " << supplicantPath << " 讀取 WiFi 資訊..." << std::endl;

    std::ifstream file(supplicantPath);
    if (!file.is_open())
    {
        std::cerr << "錯誤: 無法開啟 " << supplicantPath << " 檔案" << std::endl;
        return false;
    }

    std::string line;
    bool inNetworkBlock = false;
    std::string ssid, psk;

    while (std::getline(file, line))
    {
        // 移除前後空白
        line.erase(0, line.find_first_not_of(" \t"));
        line.erase(line.find_last_not_of(" \t") + 1);

        // 檢查是否進入 network 區塊
        if (line == "network={")
        {
            inNetworkBlock = true;
            continue;
        }

        // 檢查是否離開 network 區塊
        if (line == "}" && inNetworkBlock)
        {
            inNetworkBlock = false;
            // 如果已經找到了 ssid 和 psk，就可以結束了
            if (!ssid.empty() && !psk.empty())
            {
                break;
            }
        }

        // 只在 network 區塊內處理
        if (inNetworkBlock)
        {
            // 解析 ssid
            if (line.find("ssid=") == 0)
            {
                size_t start = line.find("\"");
                size_t end = line.rfind("\"");
                if (start != std::string::npos && end != std::string::npos && start < end)
                {
                    ssid = line.substr(start + 1, end - start - 1);
                }
            }
            // 解析 psk (password)
            else if (line.find("psk=") == 0)
            {
                size_t start = line.find("\"");
                size_t end = line.rfind("\"");
                if (start != std::string::npos && end != std::string::npos && start < end)
                {
                    psk = line.substr(start + 1, end - start - 1);
                }
            }
        }
    }

    file.close();

    if (!ssid.empty() && !psk.empty())
    {
        wifiSsid = ssid;
        wifiPassword = psk;
        std::cout << "成功從 wpa_supplicant.conf 解析 WiFi 資訊:" << std::endl;
        std::cout << "  SSID: " << wifiSsid << std::endl;
        std::cout << "  Password: " << wifiPassword << std::endl;
        return true;
    }
    else
    {
        std::cerr << "錯誤: 無法從 " << supplicantPath << " 解析完整的 WiFi 資訊" << std::endl;
        std::cerr << "  解析到的 SSID: " << ssid << std::endl;
        std::cerr << "  解析到的 PSK: " << psk << std::endl;
        return false;
    }
}
