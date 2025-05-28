#include "WeaponFactory.h"
#include "../Weapons/IWeapon.h"
#include "../EngagementManagers/IEngagementManager.h"
#include "../../Infrastructure/Configuration/SystemConfig.h"
#include <iostream>

namespace WeaponControl {

// =============================================================================
// 구체적인 무장 클래스들 (임시 구현)
// =============================================================================

class ALMWeapon : public WeaponBase {
public:
    ALMWeapon() : WeaponBase(EN_WPN_KIND::WPN_KIND_ALM) {
        auto& config = SystemConfig::getInstance();
        m_onDelay = config.getDefaultLaunchDelay();
        m_launchSteps = {
            {"ALM Power On Check", 1.0f}, 
            {"ALM System Verification", 1.0f}, 
            {"ALM Launch Sequence", 1.0f}
        };
    }
    
    WeaponSpecification getSpecification() const override {
        auto& config = SystemConfig::getInstance();
        return WeaponSpecification("ALM", config.getALMMaxRange(), config.getALMSpeed(), m_onDelay);
    }
    
protected:
    Result<void> onStateEnter(EN_WPN_CTRL_STATE state) override {
        std::cout << "ALM entering state: " << StateToString(state) << std::endl;
        return Result<void>::success();
    }
};

class ASMWeapon : public WeaponBase {
public:
    ASMWeapon() : WeaponBase(EN_WPN_KIND::WPN_KIND_ASM) {
        auto& config = SystemConfig::getInstance();
        m_onDelay = config.getDefaultLaunchDelay();
        m_launchSteps = {
            {"ASM Power On Check", 1.0f}, 
            {"ASM System Verification", 1.0f}, 
            {"ASM Launch Sequence", 1.0f}
        };
    }
    
    WeaponSpecification getSpecification() const override {
        auto& config = SystemConfig::getInstance();
        return WeaponSpecification("ASM", config.getASMMaxRange(), config.getASMSpeed(), m_onDelay);
    }
    
protected:
    Result<void> onStateEnter(EN_WPN_CTRL_STATE state) override {
        std::cout << "ASM entering state: " << StateToString(state) << std::endl;
        return Result<void>::success();
    }
};

class AAMWeapon : public WeaponBase {
public:
    AAMWeapon() : WeaponBase(EN_WPN_KIND::WPN_KIND_AAM) {
        auto& config = SystemConfig::getInstance();
        m_onDelay = config.getDefaultLaunchDelay();
        m_launchSteps = {
            {"AAM Power On Check", 1.0f}, 
            {"AAM System Verification", 1.0f}, 
            {"AAM Launch Sequence", 1.0f}
        };
    }
    
    WeaponSpecification getSpecification() const override {
        auto& config = SystemConfig::getInstance();
        return WeaponSpecification("AAM", 80.0, 350.0, m_onDelay);
    }
    
protected:
    Result<void> onStateEnter(EN_WPN_CTRL_STATE state) override {
        std::cout << "AAM entering state: " << StateToString(state) << std::endl;
        return Result<void>::success();
    }
};

class MineWeapon : public WeaponBase {
public:
    MineWeapon() : WeaponBase(EN_WPN_KIND::WPN_KIND_M_MINE) {
        auto& config = SystemConfig::getInstance();
        m_onDelay = config.getDefaultLaunchDelay();
        m_launchSteps = {
            {"Mine Power On Check", 1.0f}, 
            {"Mine System Verification", 1.0f}, 
            {"Mine Launch Sequence", 1.0f}
        };
    }
    
    WeaponSpecification getSpecification() const override {
        auto& config = SystemConfig::getInstance();
        return WeaponSpecification("MINE", 30.0, config.getMineSpeed(), m_onDelay);
    }
    
