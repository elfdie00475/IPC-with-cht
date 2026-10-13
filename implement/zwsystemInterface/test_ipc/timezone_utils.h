/**
 * @file timezone_utils.h
 * @brief 時區工具類別 - 提供時區ID與時區字串的對應
 * @date 2025/01/10
 */

#ifndef TIMEZONE_UTILS_H
#define TIMEZONE_UTILS_H

#include <map>
#include <string>
#include <vector>

/**
 * @brief 時區資訊結構
 */
struct TimezoneInfo
{
    std::string tId;           // 時區ID
    std::string displayName;   // 顯示名稱
    std::string baseUtcOffset; // UTC偏移值（秒）
    std::string tzString;      // 系統時區字串
};

/**
 * @brief 時區工具類別
 * 提供時區ID與時區字串之間的轉換功能
 */
class TimezoneUtils
{
public:
    /**
     * @brief 獲取時區映射表
     * @return 時區ID到時區字串的映射表引用
     */
    static const std::map<std::string, std::string> &getTimezoneMap();

    /**
     * @brief 根據時區ID獲取時區字串
     * @param tzId 時區ID（如 "51" 代表台北）
     * @return 時區字串（如 "CST-8"），找不到則返回空字串
     */
    static std::string getTimezoneString(const std::string &tzId);

    /**
     * @brief 檢查時區ID是否有效
     * @param tzId 時區ID
     * @return true=有效, false=無效
     */
    static bool isValidTimezoneId(const std::string &tzId);

    /**
     * @brief 獲取預設時區ID（台北）
     * @return 預設時區ID "51"
     */
    static std::string getDefaultTimezoneId();

    /**
     * @brief 獲取所有時區資訊列表
     * @return 包含所有時區詳細資訊的向量
     */
    static std::vector<TimezoneInfo> getAllTimezoneInfo();

    /**
     * @brief 根據時區ID獲取完整時區資訊
     * @param tzId 時區ID
     * @return 時區資訊結構，找不到則返回空的結構
     */
    static TimezoneInfo getTimezoneInfo(const std::string &tzId);
    // 新增：獲取所有支援的時區列表
    static std::vector<std::pair<std::string, std::string>> getAllSupportedTimezones();

    // 新增：顯示時區列表的格式化輸出
    static void displayTimezoneList();

    // 新增：根據時區名稱搜尋時區ID
    static std::vector<std::string> searchTimezoneByName(const std::string &searchTerm);

    // 新增：獲取時區的詳細資訊
    static std::string getTimezoneDetails(const std::string &timezoneId);
    static void debugTimezoneData();

private:
    /**
     * @brief 建立時區映射表（內部使用）
     * @return 完整的時區映射表
     */
    static std::map<std::string, std::string> createTimezoneMap();

    /**
     * @brief 建立完整時區資訊列表（內部使用）
     * @return 包含所有時區詳細資訊的向量
     */
    static std::vector<TimezoneInfo> createTimezoneInfoList();
};

#endif // TIMEZONE_UTILS_H