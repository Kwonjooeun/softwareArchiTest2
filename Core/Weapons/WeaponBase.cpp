#include "IWeapon.h"
#include "../../Infrastructure/Configuration/SystemConfig.h"
#include <iostream>
#include <algorithm>
#include <thread>

namespace WeaponControl {

// =============================================================================
// 기본 상태 전이 맵
// =============================================================================
const std::map<EN_WPN_CTRL_STATE, std::set<EN_WPN_CTRL_STATE>> WeaponBase::s_defaultTransitionMap = {
    {EN_WPN_CTRL_STATE::WPN_CTRL_STATE_OFF, {EN_WPN_CTRL_STATE::WPN_CTRL_STATE_ON}},
    {EN_WPN_CTRL_STATE::WPN_CTRL_STATE_ON, {EN_WPN_CTRL_STATE::WPN_CTRL_STATE_OFF}},
    {EN_WPN_CTRL_STATE::WPN_CTRL_STATE_RTL, {EN_WPN_CTRL_STATE::WPN_CTRL_STATE_LAUNCH, EN_WPN_CTRL_STATE::WPN_CTRL_STATE_OFF}},
    {EN_WPN_CTRL_STATE::WPN_CTRL_STATE_LAUNCH, {EN_WPN_CTRL_STATE::WPN_CTRL_STATE_ABORT}},
    {EN_WPN_CTRL_STATE::WPN_CTRL_STATE_ABORT, {EN_WPN_CTRL_STATE::WPN_CTRL_STATE_OFF}},
    {EN_WPN_CTRL_STATE::WPN_CTRL_STATE_POST_LAUNCH, {EN_WPN_CTRL_STATE::WPN_CTRL_STATE_OFF}}
};

// =============================================================================
// WeaponBase 구현
// =============================================================================

WeaponBase::WeaponBase(EN_WPN_KIND weaponKind)
    : m_weaponKind(weaponKind)
    , m_tubeNumber(0)
    , m_currentState(EN_WPN_CTRL_STATE::WPN_CTRL_STATE_OFF)
    , m_launched(false)
    , m_fireSolutionReady(false)
    , m_onDelay(SystemConfig::getInstance().getDefaultLaunchDelay())
{
    // 기본 발사 단계 설정
    m_launchSteps = {
        {"Power On Check", 1.0f}, 
        {"System Verification", 1.0f}, 
        {"Launch Sequence", 1.0f}
    };
    
    std::cout << "WeaponBase created for " << WeaponKindToString(weaponKind) << std::endl;
}

EN_WPN_CTRL_STATE WeaponBase::getCurrentState() const {
    return m_currentState.load();
}

Result<void> WeaponBase::requestStateChange(EN_WPN_CTRL_STATE newState, const CancellationToken& token) {
    std::lock_guard<std::mutex> lock(m_stateMutex);
    
    EN_WPN_CTRL_STATE currentState = m_currentState.load();
    
    // ABORT 명령은 언제든지 허용
    if (newState == EN_WPN_CTRL_STATE::WPN_CTRL_STATE_ABORT) {
        m_currentCancellationToken.cancel();  // 현재 작업 취소
        return processAbort();
    }
    
    if (!isValidTransition(currentState, newState)) {
        return Result<void>::failure(
            "Invalid transition from " + StateToString(currentState) + 
            " to " + StateToString(newState)
        );
    }
    
    // 취소 토큰 업데이트
    m_currentCancellationToken = token;
    
    EN_WPN_CTRL_STATE oldState = currentState;
    
    try {
        Result<void> result;
        
        switch (newState) {
            case EN_WPN_CTRL_STATE::WPN_CTRL_STATE_OFF:
                result = processTurnOff();
                break;
            case EN_WPN_CTRL_STATE::WPN_CTRL_STATE_ON:
                result = processTurnOn(token);
                break;
            case EN_WPN_CTRL_STATE::WPN_CTRL_STATE_LAUNCH:
                result = processLaunch(token);
                break;
            default:
                setState(newState);
                result = Result<void>::success();
                break;
        }
        
        if (result.isSuccess()) {
            std::cout << "Weapon " << WeaponKindToString(m_weaponKind) 
                      << " state changed: " << StateToString(oldState) 
                      << " -> " << StateToString(newState) << std::endl;
        }
        
        return result;
        
    } catch (const OperationCancelledException&) {
        std::cout << "State change operation was cancelled" << std::endl;
        return Result<void>::failure("Operation cancelled");
    }
}

bool WeaponBase::isValidTransition(EN_WPN_CTRL_STATE from, EN_WPN_CTRL_STATE to) const {
    auto transitionMap = getValidTransitionMap();
    auto it = transitionMap.find(from);
    return it != transitionMap.end() && it->second.count(to) > 0;
}

void WeaponBase::setLaunched(bool launched) {
    bool oldValue = m_launched.exchange(launched);
    if (oldValue != launched) {
        notifyLaunchStatusChanged(launched);
        
        if (launched) {
            setState(EN_WPN_CTRL_STATE::WPN_CTRL_STATE_POST_LAUNCH);
        }
    }
}

bool WeaponBase::checkInterlockConditions() const {
    // 기본 인터록 조건 - 파생 클래스에서 오버라이드
    return m_fireSolutionReady.load();
}

Result<void> WeaponBase::initialize(uint16_t tubeNumber) {
    m_tubeNumber = tubeNumber;
    reset();
    
    std::cout << "Weapon " << WeaponKindToString(m_weaponKind) 
              << " initialized on tube " << tubeNumber << std::endl;
    
    return Result<void>::success();
}

void WeaponBase::reset() {
    std::lock_guard<std::mutex> lock(m_stateMutex);
    
    m_currentState.store(EN_WPN_CTRL_STATE::WPN_CTRL_STATE_OFF);
    m_launched.store(false);
    m_fireSolutionReady.store(false);
    m_currentCancellationToken.cancel(); // 진행 중인 작업 취소
    m_stateStartTime = std::chrono::steady_clock::now();
    
    std::cout << "Weapon " << WeaponKindToString(m_weaponKind) << " reset" << std::endl;
}

void WeaponBase::update() {
    EN_WPN_CTRL_STATE currentState = m_currentState.load();
    
    // 상태별 업데이트 처리
    onStateUpdate(currentState);
    
    // RTL 상태 자동 전이 확인
    if (currentState == EN_WPN_CTRL_STATE::WPN_CTRL_STATE_ON) {
        if (checkInterlockConditions()) {
            std::cout << "Conditions met, transitioning to RTL" << std::endl;
            setState(EN_WPN_CTRL_STATE::WPN_CTRL_STATE_RTL);
        }
    }
    else if (currentState == EN_WPN_CTRL_STATE::WPN_CTRL_STATE_RTL) {
        if (!checkInterlockConditions()) {
            std::cout << "Conditions not met, returning to ON" << std::endl;
            setState(EN_WPN_CTRL_STATE::WPN_CTRL_STATE_ON);
        }
    }
}

void WeaponBase::addStateObserver(std::shared_ptr<IStateObserver> observer) {
    std::lock_guard<std::mutex> lock(m_observerMutex);
    m_observers.push_back(observer);
}

void WeaponBase::removeStateObserver(std::shared_ptr<IStateObserver> observer) {
    std::lock_guard<std::mutex> lock(m_observerMutex);
    m_observers.erase(
        std::remove_if(m_observers.begin(), m_observers.end(),
            [&](const std::weak_ptr<IStateObserver>& wp) {
                return wp.expired() || wp.lock() == observer;
            }),
        m_observers.end());
}

std::map<EN_WPN_CTRL_STATE, std::set<EN_WPN_CTRL_STATE>> WeaponBase::getValidTransitionMap() const {
    return s_defaultTransitionMap;
}

Result<void> WeaponBase::processTurnOn(const CancellationToken& token) {
    auto oldState = getCurrentState();
    onStateExit(oldState);
    
    setState(EN_WPN_CTRL_STATE::WPN_CTRL_STATE_POC);
    onStateEnter(EN_WPN_CTRL_STATE::WPN_CTRL_STATE_POC);
    
    std::cout << "Performing power-on check for " << WeaponKindToString(m_weaponKind) << "..." << std::endl;
    
    if (!sleepWithCancellationCheck(m_onDelay, token)) {
        setState(EN_WPN_CTRL_STATE::WPN_CTRL_STATE_OFF);
        return Result<void>::failure("Power-on check cancelled");
    }
    
    onStateExit(EN_WPN_CTRL_STATE::WPN_CTRL_STATE_POC);
    setState(EN_WPN_CTRL_STATE::WPN_CTRL_STATE_ON);
    onStateEnter(EN_WPN_CTRL_STATE::WPN_CTRL_STATE_ON);
    
    std::cout << "Power-on check complete." << std::endl;
    return Result<void>::success();
}

Result<void> WeaponBase::processTurnOff() {
    m_currentCancellationToken.cancel();
    auto oldState = getCurrentState();
    onStateExit(oldState);
    setState(EN_WPN_CTRL_STATE::WPN_CTRL_STATE_OFF);
    onStateEnter(EN_WPN_CTRL_STATE::WPN_CTRL_STATE_OFF);
    
    std::cout << "Weapon turned off." << std::endl;
    return Result<void>::success();
}

Result<void> WeaponBase::processLaunch(const CancellationToken& token) {
    auto oldState = getCurrentState();
    onStateExit(oldState);
    setState(EN_WPN_CTRL_STATE::WPN_CTRL_STATE_LAUNCH);
    onStateEnter(EN_WPN_CTRL_STATE::WPN_CTRL_STATE_LAUNCH);
    
    std::cout << "Launching " << WeaponKindToString(m_weaponKind) << "..." << std::endl;
    
    for (const auto& step : m_launchSteps) {
        if (token.isCancelled()) {
            setState(EN_WPN_CTRL_STATE::WPN_CTRL_STATE_ABORT);
            onStateEnter(EN_WPN_CTRL_STATE::WPN_CTRL_STATE_ABORT);
            return Result<void>::failure("Launch sequence aborted");
        }
        
        std::cout << "Step: " << step.description << " (Duration: " << step.duration << " seconds)" << std::endl;
        
        if (!sleepWithCancellationCheck(step.duration, token)) {
            setState(EN_WPN_CTRL_STATE::WPN_CTRL_STATE_ABORT);
            onStateEnter(EN_WPN_CTRL_STATE::WPN_CTRL_STATE_ABORT);
            return Result<void>::failure("Launch sequence aborted");
        }
    }
    
    onStateExit(EN_WPN_CTRL_STATE::WPN_CTRL_STATE_LAUNCH);
    setLaunched(true);  // 이것이 POST_LAUNCH로 상태 변경
    
    std::cout << "Launch complete." << std::endl;
    return Result<void>::success();
}

Result<void> WeaponBase::processAbort() {
    m_currentCancellationToken.cancel();
    auto oldState = getCurrentState();
    onStateExit(oldState);
    setState(EN_WPN_CTRL_STATE::WPN_CTRL_STATE_ABORT);
    onStateEnter(EN_WPN_CTRL_STATE::WPN_CTRL_STATE_ABORT);
    
    std::cout << "Abort command executed." << std::endl;
    return Result<void>::success();
}

bool WeaponBase::sleepWithCancellationCheck(float duration, const CancellationToken& token) {
    const int interval_ms = 50;  // 더 짧은 간격으로 취소 확인
    int total_intervals = static_cast<int>(duration * 1000 / interval_ms);
    
    for (int i = 0; i < total_intervals; ++i) {
        if (token.isCancelled() || m_currentCancellationToken.isCancelled()) {
            std::cout << "Operation cancelled." << std::endl;
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(interval_ms));
    }
    
    return true;
}

void WeaponBase::setState(EN_WPN_CTRL_STATE newState) {
    EN_WPN_CTRL_STATE oldState = m_currentState.exchange(newState);
    m_stateStartTime = std::chrono::steady_clock::now();
    
    if (oldState != newState) {
        notifyStateChanged(oldState, newState);
    }
}

void WeaponBase::notifyStateChanged(EN_WPN_CTRL_STATE oldState, EN_WPN_CTRL_STATE newState) {
    std::lock_guard<std::mutex> lock(m_observerMutex);
    
    // 만료된 weak_ptr 제거
    m_observers.erase(
        std::remove_if(m_observers.begin(), m_observers.end(),
            [](const std::weak_ptr<IStateObserver>& wp) { return wp.expired(); }),
        m_observers.end());
    
    // 관찰자들에게 상태 변화 통지
    for (auto& weakObserver : m_observers) {
        if (auto observer = weakObserver.lock()) {
            observer->onStateChanged(m_tubeNumber, oldState, newState);
        }
    }
}

void WeaponBase::notifyLaunchStatusChanged(bool launched) {
    std::lock_guard<std::mutex> lock(m_observerMutex);
    
    for (auto& weakObserver : m_observers) {
        if (auto observer = weakObserver.lock()) {
            observer->onLaunchStatusChanged(m_tubeNumber, launched);
        }
    }
}

} // namespace WeaponControl
