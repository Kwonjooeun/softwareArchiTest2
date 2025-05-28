#pragma once

#include <variant>
#include <string>
#include <memory>
#include <atomic>
#include <vector>
#include <chrono>
#include <exception>

// 기본 타입들 (AIEP_AIEP_.hpp에서 가져온 것들)
#include "../../dds_message/AIEP_AIEP_.hpp"

namespace WeaponControl {

// =============================================================================
// Result 타입 - 일관된 에러 처리
// =============================================================================

struct ErrorInfo {
    std::string message;
    int code;
    
    ErrorInfo(const std::string& msg = "", int c = -1) 
        : message(msg), code(c) {}
};

template<typename T = void>
class Result {
private:
    std::variant<T, ErrorInfo> m_data;
    
    explicit Result(T&& value) : m_data(std::forward<T>(value)) {}
    explicit Result(const ErrorInfo& error) : m_data(error) {}
    
public:
    static Result<T> success(T&& value) { 
        return Result(std::forward<T>(value)); 
    }
    
    static Result<T> failure(const std::string& message, int code = -1) { 
        return Result(ErrorInfo{message, code}); 
    }
    
    bool isSuccess() const { return std::holds_alternative<T>(m_data); }
    bool isFailure() const { return !isSuccess(); }
    
    const T& value() const { return std::get<T>(m_data); }
    T& value() { return std::get<T>(m_data); }
    
    const ErrorInfo& error() const { return std::get<ErrorInfo>(m_data); }
    
    // 편의 연산자
    explicit operator bool() const { return isSuccess(); }
};

// void 특화
template<>
class Result<void> {
private:
    std::optional<ErrorInfo> m_error;
    
    explicit Result(const ErrorInfo& error) : m_error(error) {}
    Result() : m_error(std::nullopt) {}
    
public:
    static Result<void> success() { return Result(); }
    
    static Result<void> failure(const std::string& message, int code = -1) { 
        return Result(ErrorInfo{message, code}); 
    }
    
    bool isSuccess() const { return !m_error.has_value(); }
    bool isFailure() const { return m_error.has_value(); }
    
    const ErrorInfo& error() const { return m_error.value(); }
    
    explicit operator bool() const { return isSuccess(); }
};

// =============================================================================
// Cancellation Token - ABORT 우선순위 처리
// =============================================================================

class OperationCancelledException : public std::exception {
public:
    const char* what() const noexcept override {
        return "Operation was cancelled";
    }
};

class CancellationToken {
private:
    std::shared_ptr<std::atomic<bool>> m_cancelled;
    
public:
    CancellationToken() : m_cancelled(std::make_shared<std::atomic<bool>>(false)) {}
    
    void cancel() { m_cancelled->store(true); }
    bool isCancelled() const { return m_cancelled->load(); }
    
    void throwIfCancelled() const {
        if (isCancelled()) {
            throw OperationCancelledException();
        }
    }
    