    bool checkInterlockConditions() const override {
        // 자항기뢰는 부설계획이 필요
        return WeaponBase::checkInterlockConditions();
    }
    
protected:
    Result<void> onStateEnter(EN_WPN_CTRL_STATE state) override {
        std::cout << "Mine entering state: " << StateToString(state) << std::endl;
        return Result<void>::success();
    }
};

// =============================================================================
// 구체적인 교전계획 관리자들 (임시 구현)
// =============================================================================

class ALMEngagementManager : public MissileEngagementManagerBase {
public:
    ALMEngagementManager() : MissileEngagementManagerBase(EN_WPN_KIND::WPN_KIND_ALM) {}
    
    ST_3D_GEODETIC_POSITION getCurrentPosition(float timeSinceLaunch) const override {
        return interpolatePosition(timeSinceLaunch);
    }
    
protected:
    Result<void> calculateTrajectory() override {
        // ALM 궤적 계산 로직 (임시 구현)
        m_engagementResult.isValid = hasValidTarget();
        m_engagementResult.totalTime_sec = 100.0f;
        m_engagementResult.tubeNumber = m_tubeNumber;
        m_engagementResult.weaponKind = m_weaponKind;
        
        if (hasValidTarget()) {
            m_engagementResult.targetPosition = m_targetPosition;
            
            // 간단한 궤적 생성 (실제로는 복잡한 계산)
            m_engagementResult.trajectory.clear();
            m_engagementResult.trajectory.push_back(m_launchPosition);
            m_engagementResult.trajectory.push_back(m_targetPosition);
        }
        
        std::cout << "ALM trajectory calculated for tube " << m_tubeNumber << std::endl;
        return Result<void>::success();
    }
    
    ST_3D_GEODETIC_POSITION interpolatePosition(float timeSinceLaunch) const override {
        // 시간에 따른 위치 보간 (임시 구현)
        if (m_engagementResult.trajectory.size() < 2) {
            return ST_3D_GEODETIC_POSITION();
        }
        
        // 간단한 선형 보간
        float progress = std::min(timeSinceLaunch / m_engagementResult.totalTime_sec, 1.0f);
        const auto& start = m_engagementResult.trajectory[0];
        const auto& end = m_engagementResult.trajectory.back();
        
        ST_3D_GEODETIC_POSITION result;
        result.dLatitude() = start.dLatitude() + (end.dLatitude() - start.dLatitude()) * progress;
        result.dLongitude() = start.dLongitude() + (end.dLongitude() - start.dLongitude()) * progress;
        result.fDepth() = start.fDepth() + (end.fDepth() - start.fDepth()) * progress;
        
        return result;
    }
    
    std::vector<ST_3D_GEODETIC_POSITION> calculateTurningPoints() const override {
        // ALM 선회점 계산 (임시 구현)
        std::vector<ST_3D_GEODETIC_POSITION> turningPoints;
        
        // 실제로는 복잡한 선회점 계산 알고리즘 필요
        // 지금은 경로점들을 선회점으로 사용
        for (const auto& waypoint : m_waypoints) {
            ST_3D_GEODETIC_POSITION turningPoint;
            turningPoint.dLatitude() = waypoint.dLatitude();
            turningPoint.dLongitude() = waypoint.dLongitude();
            turningPoint.fDepth() = waypoint.fDepth();
            turningPoints.push_back(turningPoint);
        }
        
        return turningPoints;
    }
};

class ASMEngagementManager : public MissileEngagementManagerBase {
public:
    ASMEngagementManager() : MissileEngagementManagerBase(EN_WPN_KIND::WPN_KIND_ASM) {}
    
    ST_3D_GEODETIC_POSITION getCurrentPosition(float timeSinceLaunch) const override {
        return interpolatePosition(timeSinceLaunch);
    }
    
protected:
    Result<void> calculateTrajectory() override {
        // ASM 궤적 계산 로직 (임시 구현)
        m_engagementResult.isValid = hasValidTarget();
        m_engagementResult.totalTime_sec = 80.0f;
        m_engagementResult.tubeNumber = m_tubeNumber;
        m_engagementResult.weaponKind = m_weaponKind;
        
        if (hasValidTarget()) {
            m_engagementResult.targetPosition = m_targetPosition;
            
            // 간단한 궤적 생성
            m_engagementResult.trajectory.clear();
            m_engagementResult.trajectory.push_back(m_launchPosition);
            m_engagementResult.trajectory.push_back(m_targetPosition);
        }
        
        std::cout << "ASM trajectory calculated for tube " << m_tubeNumber << std::endl;
        return Result<void>::success();
    }
    
