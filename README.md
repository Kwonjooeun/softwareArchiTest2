무장통제 시스템 새로운 아키텍처
1. 전체 구조
WeaponControlSystem
├── Core/                          # 핵심 비즈니스 로직
│   ├── LaunchTube/               # 발사관 관리
│   ├── Weapons/                  # 무장 관리
│   ├── EngagementManagers/       # 교전계획 관리
│   └── Services/                 # 핵심 서비스들
├── Infrastructure/               # 인프라 계층
│   ├── Communication/            # DDS 통신
│   ├── Persistence/              # 데이터 저장
│   ├── Configuration/            # 설정 관리
│   └── Logging/                  # 로깅
├── Application/                  # 애플리케이션 계층
│   ├── Controllers/              # 주 컨트롤러
│   ├── MessageHandlers/          # 메시지 처리
│   └── TaskManagers/             # 작업 관리
└── Common/                       # 공통 유틸리티
    ├── Types/                    # 공통 타입 정의
    ├── Utils/                    # 유틸리티 함수
    └── Interfaces/               # 공통 인터페이스
2. 핵심 클래스 설계
2.1 LaunchTube (완전히 단순화)
cpp// Core/LaunchTube/LaunchTube.h
class LaunchTube {
private:
    uint16_t m_tubeNumber;
    std::unique_ptr<IWeapon> m_weapon;
    std::unique_ptr<IEngagementManager> m_engagementManager;
    AssignmentInfo m_assignmentInfo;  // 할당 정보만 저장
    
public:
    explicit LaunchTube(uint16_t tubeNumber);
    
    // 할당 관리
    Result<void> assignWeapon(std::unique_ptr<IWeapon> weapon, 
                             std::unique_ptr<IEngagementManager> engManager,
                             const AssignmentInfo& info);
    void clearAssignment();
    bool hasWeapon() const { return m_weapon != nullptr; }
    
    // 단순한 정보 제공
    uint16_t getTubeNumber() const { return m_tubeNumber; }
    EN_WPN_KIND getWeaponKind() const;
    EN_WPN_CTRL_STATE getWeaponState() const;
    bool isLaunched() const;
    
    // 위임 메서드들
    Result<void> requestStateChange(EN_WPN_CTRL_STATE newState);
    Result<void> updateWaypoints(const std::vector<ST_WEAPON_WAYPOINT>& waypoints);
    Result<EngagementPlanResult> calculateEngagementPlan();
    
    void update();
};
2.2 무장 인터페이스 (더 명확한 분리)
cpp// Core/Weapons/IWeapon.h
class IWeapon {
public:
    virtual ~IWeapon() = default;
    
    // 기본 정보
    virtual EN_WPN_KIND getWeaponKind() const = 0;
    virtual WeaponSpecification getSpecification() const = 0;
    
    // 상태 관리 (Cancellation Token 포함)
    virtual EN_WPN_CTRL_STATE getCurrentState() const = 0;
    virtual Result<void> requestStateChange(EN_WPN_CTRL_STATE newState, 
                                           const CancellationToken& token = {}) = 0;
    
    // 발사 관리
    virtual bool isLaunched() const = 0;
    virtual void setLaunched(bool launched) = 0;
    
    // 조건 확인
    virtual bool checkInterlockConditions() const = 0;
    virtual bool isFireSolutionReady() const = 0;
    virtual void setFireSolutionReady(bool ready) = 0;
    
    virtual void update() = 0;
};

// Core/Weapons/WeaponBase.h
class WeaponBase : public IWeapon {
private:
    EN_WPN_KIND m_weaponKind;
    std::atomic<EN_WPN_CTRL_STATE> m_currentState;
    std::atomic<bool> m_launched;
    std::atomic<bool> m_fireSolutionReady;
    CancellationToken m_cancellationToken;
    
protected:
    virtual Result<void> onStateEnter(EN_WPN_CTRL_STATE state) { return Result<void>::success(); }
    virtual Result<void> onStateExit(EN_WPN_CTRL_STATE state) { return Result<void>::success(); }
    virtual Result<void> processLaunch(const CancellationToken& token);
    
public:
    explicit WeaponBase(EN_WPN_KIND weaponKind);
    
    // IWeapon 구현
    EN_WPN_KIND getWeaponKind() const override { return m_weaponKind; }
    EN_WPN_CTRL_STATE getCurrentState() const override { return m_currentState.load(); }
    
    Result<void> requestStateChange(EN_WPN_CTRL_STATE newState, 
                                   const CancellationToken& token = {}) override;
    
    bool isLaunched() const override { return m_launched.load(); }
    // ... 기타 구현
};
2.3 교전계획 관리자 (완전히 분리된 인터페이스)
cpp// Core/EngagementManagers/IEngagementManager.h
class IEngagementManager {
public:
    virtual ~IEngagementManager() = default;
    virtual Result<void> initialize(uint16_t tubeNumber) = 0;
    virtual Result<EngagementPlanResult> calculatePlan() = 0;
    virtual bool isEngagementPlanValid() const = 0;
    virtual void update() = 0;
};

// Core/EngagementManagers/IMineEngagementManager.h
class IMineEngagementManager : public IEngagementManager {
public:
    virtual Result<void> setDropPlan(uint32_t listNum, uint32_t planNum) = 0;
    virtual Result<void> updateDropPlanWaypoints(const std::vector<ST_WEAPON_WAYPOINT>& waypoints) = 0;
    virtual Result<AIEP_M_MINE_EP_RESULT> getMineEngagementResult() const = 0;
};

