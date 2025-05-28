#include "IEngagementManager.h"
#include "../../Infrastructure/Configuration/SystemConfig.h"
#include <cmath>
#include <iostream>

namespace WeaponControl {

// =============================================================================
// EngagementManagerBase 구현
// =============================================================================

EngagementManagerBase::EngagementManagerBase(EN_WPN_KIND weaponKind)
    : m_tubeNumber(0)
    , m_weaponKind(weaponKind)
    , m_launched(false)
    , m_axisCenter{0.0, 0.0}
    , m_launchTime(0.0f)
    , m_launchStartTime(std::chrono::steady_clock::now())
{
    std::cout << "EngagementManagerBase created for " << WeaponKindToString(weaponKind) << std::endl;
}

Result<void> EngagementManagerBase::initialize(uint16_t tubeNumber, EN_WPN_KIND weaponKind) {
    m_tubeNumber = tubeNumber;
    m_weaponKind = weaponKind;
    m_launched = false;
    
    // 교전계획 결과 초기화
    m_engagementResult.tubeNumber = tubeNumber;
    m_engagementResult.weaponKind = weaponKind;
    m_engagementResult.isValid = false;
    
    std::cout << "EngagementManager initialized for tube " << tubeNumber 
              << " with weapon " << WeaponKindToString(weaponKind) << std::endl;
              
    return Result<void>::success();
}

void EngagementManagerBase::reset() {
    m_launched = false;
    m_launchTime = 0.0f;
    m_launchStartTime = std::chrono::steady_clock::now();
    
    // 교전계획 결과 초기화
    m_engagementResult = EngagementPlanResult();
    m_engagementResult.tubeNumber = m_tubeNumber;
    m_engagementResult.weaponKind = m_weaponKind;
    
    // 경로점 및 위치 정보 초기화
    m_waypoints.clear();
    
    std::cout << "EngagementManager reset for tube " << m_tubeNumber << std::endl;
}

void EngagementManagerBase::update() {
    if (m_launched) {
        // 발사 후 위치 추적
        auto now = std::chrono::steady_clock::now();
        float timeSinceLaunch = std::chrono::duration<float>(now - m_launchStartTime).count();
        m_engagementResult.currentPosition = interpolatePosition(timeSinceLaunch);
    }
}

double EngagementManagerBase::calculateDistance(const ST_3D_GEODETIC_POSITION& p1, const ST_3D_GEODETIC_POSITION& p2) const {
    // 하버사인 공식을 사용한 거리 계산
    const double R = 6371000.0; // 지구 반지름 (미터)
    
    double lat1 = p1.dLatitude() * M_PI / 180.0;
    double lat2 = p2.dLatitude() * M_PI / 180.0;
    double deltaLat = (p2.dLatitude() - p1.dLatitude()) * M_PI / 180.0;
    double deltaLon = (p2.dLongitude() - p1.dLongitude()) * M_PI / 180.0;
    
    double a = sin(deltaLat / 2) * sin(deltaLat / 2) +
               cos(lat1) * cos(lat2) *
               sin(deltaLon / 2) * sin(deltaLon / 2);
    double c = 2 * atan2(sqrt(a), sqrt(1 - a));
    
    return R * c;
}

double EngagementManagerBase::calculateBearing(const ST_3D_GEODETIC_POSITION& from, const ST_3D_GEODETIC_POSITION& to) const {
    // 방위각 계산 (도 단위)
    double lat1 = from.dLatitude() * M_PI / 180.0;
    double lat2 = to.dLatitude() * M_PI / 180.0;
    double deltaLon = (to.dLongitude() - from.dLongitude()) * M_PI / 180.0;
    
    double y = sin(deltaLon) * cos(lat2);
    double x = cos(lat1) * sin(lat2) - sin(lat1) * cos(lat2) * cos(deltaLon);
    
    double bearing = atan2(y, x) * 180.0 / M_PI;
    
    // 0~360도 범위로 정규화
    return fmod(bearing + 360.0, 360.0);
}

// =============================================================================
// MineEngagementManagerBase 구현
// =============================================================================

MineEngagementManagerBase::MineEngagementManagerBase()
    : EngagementManagerBase(EN_WPN_KIND::WPN_KIND_M_MINE)
    , m_dropPlanListNumber(0)
    , m_dropPlanNumber(0)
{
}

Result<void> MineEngagementManagerBase::setDropPlan(uint32_t listNum, uint32_t planNum) {
    m_dropPlanListNumber = listNum;
    m_dropPlanNumber = planNum;
    
    // TODO: MineDropPlanService에서 실제 계획 로드
    // 지금은 임시로 기본값 설정
    m_dropPlan.sListID() = planNum;
    m_dropPlan.usDroppingPlanNumber() = planNum;
    
    std::cout << "Drop plan set: List " << listNum << ", Plan " << planNum << std::endl;
    return Result<void>::success();
}

Result<void> MineEngagementManagerBase::updateDropPlanWaypoints(const std::vector<ST_WEAPON_WAYPOINT>& waypoints) {
    if (waypoints.size() > 8) {
        return Result<void>::failure("Too many waypoints for mine (max 8)");
    }
    
    m_waypoints = waypoints;
    
    // 부설계획에도 반영
    m_dropPlan.usWaypointCnt() = waypoints.size();
    for (size_t i = 0; i < waypoints.size() && i < 8; ++i) {
        m_dropPlan.stWaypoint()[i] = waypoints[i];
    }
    
    // TODO: MineDropPlanService를 통해 JSON 파일 업데이트
    
    return calculateEngagementPlan();
}

Result<void> MineEngagementManagerBase::getDropPlan(ST_M_MINE_PLAN_INFO& planInfo) const {
    planInfo = m_dropPlan;
    return Result<void>::success();
}

Result<void> MineEngagementManagerBase::calculateEngagementPlan() {
    // 자항기뢰 교전계획 계산
    return calculateTrajectory();
}

Result<AIEP_M_MINE_EP_RESULT> MineEngagementManagerBase::getMineEngagementResult() const {
    AIEP_M_MINE_EP_RESULT result;
    
    // 기본 정보 설정
    result.enTubeNum() = m_tubeNumber;
    result.fEstimatedDrivingTime() = m_engagementResult.totalTime_sec;
    result.fRemainingTime() = m_engagementResult.timeToTarget_sec;
    result.bValidMslPos() = m_engagementResult.isValid && m_launched;
    
    // 현재 위치 설정
    if (m_launched) {
        result.MslPos() = m_engagementResult.currentPosition;
    }
    
    // 다음 경로점 정보
    result.numberOfNextWP() = m_engagementResult.nextWaypointIndex;
    result.timeToNextWP() = m_engagementResult.timeToNextWaypoint_sec;
    
    // 궤적 정보
    result.unCntTrajectory() = m_engagementResult.trajectory.size();
    for (size_t i = 0; i < m_engagementResult.trajectory.size() && i < 128; ++i) {
        result.stTrajectories()[i] = m_engagementResult.trajectory[i];
    }
    
    // 경로점 정보
    result.unCntWaypoint() = m_waypoints.size();
    for (size_t i = 0; i < m_waypoints.size() && i < 8; ++i) {
        result.stWaypoints()[i] = m_waypoints[i];
    }
    
    // 발사/부설 지점
    result.stLaunchPos() = m_engagementResult.launchPosition;
    result.stDropPos() = m_engagementResult.targetPosition;
    
    return Result<AIEP_M_MINE_EP_RESULT>::success(std::move(result));
}

// =============================================================================
// MissileEngagementManagerBase 구현
// =============================================================================

MissileEngagementManagerBase::MissileEngagementManagerBase(EN_WPN_KIND weaponKind)
    : EngagementManagerBase(weaponKind)
    , m_systemTargetId(0)
    , m_hasValidTarget(false)
{
    // 표적 위치 초기화
    m_targetPosition.dLatitude() = 0.0;
    m_targetPosition.dLongitude() = 0.0;
    m_targetPosition.fAltitude() = 0.0;
}

Result<void> MissileEngagementManagerBase::setTargetPosition(const SGEODETIC_POSITION& targetPos) {
    m_targetPosition = targetPos;
    m_systemTargetId = 0; // 직접 위치 지정이므로 시스템 표적 ID는 무효
    m_hasValidTarget = true;
    
    // 교전계획 재계산
    return calculateEngagementPlan();
}

Result<void> MissileEngagementManagerBase::setSystemTarget(uint32_t systemTargetId) {
    m_systemTargetId = systemTargetId;
    m_hasValidTarget = false; // 실제 표적 정보를 받아야 유효해짐
    
    std::cout << "System target ID set: " << systemTargetId << std::endl;
    return Result<void>::success();
}

void MissileEngagementManagerBase::updateTargetInfo(const TRKMGR_SYSTEMTARGET_INFO& target) {
    if (m_systemTargetId != 0 && target.unTargetSystemID() == m_systemTargetId) {
        m_targetInfo = target;
        
        // 시스템 표적 위치를 표적 위치로 설정
        m_targetPosition.dLatitude() = target.stGeodeticPosition().dLatitude();
        m_targetPosition.dLongitude() = target.stGeodeticPosition().dLongitude();
        m_targetPosition.fAltitude() = -target.stGeodeticPosition().fDepth(); // 심도를 고도로 변환
        
        m_hasValidTarget = true;
        
        // 교전계획 재계산
        calculateEngagementPlan();
        
        std::cout << "Target info updated for system target " << m_systemTargetId << std::endl;
    }
}

Result<void> MissileEngagementManagerBase::updateWaypoints(const std::vector<ST_WEAPON_WAYPOINT>& waypoints) {
    if (waypoints.size() > 8) {
        return Result<void>::failure("Too many waypoints for missile (max 8)");
    }
    
    m_waypoints = waypoints;
    
    // 교전계획 재계산
    return calculateEngagementPlan();
}

bool MissileEngagementManagerBase::hasValidTarget() const {
    return m_hasValidTarget;
}

Result<void> MissileEngagementManagerBase::calculateEngagementPlan() {
    if (!m_hasValidTarget) {
        m_engagementResult.isValid = false;
        return Result<void>::failure("No valid target set");
    }
    
    // 미사일 교전계획 계산
    return calculateTrajectory();
}

Result<AIEP_ALM_ASM_EP_RESULT> MissileEngagementManagerBase::getMissileEngagementResult() const {
    AIEP_ALM_ASM_EP_RESULT result;
    
    // 기본 정보 설정
    result.enTubeNum() = m_tubeNumber;
    result.bValidMslPos() = m_engagementResult.isValid && m_launched;
    
    // 현재 위치 설정
    if (m_launched) {
        result.MslPos() = m_engagementResult.currentPosition;
    }
    
    // 다음 경로점 정보
    result.numberOfNextWP() = m_engagementResult.nextWaypointIndex;
    result.timeToNextWP() = m_engagementResult.timeToNextWaypoint_sec;
    
    // 궤적 정보
    result.unCntTrajectory() = m_engagementResult.trajectory.size();
    for (size_t i = 0; i < m_engagementResult.trajectory.size() && i < 128; ++i) {
        result.stTrajectories()[i] = m_engagementResult.trajectory[i];
    }
    
    // 경로점 정보
    result.unCntWaypoint() = m_waypoints.size();
    for (size_t i = 0; i < m_waypoints.size() && i < 8; ++i) {
        // ST_WEAPON_WAYPOINT을 ST_3D_GEODETIC_POSITION으로 변환
        ST_3D_GEODETIC_POSITION geoPos;
        geoPos.dLatitude() = m_waypoints[i].dLatitude();
        geoPos.dLongitude() = m_waypoints[i].dLongitude();
        geoPos.fDepth() = m_waypoints[i].fDepth();
        result.stWaypoints()[i] = geoPos;
    }
    
    // 선회점 계산 및 설정
    auto turningPoints = calculateTurningPoints();
    result.unCntTurningpoints() = turningPoints.size();
    for (size_t i = 0; i < turningPoints.size() && i < 16; ++i) {
        result.stTurningpoints()[i] = turningPoints[i];
    }
    
    return Result<AIEP_ALM_ASM_EP_RESULT>::success(std::move(result));
}

} // namespace WeaponControl