    ST_3D_GEODETIC_POSITION interpolatePosition(float timeSinceLaunch) const override {
        // ASM 위치 보간 (임시 구현)
        if (m_engagementResult.trajectory.size() < 2) {
            return ST_3D_GEODETIC_POSITION();
        }
        
        float progress = std::min(timeSinceLaunch / m_engagementResult.totalTime_sec, 1.0f);
        const auto& start = m_engagementResult.trajectory[0];
        const auto& end = m_engagementResult.trajectory.back();
        
        ST_3D_GEODETIC_POSITION result;
        result.dLatitude() = start.dLatitude() + (end.dLatitude() - start.dLatitude()) * progress;
        result.dLongitude() = start.dLongitude() + (end.dLongitude() - start.dLongitude()) * progress;
        result.fDepth() = start.fDepth() + (end.fDepth() - start.fDepth()) * progress;
        
        return result;
    }
    
    std::vector<ST_3D_GEODETIC_POSITION> calculateTurningPoints() const override {
        // ASM 선회점 계산 (임시 구현)
        std::vector<ST_3D_GEODETIC_POSITION> turningPoints;
        
        for (const auto& waypoint : m_waypoints) {
            ST_3D_GEODETIC_POSITION turningPoint;
            turningPoint.dLatitude() = waypoint.dLatitude();
            turningPoint.dLongitude() = waypoint.dLongitude();
            turningPoint.fDepth() = waypoint.fDepth();
            turningPoints.push_back(turningPoint);
        }
        
        return turningPoints;
    }
};

class AAMEngagementManager : public MissileEngagementManagerBase {
public:
    AAMEngagementManager() : MissileEngagementManagerBase(EN_WPN_KIND::WPN_KIND_AAM) {}
    
    ST_3D_GEODETIC_POSITION getCurrentPosition(float timeSinceLaunch) const override {
        return interpolatePosition(timeSinceLaunch);
    }
    
protected:
    Result<void> calculateTrajectory() override {
        // AAM 궤적 계산 로직 (임시 구현)
        m_engagementResult.isValid = hasValidTarget();
        m_engagementResult.totalTime_sec = 60.0f;
        m_engagementResult.tubeNumber = m_tubeNumber;
        m_engagementResult.weaponKind = m_weaponKind;
        
        if (hasValidTarget()) {
            m_engagementResult.targetPosition = m_targetPosition;
            
            // 간단한 궤적 생성
            m_engagementResult.trajectory.clear();
            m_engagementResult.trajectory.push_back(m_launchPosition);
            m_engagementResult.trajectory.push_back(m_targetPosition);
        }
        
        std::cout << "AAM trajectory calculated for tube " << m_tubeNumber << std::endl;
        return Result<void>::success();
    }
    
    ST_3D_GEODETIC_POSITION interpolatePosition(float timeSinceLaunch) const override {
        if (m_engagementResult.trajectory.size() < 2) {
            return ST_3D_GEODETIC_POSITION();
        }
        
        float progress = std::min(timeSinceLaunch / m_engagementResult.totalTime_sec, 1.0f);
        const auto& start = m_engagementResult.trajectory[0];
        const auto& end = m_engagementResult.trajectory.back();
        
        ST_3D_GEODETIC_POSITION result;
        result.dLatitude() = start.dLatitude() + (end.dLatitude() - start.dLatitude()) * progress;
        result.dLongitude() = start.dLongitude() + (end.dLongitude() - start.dLongitude()) * progress;
        result.fDepth() = start.fDepth() + (end.fDepth() - start.fDepth()) * progress;
        
        return result;
    }
    