// Core/EngagementManagers/IMissileEngagementManager.h  
class IMissileEngagementManager : public IEngagementManager {
public:
    virtual Result<void> setTargetPosition(const SGEODETIC_POSITION& targetPos) = 0;
    virtual Result<void> setSystemTarget(uint32_t systemTargetId) = 0;
    virtual Result<void> updateWaypoints(const std::vector<ST_WEAPON_WAYPOINT>& waypoints) = 0;
    virtual Result<AIEP_ALM_ASM_EP_RESULT> getMissileEngagementResult() const = 0;
};
2.4 서비스 계층
cpp// Core/Services/WeaponControlService.h
class WeaponControlService {
private:
    std::unique_ptr<ILaunchTubeManager> m_tubeManager;
    std::unique_ptr<ITargetTrackingService> m_targetService;
    std::unique_ptr<IMineDropPlanService> m_mineService;
    
public:
    explicit WeaponControlService(std::unique_ptr<ILaunchTubeManager> tubeManager,
                                 std::unique_ptr<ITargetTrackingService> targetService,
                                 std::unique_ptr<IMineDropPlanService> mineService);
    
    // 핵심 비즈니스 로직
    Result<void> assignWeapon(const WeaponAssignmentRequest& request);
    Result<void> controlWeapon(const WeaponControlRequest& request);
    Result<void> updateWaypoints(const WaypointUpdateRequest& request);
    
    // 조회
    std::vector<LaunchTubeStatus> getAllTubeStatus() const;
    std::vector<EngagementPlanResult> getAllEngagementResults() const;
};

// Core/Services/ITargetTrackingService.h
class ITargetTrackingService {
public:
    virtual ~ITargetTrackingService() = default;
    virtual void updateTargetInfo(const TRKMGR_SYSTEMTARGET_INFO& targetInfo) = 0;
    virtual std::optional<TRKMGR_SYSTEMTARGET_INFO> getTarget(uint32_t systemTargetId) const = 0;
    virtual std::vector<uint32_t> getAllTargetIds() const = 0;
};
2.5 애플리케이션 컨트롤러
cpp// Application/Controllers/WeaponController.h
class WeaponController {
private:
    std::unique_ptr<WeaponControlService> m_weaponService;
    std::unique_ptr<IMessageHandler> m_messageHandler;
    std::unique_ptr<IPeriodicTaskManager> m_taskManager;
    std::unique_ptr<ICommunicationService> m_commService;
    
public:
    explicit WeaponController(std::unique_ptr<WeaponControlService> weaponService,
                             std::unique_ptr<IMessageHandler> messageHandler,
                             std::unique_ptr<IPeriodicTaskManager> taskManager,
                             std::unique_ptr<ICommunicationService> commService);
    
    Result<void> initialize();
    Result<void> start();
    void stop();
    
    // 상태 조회 (외부 API)
    std::vector<LaunchTubeStatus> getAllTubeStatus() const;
    SystemStatistics getSystemStatistics() const;
};
3. 주요 개선사항
3.1 의존성 주입
cpp// main.cpp에서 의존성 구성
auto config = SystemConfig::getInstance();
auto commService = std::make_unique<DdsCommunicationService>();
auto tubeManager = std::make_unique<LaunchTubeManager>(config.getMaxLaunchTubes());
auto targetService = std::make_unique<TargetTrackingService>();
auto mineService = std::make_unique<MineDropPlanService>(config.getMineDataPath());

auto weaponService = std::make_unique<WeaponControlService>(
    std::move(tubeManager), std::move(targetService), std::move(mineService));

auto messageHandler = std::make_unique<DdsMessageHandler>(weaponService.get());
auto taskManager = std::make_unique<PeriodicTaskManager>();

auto controller = std::make_unique<WeaponController>(
    std::move(weaponService), std::move(messageHandler), 
    std::move(taskManager), std::move(commService));
3.2 Result 타입으로 일관된 에러 처리
cpptemplate<typename T>
class Result {
private:
    std::variant<T, ErrorInfo> m_data;
    
public:
    static Result<T> success(T&& value) { return Result(std::forward<T>(value)); }
    static Result<T> failure(const std::string& message, int code = -1) { 
        return Result(ErrorInfo{message, code}); 
    }
    
    bool isSuccess() const { return std::holds_alternative<T>(m_data); }
    const T& value() const { return std::get<T>(m_data); }
    const ErrorInfo& error() const { return std::get<ErrorInfo>(m_data); }
};
3.3 설정 관리
cpp// config/system.ini
[System]
MaxLaunchTubes=6
UpdateIntervalMs=100
EngagementPlanIntervalMs=1000

[Paths]
MineDataPath=data/mine_plans
LogPath=logs
ConfigPath=config

[DDS]
DomainId=83
QosProfile=reliable
이 구조의 장점:

명확한 책임 분리: 각 계층과 클래스가 명확한 역할
테스트 용이성: 인터페이스 기반으로 Mock 객체 쉽게 생성
확장성: 새로운 무장 종류 추가가 쉬움
유지보수성: 변경 영향 범위가 제한적
재사용성: 각 컴포넌트가 독립적으로 재사용 가능
