/**
 * @file timezone_utils.cpp
 * @brief 時區工具類別實作
 * @date 2025/01/10
 */

#include "timezone_utils.h"
#include <iostream>
#include <algorithm>
#include <iomanip>
#include <sstream>
#include "camera_parameters_manager.h" // 需要用來獲取當前時區設定
#include "cht_p2p_agent_payload_defined.h"
#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>

std::map<std::string, std::string> TimezoneUtils::createTimezoneMap()
{
    std::map<std::string, std::string> timezoneMap;

    // 根據 CHT P2P 規格建立時區映射表
    // 格式：時區ID -> 時區字串
    timezoneMap["0"] = "WAT-1";    // (GMT+01) Casablanca
    timezoneMap["1"] = "GMT0";     // (GMT) Greenwich Mean Time: London
    timezoneMap["2"] = "CET-1";    // (GMT+01) Amsterdam, Berlin, Rome, Vienna
    timezoneMap["3"] = "EET-2";    // (GMT+02) Athens, Istanbul, Minsk
    timezoneMap["4"] = "MSK-3";    // (GMT+03) Moscow, St. Petersburg, Volgograd
    timezoneMap["5"] = "GST-4";    // (GMT+04) Abu Dhabi, Dubai, Muscat
    timezoneMap["6"] = "PKT-5";    // (GMT+05) Islamabad, Karachi, Tashkent
    timezoneMap["7"] = "BDT-6";    // (GMT+06) Dhaka
    timezoneMap["8"] = "ICT-7";    // (GMT+07) Bangkok, Hanoi, Jakarta
    timezoneMap["9"] = "CST-8";    // (GMT+08) Beijing, Hong Kong, Singapore
    timezoneMap["10"] = "JST-9";   // (GMT+09) Seoul, Tokyo, Osaka
    timezoneMap["11"] = "AEST-10"; // (GMT+10) Canberra, Melbourne, Sydney
    timezoneMap["12"] = "NCT-11";  // (GMT+11) Magadan, New Caledonia, Solomon Islands
    timezoneMap["13"] = "NZST-12"; // (GMT+12) Auckland, Wellington, Fiji
    timezoneMap["14"] = "SST11";   // (GMT-11) Midway Island, Samoa
    timezoneMap["15"] = "HST10";   // (GMT-10) Hawaii
    timezoneMap["16"] = "AKST9";   // (GMT-09) Alaska
    timezoneMap["17"] = "PST8";    // (GMT-08) Pacific Time (US & Canada)
    timezoneMap["18"] = "MST7";    // (GMT-07) Mountain Time (US & Canada)
    timezoneMap["19"] = "CST6";    // (GMT-06) Central Time (US & Canada), Mexico City
    timezoneMap["20"] = "EST5";    // (GMT-05) Eastern Time (US & Canada)
    timezoneMap["21"] = "AST4";    // (GMT-04) Atlantic Time (Canada), Caracas
    timezoneMap["22"] = "BRT3";    // (GMT-03) Brasilia, Buenos Aires, Georgetown
    timezoneMap["23"] = "MAT2";    // (GMT-02) Mid-Atlantic
    timezoneMap["24"] = "AZOT1";   // (GMT-01) Azores, Cape Verde Islands
    timezoneMap["50"] = "PST8";    // (GMT-08) Los Angeles
    timezoneMap["51"] = "CST-8";   // (GMT+08) Taipei (台北時區)

    return timezoneMap;
}

const std::map<std::string, std::string> &TimezoneUtils::getTimezoneMap()
{
    // 使用靜態區域變數確保只初始化一次
    static std::map<std::string, std::string> g_timezoneMap = createTimezoneMap();
    return g_timezoneMap;
}

std::string TimezoneUtils::getTimezoneString(const std::string &tzId)
{
    const auto &timezoneMap = getTimezoneMap();
    auto it = timezoneMap.find(tzId);

    if (it != timezoneMap.end())
    {
        return it->second;
    }

    // 如果找不到，輸出警告並返回空字串
    std::cerr << "WARNING: 找不到時區ID: " << tzId << "，返回空字串" << std::endl;
    return "";
}

