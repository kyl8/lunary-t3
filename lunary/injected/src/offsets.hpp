#pragma once
// =============================================================================
// T3 Arena - Offsets & Component Database
// Dumped from: Assembly-CSharp.dll, GameBase.dll, T3Common.dll, T3Client.dll
// Game version: Global (com.xd.t3.global)
// Unity: IL2CPP
// =============================================================================

#include <cstddef>
#include <cstdint>

namespace lunary::injected::offsets {

// =============================================================================
// IL2CPP Class Namespaces & Names
// =============================================================================
inline constexpr const char* kNamespaceT3Game = "T3Game";
inline constexpr const char* kClassEntity = "Entity";
inline constexpr const char* kClassContext = "Context";
inline constexpr const char* kClassActorSystem = "ActorSystem";

// =============================================================================
// Entity Component IDs (ushort static fields, sequential in Entity class)
// TypeDefIndex: 11672 - Entity
// =============================================================================
namespace ComponentId {
    // First 12 IDs match legacy dump — unchanged
    inline constexpr std::uint16_t ActionAll                = 0x000;
    inline constexpr std::uint16_t Action                   = 0x002;
    inline constexpr std::uint16_t ActionLeft               = 0x004;
    inline constexpr std::uint16_t ActionLower              = 0x006;
    inline constexpr std::uint16_t ActionRight              = 0x008;
    inline constexpr std::uint16_t ActionUpper              = 0x00A;
    inline constexpr std::uint16_t ActionWholeBody          = 0x00C;
    inline constexpr std::uint16_t ActiveState              = 0x00E;
    inline constexpr std::uint16_t ActivityData             = 0x010;
    inline constexpr std::uint16_t Actor                    = 0x012;
    inline constexpr std::uint16_t AddScore                 = 0x014;
    inline constexpr std::uint16_t AddScoreInfo             = 0x016;
    // Resynced from dump.cs (game update added components shifting all IDs +8 from here on)
    inline constexpr std::uint16_t AFK                      = 0x020;
    inline constexpr std::uint16_t Agonal                   = 0x022;
    inline constexpr std::uint16_t AI                       = 0x024;
    inline constexpr std::uint16_t AIDebugInfo              = 0x026;
    inline constexpr std::uint16_t Aim                      = 0x028;
    inline constexpr std::uint16_t AimControl               = 0x02A;
    inline constexpr std::uint16_t AimHit                   = 0x02C;
    inline constexpr std::uint16_t Alive                    = 0x02E;
    inline constexpr std::uint16_t AliveCount               = 0x030;
    inline constexpr std::uint16_t Analyse                  = 0x032;
    inline constexpr std::uint16_t Armor                    = 0x034;
    inline constexpr std::uint16_t ArmorTemp                = 0x036;
    inline constexpr std::uint16_t AssistedAim              = 0x038;
    inline constexpr std::uint16_t AttributeCoef            = 0x03A;
    inline constexpr std::uint16_t AttributeMoveSpeedCoef   = 0x03C;
    inline constexpr std::uint16_t AttributeRecover         = 0x03E;
    inline constexpr std::uint16_t AutoCover                = 0x040;
    inline constexpr std::uint16_t AutoForwardMove          = 0x042;
    inline constexpr std::uint16_t AutoShoot                = 0x044;
    inline constexpr std::uint16_t Bullet                   = 0x05A;
    inline constexpr std::uint16_t BulletStatus             = 0x05C;
    inline constexpr std::uint16_t Camera                   = 0x05E;
    inline constexpr std::uint16_t Camouflage               = 0x064;
    inline constexpr std::uint16_t Cancel                   = 0x066;
    inline constexpr std::uint16_t ChangeHero               = 0x068;
    inline constexpr std::uint16_t Character                = 0x06C;
    inline constexpr std::uint16_t Charge                   = 0x06E;
    inline constexpr std::uint16_t Control                  = 0x076;
    inline constexpr std::uint16_t DamageCoef               = 0x07E;
    inline constexpr std::uint16_t DamageCoefData           = 0x080;
    inline constexpr std::uint16_t DamageInfo               = 0x082;
    inline constexpr std::uint16_t DamageMultiply           = 0x084;
    inline constexpr std::uint16_t DamageReduceCoef         = 0x086;
    inline constexpr std::uint16_t DamageSourceAngleCoef    = 0x088;
    inline constexpr std::uint16_t DamageStatus             = 0x08A;
    inline constexpr std::uint16_t DamageToWeaponPower      = 0x08C;
    inline constexpr std::uint16_t Destroy                  = 0x090;
    inline constexpr std::uint16_t DisableReload            = 0x092;
    inline constexpr std::uint16_t Disarm                   = 0x094;
    inline constexpr std::uint16_t DLTrainingMode           = 0x098;
    inline constexpr std::uint16_t Fear                     = 0x0A6;
    inline constexpr std::uint16_t FightData                = 0x0AE;
    inline constexpr std::uint16_t FightDataClient          = 0x0AC;
    inline constexpr std::uint16_t Follow                   = 0x0B2;
    inline constexpr std::uint16_t ForbidenSkill            = 0x0B4;
    inline constexpr std::uint16_t Force                    = 0x0B8;
    inline constexpr std::uint16_t ForceVelocity            = 0x0BA;
    inline constexpr std::uint16_t GameData                 = 0x0BC;
    inline constexpr std::uint16_t GameMode                 = 0x0BE;
    inline constexpr std::uint16_t HealCoef                 = 0x0D2;
    inline constexpr std::uint16_t HealInfo                 = 0x0D4;
    inline constexpr std::uint16_t Health                   = 0x0D6;
    inline constexpr std::uint16_t Hit                      = 0x0DA;
    inline constexpr std::uint16_t HitPoint                 = 0x0DC;
    inline constexpr std::uint16_t HitPointContainer        = 0x0DE;
    inline constexpr std::uint16_t Hold                     = 0x0E0;
    inline constexpr std::uint16_t HoldPower                = 0x0E2;
    inline constexpr std::uint16_t Identify                 = 0x0EA;
    inline constexpr std::uint16_t Input                    = 0x0EC;
    inline constexpr std::uint16_t Invincible               = 0x0EE;
    inline constexpr std::uint16_t Jump                     = 0x0F0;
    inline constexpr std::uint16_t KnockBack                = 0x0F2;
    inline constexpr std::uint16_t KnockBackState           = 0x0F4;
    inline constexpr std::uint16_t MoveStatus               = 0x10A;
    inline constexpr std::uint16_t PassiveSkill             = 0x116;
    inline constexpr std::uint16_t Path                     = 0x118;
    inline constexpr std::uint16_t Physics                  = 0x124;
    inline constexpr std::uint16_t Player                   = 0x12C;
    inline constexpr std::uint16_t PlayerEquipment          = 0x12E;
    inline constexpr std::uint16_t PlayerShop               = 0x132;
    inline constexpr std::uint16_t PlayerSkillSlots         = 0x134;
    inline constexpr std::uint16_t Power                    = 0x13E;
    inline constexpr std::uint16_t Recoil                   = 0x158;
    inline constexpr std::uint16_t Reload                   = 0x15A;
    inline constexpr std::uint16_t ReloadDynamic            = 0x15C;
    inline constexpr std::uint16_t Retainer                 = 0x15E;
    inline constexpr std::uint16_t RetainerData             = 0x160;
    inline constexpr std::uint16_t RetainerState            = 0x162;
    inline constexpr std::uint16_t Revive                   = 0x164;
    inline constexpr std::uint16_t RigidBody                = 0x168;
    inline constexpr std::uint16_t SceneActor               = 0x170;
    inline constexpr std::uint16_t Shield                   = 0x18A;
    inline constexpr std::uint16_t ShieldTemp               = 0x18C;
    inline constexpr std::uint16_t ShootPoint               = 0x190;
    inline constexpr std::uint16_t ShootStatus              = 0x192;
    inline constexpr std::uint16_t SightBarrier             = 0x194;
    inline constexpr std::uint16_t Silent                   = 0x196;
    inline constexpr std::uint16_t Skill1                   = 0x198;
    inline constexpr std::uint16_t Skill2                   = 0x19A;
    inline constexpr std::uint16_t SkillEx                  = 0x19C;
    inline constexpr std::uint16_t SkillExCrt               = 0x19E;
    inline constexpr std::uint16_t SkillExOverflow          = 0x1A0;
    inline constexpr std::uint16_t SkillPassive             = 0x1A2;
    inline constexpr std::uint16_t Slowdown                 = 0x1A6;
    inline constexpr std::uint16_t Status                   = 0x1AE;
    inline constexpr std::uint16_t Summon                   = 0x1B6;
    inline constexpr std::uint16_t Target                   = 0x1BE;
    inline constexpr std::uint16_t Targets                  = 0x1C0;
    inline constexpr std::uint16_t Teleportation            = 0x1C2;
    inline constexpr std::uint16_t Transform                = 0x1CC;
    inline constexpr std::uint16_t TransformOffset          = 0x1CE;
    inline constexpr std::uint16_t Trigger                  = 0x1D4;
    inline constexpr std::uint16_t Velocity                 = 0x1DE;
    inline constexpr std::uint16_t VisiblesStatus           = 0x1E0;
    inline constexpr std::uint16_t WarmupCoef               = 0x1E4;
    inline constexpr std::uint16_t WeaponAim                = 0x1E6;
    inline constexpr std::uint16_t WeaponCharge             = 0x1E8;
    inline constexpr std::uint16_t Weapon                   = 0x1EA;
    inline constexpr std::uint16_t WeaponCritical           = 0x1EC;
    inline constexpr std::uint16_t WeaponHeat               = 0x1EE;
    inline constexpr std::uint16_t WeaponHitTimes           = 0x1F0;
    inline constexpr std::uint16_t WeaponLevel              = 0x1F2;
    inline constexpr std::uint16_t WeaponPower1             = 0x1F4;
    inline constexpr std::uint16_t WeaponPower2             = 0x1F6;
    inline constexpr std::uint16_t WeaponPower3             = 0x1F8;
    inline constexpr std::uint16_t WeaponShoot              = 0x1FA;
    inline constexpr std::uint16_t WeaponWarmup             = 0x1FC;
    inline constexpr std::uint16_t Weightless               = 0x1FE;
}  // namespace ComponentId

// =============================================================================
// Component Field Offsets (offset from component object base)
// Each component inherits from Component (8 bytes vtable on 64-bit)
// Fields start at offset 0x18 on 64-bit
// =============================================================================

// TypeDefIndex: 11782 - HealthComponent
struct HealthComponent {
    static constexpr std::size_t kOffset = 0x18;
    static constexpr std::size_t current = 0x18;  // int32
    static constexpr std::size_t max     = 0x1C;  // int32
};

// TypeDefIndex: 11796 - InvincibleComponent
struct InvincibleComponent {
    static constexpr std::size_t kOffset = 0x18;
    static constexpr std::size_t invincibleCount = 0x18;  // int32
};

// TypeDefIndex: 11733 - DamageCoefComponent
struct DamageCoefComponent {
    static constexpr std::size_t kOffset = 0x18;
    static constexpr std::size_t damageCoef         = 0x18;  // float
    static constexpr std::size_t weakDamageCoef     = 0x1C;  // float
    static constexpr std::size_t damageIncreaseCoef = 0x20;  // float
    static constexpr std::size_t shootDamageCoef    = 0x24;  // float
};

// TypeDefIndex: 11740 - DamageReduceCoefComponent
struct DamageReduceCoefComponent {
    static constexpr std::size_t kOffset = 0x18;
    static constexpr std::size_t damageReduceCoefs = 0x18;  // float[]
};

// TypeDefIndex: 11699 - AttributeMoveSpeedCoefComponent
struct AttributeMoveSpeedCoefComponent {
    static constexpr std::size_t kOffset = 0x18;
    static constexpr std::size_t moveSpeedCoef = 0x18;  // float
    static constexpr std::size_t gravityCoef   = 0x1C;  // float
};

// TypeDefIndex: 11810 - MoveStatusComponent
struct MoveStatusComponent {
    static constexpr std::size_t kOffset = 0x18;
    static constexpr std::size_t trampolining     = 0x18;  // bool
    static constexpr std::size_t floating         = 0x19;  // bool
    static constexpr std::size_t controllerStack  = 0x1A;  // ushort
    static constexpr std::size_t autoForward      = 0x1C;  // bool
    static constexpr std::size_t autoMoveController = 0x1D; // bool
    static constexpr std::size_t airFly           = 0x1E;  // bool
};

// TypeDefIndex: 11865 - RigidBodyComponent
struct RigidBodyComponent {
    static constexpr std::size_t kOffset = 0x18;
    static constexpr std::size_t gravity        = 0x18;  // float
    static constexpr std::size_t elasticity     = 0x1C;  // float
    static constexpr std::size_t friction       = 0x20;  // float
    static constexpr std::size_t passTarget     = 0x24;  // bool
    static constexpr std::size_t detectedAllPassTarget = 0x25; // bool
};

// TypeDefIndex: 11946 - WeightlessComponent
struct WeightlessComponent {
    static constexpr std::size_t kOffset = 0x18;
    static constexpr std::size_t WeightlessCount = 0x18;  // int32
};

// TypeDefIndex: 11920 - TransformComponent
struct TransformComponent {
    static constexpr std::size_t kOffset = 0x18;
    static constexpr std::size_t position = 0x18;  // Vector3
    static constexpr std::size_t rotation = 0x24;  // Quaternion
};

// TypeDefIndex: 11689 - AimComponent
struct AimComponent {
    static constexpr std::size_t kOffset = 0x18;
    static constexpr std::size_t direct = 0x18;  // Vector3
};

// TypeDefIndex: 11855 - RecoilComponent
struct RecoilComponent {
    static constexpr std::size_t kOffset = 0x18;
    static constexpr std::size_t recoilX         = 0x18;  // float
    static constexpr std::size_t recoilY         = 0x1C;  // float
    static constexpr std::size_t recoilXDirCount = 0x20;  // ushort
    static constexpr std::size_t scatter         = 0x24;  // float
    static constexpr std::size_t recoilXVDelta   = 0x28;  // float
    static constexpr std::size_t recoilYVDelta   = 0x2C;  // float
    static constexpr std::size_t recoilXV        = 0x30;  // float
    static constexpr std::size_t recoilYV        = 0x34;  // float
    static constexpr std::size_t updateTime      = 0x38;  // int32
    static constexpr std::size_t moveFlag        = 0x3C;  // int32
    static constexpr std::size_t recoilYMoveToTimeMs = 0x40; // int32
};

// TypeDefIndex: 11936 - WeaponComponent
struct WeaponComponent {
    static constexpr std::size_t kOffset = 0x18;
    static constexpr std::size_t weaponIndex       = 0x18;  // WeaponIndex (enum)
    static constexpr std::size_t beginShootingTime = 0x1C;  // int32
    static constexpr std::size_t shootingTime      = 0x20;  // int32
    static constexpr std::size_t shootBulletTime   = 0x24;  // int32
    static constexpr std::size_t isShooting        = 0x28;  // bool
    static constexpr std::size_t shootGapTimeMs    = 0x2A;  // ushort
    static constexpr std::size_t isBreak           = 0x2C;  // int32
    static constexpr std::size_t changeWeaponCount = 0x30;  // int32
};

// TypeDefIndex: 11944 - WeaponShootComponent
struct WeaponShootComponent {
    static constexpr std::size_t kOffset = 0x18;
    static constexpr std::size_t lastShootBulletTime = 0x18;  // int32
    static constexpr std::size_t shootNum            = 0x1C;  // ushort
    static constexpr std::size_t curShootBullet      = 0x1E;  // ushort
    static constexpr std::size_t combo               = 0x20;  // int32
};

// TypeDefIndex: 11856 - ReloadComponent
struct ReloadComponent {
    static constexpr std::size_t kOffset = 0x18;
    static constexpr std::size_t canReloadIfFullClip = 0x18;  // bool
    static constexpr std::size_t reloadDuration      = 0x1C;  // int32
    static constexpr std::size_t isCommonClip        = 0x20;  // bool
    static constexpr std::size_t maxClip             = 0x24;  // int32
    static constexpr std::size_t coverClip           = 0x28;  // int32
    static constexpr std::size_t clipUseOnce         = 0x2C;  // int32
    static constexpr std::size_t maxClip2            = 0x30;  // int32
};

struct ReloadDynamicComponent {
    static constexpr std::size_t curClip  = 0x18;
    static constexpr std::size_t curClip2 = 0x1C;
    static constexpr std::size_t curClip3 = 0x20;
};

// TypeDefIndex: 11886 - ShieldComponent
struct ShieldComponent {
    static constexpr std::size_t kOffset = 0x18;
    static constexpr std::size_t current = 0x18;  // int32
    static constexpr std::size_t max     = 0x1C;  // int32
};

// TypeDefIndex: 11797 - JumpComponent
struct JumpComponent {
    static constexpr std::size_t kOffset = 0x18;
    static constexpr std::size_t maxInAirJumpCount         = 0x18;  // int32
    static constexpr std::size_t inAirJumpCount            = 0x1C;  // int32
    static constexpr std::size_t enableInAirJump           = 0x20;  // bool
    static constexpr std::size_t enableInAirJumpPassiveSkill = 0x21; // bool
};

// TypeDefIndex: 11703 - AutoShootComponent
struct AutoShootComponent {
    static constexpr std::size_t kOffset = 0x18;
    static constexpr std::size_t disAutoShootCount = 0x18;  // int16
};

// TypeDefIndex: 11720 - CamouflageComponent
struct CamouflageComponent {
    static constexpr std::size_t kOffset = 0x18;
    static constexpr std::size_t camouflageTargetUserID = 0x18;  // string*
    static constexpr std::size_t uncamouflageFrame       = 0x20;  // int32
};

// TypeDefIndex: 11751 - DLTrainingModeComponent
struct DLTrainingModeComponent {
    static constexpr std::size_t kOffset = 0x18;
    static constexpr std::size_t HBCPTimeMS       = 0x18;  // int32
    static constexpr std::size_t HBPayLoad1TimeMS = 0x1C;  // int32
    static constexpr std::size_t HBPayLoad2TimeMS = 0x20;  // int32
    static constexpr std::size_t IgnoreSkillCD    = 0x24;  // bool
    static constexpr std::size_t ReviveTimeMs     = 0x28;  // int32
};

// TypeDefIndex: 11692 - AliveComponent
struct AliveComponent {
    static constexpr std::size_t kOffset = 0x18;
    static constexpr std::size_t livingTimeMs = 0x18;  // int32
    static constexpr std::size_t reviveTimeMs = 0x1C;  // int32
    static constexpr std::size_t spawnTime    = 0x20;  // int32
    static constexpr std::size_t deadTime     = 0x24;  // int32
};

// TypeDefIndex: 11795 - InputComponent
struct InputComponent {
    static constexpr std::size_t kOffset = 0x18;
    static constexpr std::size_t input   = 0x18;  // Vector2
    static constexpr std::size_t direct  = 0x20;  // Vector3
    static constexpr std::size_t onGround = 0x2C; // bool
    static constexpr std::size_t dstPos  = 0x30;  // Vector3
};

// TypeDefIndex: 11891 - SightBarrierComponent
struct SightBarrierComponent {
    static constexpr std::size_t kOffset = 0x18;
    // No fields - marker component
};

// TypeDefIndex: 11829 - PhysicsComponent
struct PhysicsComponent {
    static constexpr std::size_t kOffset = 0x18;
    static constexpr std::size_t shapeType    = 0x18;  // PhysicsShapeType
    static constexpr std::size_t needCollider = 0x1C;  // bool
    static constexpr std::size_t filterGroup  = 0x20;  // uint32
    static constexpr std::size_t filterMask   = 0x24;  // uint32
    static constexpr std::size_t blockType    = 0x28;  // PhysShapeBlockType
};

// TypeDefIndex: 11768 - ForceVelocityComponent
struct ForceVelocityComponent {
    static constexpr std::size_t kOffset = 0x18;
    static constexpr std::size_t velocity  = 0x18;  // Vector3[]
    static constexpr std::size_t forceFrom = 0x20;  // ushort[]
    static constexpr std::size_t catched   = 0x28;  // bool[]
};

// TypeDefIndex: 11893 - Skill1Component
struct Skill1Component {
    static constexpr std::size_t kOffset = 0x18;
    static constexpr std::size_t current    = 0x18;  // ushort
    static constexpr std::size_t max        = 0x1A;  // ushort
    static constexpr std::size_t count      = 0x1C;  // ushort
    static constexpr std::size_t countPerCD = 0x1E;  // ushort
};

// TypeDefIndex: 11890 - ShootStatusComponent
struct ShootStatusComponent {
    static constexpr std::size_t kOffset = 0x18;
    static constexpr std::size_t id           = 0x18;  // int32
    static constexpr std::size_t pos          = 0x1C;  // Vector3
    static constexpr std::size_t direct       = 0x28;  // Vector3
    static constexpr std::size_t effectSource = 0x34;  // EffectSource
};

// TypeDefIndex: 11715 - BulletComponent
struct BulletComponent {
    static constexpr std::size_t kOffset = 0x18;
    static constexpr std::size_t config           = 0x18;  // int32
    static constexpr std::size_t hitGroundActive  = 0x1C;  // bool
    static constexpr std::size_t hitTargetActive  = 0x1D;  // bool
    static constexpr std::size_t everyHitActive   = 0x1E;  // bool
    static constexpr std::size_t shootPos         = 0x20;  // Vector3
    static constexpr std::size_t coef             = 0x2C;  // float
    static constexpr std::size_t speedCoef        = 0x30;  // float
};

// TypeDefIndex: 11783 - HideClientDamageInfoComponent
struct HideClientDamageInfoComponent {
    static constexpr std::size_t kOffset = 0x18;
    static constexpr std::size_t hide = 0x18;  // bool
};

// =============================================================================
// Context Class (TypeDefIndex: 12112)
// Singleton: Context.current
// =============================================================================
struct ContextLayout {
    static constexpr std::size_t tickTime      = 0x24;  // int32
    static constexpr std::size_t startTime     = 0x28;  // int64
    static constexpr std::size_t roundState    = 0x2C;  // int32
    static constexpr std::size_t m_FixedTime   = 0x30;  // int32
    static constexpr std::size_t isRestart     = 0x34;  // int32
};

// =============================================================================
// BaseWeaponPlugin (TypeDefIndex: 12921)
// Weapon configuration - fires rate, damage, etc.
// =============================================================================
struct BaseWeaponPluginLayout {
    static constexpr std::size_t index              = 0x9FC0;  // WeaponIndex
    static constexpr std::size_t weaponSuitIndex    = 0x9FD8;  // WeaponSuitIndex
    static constexpr std::size_t shootTriggerType   = 0xA3D8;  // ShootTriggerType
    static constexpr std::size_t shootGapTime       = 0xA3F0;  // float - INTERVALO ENTRE TIROS
    static constexpr std::size_t shootAnimSpeed     = 0xA3FA;  // float
    static constexpr std::size_t shootDelayTime     = 0xA40A;  // float
    static constexpr std::size_t weaponCondition    = 0xA42A;  // ISkillConditionPlugin*
    static constexpr std::size_t shootEffect        = 0xA51E;  // IShootCastEffect*
    static constexpr std::size_t recoil             = 0xA536;  // IWeaponRecoilPlugin*
    static constexpr std::size_t scatter            = 0xA552;  // IWeaponScatterPlugin*
    static constexpr std::size_t heat               = 0xA572;  // IWeaponHeatPlugin*
    static constexpr std::size_t charger            = 0xA58E;  // IWeaponChargerPlugin*
    static constexpr std::size_t continuousFireCount = 0xA63A; // ushort
    static constexpr std::size_t continuousFireTick  = 0xA65A; // float
    static constexpr std::size_t continuousFireUse1Clip = 0xA6AA; // bool
    static constexpr std::size_t weaponlevel         = 0xA6BA;  // IWeaponLevelPlugin*
    static constexpr std::size_t extraAimPoint1      = 0xA756;  // IExtraAimPoint*
    static constexpr std::size_t extraAimPoint2      = 0xA76E;  // IExtraAimPoint*
    static constexpr std::size_t weaponWarmup        = 0xA786;  // IWeaponWarmupPlugin*
    static constexpr std::size_t weaponPower         = 0xA79E;  // IWeaponPowerPlugin*
    static constexpr std::size_t weaponCombo         = 0xA816;  // IWeaponComboPlugin*
    static constexpr std::size_t shootGapTime2       = 0xA856;  // float
    static constexpr std::size_t shootGapTime3       = 0xA8CE;  // float
};

// =============================================================================
// WeaponScatter (TypeDefIndex: 12920)
// =============================================================================
struct WeaponScatterLayout {
    static constexpr std::size_t startIndex          = 0x9E9E;  // int32
    static constexpr std::size_t shotScatter         = 0x9EB8;  // float
    static constexpr std::size_t maxShotScatter      = 0x9F30;  // float
    static constexpr std::size_t delayMoveBackTime   = 0x9F48;  // float
    static constexpr std::size_t scatterMoveBackSpeed = 0x9F60; // float
    static constexpr std::size_t scatterMoveBackAcc  = 0x9F78;  // float
    static constexpr std::size_t initScatter         = 0x9F98;  // float
    static constexpr std::size_t reverseInitScatter  = 0x9FA8;  // float
};

// =============================================================================
// WeaponRecoil (TypeDefIndex: 12915)
// =============================================================================
struct WeaponRecoilLayout {
    static constexpr std::size_t shotRecoilX = 0x9960;  // float
};

// =============================================================================
// InvincibleEffect (TypeDefIndex: 12397)
// =============================================================================
struct InvincibleEffectLayout {
    static constexpr std::size_t isInvincible = 0x0;  // bool
};

// =============================================================================
// SkillCD (TypeDefIndex: 12463) - Skill Cooldown Manager
// =============================================================================
// Methods:
//   CheckSkillCD(Entity entity, SkillActionType skillType, int time) -> bool
//   SkillCD(Entity entity, SkillActionType skillType) -> void
//   GetSkillCDCount(Entity entity, SkillActionType skillType) -> ushort

// =============================================================================
// TeleportationComponent (TypeDefIndex: component)
// =============================================================================
// TeleportationActionConfig (TypeDefIndex: 971)
// SetPositionToPoint (TypeDefIndex: 912)
// SetPositionToTargetEffect (TypeDefIndex: 913)
// SetPositionToTargetWithRotateEffect (TypeDefIndex: 914)

// =============================================================================
// T3XLuaClientCheatManagerWrap (TypeDefIndex: 1638)
// Built-in cheat manager accessible via XLua
// Method: __Register(IntPtr L) @ RVA: 0x1CB6D14
// =============================================================================
inline constexpr std::uintptr_t RVA_T3XLuaClientCheatManager_Register = 0x1CB6D14;

// =============================================================================
// Key Method RVAs for Entity Component Access
// All follow pattern: get_hasX(), get_X(), AddX(), DelX()
// =============================================================================
namespace EntityMethodRVA {
    // Entity.health getter chain
    inline constexpr std::uintptr_t get_hasHealth   = 0x17F3184;  // bool hasHealth
    inline constexpr std::uintptr_t get_health      = 0x17F31FC;  // HealthComponent health
    inline constexpr std::uintptr_t AddHealth       = 0x17F3330;  // HealthComponent AddHealth()
    inline constexpr std::uintptr_t DelHealth       = 0x17F3334;  // void DelHealth()

