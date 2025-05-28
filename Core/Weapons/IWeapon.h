#pragma once

#include "../../Common/Types/CommonTypes.h"
#include <functional>
#include <memory>
#include <vector>

namespace WeaponControl {

// =============================================================================
// 상태 변화 관찰자 인터페이스
// =============================================================================

class IStateObserver {
public:
    virtual ~IStateObserver() = default;
    virtual void onStateChanged(uint16_t tubeNumber, EN_WPN_CTRL_STATE oldState, EN_WPN_CTRL_STATE newState) = 0;
    virtual void onLaunchStatusChanged(uint16_t tubeNumber, bool launched) = 0;
};

// =============================================================================
// 무장 인터페이스
// =============================================================================

class IWeapon {
public:
    virtual ~IWeapon() = default;
    
    // ==========================================================================
    // 기본 정보
    // ==========================================================================
    virtual EN_WPN_KIND getWeaponKind() const = 0;
    virtual WeaponSpecification getSpecification() const = 0;
    virtual uint16_t getTubeNumber() const = 0;
    
    // ==========================================================================
    // 상태 관리 (Cancellation Token 포함)
    // ==========================================================================
    virtual EN_WPN_CTRL_STATE getCurrentState() const = 0;
    virtual Result<void> requestStateChange(EN_WPN_CTRL_STATE newState, 
                                           const CancellationToken& token = {}) = 0;
    virtual bool isValidTransition(EN_WPN_CTRL_STATE from, EN_WPN_CTRL_STATE to) const = 0;
    
    // ==========================================================================
    // 발사 관리
    // ==========================================================================
    virtual bool isLaunched() const = 0;
    virtual void setLaunched(bool launched) = 0;
    
    // ==========================================================================
    // 인터록 및 준비 상태 확인
    // ==========================================================================
    virtual bool checkInterlockConditions() const = 0;
    virtual bool isFireSolutionReady() const = 0;
    virtual void setFireSolutionReady(bool ready) = 0;
    
    // ==========================================================================
    // 초기화 및 업데이트
    // ==========================================================================
    virtual Result<void> initialize(uint16_t tubeNumber) = 0;
    virtual void reset() = 0;
    virtual void update() = 0;
    
    // ==========================================================================
    // 관찰자 패턴
    // ==========================================================================
    virtual void addStateObserver(std::shared_ptr<IStateObserver> observer) = 0;
    virtual void removeStateObserver(std::shared_ptr<IStateObserver> observer) = 0;
};

// =============================================================================
// 발사 단계 정보
// =============================================================================

struct LaunchStep {
    std::string description;
    float duration;
    
    LaunchStep(const std::string& desc, float dur) 
        : description(desc), duration(dur) {}
};

// =============================================================================
// 무장 기반 클래스 - 공통 기능 구현
// =============================================================================

class WeaponBase : public IWeapon {
public:
    explicit WeaponBase(EN_WPN_KIND weaponKind);
    virtual ~WeaponBase() = default;
    
    // ==========================================================================
    // IWeapon 인터페이스 구현
    // ==========================================================================
    EN_WPN_KIND getWeaponKind() const override { return m_weaponKind; }
    uint16_t getTubeNumber() const override { return m_tubeNumber; }
    
    EN_WPN_CTRL_STATE getCurrentState() const override;
    Result<void> requestStateChange(EN_WPN_CTRL_STATE newState, 
                                   const CancellationToken& token = {}) override;
    bool isValidTransition(EN_WPN_CTRL_STATE from, EN_WPN_CTRL_STATE to) const override;
    
    bool isLaunched() const override { return m_launched.load(); }
    void setLaunched(bool launched) override;
    
    bool checkInterlockConditions() const override;
    bool isFireSolutionReady() const override { return m_fireSolutionReady.load(); }
    void setFireSolutionReady(bool ready) override { m_fireSolutionReady.store(ready); }
    
    Result<void> initialize(uint16_t tubeNumber) override;
    void reset() override;
    void update() override;
    
    // 관찰자 패턴
    void addStateObserver(std::shared_ptr<IStateObserver> observer) override;
    void removeStateObserver(std::shared_ptr<IStateObserver> observer) override;
    
protected:
    // ==========================================================================
    // 상태 전이 맵 (각 무장별로 오버라이드 가능)
    // ==========================================================================
    virtual std::map<EN_WPN_CTRL_STATE, std::set<EN_WPN_CTRL_STATE>> getValidTransitionMap() const;
    
    // ==========================================================================
    // 상태별 처리 함수 (파생 클래스에서 오버라이드)
    // ==========================================================================
    virtual Result<void> onStateEnter(EN_WPN_CTRL_STATE state) { return Result<void>::success(); }
    virtual Result<void> onStateExit(EN_WPN_CTRL_STATE state) { return Result<void>::success(); }
    virtual void onStateUpdate(EN_WPN_CTRL_STATE state) {}
    
    // ==========================================================================
    // 상태 전이 처리 함수
    // ==========================================================================
    virtual Result<void> processTurnOn(const CancellationToken& token);
    virtual Result<void> processTurnOff();
    virtual Result<void> processLaunch(const CancellationToken& token);
    virtual Result<void> processAbort();
    
    // ==========================================================================
    // 유틸리티 함수
    // ==========================================================================
    bool sleepWithCancellationCheck(float duration, const CancellationToken& token);
    void setState(EN_WPN_CTRL_STATE newState);
    
    // ==========================================================================
    // 관찰자 통지
    // ==========================================================================
    void notifyStateChanged(EN_WPN_CTRL_STATE oldState, EN_WPN_CTRL_STATE newState);
    void notifyLaunchStatusChanged(bool launched);
    
    // ==========================================================================
    // 멤버 변수
    // ==========================================================================
    EN_WPN_KIND m_weaponKind;
    uint16_t m_tubeNumber;
    std::atomic<EN_WPN_CTRL_STATE> m_currentState;
    std::atomic<bool> m_launched;
    std::atomic<bool> m_fireSolutionReady;
    
    std::vector<LaunchStep> m_launchSteps;
    float m_onDelay;
    
    mutable std::mutex m_observerMutex;
    std::vector<std::weak_ptr<IStateObserver>> m_observers;
    
    mutable std::mutex m_stateMutex;
    std::chrono::steady_clock::time_point m_stateStartTime;
    
    // 현재 작업의 취소 토큰
    CancellationToken m_currentCancellationToken;
    
private:
    static const std::map<EN_WPN_CTRL_STATE, std::set<EN_WPN_CTRL_STATE>> s_defaultTransitionMap;
};

} // namespace WeaponControl
