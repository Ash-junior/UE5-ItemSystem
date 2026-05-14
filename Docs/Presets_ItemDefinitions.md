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

## 5) DA_Item_SelfSpeedBoost (Direct, buff sur soi)
**Use**: applique un buff de vitesse instantané à l'instigateur lui-même, sans ciblage.

**IdentityTags**
- `Item.Test.Speed.Boost.Self`

**Logic**
- `ExecutionClass`: `Execution_DirectApply` *(C++ direct)*
- `TargetingClass`: None
- `PayloadClass`: `BP_Payload_SelfSpeedBoost`

**Values (Payload BP)**
- SpeedMultiplier = 1.5
- Duration = 5.0
- EffectTag = `Item.Effect.ModifySpeed.Boost`

**PayloadRoutingSettings**
- `RoutingPolicy`: `InstigatorOnly`
- `bFallbackToInstigatorIfNoRecipient`: false

> Pas de `Rule.Ignore.Teammates` — le chemin `InstigatorOnly` court-circuite `ShouldAffectActor`.
> Guide complet : `Docs/Demo_ContentGuide.md`

---

## 6) DA_Item_EnemySlow (Direct + Raycast, slow sur ennemi)
**Use**: slow ciblé par rayon sur un ennemi visé ; protégé contre le tir allié.

**IdentityTags**
- `Item.Test.Speed.Slow.Enemy`
- `Rule.Ignore.Teammates`

**Logic**
- `ExecutionClass`: `Execution_DirectApply`
- `TargetingClass`: `BP_Targeting_Raycast_EnemySlow` (TraceDistance = 5000)
- `PayloadClass`: `BP_Payload_EnemySlow`

**Values (Payload BP)**
- SpeedMultiplier = 0.5
- Duration = 3.0
- EffectTag = `Item.Effect.ModifySpeed.Slow`

**PayloadRoutingSettings**
- `RoutingPolicy`: `TargetingResultOnly`
- `bFallbackToInstigatorIfNoRecipient`: false

> `Rule.Ignore.Teammates` bloque l'application si le raycast touche un allié (`ShouldAffectActor`).
> Guide complet : `Docs/Demo_ContentGuide.md`

---

## 7) Tag Notes
- Add `Rule.Ignore.Teammates` in `IdentityTags` to prevent friendly fire.
- Use `UsageBlockingTags` for stun or other blocking states.

---

## 8) DA_Item_SpeedBoost_DirectRouted
**Use**: direct (non-projectile) speed boost routed to allies or enemies by rules.

**IdentityTags**
- `Item.Test.Speed.Boost.Direct`

**Logic**
- `ExecutionClass`: `AExecution_DirectApply` (or BP derived from it)
- `TargetingClass`: optional (used for fallback only)
- `PayloadClass`: `BP_Payload_ModifySpeed_Boost`

**PayloadRouting (on ItemDefinition)**
- `RoutingPolicy`: `SearchByRules`
- `RequiredRecipientTag`: `Routing.BuffReceiver` (optional)
- `RecipientRelation`: `SameTeamAsInstigator` (ally buff) or `EnemyOfInstigator` (enemy debuff)
- `SearchSelection`: `NearestSingle` or `AllMatching`
- `SearchRadius`: `2000` (example)
- `bFallbackToTargetingResultIfNoSearchMatch`: true/false per design
- `bFallbackToInstigatorIfNoRecipient`: true for self-safe buffs