    // Context.current singleton
    // get_current() RVA is in Context class
    inline constexpr std::uintptr_t Context_get_current = 0x185D570;

    // WeaponPlugin access (via retainer data / actor config)
    // These are accessed through the actor system plugin properties
    inline constexpr std::uintptr_t BulletPassBlockPhysConditionPlugin = 0x308858; // linea 308858
}  // namespace EntityMethodRVA

// =============================================================================
// IL2CPP API Function Offsets (GameAssembly.dll exports)
// =============================================================================
namespace Il2CppExport {
    inline constexpr const char* il2cpp_class_from_name           = "il2cpp_class_from_name";
    inline constexpr const char* il2cpp_class_get_field_from_name = "il2cpp_class_get_field_from_name";
    inline constexpr const char* il2cpp_field_static_get_value    = "il2cpp_field_static_get_value";
    inline constexpr const char* il2cpp_field_get_value           = "il2cpp_field_get_value";
    inline constexpr const char* il2cpp_field_set_value           = "il2cpp_field_set_value";
    inline constexpr const char* il2cpp_class_get_method_from_name = "il2cpp_class_get_method_from_name";
    inline constexpr const char* il2cpp_runtime_invoke            = "il2cpp_runtime_invoke";
    inline constexpr const char* il2cpp_string_new                = "il2cpp_string_new";
    inline constexpr const char* il2cpp_domain_get                = "il2cpp_domain_get";
    inline constexpr const char* il2cpp_thread_attach             = "il2cpp_thread_attach";
}  // namespace Il2CppExport

// =============================================================================
// GameObject / Transform Unity Classes
// =============================================================================
namespace UnityOffsets {
    // Transform
    inline constexpr std::size_t Transform_position = 0x38;  // Vector3 (relative to 64-bit)
    inline constexpr std::size_t Transform_rotation = 0x44;  // Quaternion
    inline constexpr std::size_t Transform_localScale = 0x54; // Vector3

