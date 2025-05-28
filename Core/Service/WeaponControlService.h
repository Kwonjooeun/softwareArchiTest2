#pragma once

#include "../LaunchTube/LaunchTubeManager.h"
#include "ITargetTrackingService.h"
#include "IMineDropPlanService.h"
#include "../../Common/Types/CommonTypes.h"
#include <memory>

namespace WeaponControl {

// =============================================================================
// 무장 통제 서비스 - 핵심 비즈니스 로직
// =============================================================================

class WeaponControlService {
public:
    explicit WeaponControlService(
        std::unique_ptr<ILaunchTubeManager> tubeManager,
        std::unique_ptr<ITargetTrackingService> targetService,
        std::unique_ptr<IMineDropPlanService> mineService
    );
    
    ~WeaponControlService() = default;
    
    // ==========================================================================
    // 초기화 및 종료
    // ==========================================================================
    Result<void> initialize();
    void shutdown();
    
    // ==========================================================================
    // 핵심 비즈니스 로직
    // ==========================================================================
    Result<void> assignWeapon(const TEWA_ASSIGN_CMD& assignCmd);
    Result<void> unassignWeapon(uint16_t tubeNumber);
    Result<void> controlWeapon(const CMSHCI_AIEP_WPN_CTRL_CMD& ctrlCmd);
    Result<void> updateWaypoints(const CMSHCI_AIEP_WPN_GEO_WAYPOINTS& waypointsMsg);
    Result<void> emergencyStop();
    
    // ==========================================================================
    // 환경 정보 업데이트
    // ==========================================================================
    void updateOwnShipInfo(const NAVINF_SHIP_NAVIGATION_INFO& ownShip);
    void updateTargetInfo(const TRKMGR_SYSTEMTARGET_INFO& target);
    void setAxisCenter(const GEO_POINT_2D& axisCenter);
    
    // ==========================================================================
    // 자항기뢰 부설계획 관리
    // ==========================================================================
    Result<void> processMineDropPlanRequest(const CMSHCI_AIEP_M_MINE_DROPPING_PLAN_REQ& request);
    Result<void> processEditedPlanList(const CMSHCI_AIEP_M_MINE_EDITED_PLAN_LIST& editedList);
    Result<void> processSelectedPlan(const CMSHCI_AIEP_M_MINE_SELECTED_PLAN& selectedPlan);
    
    // ==========================================================================
    // 조회 기능
    // ==========================================================================
    std::vector<LaunchTubeStatus> getAllTubeStatus() const;
    LaunchTubeStatus getTubeStatus(uint16_t tubeNumber) const;
    std::vector<EngagementPlanResult> getAllEngagementResults() const;
    EngagementPlanResult getEngagementResult(uint16_t tubeNumber) const;
    
    // ==========================================================================
    // 주기적 작업
    // ==========================================================================
    void update();
    void calculateAllEngagementPlans();
    
    // ==========================================================================
    // 콜백 등록
    // ==========================================================================
    void setStateChangeCallback(std::function<void(uint16_t, EN_WPN_CTRL_STATE, EN_WPN_CTRL_STATE)> callback);
    void setLaunchStatusCallback(std::function<void(uint16_t, bool)> callback);
    void setEngagementPlanCallback(std::function<void(uint16_t, const EngagementPlanResult&)> callback);
    void setAssignmentChangeCallback(std::function<void(uint16_t, EN_WPN_KIND, bool)> callback);
    
    // ==========================================================================
    // 통계 정보
    // ==========================================================================
    size_t getAssignedTubeCount() const;
    size_t getReadyTubeCount() const;
    
private:
    // ==========================================================================
    // DDS 메시지 변환 헬퍼
    // ==========================================================================
    Result<WeaponAssignmentRequest> convertAssignCommand(const TEWA_ASSIGN_CMD& assignCmd);
    Result<WeaponControlRequest> convertControlCommand(const CMSHCI_AIEP_WPN_CTRL_CMD& ctrlCmd);
    Result<WaypointUpdateRequest> convertWaypointCommand(const CMSHCI_AIEP_WPN_GEO_WAYPOINTS& waypointsMsg);
    
    // ==========================================================================
    // 할당 정보 추출 헬퍼
    // ==========================================================================
    AssignmentInfo extractAssignmentInfo(const TEWA_ASSIGN_CMD& assignCmd);
    
    // ==========================================================================
    // 멤버 변수
    // ==========================================================================
    std::unique_ptr<ILaunchTubeManager> m_tubeManager;
    std::unique_ptr<ITargetTrackingService> m_targetService;
    std::unique_ptr<IMineDropPlanService> m_mineService;
    
    uint32_t m_selectedPlanListNumber;
    bool m_initialized;
};

} // namespace WeaponControl
