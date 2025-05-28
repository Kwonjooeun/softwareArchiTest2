#include "ServiceInterfaces.h"
#include "../../Infrastructure/Configuration/SystemConfig.h"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <algorithm>
#include <sstream>

namespace WeaponControl {

// =============================================================================
// TargetTrackingService 구현
// =============================================================================

void TargetTrackingService::updateTargetInfo(const TRKMGR_SYSTEMTARGET_INFO& targetInfo) {
    std::lock_guard<std::shared_mutex> lock(m_targetsMutex);
    
    TargetData data;
    data.info = targetInfo;
    data.lastUpdateTime = std::chrono::steady_clock::now();
    
    m_targets[targetInfo.unTargetSystemID()] = data;
    
    // 주기적으로 오래된 표적 정리 (간단한 구현)
    static auto lastCleanup = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();
    if (now - lastCleanup > std::chrono::minutes(1)) {
        clearOldTargets(std::chrono::minutes(5)); // 5분 이상 업데이트 없는 표적 제거
        lastCleanup = now;
    }
}

std::optional<TRKMGR_SYSTEMTARGET_INFO> TargetTrackingService::getTarget(uint32_t systemTargetId) const {
    std::shared_lock<std::shared_mutex> lock(m_targetsMutex);
    
    auto it = m_targets.find(systemTargetId);
    if (it != m_targets.end()) {
        return it->second.info;
    }
    
    return std::nullopt;
}

std::vector<uint32_t> TargetTrackingService::getAllTargetIds() const {
    std::shared_lock<std::shared_mutex> lock(m_targetsMutex);
    
    std::vector<uint32_t> targetIds;
    for (const auto& [id, data] : m_targets) {
        targetIds.push_back(id);
    }
    
    return targetIds;
}

size_t TargetTrackingService::getTargetCount() const {
    std::shared_lock<std::shared_mutex> lock(m_targetsMutex);
    return m_targets.size();
}

void TargetTrackingService::clearOldTargets(std::chrono::seconds maxAge) {
    std::lock_guard<std::shared_mutex> lock(m_targetsMutex);
    
    auto now = std::chrono::steady_clock::now();
    auto it = m_targets.begin();
    
    while (it != m_targets.end()) {
        if (now - it->second.lastUpdateTime > maxAge) {
            std::cout << "Removing old target: " << it->first << std::endl;
            it = m_targets.erase(it);
        } else {
            ++it;
        }
    }
}

// =============================================================================
// MineDropPlanService 구현
// =============================================================================

MineDropPlanService::MineDropPlanService(const std::string& planDataPath)
    : m_planDataPath(planDataPath.empty() ? SystemConfig::getInstance().getMineDataPath() : planDataPath)
    , m_maxPlanLists(SystemConfig::getInstance().getMaxPlanLists())
    , m_maxPlansPerList(SystemConfig::getInstance().getMaxPlansPerList())
    , m_initialized(false)
{
    std::cout << "MineDropPlanService created with data path: " << m_planDataPath << std::endl;
}

Result<void> MineDropPlanService::initialize(const std::string& planDataPath) {
    if (!planDataPath.empty()) {
        m_planDataPath = planDataPath;
    }
    
    try {
        // 계획 데이터 디렉토리 생성
        std::filesystem::create_directories(m_planDataPath);
        
        // 기본 계획 목록들을 로드하거나 생성
        for (uint32_t i = 1; i <= m_maxPlanLists; ++i) {
            auto result = loadPlanList(i);
            if (!result) {
                // 로드 실패시 새로 생성
                createNewPlanList(i);
            }
        }
        
        m_initialized = true;
        std::cout << "MineDropPlanService initialized with data path: " << m_planDataPath << std::endl;
        
        return Result<void>::success();
        
    } catch (const std::exception& e) {
        return Result<void>::failure("Failed to initialize MineDropPlanService: " + std::string(e.what()));
    }
}

Result<void> MineDropPlanService::loadPlanList(uint32_t planListNumber) {
    if (!isValidPlanListNumber(planListNumber)) {
        return Result<void>::failure("Invalid plan list number");
    }
    
    auto plans = loadPlanListFromFile(planListNumber);
    if (!plans) {
        return Result<void>::failure("Failed to load plan list: " + plans.error().message);
    }
    
    {
        std::lock_guard<std::shared_mutex> lock(m_plansMutex);
        m_cachedPlans[planListNumber] = plans.value();
    }
    
    std::cout << "Loaded plan list " << planListNumber << " with " << plans.value().size() << " plans" << std::endl;
    return Result<void>::success();
}

Result<void> MineDropPlanService::savePlanList(uint32_t planListNumber, const std::vector<ST_M_MINE_PLAN_INFO>& plans) {
    if (!isValidPlanListNumber(planListNumber)) {
        return Result<void>::failure("Invalid plan list number");
    }
    
    if (plans.size() > m_maxPlansPerList) {
        return Result<void>::failure("Too many plans in list");
    }
    
    // 계획들 유효성 검사
    for (const auto& plan : plans) {
        if (!validatePlan(plan)) {
            return Result<void>::failure("Invalid plan in list");
        }
    }
    
    auto result = savePlanListToFile(planListNumber, plans);
    if (!result) {
        return result;
    }
    
    {
        std::lock_guard<std::shared_mutex> lock(m_plansMutex);
        m_cachedPlans[planListNumber] = plans;
    }
    
    std::cout << "Saved plan list " << planListNumber << " with " << plans.size() << " plans" << std::endl;
    return Result<void>::success();
}

Result<void> MineDropPlanService::createNewPlanList(uint32_t planListNumber) {
    if (!isValidPlanListNumber(planListNumber)) {
        return Result<void>::failure("Invalid plan list number");
    }
    
    std::vector<ST_M_MINE_PLAN_INFO> emptyPlans;
    return savePlanList(planListNumber, emptyPlans);
}

Result<void> MineDropPlanService::deletePlanList(uint32_t planListNumber) {
    if (!isValidPlanListNumber(planListNumber)) {
        return Result<void>::failure("Invalid plan list number");
    }
    
    std::string filePath = getPlanListFilePath(planListNumber);
    
    try {
        std::filesystem::remove(filePath);
        
        {
            std::lock_guard<std::shared_mutex> lock(m_plansMutex);
            m_cachedPlans.erase(planListNumber);
        }
        
        std::cout << "Deleted plan list " << planListNumber << std::endl;
        return Result<void>::success();
        
    } catch (const std::exception& e) {
        return Result<void>::failure("Failed to delete plan list: " + std::string(e.what()));
    }
}

std::vector<ST_M_MINE_PLAN_INFO> MineDropPlanService::getPlanList(uint32_t planListNumber) const {
    if (!isValidPlanListNumber(planListNumber)) {
        return {};
    }
    
    std::shared_lock<std::shared_mutex> lock(m_plansMutex);
    auto it = m_cachedPlans.find(planListNumber);
    if (it != m_cachedPlans.end()) {
        return it->second;
    }
    
    return {};
}

Result<ST_M_MINE_PLAN_INFO> MineDropPlanService::getPlan(uint32_t planListNumber, uint32_t planNumber) const {
    auto plans = getPlanList(planListNumber);
    
    auto it = std::find_if(plans.begin(), plans.end(),
        [planNumber](const ST_M_MINE_PLAN_INFO& plan) {
            return plan.usDroppingPlanNumber() == planNumber;
        });
    
    if (it != plans.end()) {
        return Result<ST_M_MINE_PLAN_INFO>::success(*it);
    }
    
    return Result<ST_M_MINE_PLAN_INFO>::failure("Plan not found");
}

std::vector<uint32_t> MineDropPlanService::getAvailablePlanListNumbers() const {
    std::vector<uint32_t> availableNumbers;
    
    for (uint32_t i = 1; i <= m_maxPlanLists; ++i) {
        std::string filePath = getPlanListFilePath(i);
        if (std::filesystem::exists(filePath)) {
            availableNumbers.push_back(i);
        }
    }
    
    return availableNumbers;
}

Result<void> MineDropPlanService::updatePlan(uint32_t planListNumber, const ST_M_MINE_PLAN_INFO& plan) {
    if (!validatePlan(plan)) {
        return Result<void>::failure("Invalid plan");
    }
    
    auto plans = getPlanList(planListNumber);
    
    auto it = std::find_if(plans.begin(), plans.end(),
        [&plan](const ST_M_MINE_PLAN_INFO& existingPlan) {
            return existingPlan.usDroppingPlanNumber() == plan.usDroppingPlanNumber();
        });
    
    if (it != plans.end()) {
        *it = plan;
    } else {
        plans.push_back(plan);
    }
    
    return savePlanList(planListNumber, plans);
}

Result<void> MineDropPlanService::addPlan(uint32_t planListNumber, const ST_M_MINE_PLAN_INFO& plan) {
    if (!validatePlan(plan)) {
        return Result<void>::failure("Invalid plan");
    }
    
    auto plans = getPlanList(planListNumber);
    
    if (plans.size() >= m_maxPlansPerList) {
        return Result<void>::failure("Plan list is full");
    }
    
    // 중복 계획 번호 확인
    auto it = std::find_if(plans.begin(), plans.end(),
        [&plan](const ST_M_MINE_PLAN_INFO& existingPlan) {
            return existingPlan.usDroppingPlanNumber() == plan.usDroppingPlanNumber();
        });
    
    if (it != plans.end()) {
        return Result<void>::failure("Plan number already exists");
    }
    
    plans.push_back(plan);
    return savePlanList(planListNumber, plans);
}

Result<void> MineDropPlanService::removePlan(uint32_t planListNumber, uint32_t planNumber) {
    auto plans = getPlanList(planListNumber);
    
    auto it = std::find_if(plans.begin(), plans.end(),
        [planNumber](const ST_M_MINE_PLAN_INFO& plan) {
            return plan.usDroppingPlanNumber() == planNumber;
        });
    
    if (it == plans.end()) {
        return Result<void>::failure("Plan not found");
    }
    
    plans.erase(it);
    return savePlanList(planListNumber, plans);
}

Result<AIEP_CMSHCI_M_MINE_ALL_PLAN_LIST> MineDropPlanService::convertToAllPlanListMessage(uint32_t planListNumber) const {
    AIEP_CMSHCI_M_MINE_ALL_PLAN_LIST message;
    
    // TODO: 실제 DDS 메시지 구조에 맞게 설정
    // 지금은 간단한 구현
    auto plans = getPlanList(planListNumber);
    
    return Result<AIEP_CMSHCI_M_MINE_ALL_PLAN_LIST>::success(std::move(message));
}

Result<void> MineDropPlanService::updateFromEditedPlanList(const CMSHCI_AIEP_M_MINE_EDITED_PLAN_LIST& editedPlanList) {
    // TODO: DDS 메시지에서 계획 목록 번호 추출
    uint32_t planListNumber = 1; // 임시
    
    // TODO: DDS 메시지에서 계획 목록 추출
    std::vector<ST_M_MINE_PLAN_INFO> plans;
    
    return savePlanList(planListNumber, plans);
}

bool MineDropPlanService::isValidPlanListNumber(uint32_t planListNumber) const {
    return planListNumber >= 1 && planListNumber <= m_maxPlanLists;
}

bool MineDropPlanService::isValidPlanNumber(uint32_t planListNumber, uint32_t planNumber) const {
    auto plans = getPlanList(planListNumber);
    
    return std::any_of(plans.begin(), plans.end(),
        [planNumber](const ST_M_MINE_PLAN_INFO& plan) {
            return plan.usDroppingPlanNumber() == planNumber;
        });
}

bool MineDropPlanService::validatePlan(const ST_M_MINE_PLAN_INFO& plan) const {
    // 계획 번호 검사
    if (plan.usDroppingPlanNumber() == 0) {
        return false;
    }
    
    // 위치 검사
    if (!validatePosition(plan.stLaunchPos()) || !validatePosition(plan.stDropPos())) {
        return false;
    }
    
    // 경로점 검사
    for (size_t i = 0; i < plan.usWaypointCnt() && i < 8; ++i) {
        if (!validatePosition(plan.stWaypoint()[i])) {
            return false;
        }
    }
    
    return true;
}

size_t MineDropPlanService::getPlanCount(uint32_t planListNumber) const {
    return getPlanList(planListNumber).size();
}

size_t MineDropPlanService::getTotalPlanListCount() const {
    return getAvailablePlanListNumbers().size();
}

// =============================================================================
// Private 메서드들
// =============================================================================

std::string MineDropPlanService::getPlanListFilePath(uint32_t planListNumber) const {
    return m_planDataPath + "/plan_list_" + std::to_string(planListNumber) + ".json";
}

Result<void> MineDropPlanService::savePlanListToFile(uint32_t planListNumber, const std::vector<ST_M_MINE_PLAN_INFO>& plans) {
    return writeJsonToFile(getPlanListFilePath(planListNumber), planListNumber, plans);
}

Result<std::vector<ST_M_MINE_PLAN_INFO>> MineDropPlanService::loadPlanListFromFile(uint32_t planListNumber) {
    return readJsonFromFile(getPlanListFilePath(planListNumber));
}

Result<void> MineDropPlanService::writeJsonToFile(const std::string& filePath, uint32_t planListNumber, const std::vector<ST_M_MINE_PLAN_INFO>& plans) {
    try {
        std::ofstream file(filePath);
        if (!file.is_open()) {
            return Result<void>::failure("Cannot open file for writing");
        }
        
        file << "{\n";
        file << "  \"planListNumber\": " << planListNumber << ",\n";
        file << "  \"plans\": [\n";
        
        for (size_t i = 0; i < plans.size(); ++i) {
            const auto& plan = plans[i];
            file << "    {\n";
            file << "      \"planNumber\": " << plan.usDroppingPlanNumber() << ",\n";
            file << "      \"planName\": \"Plan_" << plan.usDroppingPlanNumber() << "\",\n";
            file << "      \"launchLat\": " << plan.stLaunchPos().dLatitude() << ",\n";
            file << "      \"launchLon\": " << plan.stLaunchPos().dLongitude() << ",\n";
            file << "      \"dropLat\": " << plan.stDropPos().dLatitude() << ",\n";
            file << "      \"dropLon\": " << plan.stDropPos().dLongitude() << ",\n";
            file << "      \"waypointCount\": " << plan.usWaypointCnt() << "\n";
            file << "    }";
            
            if (i < plans.size() - 1) {
                file << ",";
            }
            file << "\n";
        }
        
        file << "  ]\n";
        file << "}\n";
        
        return Result<void>::success();
        
    } catch (const std::exception& e) {
        return Result<void>::failure("Failed to write JSON file: " + std::string(e.what()));
    }
}

Result<std::vector<ST_M_MINE_PLAN_INFO>> MineDropPlanService::readJsonFromFile(const std::string& filePath) {
    try {
        if (!std::filesystem::exists(filePath)) {
            return Result<std::vector<ST_M_MINE_PLAN_INFO>>::failure("File not found");
        }
        
        std::ifstream file(filePath);
        if (!file.is_open()) {
            return Result<std::vector<ST_M_MINE_PLAN_INFO>>::failure("Cannot open file");
        }
        
        // 간단한 JSON 파싱 (실제 환경에서는 JSON 라이브러리 사용)
        // 지금은 기본값으로 초기화
        std::vector<ST_M_MINE_PLAN_INFO> plans;
        
        return Result<std::vector<ST_M_MINE_PLAN_INFO>>::success(std::move(plans));
        
    } catch (const std::exception& e) {
        return Result<std::vector<ST_M_MINE_PLAN_INFO>>::failure("Failed to read JSON file: " + std::string(e.what()));
    }
}

bool MineDropPlanService::validateWaypoints(const std::vector<ST_WEAPON_WAYPOINT>& waypoints) const {
    for (const auto& waypoint : waypoints) {
        if (!validatePosition(waypoint)) {
            return false;
        }
    }
    return true;
}

bool MineDropPlanService::validatePosition(const ST_WEAPON_WAYPOINT& position) const {
    // 위도 범위: -90 ~ 90
    if (position.dLatitude() < -90.0 || position.dLatitude() > 90.0) {
        return false;
    }
    
    // 경도 범위: -180 ~ 180
    if (position.dLongitude() < -180.0 || position.dLongitude() > 180.0) {
        return false;
    }
    
    // 고도 범위: -1000 ~ 10000 (미터)
    if (position.fDepth() < -1000.0 || position.fDepth() > 10000.0) {
        return false;
    }
    
    return true;
}

} // namespace WeaponControl
