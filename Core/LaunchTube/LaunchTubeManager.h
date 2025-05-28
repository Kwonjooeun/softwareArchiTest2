#pragma once

#include "LaunchTube.h"
#include "../../Common/Types/CommonTypes.h"
#include "../../Infrastructure/Configuration/SystemConfig.h"
#include <array>
#include <memory>
#include <vector>
#include <functional>
#include <mutex>
#include <shared_mutex>

namespace WeaponControl {

// =============================================================================
// 발사관 관리자 인터페이스
// =============================================================================

class ILaunchTubeManager {
public:
    virtual ~ILaunchTubeManager() = default;
    
    // 초기화
    virtual Result<void> initialize() = 0;
    virtual void shutdown() = 0;
    
    // 무장 할당 관리
    virtual Result<void> assignWeapon(const WeaponAssignmentRequest& request) = 0;
    virtual Result<void> unassignWeapon(uint16_t tubeNumber) = 0;
    virtual bool isAssigned(uint16_t tubeNumber) const = 0;
    virtual bool canAssignWeapon(uint16_t tubeNumber, EN_WPN_KIND weaponKind) const = 0;
    
    // 무장 상태 통제
    virtual Result<void> requestWeaponStateChange(const WeaponControlRequest& request) = 0;
    virtual Result<void> requestAllWeaponStateChange(EN_WPN_CTRL_STATE newState) = 0;
    virtual bool canChangeState(uint16_t tubeNumber, EN_WPN_CTRL_STATE newState) const = 0;
    virtual Result<void> emergencyStop() = 0;
    
    // 환경 정보 업데이트
    virtual void updateOwnShipInfo(const NAVINF_SHIP_NAVIGATION_INFO& ownShip) = 0;
    virtual void updateTargetInfo(const TRKMGR_SYSTEMTARGET_INFO& target) = 0;
    virtual void setAxisCenter(const GEO_POINT_2D& axisCenter) = 0;
    
    // 경로점 관리
    virtual Result<void> updateWaypoints(const WaypointUpdateRequest& request) = 0;
    
    // 교전계획 관리
    virtual Result<void> calculateEngagementPlan(uint16_t tubeNumber) = 0;
    virtual void calculateAllEngagementPlans() = 0;
    
    // 상태 조회
    virtual std::vector<LaunchTubeStatus> getAllTubeStatus() const = 0;
    virtual LaunchTubeStatus getTubeStatus(uint16_t tubeNumber) const = 0;
    virtual std::vector<EngagementPlanResult> getAllEngagementResults() const = 0;
    virtual EngagementPlanResult getEngagementResult(uint16_t tubeNumber) const = 0;
    
    // 발사관 조회
    virtual std::shared_ptr<LaunchTube> getLaunchTube(uint16_t tubeNumber) = 0;
    virtual std::shared_ptr<const LaunchTube> getLaunchTube(uint16_t tubeNumber) const = 0;
    virtual std::vector<std::shared_ptr<LaunchTube>> getAssignedTubes() const = 0;
    
    // 주기적 업데이트
    virtual void update() = 0;
    
    // 콜백 등록
    virtual void setStateChangeCallback(std::function<void(uint16_t, EN_WPN_CTRL_STATE, EN_WPN_CTRL_STATE)> callback) = 0;
    virtual void setLaunchStatusCallback(std::function<void(uint16_t, bool)> callback) = 0;
    virtual void setEngagementPlanCallback(std::function<void(uint16_t, const EngagementPlanResult&)> callback) = 0;
    virtual void setAssignmentChangeCallback(std::function<void(uint16_t, EN_WPN_KIND, bool)> callback) = 0;
    
    // 유틸리티
    virtual bool isValidTubeNumber(uint16_t tubeNumber) const = 0;
    virtual size_t getAssignedTubeCount() const = 0;
    virtual size_t getReadyTubeCount() const = 0;
};

// =============================================================================
// 발사관 관리자 구현 클래스
// =============================================================================

class LaunchTubeManager : public ILaunchTubeManager {
public:
    explicit LaunchTubeManager(uint16_t maxTubes = 0);
    ~LaunchTubeManager() = default;
    
    // ILaunchTubeManager 구현
    Result<void> initialize() override;
    void shutdown() override;
    
