# ItemSystem Presets (DA_Item_*)

This document lists recommended presets for core gameplay tests.

## 1) DA_Item_SkillShot
**Use**: skill-shot projectile with explosion payload.

**IdentityTags**
- `Item.Test.SkillShot`

**Logic**
- `ExecutionClass`: `BP_Execution_Projectile_SkillShot`
- `TargetingClass`: None
- `PayloadClass`: `BP_Payload_Explosion_SkillShot`

**Values (Execution BP)**
- Speed = 2000
- GravityScale = 1.0
- bIsHoming = false
- bShowDebugVisuals = true

**Values (Payload BP)**
- DamageAmount = 20
- DamageRadius = 300
- bDoFullDamage = false
- bIgnoreInstigator = true

---

## 2) DA_Item_SpeedBoost
**Use**: projectile applies speed boost.

**IdentityTags**
- `Item.Test.Speed.Boost`

**Logic**
- `ExecutionClass`: `BP_Execution_Projectile_Speed`
- `TargetingClass`: None
- `PayloadClass`: `BP_Payload_ModifySpeed_Boost`

**Values (Payload BP)**
- SpeedMultiplier = 1.3
- Duration = 3.0
- EffectTag = `Item.Effect.ModifySpeed.Boost`

---

## 3) DA_Item_SpeedSlow
**Use**: projectile applies speed slow.

**IdentityTags**
- `Item.Test.Speed.Slow`

**Logic**
- `ExecutionClass`: `BP_Execution_Projectile_Speed`
- `TargetingClass`: None
- `PayloadClass`: `BP_Payload_ModifySpeed_Slow`

**Values (Payload BP)**
- SpeedMultiplier = 0.6
- Duration = 3.0
- EffectTag = `Item.Effect.ModifySpeed.Slow`

---

## 4) DA_Item_Trap_Explosion
**Use**: mine/trap with explosion payload.

**IdentityTags**
- `Item.Test.Trap.Explosion`

**Logic**
- `ExecutionClass`: `BP_Execution_Trap_Explosion`
- `TargetingClass`: None
- `PayloadClass`: `BP_Payload_Explosion_Trap`

**Values (Execution BP)**
- TriggerRadius = 150
- LifeSpanSeconds = 30
- bShowDebugVisuals = true

---

## 5) Tag Notes
- Add `Rule.Ignore.Teammates` in `IdentityTags` to prevent friendly fire.
- Use `UsageBlockingTags` for stun or other blocking states.
