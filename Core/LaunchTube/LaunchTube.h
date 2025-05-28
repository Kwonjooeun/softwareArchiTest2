#pragma once

#include "../Weapons/IWeapon.h"
#include "../EngagementManagers/IEngagementManager.h"
#include "../../Common/Types/CommonTypes.h"
#include <memory>
#include <functional>

namespace WeaponControl {

// =============================================================================
// 개별 발사관 클래스 (단순화된 컨테이너 역할)
// =============================================================================

class LaunchTube : public IStateObserver, public std::enable_shared_from_this<LaunchTube> {
public:
    explicit LaunchTube(uint16_t tubeNumber);
    ~LaunchTube() = default;
    
    // ==========================================================================
    // 기본 정보
    // ==========================================================================
    uint16_t getTubeNumber() const { return m_tubeNumber; }
    bool hasWeapon() const { return m_weapon != nullptr; }
    
    // ==========================================================================
    // 무장 관리
    // ==========================================================================
    Result<void> assignWeapon(WeaponPtr weapon, EngagementManagerPtr engagementMgr, const AssignmentInfo& assignmentInfo);
    void clearAssignment();
    std::shared_ptr<IWeapon> getWeapon() const { return m_weapon; }
    std::shared_ptr<IEngagementManager> getEngagementManager() const { return m_engagementMgr; }
    
    // ==========================================================================
    // 할당 정보
    // ==========================================================================
    const AssignmentInfo& getAssignmentInfo() const { return m_assignmentInfo; }
    Result<void> updateAssignmentInfo(const AssignmentInfo& info);
    
    // ==========================================================================
    // 환경 정보 업데이트
    // ==========================================================================
    void updateOwnShipInfo(const NAVINF_SHIP_NAVIGATION_INFO& ownShip);
    void updateTargetInfo(const TRKMGR_SYSTEMTARGET_INFO& target);
    void setAxisCenter(const GEO_POINT_2D& axisCenter);
    
    // ==========================================================================
    // 무장 통제 (위임)
    // ==========================================================================
    Result<void> requestWeaponStateChange(EN_WPN_CTRL_STATE newState, const CancellationToken& token = {});
    EN_WPN_CTRL_STATE getWeaponState() const;
    bool isLaunched() const;
    
    // ==========================================================================
    // 경로점 관리 (위임)
    // ==========================================================================
    Result<void> updateWaypoints(const std::vector<ST_WEAPON_WAYPOINT>& waypoints);
    
    // ==========================================================================
    // 교전계획 (위임)
    // ==========================================================================
    Result<void> calculateEngagementPlan();
    EngagementPlanResult getEngagementResult() const;
    bool isEngagementPlanValid() const;
    
    // ==========================================================================
    // 주기적 업데이트
    // ==========================================================================
    void update();
    
    // ==========================================================================
    // IStateObserver 구현
    // ==========================================================================
    void onStateChanged(uint16_t tubeNumber, EN_WPN_CTRL_STATE oldState, EN_WPN_CTRL_STATE newState) override;
    void onLaunchStatusChanged(uint16_t tubeNumber, bool launched) override;
    
    // ==========================================================================
    // 콜백 등록
    // ==========================================================================
    void setStateChangeCallback(std::function<void(uint16_t, EN_WPN_CTRL_STATE, EN_WPN_CTRL_STATE)> callback);
    void setLaunchStatusCallback(std::function<void(uint16_t, bool)> callback);
    void setEngagementPlanCallback(std::function<void(uint16_t, const EngagementPlanResult&)> callback);
    
    // ==========================================================================
    // 상태 정보 (단순화)
    // ==========================================================================
    LaunchTubeStatus getStatus() const;

private:
    // ==========================================================================
    // 멤버 변수
    // ==========================================================================
    uint16_t m_tubeNumber;
    
    std::shared_ptr<IWeapon> m_weapon;
    std::shared_ptr<IEngagementManager> m_engagementMgr;
    AssignmentInfo m_assignmentInfo;
    
