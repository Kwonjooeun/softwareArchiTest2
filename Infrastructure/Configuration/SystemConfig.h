#pragma once

#include "../../Common/Types/CommonTypes.h"
#include <map>
#include <string>
#include <memory>
#include <chrono>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <mutex>

namespace WeaponControl {

class SystemConfig {
private:
    static std::unique_ptr<SystemConfig> s_instance;
    static std::mutex s_mutex;
    
    std::map<std::string, std::string> m_config;
    mutable std::mutex m_configMutex;
    bool m_loaded;
    
    SystemConfig() : m_loaded(false) {}
    
public:
    static SystemConfig& getInstance() {
        std::lock_guard<std::mutex> lock(s_mutex);
        if (!s_instance) {
            s_instance = std::unique_ptr<SystemConfig>(new SystemConfig());
        }
        return *s_instance;
    }
    
    // 복사 및 이동 금지
    SystemConfig(const SystemConfig&) = delete;
    SystemConfig& operator=(const SystemConfig&) = delete;
    SystemConfig(SystemConfig&&) = delete;
    SystemConfig& operator=(SystemConfig&&) = delete;
    
    Result<void> loadFromFile(const std::string& filename) {
        std::lock_guard<std::mutex> lock(m_configMutex);
        
        if (!std::filesystem::exists(filename)) {
            return Result<void>::failure("Config file not found: " + filename);
        }
        
        std::ifstream file(filename);
        if (!file.is_open()) {
            return Result<void>::failure("Cannot open config file: " + filename);
        }
        
        std::string line;
        std::string currentSection = "";
        
        while (std::getline(file, line)) {
            line = trim(line);
            
            // 빈 줄이나 주석 스킵
            if (line.empty() || line[0] == ';' || line[0] == '#') {
                continue;
            }
            
            // 섹션 처리
            if (line[0] == '[' && line.back() == ']') {
                currentSection = line.substr(1, line.length() - 2);
                continue;
            }
            
            // 키=값 처리
            auto equalPos = line.find('=');
            if (equalPos != std::string::npos) {
                std::string key = trim(line.substr(0, equalPos));
                std::string value = trim(line.substr(equalPos + 1));
                
                if (!currentSection.empty()) {
                    key = currentSection + "." + key;
                }
                
                m_config[key] = value;
            }
        }
        
        m_loaded = true;
        return Result<void>::success();
    }
    
    Result<void> loadConfigs() {
        auto result = loadFromFile("config/system.ini");
        if (!result) {
            return result;
        }
        
        // 선택적 설정 파일들
        loadFromFile("config/weapons.ini");  // 실패해도 무시
        loadFromFile("config/dds.ini");      // 실패해도 무시
        
        return Result<void>::success();
    }
    
    template<typename T>
    T get(const std::string& key, const T& defaultValue = T{}) const {
        std::lock_guard<std::mutex> lock(m_configMutex);
        
        auto it = m_config.find(key);
        if (it == m_config.end()) {
            return defaultValue;
        }
        
        return convertValue<T>(it->second, defaultValue);
    }
    
    // 편의 함수들
    uint16_t getMaxLaunchTubes() const {
        return get<uint16_t>("System.MaxLaunchTubes", 6);
    }
    
    std::chrono::milliseconds getUpdateInterval() const {
        auto ms = get<int>("System.UpdateIntervalMs", 100);
        return std::chrono::milliseconds(ms);
    }
    
    std::chrono::milliseconds getEngagementPlanInterval() const {
        auto ms = get<int>("System.EngagementPlanIntervalMs", 1000);
        return std::chrono::milliseconds(ms);
    }
    
    std::chrono::milliseconds getStatusReportInterval() const {
        auto ms = get<int>("System.StatusReportIntervalMs", 1000);
        return std::chrono::milliseconds(ms);
    }
    
    std::string getMineDataPath() const {
        return get<std::string>("Paths.MineDataPath", "data/mine_plans");
    }
    
    std::string getLogPath() const {
        return get<std::string>("Paths.LogPath", "logs");
    }
    
    std::string getConfigPath() const {
        return get<std::string>("Paths.ConfigPath", "config");
    }
    