bool TimezoneUtils::isValidTimezoneId(const std::string &tzId)
{
    const auto &timezoneMap = getTimezoneMap();
    return timezoneMap.find(tzId) != timezoneMap.end();
}

std::string TimezoneUtils::getDefaultTimezoneId()
{
    return "51"; // 台北時區
}

std::vector<TimezoneInfo> TimezoneUtils::createTimezoneInfoList()
{
    std::vector<TimezoneInfo> timezones;

    // 建立完整的時區資訊列表
    timezones.push_back({"0", "(GMT+01) Casablanca", "3600", "WAT-1"});
    timezones.push_back({"1", "(GMT) Greenwich Mean Time: London", "0", "GMT0"});
    timezones.push_back({"2", "(GMT+01) Amsterdam, Berlin, Rome, Vienna", "3600", "CET-1"});
    timezones.push_back({"3", "(GMT+02) Athens, Istanbul, Minsk", "7200", "EET-2"});
    timezones.push_back({"4", "(GMT+03) Moscow, St. Petersburg, Volgograd", "10800", "MSK-3"});
    timezones.push_back({"5", "(GMT+04) Abu Dhabi, Dubai, Muscat", "14400", "GST-4"});
    timezones.push_back({"6", "(GMT+05) Islamabad, Karachi, Tashkent", "18000", "PKT-5"});
    timezones.push_back({"7", "(GMT+06) Dhaka", "21600", "BDT-6"});
    timezones.push_back({"8", "(GMT+07) Bangkok, Hanoi, Jakarta", "25200", "ICT-7"});
    timezones.push_back({"9", "(GMT+08) Beijing, Hong Kong, Singapore", "28800", "CST-8"});
    timezones.push_back({"10", "(GMT+09) Seoul, Tokyo, Osaka", "32400", "JST-9"});
    timezones.push_back({"11", "(GMT+10) Canberra, Melbourne, Sydney", "36000", "AEST-10"});
    timezones.push_back({"12", "(GMT+11) Magadan, New Caledonia, Solomon Islands", "39600", "NCT-11"});
    timezones.push_back({"13", "(GMT+12) Auckland, Wellington, Fiji", "43200", "NZST-12"});
    timezones.push_back({"14", "(GMT-11) Midway Island, Samoa", "-39600", "SST11"});
    timezones.push_back({"15", "(GMT-10) Hawaii", "-36000", "HST10"});
    timezones.push_back({"16", "(GMT-09) Alaska", "-32400", "AKST9"});
    timezones.push_back({"17", "(GMT-08) Pacific Time (US & Canada)", "-28800", "PST8"});
    timezones.push_back({"18", "(GMT-07) Mountain Time (US & Canada)", "-25200", "MST7"});
    timezones.push_back({"19", "(GMT-06) Central Time (US & Canada), Mexico City", "-21600", "CST6"});
    timezones.push_back({"20", "(GMT-05) Eastern Time (US & Canada)", "-18000", "EST5"});
    timezones.push_back({"21", "(GMT-04) Atlantic Time (Canada), Caracas", "-14400", "AST4"});
    timezones.push_back({"22", "(GMT-03) Brasilia, Buenos Aires, Georgetown", "-10800", "BRT3"});
    timezones.push_back({"23", "(GMT-02) Mid-Atlantic", "-7200", "MAT2"});
    timezones.push_back({"24", "(GMT-01) Azores, Cape Verde Islands", "-3600", "AZOT1"});
    timezones.push_back({"50", "(GMT-08) Los Angeles", "-28800", "PST8"});
    timezones.push_back({"51", "(GMT+08) Taipei", "28800", "CST-8"});

    return timezones;
}

std::vector<TimezoneInfo> TimezoneUtils::getAllTimezoneInfo()
{
    // 使用靜態區域變數確保只初始化一次
    static std::vector<TimezoneInfo> g_timezoneInfoList = createTimezoneInfoList();
    return g_timezoneInfoList;
}