    // 콜백 함수들
    std::function<void(uint16_t, EN_WPN_CTRL_STATE, EN_WPN_CTRL_STATE)> m_stateChangeCallback;
    std::function<void(uint16_t, bool)> m_launchStatusCallback;
    std::function<void(uint16_t, const EngagementPlanResult&)> m_engagementPlanCallback;
    
    // 마지막 교전계획 결과 (변화 감지용)
    mutable EngagementPlanResult m_lastEngagementResult;
    
    // ==========================================================================
    // 헬퍼 함수들
    // ==========================================================================
    Result<void> setupMineSpecificAssignment();
    Result<void> setupMissileSpecificAssignment();
    void notifyEngagementPlanChange();
};

// =============================================================================
// LaunchTube 구현
// =============================================================================

inline LaunchTube::LaunchTube(uint16_t tubeNumber)
    : m_tubeNumber(tubeNumber)
    , m_weapon(nullptr)
    , m_engagementMgr(nullptr)
{
    std::cout << "LaunchTube " << tubeNumber << " created" << std::endl;
}

inline Result<void> LaunchTube::assignWeapon(WeaponPtr weapon, EngagementManagerPtr engagementMgr, const AssignmentInfo& assignmentInfo) {
    if (!weapon || !engagementMgr) {
        return Result<void>::failure("Invalid weapon or engagement manager");
    }
    
    if (hasWeapon()) {
        return Result<void>::failure("Tube " + std::to_string(m_tubeNumber) + " already has assigned weapon");
    }
    
    if (assignmentInfo.tubeNumber != m_tubeNumber) {
        return Result<void>::failure("Assignment info tube number mismatch");
    }
    
    // 무장과 교전계획 관리자를 shared_ptr로 저장
    m_weapon = std::move(weapon);
    m_engagementMgr = std::move(engagementMgr);
    m_assignmentInfo = assignmentInfo;
    
    // 무장 초기화
    auto weaponResult = m_weapon->initialize(m_tubeNumber);
    if (!weaponResult) {
        clearAssignment();
        return Result<void>::failure("Failed to initialize weapon: " + weaponResult.error().message);
    }
    
    // 교전계획 관리자 초기화
    auto engagementResult = m_engagementMgr->initialize(m_tubeNumber, assignmentInfo.weaponKind);
    if (!engagementResult) {
        clearAssignment();
        return Result<void>::failure("Failed to initialize engagement manager: " + engagementResult.error().message);
    }
    
    // 관찰자 등록
    m_weapon->addStateObserver(shared_from_this());
    
    // 무장별 특화 설정
    Result<void> setupResult;
    if (assignmentInfo.weaponKind == EN_WPN_KIND::WPN_KIND_M_MINE) {
        setupResult = setupMineSpecificAssignment();
    } else {
        setupResult = setupMissileSpecificAssignment();
    }
    
    if (!setupResult) {
        clearAssignment();
        return setupResult;
    }
    
    std::cout << "Weapon " << WeaponKindToString(assignmentInfo.weaponKind)
              << " assigned to tube " << m_tubeNumber << std::endl;
    
    return Result<void>::success();
}

inline void LaunchTube::clearAssignment() {
    if (m_weapon) {
        m_weapon->removeStateObserver(shared_from_this());
        m_weapon->reset();
    }
    
    if (m_engagementMgr) {
        m_engagementMgr->reset();
    }
    
    m_weapon.reset();
    m_engagementMgr.reset();
    m_assignmentInfo = AssignmentInfo();
    
    std::cout << "Assignment cleared for tube " << m_tubeNumber << std::endl;
}

inline Result<void> LaunchTube::updateAssignmentInfo(const AssignmentInfo& info) {
    if (!hasWeapon()) {
        return Result<void>::failure("No weapon assigned to tube " + std::to_string(m_tubeNumber));
    }
    
    m_assignmentInfo = info;
    
    // 무장별 특화 업데이트
    if (info.weaponKind == EN_WPN_KIND::WPN_KIND_M_MINE) {
        return setupMineSpecificAssignment();
    } else {
        return setupMissileSpecificAssignment();
    }
}

inline void LaunchTube::updateOwnShipInfo(const NAVINF_SHIP_NAVIGATION_INFO& ownShip) {
    if (m_engagementMgr) {
        m_engagementMgr->updateOwnShipInfo(ownShip);
    }
}

inline void LaunchTube::updateTargetInfo(const TRKMGR_SYSTEMTARGET_INFO& target) {
    if (m_engagementMgr) {
        // 미사일 타입만 표적 정보 업데이트
        if (auto missileManager = std::dynamic_pointer_cast<IMissileEngagementManager>(m_engagementMgr)) {
            missileManager->updateTargetInfo(target);
        }
    }
}

inline void LaunchTube::setAxisCenter(const GEO_POINT_2D& axisCenter) {
    if (m_engagementMgr) {
        m_engagementMgr->setAxisCenter(axisCenter);
    }
}

inline Result<void> LaunchTube::requestWeaponStateChange(EN_WPN_CTRL_STATE newState, const CancellationToken& token) {
    if (!hasWeapon()) {
        return Result<void>::failure("No weapon assigned to tube " + std::to_string(m_tubeNumber));
    }
    
    return m_weapon->requestStateChange(newState, token);
}

inline EN_WPN_CTRL_STATE LaunchTube::getWeaponState() const {
    if (!hasWeapon()) {
        return EN_WPN_CTRL_STATE::WPN_CTRL_STATE_OFF;
    }
    
    return m_weapon->getCurrentState();
}

inline bool LaunchTube::isLaunched() const {
    if (!hasWeapon()) {
        return false;
    }
    
    return m_weapon->isLaunched();
}

inline Result<void> LaunchTube::updateWaypoints(const std::vector<ST_WEAPON_WAYPOINT>& waypoints) {
    if (!hasWeapon()) {
        return Result<void>::failure("No weapon assigned to tube " + std::to_string(m_tubeNumber));
    }
    
    // 무장별 특화 처리
    if (m_assignmentInfo.weaponKind == EN_WPN_KIND::WPN_KIND_M_MINE) {
        if (auto mineManager = std::dynamic_pointer_cast<IMineEngagementManager>(m_engagementMgr)) {
            return mineManager->updateDropPlanWaypoints(waypoints);
        }
    } else {
        if (auto missileManager = std::dynamic_pointer_cast<IMissileEngagementManager>(m_engagementMgr)) {
            return missileManager->updateWaypoints(waypoints);
        }
    }
    
    return Result<void>::failure("Failed to update waypoints");
}

inline Result<void> LaunchTube::calculateEngagementPlan() {
    if (!hasWeapon()) {
        return Result<void>::failure("No weapon assigned to tube " + std::to_string(m_tubeNumber));
    }
    
    auto result = m_engagementMgr->calculateEngagementPlan();
    
    if (result.isSuccess()) {
        // 교전계획이 준비되었음을 무장에 알림
        m_weapon->setFireSolutionReady(m_engagementMgr->isEngagementPlanValid());
        
        // 변화 확인 및 콜백 호출
        notifyEngagementPlanChange();
    }
    
    return result;
}

inline EngagementPlanResult LaunchTube::getEngagementResult() const {
    if (!hasWeapon()) {
        EngagementPlanResult emptyResult;
        emptyResult.tubeNumber = m_tubeNumber;
        return emptyResult;
    }
    
    return m_engagementMgr->getEngagementResult();
}

inline bool LaunchTube::isEngagementPlanValid() const {
    if (!hasWeapon()) {
        return false;
    }
    
    return m_engagementMgr->isEngagementPlanValid();
}

inline void LaunchTube::update() {
    if (!hasWeapon()) {
        return;
    }
    
    // 무장 업데이트
    m_weapon->update();
    
    // 교전계획 업데이트
    m_engagementMgr->update();
    
    // 정기적으로 교전계획 재계산 (발사 전에만)
    if (!m_weapon->isLaunched()) {
        calculateEngagementPlan();
    }
}

inline void LaunchTube::onStateChanged(uint16_t tubeNumber, EN_WPN_CTRL_STATE oldState, EN_WPN_CTRL_STATE newState) {
    if (tubeNumber != m_tubeNumber) {
        return;
    }
    
    std::cout << "Tube " << m_tubeNumber << " weapon state changed: "
              << StateToString(oldState) << " -> " << StateToString(newState) << std::endl;
    
    if (m_stateChangeCallback) {
        m_stateChangeCallback(tubeNumber, oldState, newState);
    }
}

inline void LaunchTube::onLaunchStatusChanged(uint16_t tubeNumber, bool launched) {
    if (tubeNumber != m_tubeNumber) {
        return;
    }
    
    std::cout << "Tube " << m_tubeNumber << " launch status changed: "
              << (launched ? "LAUNCHED" : "NOT_LAUNCHED") << std::endl;
    
    if (launched && m_engagementMgr) {
        m_engagementMgr->setLaunched(true);
    }
    
    if (m_launchStatusCallback) {
        m_launchStatusCallback(tubeNumber, launched);
    }
}

inline void LaunchTube::setStateChangeCallback(std::function<void(uint16_t, EN_WPN_CTRL_STATE, EN_WPN_CTRL_STATE)> callback) {
    m_stateChangeCallback = callback;
}

inline void LaunchTube::setLaunchStatusCallback(std::function<void(uint16_t, bool)> callback) {
    m_launchStatusCallback = callback;
}

inline void LaunchTube::setEngagementPlanCallback(std::function<void(uint16_t, const EngagementPlanResult&)> callback) {
    m_engagementPlanCallback = callback;
}

inline LaunchTubeStatus LaunchTube::getStatus() const {
    LaunchTubeStatus status;
    status.tubeNumber = m_tubeNumber;
    status.hasWeapon = hasWeapon();
    
    if (hasWeapon()) {
        status.weaponKind = m_weapon->getWeaponKind();
        status.weaponState = m_weapon->getCurrentState();
        status.launched = m_weapon->isLaunched();
        status.engagementPlanValid = m_engagementMgr->isEngagementPlanValid();
    } else {
        status.weaponKind = EN_WPN_KIND::WPN_KIND_NA;
        status.weaponState = EN_WPN_CTRL_STATE::WPN_CTRL_STATE_OFF;
        status.launched = false;
        status.engagementPlanValid = false;
    }
    
    return status;
}

inline Result<void> LaunchTube::setupMineSpecificAssignment() {
    auto mineManager = std::dynamic_pointer_cast<IMineEngagementManager>(m_engagementMgr);
    if (!mineManager) {
        return Result<void>::failure("Invalid mine engagement manager");
    }
    
    // 부설계획 설정
    if (m_assignmentInfo.dropPlanListNumber > 0 && m_assignmentInfo.dropPlanNumber > 0) {
        auto result = mineManager->setDropPlan(m_assignmentInfo.dropPlanListNumber, m_assignmentInfo.dropPlanNumber);
        if (!result) {
            return result;
        }
    }
    
    return Result<void>::success();
}

inline Result<void> LaunchTube::setupMissileSpecificAssignment() {
    auto missileManager = std::dynamic_pointer_cast<IMissileEngagementManager>(m_engagementMgr);
    if (!missileManager) {
        return Result<void>::failure("Invalid missile engagement manager");
    }
    
    // 표적 설정
    if (m_assignmentInfo.systemTargetId > 0) {
        // 시스템 표적 설정
        auto result = missileManager->setSystemTarget(m_assignmentInfo.systemTargetId);
        if (!result) {
            return result;
        }
    } else {
        // 직접 위치 설정
        auto result = missileManager->setTargetPosition(m_assignmentInfo.targetPos);
        if (!result) {
            return result;
        }
    }
    
    return Result<void>::success();
}

inline void LaunchTube::notifyEngagementPlanChange() {
    auto currentResult = getEngagementResult();
    
    // 결과가 변경되었는지 확인 (간단한 비교)
    bool changed = (currentResult.isValid != m_lastEngagementResult.isValid) ||
                   (currentResult.totalTime_sec != m_lastEngagementResult.totalTime_sec) ||
                   (currentResult.trajectory.size() != m_lastEngagementResult.trajectory.size());
    
    if (changed && m_engagementPlanCallback) {
        m_engagementPlanCallback(m_tubeNumber, currentResult);
        m_lastEngagementResult = currentResult;
    }
}

} // namespace WeaponControl
