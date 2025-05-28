#pragma once

#include "../../Common/Types/CommonTypes.h"
#include <memory>
#include <chrono>

namespace WeaponControl {

// =============================================================================
// 기본 교전계획 관리자 인터페이스
// =============================================================================

class IEngagementManager {
public:
    virtual ~IEngagementManager() = default;
    
    // ==========================================================================
    // 초기화 및 리셋
    // ==========================================================================
    virtual Result<void> initialize(uint16_t tubeNumber, EN_WPN_KIND weaponKind) = 0;
    virtual void reset() = 0;
    
    // ==========================================================================
    // 교전계획 계산
    // ==========================================================================
    virtual Result<void> calculateEngagementPlan() = 0;
    virtual EngagementPlanResult getEngagementResult() const = 0;
    virtual bool isEngagementPlanValid() const = 0;
    
    // ==========================================================================
    // 환경 정보 업데이트
    // ==========================================================================
    virtual void updateOwnShipInfo(const NAVINF_SHIP_NAVIGATION_INFO& ownShip) = 0;
    virtual void setAxisCenter(const GEO_POINT_2D& axisCenter) = 0;
    
    // ==========================================================================
    // 발사 후 추적
    // ==========================================================================
    virtual void setLaunched(bool launched) = 0;
    virtual bool isLaunched() const = 0;
    virtual ST_3D_GEODETIC_POSITION getCurrentPosition(float timeSinceLaunch) const = 0;
    
    // ==========================================================================
    // 주기적 업데이트
    // ==========================================================================
    virtual void update() = 0;
    
    // ==========================================================================
    // 무장별 특수 기능 확인
    // ==========================================================================
    virtual bool supportsWaypointModification() const { return true; }
    virtual bool requiresPrePlanning() const { return false; }
    
    // ==========================================================================
    // 기본 정보
    // ==========================================================================
    virtual uint16_t getTubeNumber() const = 0;
    virtual EN_WPN_KIND getWeaponKind() const = 0;
};

// =============================================================================
// 자항기뢰 전용 교전계획 관리자 인터페이스
// =============================================================================

class IMineEngagementManager : public IEngagementManager {
public:
    virtual ~IMineEngagementManager() = default;
    
    // ==========================================================================
    // 부설계획 관리
    // ==========================================================================
    virtual Result<void> setDropPlan(uint32_t listNum, uint32_t planNum) = 0;
    virtual Result<void> updateDropPlanWaypoints(const std::vector<ST_WEAPON_WAYPOINT>& waypoints) = 0;
    virtual Result<void> getDropPlan(ST_M_MINE_PLAN_INFO& planInfo) const = 0;
    
    // ==========================================================================
    // 자항기뢰 특화 결과
    // ==========================================================================
    virtual Result<AIEP_M_MINE_EP_RESULT> getMineEngagementResult() const = 0;
    
    // ==========================================================================
    // 자항기뢰 특화 기능
    // ==========================================================================
    bool requiresPrePlanning() const override { return true; }
    
    virtual uint32_t getDropPlanListNumber() const = 0;
    virtual uint32_t getDropPlanNumber() const = 0;
};

// =============================================================================
// 미사일 전용 교전계획 관리자 인터페이스 (ALM, ASM, AAM)
// =============================================================================

class IMissileEngagementManager : public IEngagementManager {
public:
    virtual ~IMissileEngagementManager() = default;
    
    // ==========================================================================
    // 표적 관리
    // ==========================================================================
    virtual Result<void> setTargetPosition(const SGEODETIC_POSITION& targetPos) = 0;
    virtual Result<void> setSystemTarget(uint32_t systemTargetId) = 0;
    virtual void updateTargetInfo(const TRKMGR_SYSTEMTARGET_INFO& target) = 0;
    
    // ==========================================================================
    // 경로점 관리
    // ==========================================================================
    virtual Result<void> updateWaypoints(const std::vector<ST_WEAPON_WAYPOINT>& waypoints) = 0;
    virtual std::vector<ST_WEAPON_WAYPOINT> getWaypoints() const = 0;
    
    // ==========================================================================
    // 미사일 특화 결과
    // ==========================================================================
    virtual Result<AIEP_ALM_ASM_EP_RESULT> getMissileEngagementResult() const = 0;
    
    // ==========================================================================
    // 표적 정보
    // ==========================================================================
    virtual uint32_t getSystemTargetId() const = 0;
    virtual SGEODETIC_POSITION getTargetPosition() const = 0;
    virtual bool hasValidTarget() const = 0;
};

// =============================================================================
// 교전계획 관리자 기반 클래스
// =============================================================================

class EngagementManagerBase : public IEngagementManager {
public:
    explicit EngagementManagerBase(EN_WPN_KIND weaponKind);
    virtual ~EngagementManagerBase() = default;
    
    // ==========================================================================
    // 공통 구현
    // ==========================================================================
    Result<void> initialize(uint16_t tubeNumber, EN_WPN_KIND weaponKind) override;
    void reset() override;
    