TimezoneInfo TimezoneUtils::getTimezoneInfo(const std::string &tzId)
{
    const auto &timezoneList = getAllTimezoneInfo();

    for (const auto &timezone : timezoneList)
    {
        if (timezone.tId == tzId)
        {
            return timezone;
        }
    }

    // 如果找不到，返回空的結構
    std::cerr << "WARNING: 找不到時區ID: " << tzId << "，返回空的時區資訊" << std::endl;
    return TimezoneInfo{"", "", "", ""};
}

// 新增：獲取所有支援的時區列表
std::vector<std::pair<std::string, std::string>> TimezoneUtils::getAllSupportedTimezones()
{
    std::vector<std::pair<std::string, std::string>> result;

    // 使用現有的 getAllTimezoneInfo() 方法
    auto timezoneInfoList = getAllTimezoneInfo();

    for (const auto &tz : timezoneInfoList)
    {
        result.push_back({tz.tId, tz.displayName});
    }

    // 如果列表為空，從對應表獲取
    if (result.empty())
    {
        const auto &timezoneMap = getTimezoneMap();
        for (const auto &pair : timezoneMap)
        {
            result.push_back({pair.first, "時區ID " + pair.first + " (" + pair.second + ")"});
        }
    }

    return result;
}

// ===== 在 timezone_utils.cpp 文件末尾新增以下實現 =====

void TimezoneUtils::debugTimezoneData()
{
    std::cout << "=== 調試時區資料 ===" << std::endl;

    // 檢查時區映射表
    const auto &timezoneMap = getTimezoneMap();
    std::cout << "時區映射表包含 " << timezoneMap.size() << " 個項目:" << std::endl;
    for (const auto &pair : timezoneMap)
    {
        std::cout << "  ID: " << pair.first << " -> " << pair.second << std::endl;
    }

    std::cout << "\n時區資訊列表包含項目:" << std::endl;
    auto timezoneInfoList = getAllTimezoneInfo();
    std::cout << "共 " << timezoneInfoList.size() << " 個時區資訊:" << std::endl;
    for (const auto &tz : timezoneInfoList)
    {
        std::cout << "  ID: " << tz.tId << " -> " << tz.displayName << std::endl;
    }
}

// 新增：顯示時區列表的格式化輸出
void TimezoneUtils::displayTimezoneList()
{
    auto timezones = getAllSupportedTimezones();

    std::cout << "\n╔══════════════════════════════════════════════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║                                   支援的時區列表                                           ║" << std::endl;
    std::cout << "╠══════════════════════════════════════════════════════════════════════════════════════════╣" << std::endl;
    std::cout << "║ ID  │                            時區描述                                                 ║" << std::endl;
    std::cout << "╠═════╪══════════════════════════════════════════════════════════════════════════════════════╣" << std::endl;

    for (const auto &tz : timezones)
    {
        std::cout << "║ " << std::setw(2) << std::setfill(' ') << tz.first
                  << "  │ " << std::setw(84) << std::left << tz.second << " ║" << std::endl;
    }

    std::cout << "╚═════╧══════════════════════════════════════════════════════════════════════════════════════╝" << std::endl;

    try
    {
        // 顯示當前設定
        std::cout << "\n當前時區設定資訊:" << std::endl;
        auto &paramsManager = CameraParametersManager::getInstance();
        std::string currentTzId = paramsManager.getTimeZone();

        std::cout << "  ► 當前時區ID: " << currentTzId << std::endl;

        // 檢查時區ID是否有效
        if (isValidTimezoneId(currentTzId))
        {
            std::string currentTzString = getTimezoneString(currentTzId);
            std::cout << "  ► 當前時區字串: " << currentTzString << std::endl;

            // 從列表中找到對應的描述
            for (const auto &tz : timezones)
            {
                if (tz.first == currentTzId)
                {
                    std::cout << "  ► 當前時區描述: " << tz.second << std::endl;
                    break;
                }
            }
        }
        else
        {
            std::cout << "  ► 當前時區ID無效，請重新設定" << std::endl;
        }

        std::cout << "  ► 當前系統時間: ";
        if (system("date") != 0) {
            std::cout << "無法獲取系統時間" << std::endl;
        }
    }
    catch (...)
    {
        std::cout << "  ► 無法獲取當前時區設定" << std::endl;
    }

    std::cout << "\n可用的時區ID範圍:" << std::endl;

    // 顯示實際可用的時區ID
    const auto &timezoneMap = getTimezoneMap();
    std::cout << "  • 基本時區: ";
    bool first = true;
    for (const auto &pair : timezoneMap)
    {
        if (!first)
            std::cout << ", ";
        std::cout << pair.first;
        first = false;
    }
    std::cout << std::endl;

    std::cout << "\n使用說明:" << std::endl;
    std::cout << "  • 選擇功能 3 (設置時區) 並輸入對應的 ID 來切換時區" << std::endl;
    std::cout << "  • 台灣時區為 ID: 51 (預設)" << std::endl;
    std::cout << "  • 中國時區為 ID: 9 (Beijing, Hong Kong, Singapore)" << std::endl;
    std::cout << "  • 日本時區為 ID: 10 (Tokyo, Seoul)" << std::endl;
    std::cout << "  • 美國東岸為 ID: 20 (Eastern Time)" << std::endl;
    std::cout << "  • 歐洲中部為 ID: 2 (Amsterdam, Berlin, Rome)" << std::endl;
}