    int getDdsDomainId() const {
        return get<int>("DDS.DomainId", 83);
    }
    
    std::string getDdsQosProfile() const {
        return get<std::string>("DDS.QosProfile", "reliable");
    }
    
    uint32_t getMaxPlanLists() const {
        return get<uint32_t>("MineDropPlan.MaxPlanLists", 15);
    }
    
    uint32_t getMaxPlansPerList() const {
        return get<uint32_t>("MineDropPlan.MaxPlansPerList", 15);
    }
    
    double getMineSpeed() const {
        return get<double>("Weapon.MineSpeed", 5.0);
    }
    
    double getALMMaxRange() const {
        return get<double>("Weapon.ALMMaxRange", 50.0);
    }
    
    double getASMMaxRange() const {
        return get<double>("Weapon.ASMMaxRange", 100.0);
    }
    
    double getALMSpeed() const {
        return get<double>("Weapon.ALMSpeed", 300.0);
    }
    
    double getASMSpeed() const {
        return get<double>("Weapon.ASMSpeed", 400.0);
    }
    
    double getDefaultLaunchDelay() const {
        return get<double>("Weapon.DefaultLaunchDelay", 3.0);
    }
    
    bool isLoaded() const {
        std::lock_guard<std::mutex> lock(m_configMutex);
        return m_loaded;
    }
    
    void set(const std::string& key, const std::string& value) {
        std::lock_guard<std::mutex> lock(m_configMutex);
        m_config[key] = value;
    }
    
    // 설정 저장
    Result<void> saveToFile(const std::string& filename) const {
        std::lock_guard<std::mutex> lock(m_configMutex);
        
        std::ofstream file(filename);
        if (!file.is_open()) {
            return Result<void>::failure("Cannot open file for writing: " + filename);
        }
        
        std::map<std::string, std::map<std::string, std::string>> sections;
        
        // 섹션별로 그룹화
        for (const auto& [key, value] : m_config) {
            auto dotPos = key.find('.');
            if (dotPos != std::string::npos) {
                std::string section = key.substr(0, dotPos);
                std::string keyName = key.substr(dotPos + 1);
                sections[section][keyName] = value;
            } else {
                sections[""][key] = value;
            }
        }
        
        // 파일에 쓰기
        for (const auto& [sectionName, sectionKeys] : sections) {
            if (!sectionName.empty()) {
                file << "[" << sectionName << "]\n";
            }
            
            for (const auto& [key, value] : sectionKeys) {
                file << key << "=" << value << "\n";
            }
            
            file << "\n";
        }
        
        return Result<void>::success();
    }
    
private:
    std::string trim(const std::string& str) const {
        size_t start = str.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) return "";
        
        size_t end = str.find_last_not_of(" \t\r\n");
        return str.substr(start, end - start + 1);
    }
    
    template<typename T>
    T convertValue(const std::string& value, const T& defaultValue) const {
        try {
            if constexpr (std::is_same_v<T, std::string>) {
                return value;
            } else if constexpr (std::is_same_v<T, int>) {
                return std::stoi(value);
            } else if constexpr (std::is_same_v<T, uint16_t>) {
                return static_cast<uint16_t>(std::stoul(value));
            } else if constexpr (std::is_same_v<T, uint32_t>) {
                return static_cast<uint32_t>(std::stoul(value));
            } else if constexpr (std::is_same_v<T, double>) {
                return std::stod(value);
            } else if constexpr (std::is_same_v<T, float>) {
                return std::stof(value);
            } else if constexpr (std::is_same_v<T, bool>) {
                std::string lowerValue = value;
                std::transform(lowerValue.begin(), lowerValue.end(), lowerValue.begin(), ::tolower);
                return (lowerValue == "true" || lowerValue == "1" || lowerValue == "yes");
            }
        } catch (const std::exception&) {
            return defaultValue;
        }
        
        return defaultValue;
    }
};

// 정적 멤버 초기화
std::unique_ptr<SystemConfig> SystemConfig::s_instance = nullptr;
std::mutex SystemConfig::s_mutex;

} // namespace WeaponControl
