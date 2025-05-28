#pragma once

#include "../../Common/Types/CommonTypes.h"
#include <optional>
#include <vector>
#include <string>

namespace WeaponControl {

// =============================================================================
// 표적 추적 서비스 인터페이스
// =============================================================================

class ITargetTrackingService {
public:
    virtual ~ITargetTrackingService() = default;
    
    virtual void updateTargetInfo(const TRKMGR_SYSTEMTARGET_INFO& targetInfo) = 0;
    virtual std::optional<TRKMGR_SYSTEMTARGET_INFO> getTarget(uint32_t systemTargetId) const = 0;
    virtual std::vector<uint32_t> getAllTargetIds() const = 0;
    virtual size_t getTargetCount() const = 0;
    virtual void clearOldTargets(std::chrono::seconds maxAge) = 0;
};

// =============================================================================
// 자항기뢰 부설계획 서비스 인터페이스
// =============================================================================

class IMineDropPlanService {
public:
    virtual ~IMineDropPlanService() = default;
    
    // 초기화
    virtual Result<void> initialize(const std::string& planDataPath = "") = 0;
    
    // 부설계획 목록 관리
    virtual Result<void> loadPlanList(uint32_t planListNumber) = 0;
    virtual Result<void> savePlanList(uint32_t planListNumber, const std::vector<ST_M_MINE_PLAN_INFO>& plans) = 0;
    virtual Result<void> createNewPlanList(uint32_t planListNumber) = 0;
    virtual Result<void> deletePlanList(uint32_t planListNumber) = 0;
    
    // 부설계획 조회
    virtual std::vector<ST_M_MINE_PLAN_INFO> getPlanList(uint32_t planListNumber) const = 0;
    virtual Result<ST_M_MINE_PLAN_INFO> getPlan(uint32_t planListNumber, uint32_t planNumber) const = 0;
    virtual std::vector<uint32_t> getAvailablePlanListNumbers() const = 0;
    
    // 부설계획 편집
    virtual Result<void> updatePlan(uint32_t planListNumber, const ST_M_MINE_PLAN_INFO& plan) = 0;
    virtual Result<void> addPlan(uint32_t planListNumber, const ST_M_MINE_PLAN_INFO& plan) = 0;
    virtual Result<void> removePlan(uint32_t planListNumber, uint32_t planNumber) = 0;
    
    // DDS 메시지 변환
    virtual Result<AIEP_CMSHCI_M_MINE_ALL_PLAN_LIST> convertToAllPlanListMessage(uint32_t planListNumber) const = 0;
    virtual Result<void> updateFromEditedPlanList(const CMSHCI_AIEP_M_MINE_EDITED_PLAN_LIST& editedPlanList) = 0;
    
    // 유효성 검사
    virtual bool isValidPlanListNumber(uint32_t planListNumber) const = 0;
    virtual bool isValidPlanNumber(uint32_t planListNumber, uint32_t planNumber) const = 0;
    virtual bool validatePlan(const ST_M_MINE_PLAN_INFO& plan) const = 0;
    
    // 통계 정보
    virtual size_t getPlanCount(uint32_t planListNumber) const = 0;
    virtual size_t getTotalPlanListCount() const = 0;
};

// =============================================================================
// 표적 추적 서비스 구현
// =============================================================================

class TargetTrackingService : public ITargetTrackingService {
public:
    TargetTrackingService() = default;
    ~TargetTrackingService() = default;
    
    void updateTargetInfo(const TRKMGR_SYSTEMTARGET_INFO& targetInfo) override;
    std::optional<TRKMGR_SYSTEMTARGET_INFO> getTarget(uint32_t systemTargetId) const override;
    std::vector<uint32_t> getAllTargetIds() const override;
    size_t getTargetCount() const override;
    void clearOldTargets(std::chrono::seconds maxAge) override;

private:
    struct TargetData {
        TRKMGR_SYSTEMTARGET_INFO info;
        std::chrono::steady_clock::time_point lastUpdateTime;
    };
    
    mutable std::shared_mutex m_targetsMutex;
    std::map<uint32_t, TargetData> m_targets;
};

// =============================================================================
// 자항기뢰 부설계획 서비스 구현
// =============================================================================

class MineDropPlanService : public IMineDropPlanService {
public:
    explicit MineDropPlanService(const std::string& planDataPath = "");
    ~MineDropPlanService() = default;
    
    Result<void> initialize(const std::string& planDataPath = "") override;
    
    Result<void> loadPlanList(uint32_t planListNumber) override;
    Result<void> savePlanList(uint32_t planListNumber, const std::vector<ST_M_MINE_PLAN_INFO>& plans) override;
    Result<void> createNewPlanList(uint32_t planListNumber) override;
    Result<void> deletePlanList(uint32_t planListNumber) override;
    
    std::vector<ST_M_MINE_PLAN_INFO> getPlanList(uint32_t planListNumber) const override;
    Result<ST_M_MINE_PLAN_INFO> getPlan(uint32_t planListNumber, uint32_t planNumber) const override;
    std::vector<uint32_t> getAvailablePlanListNumbers() const override;
    
    Result<void> updatePlan(uint32_t planListNumber, const ST_M_MINE_PLAN_INFO& plan) override;
    Result<void> addPlan(uint32_t planListNumber, const ST_M_MINE_PLAN_INFO& plan) override;
    Result<void> removePlan(uint32_t planListNumber, uint32_t planNumber) override;
    
    Result<AIEP_CMSHCI_M_MINE_ALL_PLAN_LIST> convertToAllPlanListMessage(uint32_t planListNumber) const override;
    Result<void> updateFromEditedPlanList(const CMSHCI_AIEP_M_MINE_EDITED_PLAN_LIST& editedPlanList) override;
    
    bool isValidPlanListNumber(uint32_t planListNumber) const override;
    bool isValidPlanNumber(uint32_t planListNumber, uint32_t planNumber) const override;
    bool validatePlan(const ST_M_MINE_PLAN_INFO& plan) const override;
    
    size_t getPlanCount(uint32_t planListNumber) const override;
    size_t getTotalPlanListCount() const override;

private:
    // JSON 파일 I/O
    std::string getPlanListFilePath(uint32_t planListNumber) const;
    Result<void> savePlanListToFile(uint32_t planListNumber, const std::vector<ST_M_MINE_PLAN_INFO>& plans);
    Result<std::vector<ST_M_MINE_PLAN_INFO>> loadPlanListFromFile(uint32_t planListNumber);
    
    // JSON 변환 (간단한 구현)
    Result<void> writeJsonToFile(const std::string& filePath, uint32_t planListNumber, const std::vector<ST_M_MINE_PLAN_INFO>& plans);
    Result<std::vector<ST_M_MINE_PLAN_INFO>> readJsonFromFile(const std::string& filePath);
    
    // 데이터 검증
    bool validateWaypoints(const std::vector<ST_WEAPON_WAYPOINT>& waypoints) const;
    bool validatePosition(const ST_WEAPON_WAYPOINT& position) const;
    
    // 캐시된 계획 데이터
    mutable std::map<uint32_t, std::vector<ST_M_MINE_PLAN_INFO>> m_cachedPlans;
    mutable std::shared_mutex m_plansMutex;
    
    // 설정
    std::string m_planDataPath;
    uint32_t m_maxPlanLists;
    uint32_t m_maxPlansPerList;
    
    bool m_initialized;
};

} // namespace WeaponControl