    std::vector<ST_3D_GEODETIC_POSITION> calculateTurningPoints() const override {
        std::vector<ST_3D_GEODETIC_POSITION> turningPoints;
        
        for (const auto& waypoint : m_waypoints) {
            ST_3D_GEODETIC_POSITION turningPoint;
            turningPoint.dLatitude() = waypoint.dLatitude();
            turningPoint.dLongitude() = waypoint.dLongitude();
            turningPoint.fDepth() = waypoint.fDepth();
            turningPoints.push_back(turningPoint);
        }
        
        return turningPoints;
    }
};

class MineEngagementManager : public MineEngagementManagerBase {
public:
    MineEngagementManager() : MineEngagementManagerBase() {}
    
    ST_3D_GEODETIC_POSITION getCurrentPosition(float timeSinceLaunch) const override {
        return interpolatePosition(timeSinceLaunch);
    }
    
protected:
    Result<void> calculateTrajectory() override {
        // 자항기뢰 궤적 계산 로직 (임시 구현)
        m_engagementResult.isValid = true; // 부설계획이 설정되면 유효
        m_engagementResult.totalTime_sec = 300.0f; // 더 긴 시간
        m_engagementResult.tubeNumber = m_tubeNumber;
        m_engagementResult.weaponKind = m_weaponKind;
        
        // 간단한 궤적 생성
        m_engagementResult.trajectory.clear();
        
        // 발사 지점에서 시작
        m_engagementResult.trajectory.push_back(m_launchPosition);
        
        // 경로점들 추가
        for (const auto& waypoint : m_waypoints) {
            ST_3D_GEODETIC_POSITION pos;
            pos.dLatitude() = waypoint.dLatitude();
            pos.dLongitude() = waypoint.dLongitude();
            pos.fDepth() = waypoint.fDepth();
            m_engagementResult.trajectory.push_back(pos);
        }
        
        // 부설 지점에서 종료
        m_engagementResult.trajectory.push_back(m_engagementResult.targetPosition);
        
        std::cout << "Mine trajectory calculated for tube " << m_tubeNumber << std::endl;
        return Result<void>::success();
    }
    
