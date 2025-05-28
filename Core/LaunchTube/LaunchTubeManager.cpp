#include "LaunchTubeManager.h"
#include "../Factory/WeaponFactory.h"
#include <iostream>
#include <algorithm>

namespace WeaponControl {

// =============================================================================
// LaunchTubeManager 구현
// =============================================================================

LaunchTubeManager::LaunchTubeManager(uint16_t maxTubes)
    : m_maxTubes(maxTubes == 0 ? SystemConfig::getInstance().getMaxLaunchTubes() : maxTubes)
    , m_minTubeNumber(1)
    , m_maxTubeNumber(m_maxTubes)
    , m_axisCenter{0.0, 0.0}
    , m_initialized(false)
{
    std::cout << "LaunchTubeManager created with " << m_maxTubes << " tubes" << std::endl;
}

Result<void> LaunchTubeManager::initialize() {
    if (m_initialized) {
        std::cout << "LaunchTubeManager already initialized" << std::endl;
        return Result<void>::success();
    }
    
    try {
        // 발사관들 생성 (1부터 maxTubes까지)
        m_launchTubes.resize(m_maxTubes + 1); // 0번 인덱스는 사용하지 않음
        
        for (uint16_t i = m_minTubeNumber; i <= m_maxTubeNumber; ++i) {
            m_launchTubes[i] = std::make_shared<LaunchTube>(i);
            
            // 콜백 등록
            m_launchTubes[i]->setStateChangeCallback(
                [this](uint16_t tubeNumber, EN_WPN_CTRL_STATE oldState, EN_WPN_CTRL_STATE newState) {
                    onTubeStateChanged(tubeNumber, oldState, newState);
                });
            
            m_launchTubes[i]->setLaunchStatusCallback(
                [this](uint16_t tubeNumber, bool launched) {
                    onTubeLaunchStatusChanged(tubeNumber, launched);
                });
            
            m_launchTubes[i]->setEngagementPlanCallback(
                [this](uint16_t tubeNumber, const EngagementPlanResult& result) {
                    onTubeEngagementPlanUpdated(tubeNumber, result);
                });
        }
        
        m_initialized = true;
        std::cout << "LaunchTubeManager initialized with " << m_maxTubes << " tubes" << std::endl;
        
        return Result<void>::success();
        
    } catch (const std::exception& e) {
        return Result<void>::failure("Failed to initialize LaunchTubeManager: " + std::string(e.what()));
    }
}

void LaunchTubeManager::shutdown() {
    std::lock_guard<std::shared_mutex> lock(m_tubesMutex);
    
    // 모든 발사관 할당 해제
    for (uint16_t i = m_minTubeNumber; i <= m_maxTubeNumber; ++i) {
        if (m_launchTubes[i] && m_launchTubes[i]->hasWeapon()) {
            m_launchTubes[i]->clearAssignment();
        }
    }
    
    m_initialized = false;
    std::cout << "LaunchTubeManager shutdown complete" << std::endl;
}

Result<void> LaunchTubeManager::assignWeapon(const WeaponAssignmentRequest& request) {
    auto tube = getValidatedTube(request.tubeNumber);
    if (!tube) {
        return Result<void>::failure("Invalid tube number: " + std::to_string(request.tubeNumber));
    }
    
    if (tube->hasWeapon()) {
        return Result<void>::failure("Tube " + std::to_string(request.tubeNumber) + " already assigned");
    }
    
    // 무장과 교전계획 관리자 생성
    auto weaponResult = createWeaponAndManager(request.weaponKind);
    if (!weaponResult) {
        return Result<void>::failure("Failed to create weapon: " + weaponResult.error().message);
    }
    
    auto [weapon, engagementMgr] = weaponResult.value();
    
    // 발사관에 할당
    auto assignResult = tube->assignWeapon(std::move(weapon), std::move(engagementMgr), request.assignmentInfo);
    if (!assignResult) {
        return assignResult;
    }
    
    // 환경 정보 업데이트
    {
        std::shared_lock<std::shared_mutex> envLock(m_environmentMutex);
        tube->setAxisCenter(m_axisCenter);
        tube->updateOwnShipInfo(m_ownShipInfo);
        
        // 할당 명령에서 표적 ID 추출하여 표적 정보 업데이트
        uint32_t targetId = request.assignmentInfo.systemTargetId;
        if (targetId > 0) {
            auto targetIt = m_targetInfoMap.find(targetId);
            if (targetIt != m_targetInfoMap.end()) {
                tube->updateTargetInfo(targetIt->second);
            }
        }
    }
    
    // 할당 변경 콜백 호출
    if (m_assignmentChangeCallback) {
        m_assignmentChangeCallback(request.tubeNumber, request.weaponKind, true);
    }
    
    std::cout << "Successfully assigned " << WeaponKindToString(request.weaponKind) 
              << " to tube " << request.tubeNumber << std::endl;
    
    return Result<void>::success();
}

Result<void> LaunchTubeManager::unassignWeapon(uint16_t tubeNumber) {
    auto tube = getValidatedTube(tubeNumber);
    if (!tube) {
        return Result<void>::failure("Invalid tube number: " + std::to_string(tubeNumber));
    }
    
    if (!tube->hasWeapon()) {
        return Result<void>::failure("Tube " + std::to_string(tubeNumber) + " is not assigned");
    }
    
    EN_WPN_KIND weaponKind = tube->getWeapon()->getWeaponKind();
    tube->clearAssignment();
    
    // 할당 변경 콜백 호출
    if (m_assignmentChangeCallback) {
        m_assignmentChangeCallback(tubeNumber, weaponKind, false);
    }
    
    std::cout << "Successfully unassigned weapon from tube " << tubeNumber << std::endl;
    return Result<void>::success();
}

bool LaunchTubeManager::isAssigned(uint16_t tubeNumber) const {
    auto tube = getValidatedTube(tubeNumber);
    return tube && tube->hasWeapon();
}

bool LaunchTubeManager::canAssignWeapon(uint16_t tubeNumber, EN_WPN_KIND weaponKind) const {
    auto tube = getValidatedTube(tubeNumber);
    if (!tube) {
        return false;
    }
    
    // 이미 할당된 경우 불가
    if (tube->hasWeapon()) {
        return false;
    }
    
    // 무장 종류가 지원되는지 확인
    auto& factory = WeaponFactory::getInstance();
    return factory.isWeaponSupported(weaponKind);
}

Result<void> LaunchTubeManager::requestWeaponStateChange(const WeaponControlRequest& request) {
    auto tube = getValidatedTube(request.tubeNumber);
    if (!tube) {
        return Result<void>::failure("Invalid tube number: " + std::to_string(request.tubeNumber));
    }
    
    return tube->requestWeaponStateChange(request.targetState, request.cancellationToken);
}

Result<void> LaunchTubeManager::requestAllWeaponStateChange(EN_WPN_CTRL_STATE newState) {
    auto assignedTubes = getAssignedTubes();
    bool allSuccess = true;
    std::string errors;
    
    for (auto& tube : assignedTubes) {
        auto result = tube->requestWeaponStateChange(newState);
        if (!result) {
            allSuccess = false;
            errors += "Tube " + std::to_string(tube->getTubeNumber()) + ": " + result.error().message + "; ";
        }
    }
    
    if (allSuccess) {
        return Result<void>::success();
    } else {
        return Result<void>::failure("Some state changes failed: " + errors);
    }
}

bool LaunchTubeManager::canChangeState(uint16_t tubeNumber, EN_WPN_CTRL_STATE newState) const {
    auto tube = getValidatedTube(tubeNumber);
    if (!tube || !tube->hasWeapon()) {
        return false;
    }
    
    auto weapon = tube->getWeapon();
    return weapon->isValidTransition(weapon->getCurrentState(), newState);
}

Result<void> LaunchTubeManager::emergencyStop() {
    std::cout << "EMERGENCY STOP initiated" << std::endl;
    
    auto assignedTubes = getAssignedTubes();
    bool allSuccess = true;
    std::string errors;
    
    for (auto& tube : assignedTubes) {
        EN_WPN_CTRL_STATE currentState = tube->getWeaponState();
        
        // 발사 중이면 중단, 그렇지 않으면 끔
        EN_WPN_CTRL_STATE targetState = (currentState == EN_WPN_CTRL_STATE::WPN_CTRL_STATE_LAUNCH) 
            ? EN_WPN_CTRL_STATE::WPN_CTRL_STATE_ABORT 
            : EN_WPN_CTRL_STATE::WPN_CTRL_STATE_OFF;
        
        // 긴급 취소 토큰 생성
        CancellationToken emergencyToken;
        if (targetState == EN_WPN_CTRL_STATE::WPN_CTRL_STATE_ABORT) {
            emergencyToken.cancel(); // 즉시 취소
        }
        
        auto result = tube->requestWeaponStateChange(targetState, emergencyToken);
        if (!result) {
            allSuccess = false;
            errors += "Tube " + std::to_string(tube->getTubeNumber()) + ": " + result.error().message + "; ";
        }
    }
    
    if (allSuccess) {
        std::cout << "Emergency stop completed successfully" << std::endl;
        return Result<void>::success();
    } else {
        return Result<void>::failure("Emergency stop partially failed: " + errors);
    }
}

void LaunchTubeManager::updateOwnShipInfo(const NAVINF_SHIP_NAVIGATION_INFO& ownShip) {
    {
        std::lock_guard<std::shared_mutex> lock(m_environmentMutex);
        m_ownShipInfo = ownShip;
    }
    
    // 모든 할당된 발사관에 업데이트
    auto assignedTubes = getAssignedTubes();
    for (auto& tube : assignedTubes) {
        tube->updateOwnShipInfo(ownShip);
    }
}

void LaunchTubeManager::updateTargetInfo(const TRKMGR_SYSTEMTARGET_INFO& target) {
    {
        std::lock_guard<std::shared_mutex> lock(m_environmentMutex);
        m_targetInfoMap[target.unTargetSystemID()] = target;
    }
    
    // 모든 할당된 발사관에 업데이트
    auto assignedTubes = getAssignedTubes();
    for (auto& tube : assignedTubes) {
        tube->updateTargetInfo(target);
    }
}

void LaunchTubeManager::setAxisCenter(const GEO_POINT_2D& axisCenter) {
    {
        std::lock_guard<std::shared_mutex> lock(m_environmentMutex);
        m_axisCenter = axisCenter;
    }
    
    // 모든 할당된 발사관에 업데이트
    auto assignedTubes = getAssignedTubes();
    for (auto& tube : assignedTubes) {
        tube->setAxisCenter(axisCenter);
    }
}

Result<void> LaunchTubeManager::updateWaypoints(const WaypointUpdateRequest& request) {
    auto tube = getValidatedTube(request.tubeNumber);
    if (!tube) {
        return Result<void>::failure("Invalid tube number: " + std::to_string(request.tubeNumber));
    }
    
    return tube->updateWaypoints(request.waypoints);
}

Result<void> LaunchTubeManager::calculateEngagementPlan(uint16_t tubeNumber) {
    auto tube = getValidatedTube(tubeNumber);
    if (!tube) {
        return Result<void>::failure("Invalid tube number: " + std::to_string(tubeNumber));
    }
    
    return tube->calculateEngagementPlan();
}

void LaunchTubeManager::calculateAllEngagementPlans() {
    auto assignedTubes = getAssignedTubes();
    for (auto& tube : assignedTubes) {
        tube->calculateEngagementPlan();
    }
}

std::vector<LaunchTubeStatus> LaunchTubeManager::getAllTubeStatus() const {
    std::vector<LaunchTubeStatus> statuses;
    
    std::shared_lock<std::shared_mutex> lock(m_tubesMutex);
    for (uint16_t i = m_minTubeNumber; i <= m_maxTubeNumber; ++i) {
        if (m_launchTubes[i]) {
            statuses.push_back(m_launchTubes[i]->getStatus());
        }
    }
    
    return statuses;
}

LaunchTubeStatus LaunchTubeManager::getTubeStatus(uint16_t tubeNumber) const {
    auto tube = getValidatedTube(tubeNumber);
    if (tube) {
        return tube->getStatus();
    }
    
    LaunchTubeStatus emptyStatus;
    emptyStatus.tubeNumber = tubeNumber;
    return emptyStatus;
}

std::vector<EngagementPlanResult> LaunchTubeManager::getAllEngagementResults() const {
    std::vector<EngagementPlanResult> results;
    
    auto assignedTubes = getAssignedTubes();
    for (auto& tube : assignedTubes) {
        results.push_back(tube->getEngagementResult());
    }
    
    return results;
}

EngagementPlanResult LaunchTubeManager::getEngagementResult(uint16_t tubeNumber) const {
    auto tube = getValidatedTube(tubeNumber);
    if (tube) {
        return tube->getEngagementResult();
    }
    
    EngagementPlanResult emptyResult;
    emptyResult.tubeNumber = tubeNumber;
    return emptyResult;
}

std::shared_ptr<LaunchTube> LaunchTubeManager::getLaunchTube(uint16_t tubeNumber) {
    return getValidatedTube(tubeNumber);
}

std::shared_ptr<const LaunchTube> LaunchTubeManager::getLaunchTube(uint16_t tubeNumber) const {
    return getValidatedTube(tubeNumber);
}

std::vector<std::shared_ptr<LaunchTube>> LaunchTubeManager::getAssignedTubes() const {
    std::vector<std::shared_ptr<LaunchTube>> assignedTubes;
    
    std::shared_lock<std::shared_mutex> lock(m_tubesMutex);
    for (uint16_t i = m_minTubeNumber; i <= m_maxTubeNumber; ++i) {
        if (m_launchTubes[i] && m_launchTubes[i]->hasWeapon()) {
            assignedTubes.push_back(m_launchTubes[i]);
        }
    }
    
    return assignedTubes;
}

void LaunchTubeManager::update() {
    auto assignedTubes = getAssignedTubes();
    for (auto& tube : assignedTubes) {
        tube->update();
    }
}

void LaunchTubeManager::setStateChangeCallback(std::function<void(uint16_t, EN_WPN_CTRL_STATE, EN_WPN_CTRL_STATE)> callback) {
    m_stateChangeCallback = callback;
}

void LaunchTubeManager::setLaunchStatusCallback(std::function<void(uint16_t, bool)> callback) {
    m_launchStatusCallback = callback;
}

void LaunchTubeManager::setEngagementPlanCallback(std::function<void(uint16_t, const EngagementPlanResult&)> callback) {
    m_engagementPlanCallback = callback;
}

void LaunchTubeManager::setAssignmentChangeCallback(std::function<void(uint16_t, EN_WPN_KIND, bool)> callback) {
    m_assignmentChangeCallback = callback;
}

bool LaunchTubeManager::isValidTubeNumber(uint16_t tubeNumber) const {
    return tubeNumber >= m_minTubeNumber && tubeNumber <= m_maxTubeNumber;
}

size_t LaunchTubeManager::getAssignedTubeCount() const {
    return getAssignedTubes().size();
}

size_t LaunchTubeManager::getReadyTubeCount() const {
    size_t readyCount = 0;
    auto statuses = getAllTubeStatus();
    
    for (const auto& status : statuses) {
        if (status.hasWeapon && status.weaponState == EN_WPN_CTRL_STATE::WPN_CTRL_STATE_RTL) {
            readyCount++;
        }
    }
    
    return readyCount;
}

// =============================================================================
// Private 메서드들
// =============================================================================

std::shared_ptr<LaunchTube> LaunchTubeManager::getValidatedTube(uint16_t tubeNumber) {
    if (!isValidTubeNumber(tubeNumber)) {
        std::cout << "Invalid tube number: " << tubeNumber << std::endl;
        return nullptr;
    }
    
    std::shared_lock<std::shared_mutex> lock(m_tubesMutex);
    return m_launchTubes[tubeNumber];
}

std::shared_ptr<const LaunchTube> LaunchTubeManager::getValidatedTube(uint16_t tubeNumber) const {
    if (!isValidTubeNumber(tubeNumber)) {
        std::cout << "Invalid tube number: " << tubeNumber << std::endl;
        return nullptr;
    }
    
    std::shared_lock<std::shared_mutex> lock(m_tubesMutex);
    return m_launchTubes[tubeNumber];
}

void LaunchTubeManager::onTubeStateChanged(uint16_t tubeNumber, EN_WPN_CTRL_STATE oldState, EN_WPN_CTRL_STATE newState) {
    if (m_stateChangeCallback) {
        m_stateChangeCallback(tubeNumber, oldState, newState);
    }
}

void LaunchTubeManager::onTubeLaunchStatusChanged(uint16_t tubeNumber, bool launched) {
    if (m_launchStatusCallback) {
        m_launchStatusCallback(tubeNumber, launched);
    }
}

void LaunchTubeManager::onTubeEngagementPlanUpdated(uint16_t tubeNumber, const EngagementPlanResult& result) {
    if (m_engagementPlanCallback) {
        m_engagementPlanCallback(tubeNumber, result);
    }
}

Result<std::pair<WeaponPtr, EngagementManagerPtr>> LaunchTubeManager::createWeaponAndManager(EN_WPN_KIND weaponKind) {
    auto& factory = WeaponFactory::getInstance();
    
    auto weapon = factory.createWeapon(weaponKind);
    if (!weapon) {
        return Result<std::pair<WeaponPtr, EngagementManagerPtr>>::failure("Failed to create weapon");
    }
    
    auto engagementMgr = factory.createEngagementManager(weaponKind);
    if (!engagementMgr) {
        return Result<std::pair<WeaponPtr, EngagementManagerPtr>>::failure("Failed to create engagement manager");
    }
    
    return Result<std::pair<WeaponPtr, EngagementManagerPtr>>::success(
        std::make_pair(std::move(weapon), std::move(engagementMgr))
    );
}

} // namespace WeaponControl