// 新增：根據時區名稱搜尋時區ID
std::vector<std::string> TimezoneUtils::searchTimezoneByName(const std::string &searchTerm)
{
    std::vector<std::string> results;

    std::string lowerSearchTerm = searchTerm;
    std::transform(lowerSearchTerm.begin(), lowerSearchTerm.end(), lowerSearchTerm.begin(), ::tolower);

    // 首先從詳細資訊列表搜尋
    auto timezoneInfoList = getAllTimezoneInfo();
    for (const auto &tz : timezoneInfoList)
    {
        std::string lowerDesc = tz.displayName;
        std::transform(lowerDesc.begin(), lowerDesc.end(), lowerDesc.begin(), ::tolower);

        if (lowerDesc.find(lowerSearchTerm) != std::string::npos)
        {
            results.push_back("ID: " + tz.tId + " - " + tz.displayName);
        }
    }

    // 如果沒有找到，從對應表搜尋
    if (results.empty())
    {
        const auto &timezoneMap = getTimezoneMap();
        for (const auto &pair : timezoneMap)
        {
            std::string searchTarget = "timezone " + pair.first + " " + pair.second;
            std::transform(searchTarget.begin(), searchTarget.end(), searchTarget.begin(), ::tolower);

            if (searchTarget.find(lowerSearchTerm) != std::string::npos)
            {
                results.push_back("ID: " + pair.first + " - " + pair.second);
            }
        }
    }

    return results;
}

// 新增：獲取時區的詳細資訊
std::string TimezoneUtils::getTimezoneDetails(const std::string &timezoneId)
{
    // 首先檢查是否在對應表中
    if (!isValidTimezoneId(timezoneId))
    {
        return "找不到時區ID: " + timezoneId + "\n可用的時區ID: 0-24, 50-51";
    }

    // 獲取時區字串
    std::string tzString = getTimezoneString(timezoneId);

    // 嘗試從詳細資訊列表獲取
    auto timezoneInfo = getTimezoneInfo(timezoneId);

    std::ostringstream oss;
    oss << "時區ID: " << timezoneId << "\n";

    if (!timezoneInfo.tId.empty())
    {
        // 有詳細資訊
        oss << "描述: " << timezoneInfo.displayName << "\n";
        oss << "UTC偏移: " << timezoneInfo.baseUtcOffset << " 秒\n";
        oss << "時區字串: " << timezoneInfo.tzString;
    }
    else
    {
        // 只有基本資訊
        oss << "時區字串: " << tzString << "\n";
        oss << "描述: 時區 " << timezoneId;
    }

    oss << "\n狀態: 有效";

    return oss.str();
}