    // 지정된 시간 동안 대기하면서 취소 확인
    template<typename Rep, typename Period>
    bool waitFor(const std::chrono::duration<Rep, Period>& duration) const {
        auto start = std::chrono::steady_clock::now();
        while (std::chrono::steady_clock::now() - start < duration) {
            if (isCancelled()) {
                return false; // 취소됨
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        return true; // 정상 완료
    }
};

// =============================================================================
// 무장 사양 정보
// =============================================================================

struct WeaponSpecification {
    std::string name;
    double maxRange_km;
    double speed_mps;
    double launchDelay_sec;
    std::vector<std::string> supportedModes;
    
    WeaponSpecification() 
        : name(""), maxRange_km(0.0), speed_mps(0.0), launchDelay_sec(0.0) {}
    
    WeaponSpecification(const std::string& n, double range, double speed, double delay)
        : name(n), maxRange_km(range), speed_mps(speed), launchDelay_sec(delay) {}
};

// =============================================================================
// 할당 정보
// =============================================================================

struct AssignmentInfo {
    uint16_t tubeNumber;
    EN_WPN_KIND weaponKind;
    uint32_t systemTargetId;        // 시스템 표적 번호 (0이면 무효)
    SGEODETIC_POSITION targetPos;   // 직접 지정된 표적 위치
    
    // 자항기뢰 전용
    uint32_t dropPlanListNumber;    // 부설계획 목록 번호
    uint32_t dropPlanNumber;        // 부설계획 번호
    
    AssignmentInfo() 
        : tubeNumber(0), weaponKind(EN_WPN_KIND::WPN_KIND_NA)
        , systemTargetId(0), dropPlanListNumber(0), dropPlanNumber(0) {
        // targetPos 초기화
        targetPos.dLatitude() = 0.0;
        targetPos.dLongitude() = 0.0;
        targetPos.fAltitude() = 0.0;
    }
};

// =============================================================================
// 발사관 상태 정보 (단순화)
// =============================================================================

struct LaunchTubeStatus {
    uint16_t tubeNumber;
    bool hasWeapon;
    EN_WPN_KIND weaponKind;
    EN_WPN_CTRL_STATE weaponState;
    bool launched;
    bool engagementPlanValid;
    
    LaunchTubeStatus()
        : tubeNumber(0), hasWeapon(false), weaponKind(EN_WPN_KIND::WPN_KIND_NA)
        , weaponState(EN_WPN_CTRL_STATE::WPN_CTRL_STATE_OFF)
        , launched(false), engagementPlanValid(false) {}
};

// =============================================================================
// 교전계획 결과
// =============================================================================

struct EngagementPlanResult {
    uint16_t tubeNumber;
    EN_WPN_KIND weaponKind;
    bool isValid;
    float totalTime_sec;
    float timeToTarget_sec;
    uint32_t nextWaypointIndex;
    float timeToNextWaypoint_sec;
    
    std::vector<ST_3D_GEODETIC_POSITION> trajectory;
    std::vector<ST_WEAPON_WAYPOINT> waypoints;
    ST_3D_GEODETIC_POSITION currentPosition;
    ST_3D_GEODETIC_POSITION launchPosition;
    ST_3D_GEODETIC_POSITION targetPosition;
    
    EngagementPlanResult() 
        : tubeNumber(0), weaponKind(EN_WPN_KIND::WPN_KIND_NA), isValid(false)
        , totalTime_sec(0.0f), timeToTarget_sec(0.0f), nextWaypointIndex(0)
        , timeToNextWaypoint_sec(0.0f) {}
};

// =============================================================================
// 시스템 통계
// =============================================================================

struct SystemStatistics {
    uint32_t totalCommands;
    uint32_t successfulCommands;
    uint32_t failedCommands;
    uint32_t assignedTubes;
    uint32_t readyTubes;
    uint32_t launchedWeapons;
    std::chrono::steady_clock::time_point systemStartTime;
    std::chrono::steady_clock::time_point lastUpdateTime;
    
    SystemStatistics()
        : totalCommands(0), successfulCommands(0), failedCommands(0)
        , assignedTubes(0), readyTubes(0), launchedWeapons(0)
        , systemStartTime(std::chrono::steady_clock::now())
        , lastUpdateTime(std::chrono::steady_clock::now()) {}
};

// =============================================================================
// 요청 구조체들
// =============================================================================

struct WeaponAssignmentRequest {
    uint16_t tubeNumber;
    EN_WPN_KIND weaponKind;
    AssignmentInfo assignmentInfo;
};

struct WeaponControlRequest {
    uint16_t tubeNumber;
    EN_WPN_CTRL_STATE targetState;
    CancellationToken cancellationToken;
};

struct WaypointUpdateRequest {
    uint16_t tubeNumber;
    std::vector<ST_WEAPON_WAYPOINT> waypoints;
};

// =============================================================================
// 유틸리티 함수들
// =============================================================================

inline std::string WeaponKindToString(EN_WPN_KIND kind) {
    switch(kind) {
        case EN_WPN_KIND::WPN_KIND_ALM: return "ALM";
        case EN_WPN_KIND::WPN_KIND_ASM: return "ASM";
        case EN_WPN_KIND::WPN_KIND_AAM: return "AAM";
        case EN_WPN_KIND::WPN_KIND_WGT: return "WGT";
        case EN_WPN_KIND::WPN_KIND_M_MINE: return "MINE";
        default: return "NA";
    }
}

inline std::string StateToString(EN_WPN_CTRL_STATE state) {
    switch(state) {
        case EN_WPN_CTRL_STATE::WPN_CTRL_STATE_OFF: return "OFF";
        case EN_WPN_CTRL_STATE::WPN_CTRL_STATE_POC: return "POC";
        case EN_WPN_CTRL_STATE::WPN_CTRL_STATE_ON: return "ON";
        case EN_WPN_CTRL_STATE::WPN_CTRL_STATE_RTL: return "RTL";
        case EN_WPN_CTRL_STATE::WPN_CTRL_STATE_LAUNCH: return "LAUNCH";
        case EN_WPN_CTRL_STATE::WPN_CTRL_STATE_POST_LAUNCH: return "POST_LAUNCH";
        case EN_WPN_CTRL_STATE::WPN_CTRL_STATE_ABORT: return "ABORT";
        default: return "UNKNOWN";
    }
}

// 스마트 포인터 타입 정의
class IWeapon;
class IEngagementManager;
class ILaunchTubeManager;

using WeaponPtr = std::unique_ptr<IWeapon>;
using EngagementManagerPtr = std::unique_ptr<IEngagementManager>;
using LaunchTubeManagerPtr = std::unique_ptr<ILaunchTubeManager>;

} // namespace WeaponControl