    void setAxisCenter(const GEO_POINT_2D& axisCenter) override { m_axisCenter = axisCenter; }
    void updateOwnShipInfo(const NAVINF_SHIP_NAVIGATION_INFO& ownShip) override { m_ownShipInfo = ownShip; }
    
    void setLaunched(bool launched) override { m_launched = launched; }
    bool isLaunched() const override { return m_launched; }
    
    EngagementPlanResult getEngagementResult() const override { return m_engagementResult; }
    bool isEngagementPlanValid() const override { return m_engagementResult.isValid; }
    
    uint16_t getTubeNumber() const override { return m_tubeNumber; }
    EN_WPN_KIND getWeaponKind() const override { return m_weaponKind; }
    
    void update() override;
    
protected:
    // ==========================================================================
    // 파생 클래스에서 구현해야 할 순수 가상 함수
    // ==========================================================================
    virtual Result<void> calculateTrajectory() = 0;
    virtual ST_3D_GEODETIC_POSITION interpolatePosition(float timeSinceLaunch) const = 0;
    
    // ==========================================================================
    // 유틸리티 함수
    // ==========================================================================
    double calculateDistance(const ST_3D_GEODETIC_POSITION& p1, const ST_3D_GEODETIC_POSITION& p2) const;
    double calculateBearing(const ST_3D_GEODETIC_POSITION& from, const ST_3D_GEODETIC_POSITION& to) const;
    
    // ==========================================================================
    // 멤버 변수
    // ==========================================================================
    uint16_t m_tubeNumber;
    EN_WPN_KIND m_weaponKind;
    bool m_launched;
    
    GEO_POINT_2D m_axisCenter;
    EngagementPlanResult m_engagementResult;
    
    std::vector<ST_WEAPON_WAYPOINT> m_waypoints;
    ST_3D_GEODETIC_POSITION m_launchPosition;
    ST_3D_GEODETIC_POSITION m_targetPosition;
    
    NAVINF_SHIP_NAVIGATION_INFO m_ownShipInfo;
    
    float m_launchTime;
    std::chrono::steady_clock::time_point m_launchStartTime;
};

// =============================================================================
// 자항기뢰 교전계획 관리자 기반 클래스
// =============================================================================

class MineEngagementManagerBase : public EngagementManagerBase, public IMineEngagementManager {
public:
    explicit MineEngagementManagerBase();
    
    // IMineEngagementManager 구현
    Result<void> setDropPlan(uint32_t listNum, uint32_t planNum) override;
    Result<void> updateDropPlanWaypoints(const std::vector<ST_WEAPON_WAYPOINT>& waypoints) override;
    Result<void> getDropPlan(ST_M_MINE_PLAN_INFO& planInfo) const override;
    
    uint32_t getDropPlanListNumber() const override { return m_dropPlanListNumber; }
    uint32_t getDropPlanNumber() const override { return m_dropPlanNumber; }
    
    // 자항기뢰 특화 계산
    Result<void> calculateEngagementPlan() override;
    Result<AIEP_M_MINE_EP_RESULT> getMineEngagementResult() const override;
    
protected:
    uint32_t m_dropPlanListNumber;
    uint32_t m_dropPlanNumber;
    ST_M_MINE_PLAN_INFO m_dropPlan;
};

// =============================================================================
// 미사일 교전계획 관리자 기반 클래스
// =============================================================================

class MissileEngagementManagerBase : public EngagementManagerBase, public IMissileEngagementManager {
public:
    explicit MissileEngagementManagerBase(EN_WPN_KIND weaponKind);
    
    // IMissileEngagementManager 구현
    Result<void> setTargetPosition(const SGEODETIC_POSITION& targetPos) override;
    Result<void> setSystemTarget(uint32_t systemTargetId) override;
    void updateTargetInfo(const TRKMGR_SYSTEMTARGET_INFO& target) override;
    
    Result<void> updateWaypoints(const std::vector<ST_WEAPON_WAYPOINT>& waypoints) override;
    std::vector<ST_WEAPON_WAYPOINT> getWaypoints() const override { return m_waypoints; }
    
    uint32_t getSystemTargetId() const override { return m_systemTargetId; }
    SGEODETIC_POSITION getTargetPosition() const override { return m_targetPosition; }
    bool hasValidTarget() const override;
    
    // 미사일 특화 계산
    Result<void> calculateEngagementPlan() override;
    Result<AIEP_ALM_ASM_EP_RESULT> getMissileEngagementResult() const override;
    
protected:
    uint32_t m_systemTargetId;
    SGEODETIC_POSITION m_targetPosition;
    TRKMGR_SYSTEMTARGET_INFO m_targetInfo;
    bool m_hasValidTarget;
    
    // 선회점 계산 (파생 클래스에서 구체적 구현)
    virtual std::vector<ST_3D_GEODETIC_POSITION> calculateTurningPoints() const = 0;
};

} // namespace WeaponControl