    Result<void> assignWeapon(const WeaponAssignmentRequest& request) override;
    Result<void> unassignWeapon(uint16_t tubeNumber) override;
    bool isAssigned(uint16_t tubeNumber) const override;
    bool canAssignWeapon(uint16_t tubeNumber, EN_WPN_KIND weaponKind) const override;
    
    Result<void> requestWeaponStateChange(const WeaponControlRequest& request) override;
    Result<void> requestAllWeaponStateChange(EN_WPN_CTRL_STATE newState) override;
    bool canChangeState(uint16_t tubeNumber, EN_WPN_CTRL_STATE newState) const override;
    Result<void> emergencyStop() override;
    
    void updateOwnShipInfo(const NAVINF_SHIP_NAVIGATION_INFO& ownShip) override;
    void updateTargetInfo(const TRKMGR_SYSTEMTARGET_INFO& target) override;
    void setAxisCenter(const GEO_POINT_2D& axisCenter) override;
    
    Result<void> updateWaypoints(const WaypointUpdateRequest& request) override;
    
    Result<void> calculateEngagementPlan(uint16_t tubeNumber) override;
    void calculateAllEngagementPlans() override;
    
    std::vector<LaunchTubeStatus> getAllTubeStatus() const override;
    LaunchTubeStatus getTubeStatus(uint16_t tubeNumber) const override;
    std::vector<EngagementPlanResult> getAllEngagementResults() const override;
    EngagementPlanResult getEngagementResult(uint16_t tubeNumber) const override;
    
    std::shared_ptr<LaunchTube> getLaunchTube(uint16_t tubeNumber) override;
    std::shared_ptr<const LaunchTube> getLaunchTube(uint16_t tubeNumber) const override;
    std::vector<std::shared_ptr<LaunchTube>> getAssignedTubes() const override;
    
    void update() override;
    
    void setStateChangeCallback(std::function<void(uint16_t, EN_WPN_CTRL_STATE, EN_WPN_CTRL_STATE)> callback) override;
    void setLaunchStatusCallback(std::function<void(uint16_t, bool)> callback) override;
    void setEngagementPlanCallback(std::function<void(uint16_t, const EngagementPlanResult&)> callback) override;
    void setAssignmentChangeCallback(std::function<void(uint16_t, EN_WPN_KIND, bool)> callback) override;
    
    bool isValidTubeNumber(uint16_t tubeNumber) const override;
    size_t getAssignedTubeCount() const override;
    size_t getReadyTubeCount() const override;

private:
    // 발사관 검증
    std::shared_ptr<LaunchTube> getValidatedTube(uint16_t tubeNumber);
    std::shared_ptr<const LaunchTube> getValidatedTube(uint16_t tubeNumber) const;
    
    // 콜백 전달
    void onTubeStateChanged(uint16_t tubeNumber, EN_WPN_CTRL_STATE oldState, EN_WPN_CTRL_STATE newState);
    void onTubeLaunchStatusChanged(uint16_t tubeNumber, bool launched);
    void onTubeEngagementPlanUpdated(uint16_t tubeNumber, const EngagementPlanResult& result);
    
    // 무장 생성 (WeaponFactory 사용)
    Result<std::pair<WeaponPtr, EngagementManagerPtr>> createWeaponAndManager(EN_WPN_KIND weaponKind);
    
    // 발사관 배열 (동적 크기)
    std::vector<std::shared_ptr<LaunchTube>> m_launchTubes;
    uint16_t m_maxTubes;
    uint16_t m_minTubeNumber;
    uint16_t m_maxTubeNumber;
    
    // 공통 환경 정보
    GEO_POINT_2D m_axisCenter;
    NAVINF_SHIP_NAVIGATION_INFO m_ownShipInfo;
    std::map<uint32_t, TRKMGR_SYSTEMTARGET_INFO> m_targetInfoMap;
    
    // 콜백 함수들
    std::function<void(uint16_t, EN_WPN_CTRL_STATE, EN_WPN_CTRL_STATE)> m_stateChangeCallback;
    std::function<void(uint16_t, bool)> m_launchStatusCallback;
    std::function<void(uint16_t, const EngagementPlanResult&)> m_engagementPlanCallback;
    std::function<void(uint16_t, EN_WPN_KIND, bool)> m_assignmentChangeCallback;
    
    // 스레드 안전성
    mutable std::shared_mutex m_tubesMutex;
    mutable std::shared_mutex m_environmentMutex;
    
    // 초기화 상태
    bool m_initialized;
};

} // namespace WeaponControl