    ST_3D_GEODETIC_POSITION interpolatePosition(float timeSinceLaunch) const override {
        // 자항기뢰 위치 보간 (경로점을 따라 이동)
        if (m_engagementResult.trajectory.size() < 2) {
            return ST_3D_GEODETIC_POSITION();
        }
        
        float progress = std::min(timeSinceLaunch / m_engagementResult.totalTime_sec, 1.0f);
        
        // 궤적상의 어느 구간에 있는지 계산
        float segmentProgress = progress * (m_engagementResult.trajectory.size() - 1);
        size_t segmentIndex = static_cast<size_t>(segmentProgress);
        float localProgress = segmentProgress - segmentIndex;
        
        if (segmentIndex >= m_engagementResult.trajectory.size() - 1) {
            return m_engagementResult.trajectory.back();
        }
        
        // 구간 내에서 선형 보간
        const auto& start = m_engagementResult.trajectory[segmentIndex];
        const auto& end = m_engagementResult.trajectory[segmentIndex + 1];
        
        ST_3D_GEODETIC_POSITION result;
        result.dLatitude() = start.dLatitude() + (end.dLatitude() - start.dLatitude()) * localProgress;
        result.dLongitude() = start.dLongitude() + (end.dLongitude() - start.dLongitude()) * localProgress;
        result.fDepth() = start.fDepth() + (end.fDepth() - start.fDepth()) * localProgress;
        
        return result;
    }
};

// =============================================================================
// WeaponFactory 구현
// =============================================================================

WeaponFactory& WeaponFactory::getInstance() {
    static WeaponFactory instance;
    return instance;
}

WeaponFactory::WeaponFactory() {
    registerDefaultCreators();
}

WeaponPtr WeaponFactory::createWeapon(EN_WPN_KIND weaponKind) const {
    auto it = m_weaponCreators.find(weaponKind);
    if (it != m_weaponCreators.end()) {
        return it->second();
    }
    
    std::cout << "Unsupported weapon kind: " << WeaponKindToString(weaponKind) << std::endl;
    return nullptr;
}

EngagementManagerPtr WeaponFactory::createEngagementManager(EN_WPN_KIND weaponKind) const {
    auto it = m_engagementManagerCreators.find(weaponKind);
    if (it != m_engagementManagerCreators.end()) {
        return it->second();
    }
    
    std::cout << "Unsupported engagement manager for weapon: " << WeaponKindToString(weaponKind) << std::endl;
    return nullptr;
}

void WeaponFactory::registerWeaponCreator(EN_WPN_KIND weaponKind, WeaponCreator creator) {
    m_weaponCreators[weaponKind] = creator;
}

void WeaponFactory::registerEngagementManagerCreator(EN_WPN_KIND weaponKind, EngagementManagerCreator creator) {
    m_engagementManagerCreators[weaponKind] = creator;
}

bool WeaponFactory::isWeaponSupported(EN_WPN_KIND weaponKind) const {
    return m_weaponCreators.find(weaponKind) != m_weaponCreators.end();
}

WeaponSpecification WeaponFactory::getWeaponSpecification(EN_WPN_KIND weaponKind) const {
    auto it = m_weaponSpecs.find(weaponKind);
    if (it != m_weaponSpecs.end()) {
        return it->second;
    }
    
    return WeaponSpecification();
}

void WeaponFactory::registerDefaultCreators() {
    auto& config = SystemConfig::getInstance();
    
    // 무장 생성자 등록
    registerWeaponCreator(EN_WPN_KIND::WPN_KIND_ALM, []() -> WeaponPtr {
        return std::make_unique<ALMWeapon>();
    });
    
    registerWeaponCreator(EN_WPN_KIND::WPN_KIND_ASM, []() -> WeaponPtr {
        return std::make_unique<ASMWeapon>();
    });
    
    registerWeaponCreator(EN_WPN_KIND::WPN_KIND_AAM, []() -> WeaponPtr {
        return std::make_unique<AAMWeapon>();
    });
    
    registerWeaponCreator(EN_WPN_KIND::WPN_KIND_M_MINE, []() -> WeaponPtr {
        return std::make_unique<MineWeapon>();
    });
    
    // 교전계획 관리자 생성자 등록
    registerEngagementManagerCreator(EN_WPN_KIND::WPN_KIND_ALM, []() -> EngagementManagerPtr {
        return std::make_unique<ALMEngagementManager>();
    });
    
    registerEngagementManagerCreator(EN_WPN_KIND::WPN_KIND_ASM, []() -> EngagementManagerPtr {
        return std::make_unique<ASMEngagementManager>();
    });
    
    registerEngagementManagerCreator(EN_WPN_KIND::WPN_KIND_AAM, []() -> EngagementManagerPtr {
        return std::make_unique<AAMEngagementManager>();
    });
    
    registerEngagementManagerCreator(EN_WPN_KIND::WPN_KIND_M_MINE, []() -> EngagementManagerPtr {
        return std::make_unique<MineEngagementManager>();
    });
    
    // 무장 사양 등록
    m_weaponSpecs[EN_WPN_KIND::WPN_KIND_ALM] = WeaponSpecification("ALM", config.getALMMaxRange(), config.getALMSpeed(), config.getDefaultLaunchDelay());
    m_weaponSpecs[EN_WPN_KIND::WPN_KIND_ASM] = WeaponSpecification("ASM", config.getASMMaxRange(), config.getASMSpeed(), config.getDefaultLaunchDelay());
    m_weaponSpecs[EN_WPN_KIND::WPN_KIND_AAM] = WeaponSpecification("AAM", 80.0, 350.0, config.getDefaultLaunchDelay());
    m_weaponSpecs[EN_WPN_KIND::WPN_KIND_M_MINE] = WeaponSpecification("MINE", 30.0, config.getMineSpeed(), config.getDefaultLaunchDelay());
    
    std::cout << "WeaponFactory default creators registered" << std::endl;
}

} // namespace WeaponControl