    // GameObject
    inline constexpr std::size_t GameObject_Components = 0x30; // Component list offset
}  // namespace UnityOffsets

// =============================================================================
// Entity Helper: Component Array Access
// Each entity stores components indexed by component ID.
// The component pointer array is at a fixed offset in the entity object.
// =============================================================================
inline constexpr std::size_t Entity_ComponentArray = 0x10;  // Component*[] (pointer to array)

// =============================================================================
// GetLocalPlayerEntity helpers
// Multiple classes expose this:
//   - TypeDefIndex around 13484 (SelectSkinItemView) @ RVA: 0x1A1CE80
//   - Static version @ RVA: 0x1A287D4
//   - Alternate @ RVA: 0x1AA35DC
// =============================================================================
inline constexpr std::uintptr_t RVA_GetLocalPlayerEntity_Instance = 0x1A1CE80;
inline constexpr std::uintptr_t RVA_GetLocalPlayerEntity_Static   = 0x1A287D4;

// =============================================================================
// Entity property: isLocalPlayer, isFriend, isEnemy, isDead
// =============================================================================
// Entity.isLocalPlayer getter RVA (from Entity class methods)
inline constexpr std::uintptr_t RVA_Entity_get_isLocalPlayer = 0x17F3184;  // aproximado
inline constexpr std::uintptr_t RVA_Entity_get_isDead        = 0x17F3184;  // precisa verificar

// =============================================================================
// ActorSystem (TypeDefIndex: 13022) 
// Used to iterate all entities
// =============================================================================
// ActorSystem has entity iterators via Context.Cursor
// Accessed via: Context.current.actorSystem

// =============================================================================
// Useful constants for feature tuning
// =============================================================================
namespace Defaults {
    inline constexpr float kNormalMoveSpeed     = 1.0f;
    inline constexpr float kSpeedHackMultiplier = 3.0f;
    inline constexpr float kNormalDamage        = 1.0f;
    inline constexpr float kOneHitKillDamage    = 999.0f;
    inline constexpr float kNoGravity           = 0.0f;
    inline constexpr float kInfiniteShieldValue = 99999;
    inline constexpr int   kInfiniteAmmoClip    = 999;
    inline constexpr int   kMaxAirJumps         = 99;
    inline constexpr int   kInvincibleCount     = 1;
    inline constexpr float kRapidFireGapMs      = 0.0f;
    inline constexpr float kNoRecoilValue       = 0.0f;
    inline constexpr float kNoSpreadValue       = 0.0f;
}  // namespace Defaults

// =============================================================================
// WeaponSystem function RVAs (TypeDefIndex: 13211)
// Static methods that control recoil, scatter, fire rate
// =============================================================================
inline constexpr std::uintptr_t RVA_WeaponSystem_DoRecoilAndScatter = 0x19D0D58;
inline constexpr std::uintptr_t RVA_WeaponSystem_DoRecoilV          = 0x19D02D4;
inline constexpr std::uintptr_t RVA_WeaponSystem_DoScatterV         = 0x19D07B0;
inline constexpr std::uintptr_t RVA_WeaponSystem_DoScatter          = 0x19D18C8;
inline constexpr std::uintptr_t RVA_WeaponSystem_GetShootDelayTime  = 0x19D08A0;
inline constexpr std::uintptr_t RVA_WeaponSystem_GetShootGapTime    = 0x19CF92C;
inline constexpr std::uintptr_t RVA_WeaponSystem_CanShoot           = 0x19CF6FC;
inline constexpr std::uintptr_t RVA_WeaponSystem_ResetRecoil        = 0x19D7B30;

// Stable Entity getters used by retained panel features.  Calling these
// avoids treating Entity static-field offsets as runtime component IDs.
inline constexpr std::uintptr_t RVA_Entity_GetMoveSpeedCoef = 0x17F67E8;
inline constexpr std::uintptr_t RVA_Entity_GetMoveStatus    = 0x1803FC8;
inline constexpr std::uintptr_t RVA_Entity_GetRigidBody     = 0x17D7FC8;
inline constexpr std::uintptr_t RVA_Entity_GetWeightless    = 0x1813E38;
inline constexpr std::uintptr_t RVA_Entity_GetReload        = 0x180945C;
inline constexpr std::uintptr_t RVA_Entity_GetReloadDynamic = 0x1809684;
inline constexpr std::uintptr_t RVA_Entity_GetRecoil        = 0x1809234;
inline constexpr std::uintptr_t RVA_Entity_GetSkill1        = 0x180D600;
inline constexpr std::uintptr_t RVA_Entity_GetSkill2        = 0x180D828;
inline constexpr std::uintptr_t RVA_Entity_GetSkillEx       = 0x180DA50;
inline constexpr std::uintptr_t RVA_Entity_GetTransform     = 0x17D8394;
inline constexpr std::uintptr_t RVA_Entity_GetWeapon        = 0x18128A8;
inline constexpr std::uintptr_t RVA_Entity_GetWeaponShoot   = 0x18139E8;
inline constexpr std::uintptr_t RVA_Entity_GetAim           = 0x17F5404;
inline constexpr std::uintptr_t RVA_AimComponent_SetDirect  = 0x17E1BB4;
inline constexpr std::uintptr_t RVA_Entity_HasForceVelocity = 0x17FEBF0;
inline constexpr std::uintptr_t RVA_Entity_GetForceVelocity = 0x17FEC68;
inline constexpr std::uintptr_t RVA_Entity_DelForceVelocity = 0x17FEDA0;
inline constexpr std::uintptr_t RVA_Entity_HasKnockBack     = 0x1802570;
inline constexpr std::uintptr_t RVA_Entity_GetKnockBack     = 0x18025E8;
inline constexpr std::uintptr_t RVA_Entity_DelKnockBack     = 0x1802720;
inline constexpr std::uintptr_t RVA_Entity_HasKnockBackState = 0x1802798;
inline constexpr std::uintptr_t RVA_Entity_DelKnockBackState = 0x1802948;

inline constexpr std::uintptr_t RVA_SkillSystem_CheckSkillCD = 0x19B7A9C;
inline constexpr std::uintptr_t RVA_BulletPhysCondition      = 0x1704EEC;
inline constexpr std::uintptr_t RVA_BulletPassBlockCondition = 0x1705180;
// The game can select either UnityPhysicsSystemProxy (above) or the native
// T3Game.PhysicsSystem backend (below), depending on the hero/projectile.
inline constexpr std::uintptr_t RVA_PhysicsSystem_BulletPhysCondition = 0x1982F38;
inline constexpr std::uintptr_t RVA_PhysicsSystem_BulletPassBlockCondition = 0x1983198;
inline constexpr std::uintptr_t RVA_PhysicsSystem_RaycastStaticAcrossTest = 0x1984A24;
inline constexpr std::uintptr_t RVA_PhysicsSystem_CheckStaticBlock = 0x19846CC;
inline constexpr std::uintptr_t RVA_PhysicsSystem_Execute = 0x1987B60;
inline constexpr std::uintptr_t RVA_IsCoveredBySightCover    = 0x17109BC;
inline constexpr std::uintptr_t RVA_KnockBack_IsActive       = 0x1818820;
inline constexpr std::uintptr_t RVA_MaterialAdjust_ECSUpdate = 0x240C88C;
inline constexpr std::uintptr_t RVA_MaterialAdjust_ForceOpenOutline = 0x2411404;
inline constexpr std::uintptr_t RVA_MaterialAdjust_ForceCloseOutline = 0x2411428;
inline constexpr std::uintptr_t RVA_VisualComponent_GetMaterialAdjust = 0x244CD98;

// Safe pieces of DoorActionServer.SetPosition. Context.Current and
// PhysSimulateSystem.RecordRewind are encrypted entry points in this build;
// entering either from the injected NativeBridge module raises SIGILL. The
// teleport path therefore updates Transform, marks it dirty and synchronizes
// the already-created PhysicsActor without entering those protected bodies.
inline constexpr std::uintptr_t RVA_Component_SetDirty          = 0x3195A10;
inline constexpr std::uintptr_t RVA_PhysicsActor_SetPosition    = 0x3F09B9C;
inline constexpr std::uintptr_t RVA_PhysicsActor_SetRotation    = 0x3F09CCC;
inline constexpr std::uintptr_t RVA_Metadata_ComponentSetDirty  = 0x6917FE0;
inline constexpr std::uintptr_t RVA_PhysicsActor_PositionNative = 0x6C2D7F0;
inline constexpr std::uintptr_t RVA_PhysicsActor_RotationNative = 0x6C2D800;

// =============================================================================
// CameraController ApplyRecoildToCamera RVAs
// Reads RecoilComponent and applies to camera rotation
// =============================================================================
inline constexpr std::uintptr_t RVA_CameraControllerClient_ApplyRecoildToCamera = 0x17632E4;
inline constexpr std::uintptr_t RVA_CameraControllerServer_ApplyRecoildToCamera = 0x17A3914;

// =============================================================================
// WeightlessSystem (TypeDefIndex: 13212)
// Controls gravity/weightless state for fly hack
// =============================================================================
inline constexpr std::uintptr_t RVA_WeightlessSystem_Execute = 0x19F0568;

}  // namespace lunary::injected::offsets
