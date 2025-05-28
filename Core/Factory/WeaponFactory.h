#pragma once

#include "../Weapons/IWeapon.h"
#include "../EngagementManagers/IEngagementManager.h"
#include "../../Common/Types/CommonTypes.h"
#include <memory>
#include <functional>
#include <map>

namespace WeaponControl {

// =============================================================================
// 무장 생성 팩토리
// =============================================================================

class WeaponFactory {
public:
    // 무장 생성자 타입 정의
    using WeaponCreator = std::function<WeaponPtr()>;
    using EngagementManagerCreator = std::function<EngagementManagerPtr()>;
    
    // 싱글톤 인스턴스 획득
    static WeaponFactory& getInstance();
    
    // 무장 및 교전계획 관리자 생성
    WeaponPtr createWeapon(EN_WPN_KIND weaponKind) const;
    EngagementManagerPtr createEngagementManager(EN_WPN_KIND weaponKind) const;
    
    // 생성자 등록 (확장성을 위해)
    void registerWeaponCreator(EN_WPN_KIND weaponKind, WeaponCreator creator);
    void registerEngagementManagerCreator(EN_WPN_KIND weaponKind, EngagementManagerCreator creator);
    
    // 지원되는 무장 종류 확인
    bool isWeaponSupported(EN_WPN_KIND weaponKind) const;
    
    // 무장 사양 정보 제공
    WeaponSpecification getWeaponSpecification(EN_WPN_KIND weaponKind) const;
    
private:
    WeaponFactory();
    ~WeaponFactory() = default;
    
    // 복사 및 이동 금지
    WeaponFactory(const WeaponFactory&) = delete;
    WeaponFactory& operator=(const WeaponFactory&) = delete;
    WeaponFactory(WeaponFactory&&) = delete;
    WeaponFactory& operator=(WeaponFactory&&) = delete;
    
    // 기본 생성자들 등록
    void registerDefaultCreators();
    
    // 생성자 맵
    std::map<EN_WPN_KIND, WeaponCreator> m_weaponCreators;
    std::map<EN_WPN_KIND, EngagementManagerCreator> m_engagementManagerCreators;
    std::map<EN_WPN_KIND, WeaponSpecification> m_weaponSpecs;
};

} // namespace WeaponControl
