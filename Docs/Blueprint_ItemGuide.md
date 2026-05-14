# Blueprint Item Implementation Guide

Complete step-by-step guide for implementing 10 items in Blueprint using the ItemSystem plugin.

---

## Table of Contents

1. [Prerequisites & Architecture](#1-prerequisites--architecture)
2. [GameplayTags Setup](#2-gameplay-tags-setup)
3. [BP_ItemEffectHandlerComponent](#3-bp_itemeffecthandlercomponent)
4. [Pawn Wiring (ApplyItemEffect)](#4-pawn-wiring-applyitemeffect)
5. [Items](#5-items)
   - [Speed Boost](#51-speed-boost-self)
   - [Bouclier](#52-bouclier-shield)
   - [Mini Tornade](#53-mini-tornade)
   - [Confusion](#54-confusion)
   - [Boule (stun projectile)](#55-boule-stun-projectile)
   - [Mine de glace](#56-mine-de-glace)
   - [Mine de poison](#57-mine-de-poison)
   - [Mine explosive](#58-mine-explosive)
   - [Bombe à retardement](#59-bombe-à-retardement)
   - [Bombe à tornade](#510-bombe-à-tornade)

---

## 1. Prerequisites & Architecture

### Plugin classes involved

| Class | Role |
|---|---|
| `AExecution_DirectApply` | Applies effect immediately to routed recipients (no physics actor) |
| `AExecution_Projectile` | Thrown/fired projectile — triggers payload on hit/overlap |
| `AExecution_Trap` | Stationary mine — triggers payload on proximity overlap |
| `UItemPayloadStrategy` | `ApplyEffect(Target, Context)` — what happens to the target |
| `UItemEffectHandlerComponent` | Component on the Pawn — receives `FItemEffectSpec` and dispatches the effect |
| `FItemEffectSpec` | `{EffectTag, Magnitude, Duration}` — the routed effect descriptor |
| `FItemContext` | Universal context bag passed through the whole pipeline |

### Effect routing flow

```
Pawn presses Use
  → InventoryComponent::Server_TryActivateItem
    → Manager::SpawnItemExecution  (spawns the Execution actor)
      → Execution actor (BeginPlay / overlap / hit)
        → PayloadStrategy::ApplyEffect(Target, Context)
          → IItemInterface::Execute_ApplyItemEffect(Target, Spec, Context)
            → BP_ItemEffectHandlerComponent::HandleEffect(Spec, Context)
              → per-effect sub-functions inside the component
```

### Team filter

`ShouldAffectActor` (called internally by Execution actors) only filters teams when the item's **IdentityTags** contains `Rule.Ignore.Teammates`. Without that tag, the item affects everyone, including the instigator. Use this deliberately:

- Items affecting **only enemies** → add `Rule.Ignore.Teammates`
- Items affecting **everyone** (e.g. ice mine) → omit `Rule.Ignore.Teammates`
- Items affecting **self only** → use `PayloadRouting = InstigatorOnly` (skips ShouldAffectActor entirely)

---

## 2. Gameplay Tags Setup

Open **Project Settings → Project → Gameplay Tags → Manage Gameplay Tags** and add the following tags, or paste them into `Config/DefaultGameplayTags.ini`:

```ini
[/Script/GameplayTags.GameplayTagsSettings]
+GameplayTagList=(Tag="Item.Effect.SpeedBoost",DevComment="Speed multiplier on self")
+GameplayTagList=(Tag="Item.Effect.Shield",DevComment="Absorbs incoming projectiles")
+GameplayTagList=(Tag="Item.Effect.TornadoAura",DevComment="Repulsion aura around self")
+GameplayTagList=(Tag="Item.Effect.Confusion",DevComment="Inverts target movement input")
+GameplayTagList=(Tag="Item.Effect.Stun",DevComment="Slows target and blocks item use")
+GameplayTagList=(Tag="Item.Effect.Freeze",DevComment="Completely stops target movement")
+GameplayTagList=(Tag="Item.Effect.Poison",DevComment="Blocks jumping for duration")
+GameplayTagList=(Tag="Item.Effect.Knockback",DevComment="Radial launch force")
+GameplayTagList=(Tag="Status.Stunned",DevComment="Blocks item use while active")
+GameplayTagList=(Tag="Status.Confused",DevComment="Movement input inverted")
+GameplayTagList=(Tag="Status.Frozen",DevComment="Movement fully disabled")
+GameplayTagList=(Tag="Status.Poisoned",DevComment="Jump blocked while active")
+GameplayTagList=(Tag="Status.Shielded",DevComment="Projectile immunity active")
+GameplayTagList=(Tag="Rule.Ignore.Teammates",DevComment="Item will not affect same-team actors")
```

---

## 3. BP_ItemEffectHandlerComponent

Create a new Blueprint class inheriting from `UItemEffectHandlerComponent`.
Name it `BP_ItemEffectHandlerComponent`.

### 3.1 Variables

Add these variables to the component (all private, no replication needed — effects are applied server-authoritative and the pawn state drives visuals):

| Name | Type | Default | Purpose |
|---|---|---|---|
| `OriginalMaxWalkSpeed` | Float | 600.0 | Cached before any speed modification |
| `bShieldActive` | Boolean | false | True while shield is absorbing |
| `bConfused` | Boolean | false | True while inputs are inverted |
| `bPoisoned` | Boolean | false | True while jump is blocked |
| `ShieldTimerHandle` | TimerHandle | — | Used to clear shield after duration |
| `SpeedTimerHandle` | TimerHandle | — | Used to restore speed after boost |
| `ConfusionTimerHandle` | TimerHandle | — | Used to clear confusion |
| `StunTimerHandle` | TimerHandle | — | Used to clear stun |
| `FreezeTimerHandle` | TimerHandle | — | Used to unfreeze |
| `PoisonTimerHandle` | TimerHandle | — | Used to clear poison |
| `TornadoAuraActor` | Actor Object Reference | — | Tracks the attached tornado actor |

### 3.2 Override HandleEffect

Override the `HandleEffect` BlueprintNativeEvent (it appears under **Override** in the functions panel).

In the override graph:
1. Call each sub-function in sequence, passing `Spec` and `Context`
2. Return **OR** of all results (true if any handled it)

```
[Event HandleEffect (Spec, Context)]
  → [Handle_SpeedBoost(Spec, Context)] → [OR]
  → [Handle_Shield(Spec, Context)]     → [OR]
  → [Handle_TornadoAura(Spec, Context)]→ [OR]
  → [Handle_Confusion(Spec, Context)]  → [OR]
  → [Handle_Stun(Spec, Context)]       → [OR]
  → [Handle_Freeze(Spec, Context)]     → [OR]
  → [Handle_Poison(Spec, Context)]     → [OR]
  → [Handle_Knockback(Spec, Context)]  → [OR]
  → [Return Node: ReturnValue = combined OR result]
```

Each sub-function returns a boolean: `true` if the tag matched.

### 3.3 Sub-functions

#### Handle_SpeedBoost

```
[Function: Handle_SpeedBoost(Spec FItemEffectSpec, Context FItemContext) → bool]

[Break FItemEffectSpec] → EffectTag
[== "Item.Effect.SpeedBoost"]? → [Branch]
  False: → Return false

  True:
  [GetOwner] → [Get CharacterMovementComponent]
  [Get MaxWalkSpeed] → [Set OriginalMaxWalkSpeed (save)]
  [MaxWalkSpeed * Spec.Magnitude] → [Set MaxWalkSpeed]
  [Clear & Invalidate Timer by Handle: SpeedTimerHandle]
  [Set Timer by Event → Duration = Spec.Duration → SpeedTimerHandle]
    (timer callback: [Set MaxWalkSpeed = OriginalMaxWalkSpeed])
  → Return true
```

> **Note:** Retrieve `CharacterMovementComponent` via `GetOwner → Cast to Character → GetCharacterMovement`.

#### Handle_Shield

```
[Function: Handle_Shield(Spec FItemEffectSpec, Context FItemContext) → bool]

[Break FItemEffectSpec] → EffectTag
[== "Item.Effect.Shield"]? → [Branch]
  False: → Return false

  True:
  [Set bShieldActive = true]
  [GetOwner → HasGameplayTag("Status.Shielded")]? skip tag add if already present
  (optional: notify pawn to add Status.Shielded tag via custom event or interface)
  [Clear & Invalidate Timer: ShieldTimerHandle]
  [Set Timer by Event → Duration → ShieldTimerHandle]
    (callback: Set bShieldActive = false, remove Status.Shielded tag)
  → Return true
```

> The pawn's projectile collision logic should check `GetComponentsByClass(BP_ItemEffectHandlerComponent) → bShieldActive`. Any overlapping projectile checks this flag before dealing damage: if `bShieldActive`, destroy the projectile without effect.

#### Handle_TornadoAura

```
[Function: Handle_TornadoAura(Spec FItemEffectSpec, Context FItemContext) → bool]

[Break FItemEffectSpec] → EffectTag
[== "Item.Effect.TornadoAura"]? → [Branch]
  False: → Return false

  True:
  [Is Valid(TornadoAuraActor)]? → destroy it first
  [SpawnActorFromClass: BP_TornadoActor_Attached]
    Location: GetOwner → GetActorLocation
    Owner: GetOwner
  [AttachActorToActor: TornadoAuraActor → GetOwner, SnapToTargetNotIncludingScale]
  [Set TornadoAuraActor = spawned actor]
  [Pass Spec.Magnitude and Context to the actor via custom interface or Set variable]
  [Set Timer by Event → Duration → destroy TornadoAuraActor]
  → Return true
```

#### Handle_Confusion

```
[Function: Handle_Confusion(Spec FItemEffectSpec, Context FItemContext) → bool]

[Break FItemEffectSpec] → EffectTag
[== "Item.Effect.Confusion"]? → [Branch]
  False: → Return false

  True:
  [Set bConfused = true]
  [Clear & Invalidate Timer: ConfusionTimerHandle]
  [Set Timer by Event → Duration → ConfusionTimerHandle]
    (callback: Set bConfused = false)
  → Return true
```

> In the Pawn's movement input (Enhanced Input `IA_Move` or axis events): read `bConfused` from the handler component and multiply the axis value by `-1.0` if true.

#### Handle_Stun

```
[Function: Handle_Stun(Spec FItemEffectSpec, Context FItemContext) → bool]

[Break FItemEffectSpec] → EffectTag
[== "Item.Effect.Stun"]? → [Branch]
  False: → Return false

  True:
  [GetOwner → GetCharacterMovement → Set MaxWalkSpeed = OriginalMaxWalkSpeed * Spec.Magnitude]
    (Magnitude = 0.3 means 30% of normal speed)
  [GetOwner → HasGameplayTag check, then add "Status.Stunned" tag to pawn]
  [Clear & Invalidate Timer: StunTimerHandle]
  [Set Timer by Event → Duration → StunTimerHandle]
    (callback: restore MaxWalkSpeed = OriginalMaxWalkSpeed, remove Status.Stunned tag)
  → Return true
```

> Adding `Status.Stunned` to the pawn's gameplay tags is what makes `InventoryComponent::CanUseItem` block item usage via `UsageBlockingTags`.

#### Handle_Freeze

```
[Function: Handle_Freeze(Spec FItemEffectSpec, Context FItemContext) → bool]

[Break FItemEffectSpec] → EffectTag
[== "Item.Effect.Freeze"]? → [Branch]
  False: → Return false

  True:
  [GetOwner → GetCharacterMovement]
  [Set MaxWalkSpeed = 0.0]
  [Set bOrientRotationToMovement = false]
  [Disable Movement (SetMovementMode → None)]
  [Add "Status.Frozen" tag to pawn]  (blocks item use)
  [Clear & Invalidate Timer: FreezeTimerHandle]
  [Set Timer by Event → Duration → FreezeTimerHandle]
    (callback:
      Set MaxWalkSpeed = OriginalMaxWalkSpeed
      Set bOrientRotationToMovement = true
      SetMovementMode → Walking
      Remove "Status.Frozen" tag)
  → Return true
```

#### Handle_Poison

```
[Function: Handle_Poison(Spec FItemEffectSpec, Context FItemContext) → bool]

[Break FItemEffectSpec] → EffectTag
[== "Item.Effect.Poison"]? → [Branch]
  False: → Return false

  True:
  [Set bPoisoned = true]
  [Add "Status.Poisoned" tag to pawn]
  [Clear & Invalidate Timer: PoisonTimerHandle]
  [Set Timer by Event → Duration → PoisonTimerHandle]
    (callback: Set bPoisoned = false, Remove "Status.Poisoned" tag)
  → Return true
```

> In the Pawn's `CanJump` override (or `Jump` event): check `bPoisoned` on the handler component and return false to block the jump.

#### Handle_Knockback

```
[Function: Handle_Knockback(Spec FItemEffectSpec, Context FItemContext) → bool]

[Break FItemEffectSpec] → EffectTag
[== "Item.Effect.Knockback"]? → [Branch]
  False: → Return false

  True:
  [Break FItemContext] → Instigator (or ImpactPoint)
  [Direction = GetOwner.Location - Context.ImpactPoint → Normalize]
  [Add Z = 0.4 to give slight upward arc]
  [Normalize Direction]
  [GetOwner → Cast to Character → LaunchCharacter]
    LaunchVelocity = Direction * Spec.Magnitude
    bXYOverride = true
    bZOverride = false
  → Return true
```

> If `Context.bHasImpactPoint` is false, fall back to `Context.Instigator.GetActorLocation()` as the knockback source.

### 3.4 Tag management helpers

Your pawn needs a `TArray<FGameplayTag> ActiveTags` (or equivalent) to implement `IItemInterface::HasGameplayTag`. Two helper functions on the pawn or on the handler component:

- `AddStatusTag(Tag)` — adds the tag to the pawn's array
- `RemoveStatusTag(Tag)` — removes it

Make sure `HasGameplayTag_Implementation` in the pawn reads from this array.

---

## 4. Pawn Wiring (ApplyItemEffect)

In your Pawn Blueprint (which implements `IItemInterface`):

### ApplyItemEffect

```
[Event ApplyItemEffect (Effect FItemEffectSpec, Context FItemContext)]
[GetComponentByClass → BP_ItemEffectHandlerComponent] → [Is Valid?]
  → [Call HandleEffect(Effect, Context)]
  → [Return Value from HandleEffect]
[Return Node: ReturnValue = result (false if component not found)]
```

### Movement input inversion (for Confusion)

In your Enhanced Input move action (or axis binding):

```
[IA_Move InputAction event] → [Get InputAxisValue]
[GetComponentByClass → BP_ItemEffectHandlerComponent]
  → [Get bConfused]
  → [Select float: True=-1.0, False=1.0]
  → [Multiply by AxisValue]
  → [AddMovementInput]
```

### Jump block (for Poison)

Override `CanJump` or in the Jump event:

```
[Event Jump]
[GetComponentByClass → BP_ItemEffectHandlerComponent]
  → [Get bPoisoned]
  → [Branch: True → return (blocked), False → [Super Jump]]
```

---

## 5. Items

---

### 5.1 Speed Boost (self)

**What it does:** Multiplies the instigator's movement speed by a configurable factor for a set duration.

#### Data Asset: `DA_SpeedBoost`

| Field | Value |
|---|---|
| Display Name | Speed Boost |
| IdentityTags | `Item.Type.Utility` |
| UsageBlockingTags | `Status.Stunned`, `Status.Frozen` |
| MaxStack | 1 |
| Cooldown | 10.0 |
| ExecutionClass | `Execution_DirectApply` |
| TargetingClass | *(empty)* |
| PayloadClass | `BP_Payload_SpeedBoost` |
| PayloadRouting → RoutingPolicy | `InstigatorOnly` |

#### Blueprint: `BP_Payload_SpeedBoost`

Parent class: `UItemPayloadStrategy`

Override `ApplyEffect`:

```
[Event ApplyEffect (Target Actor, Context FItemContext)]
[Make FItemEffectSpec]
  EffectTag = "Item.Effect.SpeedBoost"
  Magnitude = 1.5          ← set as variable "SpeedMultiplier" for easy tuning
  Duration  = 5.0          ← set as variable "BoostDuration"
[IItemInterface → Execute ApplyItemEffect (Target, Spec, Context)]
```

No Execution or Targeting Blueprint needed — `Execution_DirectApply` + `InstigatorOnly` routing handles delivery.

---

### 5.2 Bouclier (Shield)

**What it does:** Activates a shield on the instigator for a set duration. While the shield is active, the pawn's projectile collision ignores incoming projectile effects.

#### Data Asset: `DA_Bouclier`

| Field | Value |
|---|---|
| Display Name | Bouclier |
| IdentityTags | `Item.Type.Utility` |
| UsageBlockingTags | `Status.Stunned`, `Status.Frozen` |
| MaxStack | 1 |
| Cooldown | 15.0 |
| ExecutionClass | `Execution_DirectApply` |
| TargetingClass | *(empty)* |
| PayloadClass | `BP_Payload_Bouclier` |
| PayloadRouting → RoutingPolicy | `InstigatorOnly` |

#### Blueprint: `BP_Payload_Bouclier`

```
[Event ApplyEffect (Target, Context)]
[Make FItemEffectSpec]
  EffectTag = "Item.Effect.Shield"
  Magnitude = 0.0
  Duration  = 5.0
[Execute ApplyItemEffect (Target, Spec, Context)]
```

#### Pawn-side: Projectile Absorption

In your projectile's `OnComponentBeginOverlap` (or in the pawn's hit event):

```
[OtherActor → GetComponentByClass → BP_ItemEffectHandlerComponent]
  → [Get bShieldActive]
  → [Branch: True → DestroyActor (projectile absorbed), False → apply damage/effect]
```

Optionally: spawn a shield-break VFX at the pawn's location when the shield blocks a hit, then start a short invulnerability window.

---

### 5.3 Mini Tornade

**What it does:** Spawns a tornado aura around the instigator that periodically applies radial repulsion to nearby enemies.

#### Data Asset: `DA_MiniTornade`

| Field | Value |
|---|---|
| Display Name | Mini Tornade |
| IdentityTags | `Item.Type.Utility` |
| UsageBlockingTags | `Status.Stunned`, `Status.Frozen` |
| Cooldown | 12.0 |
| ExecutionClass | `Execution_DirectApply` |
| PayloadClass | `BP_Payload_MiniTornade` |
| PayloadRouting → RoutingPolicy | `InstigatorOnly` |

#### Blueprint: `BP_Payload_MiniTornade`

```
[Event ApplyEffect (Target, Context)]
[Make FItemEffectSpec]
  EffectTag = "Item.Effect.TornadoAura"
  Magnitude = 800.0        ← repulsion force
  Duration  = 5.0
[Execute ApplyItemEffect (Target, Spec, Context)]
```

#### Blueprint: `BP_TornadoActor_Attached`

Parent: `Actor`

Components:
- `SphereComponent` (radius 400, QueryOnly, overlaps Pawn channel, `GenerateOverlapEvents = true`)
- `NiagaraComponent` (tornado VFX — auto-activate)

Variables:
- `RepulsionForce` (float) — set by the handler
- `InstigatorRef` (Actor Object Reference)

**Event Graph:**

```
[Event BeginPlay]
[Set Lifespan = (duration passed from handler)]
[SphereComponent → OnComponentBeginOverlap → Bind to ApplyRepulsion]

[Function: ApplyRepulsion (OtherActor)]
  [OtherActor == InstigatorRef]? → Branch: True → return (skip self)
  [Direction = OtherActor.Location - GetActorLocation → Normalize]
  [Make FItemEffectSpec]
    EffectTag = "Item.Effect.Knockback"
    Magnitude = RepulsionForce
    Duration  = 0.0
  [Make FItemContext] ← set ImpactPoint = GetActorLocation, bHasImpactPoint = true, Instigator = InstigatorRef
  [Execute ApplyItemEffect (OtherActor, Spec, Context)]
```

> The handler component's `Handle_Knockback` will launch `OtherActor` away from the tornado's location using `Context.ImpactPoint` as the source. Set Lifespan on the actor in `BeginPlay` to auto-destroy after the duration.

---

### 5.4 Confusion

**What it does:** Inverts the movement controls of all nearby enemies for a set duration.

#### Data Asset: `DA_Confusion`

| Field | Value |
|---|---|
| Display Name | Confusion |
| IdentityTags | `Item.Type.Offense`, `Rule.Ignore.Teammates` |
| UsageBlockingTags | `Status.Stunned`, `Status.Frozen` |
| Cooldown | 10.0 |
| ExecutionClass | `Execution_DirectApply` |
| PayloadClass | `BP_Payload_Confusion` |
| PayloadRouting → RoutingPolicy | `SearchByRules` |
| PayloadRouting → RecipientRelation | `EnemyOfInstigator` |
| PayloadRouting → SearchSelection | `AllMatching` |
| PayloadRouting → SearchRadius | 800.0 |

> `SearchByRules` with `AllMatching` causes `Execution_DirectApply` to find all enemies within 800 units and call `ApplyEffect` on each one automatically.

#### Blueprint: `BP_Payload_Confusion`

```
[Event ApplyEffect (Target, Context)]
[Make FItemEffectSpec]
  EffectTag = "Item.Effect.Confusion"
  Magnitude = 0.0
  Duration  = 4.0
[Execute ApplyItemEffect (Target, Spec, Context)]
```

---

### 5.5 Boule (Stun Projectile)

**What it does:** A thrown projectile that hits an enemy, slows them to 30% speed, and adds `Status.Stunned` (blocking their item use) for 3 seconds.

#### Data Asset: `DA_Boule`

| Field | Value |
|---|---|
| Display Name | Boule |
| IdentityTags | `Item.Type.Offense`, `Rule.Ignore.Teammates` |
| UsageBlockingTags | `Status.Stunned`, `Status.Frozen` |
| Cooldown | 8.0 |
| ExecutionClass | `BP_Execution_Boule` |
| TargetingClass | *(empty)* |
| PayloadClass | `BP_Payload_Boule` |
| PayloadRouting → RoutingPolicy | `TargetingResultOnly` |

#### Blueprint: `BP_Execution_Boule`

Parent: `AExecution_Projectile`

In Class Defaults:
- `LaunchMode` = `ArcThrow`
- `Speed` = 2000.0
- `GravityScale` = 1.0
- `ArcTraceDistance` = 5000.0
- `bFavorHighArc` = false
- `TrailVFX` = *(assign a blue/purple Niagara trail)*
- `ImpactVFX` = *(stun burst Niagara)*

Add a `StaticMeshComponent` or `SphereComponent` as the collision root (required by the C++ parent for hit detection). Set:
- Collision Preset: `Projectile`
- Generate Overlap Events: true
- Simulate Physics: false

No event graph changes needed — the base `AExecution_Projectile` handles hit → `ApplyEffect` automatically.

#### Blueprint: `BP_Payload_Boule`

```
[Event ApplyEffect (Target, Context)]
[Make FItemEffectSpec]
  EffectTag = "Item.Effect.Stun"
  Magnitude = 0.3          ← 30% of normal speed
  Duration  = 3.0
[Execute ApplyItemEffect (Target, Spec, Context)]
```

#### Stun UI feedback (optional)

In the VICTIM's handler component `Handle_Stun`, trigger a screen effect (e.g., material parameter on a UMG overlay) for the stun duration.

---

### 5.6 Mine de Glace

**What it does:** A mine dropped at the instigator's feet. Anyone who steps on it — including the instigator — is frozen in place for 3 seconds.

> Because the `Item.IdentityTags` does **not** include `Rule.Ignore.Teammates`, `ShouldAffectActor` returns true for all actors, including the instigator.

#### Data Asset: `DA_MineGlace`

| Field | Value |
|---|---|
| Display Name | Mine de Glace |
| IdentityTags | `Item.Type.Trap` ← deliberately NO `Rule.Ignore.Teammates` |
| UsageBlockingTags | `Status.Stunned`, `Status.Frozen` |
| Cooldown | 5.0 |
| ExecutionClass | `BP_Execution_MineGlace` |
| PayloadClass | `BP_Payload_MineGlace` |
| PayloadRouting → RoutingPolicy | `TargetingResultOnly` |

#### Blueprint: `BP_Execution_MineGlace`

Parent: `AExecution_Trap`

Class Defaults:
- `TriggerRadius` = 180.0
- `LifeSpanSeconds` = 30.0
- `ImpactVFX` = *(ice burst Niagara)*

Add components:
- `StaticMeshComponent` (ice mine mesh, no collision)

No event graph changes needed — the C++ trap handles the rest.

#### Blueprint: `BP_Payload_MineGlace`

```
[Event ApplyEffect (Target, Context)]
[Make FItemEffectSpec]
  EffectTag = "Item.Effect.Freeze"
  Magnitude = 0.0
  Duration  = 3.0
[Execute ApplyItemEffect (Target, Spec, Context)]
```

---

### 5.7 Mine de Poison

**What it does:** A mine that poisons anyone who steps on it. While poisoned, jumping is blocked for 5 seconds.

#### Data Asset: `DA_MinePoison`

| Field | Value |
|---|---|
| Display Name | Mine de Poison |
| IdentityTags | `Item.Type.Trap`, `Rule.Ignore.Teammates` |
| UsageBlockingTags | `Status.Stunned`, `Status.Frozen` |
| Cooldown | 5.0 |
| ExecutionClass | `BP_Execution_MinePoison` |
| PayloadClass | `BP_Payload_MinePoison` |
| PayloadRouting → RoutingPolicy | `TargetingResultOnly` |

> Remove `Rule.Ignore.Teammates` if you want the mine to also poison teammates.

#### Blueprint: `BP_Execution_MinePoison`

Parent: `AExecution_Trap`

Class Defaults:
- `TriggerRadius` = 200.0
- `LifeSpanSeconds` = 30.0
- `ImpactVFX` = *(green cloud Niagara)*

#### Blueprint: `BP_Payload_MinePoison`

```
[Event ApplyEffect (Target, Context)]
[Make FItemEffectSpec]
  EffectTag = "Item.Effect.Poison"
  Magnitude = 0.0
  Duration  = 5.0
[Execute ApplyItemEffect (Target, Spec, Context)]
```

---

### 5.8 Mine Explosive

**What it does:** A mine that, on trigger, applies a radial knockback to all nearby enemies within a configurable radius.

The payload handles the sphere search itself. `PayloadRouting = TargetingResultOnly` delivers the triggering actor to `ApplyEffect`, but the payload ignores that and does its own radius sweep.

#### Data Asset: `DA_MineExplosive`

| Field | Value |
|---|---|
| Display Name | Mine Explosive |
| IdentityTags | `Item.Type.Trap`, `Rule.Ignore.Teammates` |
| UsageBlockingTags | `Status.Stunned`, `Status.Frozen` |
| Cooldown | 5.0 |
| ExecutionClass | `BP_Execution_MineExplosive` |
| PayloadClass | `BP_Payload_MineExplosive` |
| PayloadRouting → RoutingPolicy | `TargetingResultOnly` |

#### Blueprint: `BP_Execution_MineExplosive`

Parent: `AExecution_Trap`

Class Defaults:
- `TriggerRadius` = 150.0
- `LifeSpanSeconds` = 30.0
- `ImpactVFX` = *(explosion Niagara — use SpawnSystemAtLocation)*

#### Blueprint: `BP_Payload_MineExplosive`

Variables:
- `ExplosionRadius` (float) = 600.0
- `KnockbackForce` (float) = 1200.0

```
[Event ApplyEffect (Target Actor, Context FItemContext)]

[GetActorLocation from Target OR Context.ImpactPoint]
  → use "Context.ImpactPoint" if "Context.bHasImpactPoint" is true, else Target.GetActorLocation

[SphereOverlapActors]
  Center = explosion location
  Radius = ExplosionRadius
  ObjectTypes = [Pawn]
  → OutActors

[ForEach OutActors]
  [Make FItemEffectSpec]
    EffectTag = "Item.Effect.Knockback"
    Magnitude = KnockbackForce
    Duration  = 0.0
  [Make FItemContext from Context] → set ImpactPoint = explosion location, bHasImpactPoint = true
  [Execute ApplyItemEffect (CurrentActor, Spec, NewContext)]
```

> The handler component's `Handle_Knockback` uses `Context.ImpactPoint` as the launch source, so setting it to the explosion location produces correct directional knockback.

---

### 5.9 Bombe à Retardement

**What it does:** A thrown grenade (ArcThrow) that lands, waits a configurable delay, then explodes with radial knockback. Uses a dedicated `BP_TimeBombActor` to handle the timer-based explosion independently.

#### Architecture

```
BP_Execution_BombeRetardement (Projectile, ArcThrow)
  → on hit: calls ApplyEffect
    → BP_Payload_BombeRetardement::ApplyEffect
      → Spawns BP_TimeBombActor at Context.ImpactPoint
        → Timer fires → sphere overlap → ApplyItemEffect on each victim
```

#### Data Asset: `DA_BombeRetardement`

| Field | Value |
|---|---|
| Display Name | Bombe à Retardement |
| IdentityTags | `Item.Type.Offense`, `Rule.Ignore.Teammates` |
| UsageBlockingTags | `Status.Stunned`, `Status.Frozen` |
| Cooldown | 8.0 |
| ExecutionClass | `BP_Execution_BombeRetardement` |
| PayloadClass | `BP_Payload_BombeRetardement` |
| PayloadRouting → RoutingPolicy | `TargetingResultOnly` |

#### Blueprint: `BP_Execution_BombeRetardement`

Parent: `AExecution_Projectile`

Class Defaults:
- `LaunchMode` = `ArcThrow`
- `Speed` = 1600.0
- `GravityScale` = 1.2
- `TrailVFX` = *(smoke trail Niagara)*

Add a visible mesh component (grenade mesh) for visual feedback.

#### Blueprint: `BP_Payload_BombeRetardement`

Variables:
- `BombDelay` (float) = 3.0
- `ExplosionRadius` (float) = 700.0
- `KnockbackForce` (float) = 1400.0

```
[Event ApplyEffect (Target Actor, Context FItemContext)]

[Spawn Actor from Class: BP_TimeBombActor]
  Location = Context.ImpactPoint (if bHasImpactPoint) else Target.GetActorLocation
  SpawnParameters: NoFail

[Set on spawned actor:]
  → Delay = BombDelay
  → ExplosionRadius = ExplosionRadius
  → KnockbackForce = KnockbackForce
  → InstigatorRef = Context.Instigator
  → ItemContextRef = Context
```

#### Blueprint: `BP_TimeBombActor`

Parent: `Actor`

Components:
- `StaticMeshComponent` (grenade/bomb mesh)
- `NiagaraComponent` (ticking fuse VFX, auto-activate)

Variables:
- `Delay` (float) = 3.0
- `ExplosionRadius` (float) = 700.0
- `KnockbackForce` (float) = 1400.0
- `InstigatorRef` (Actor Object Reference)
- `StoredContext` (FItemContext)
- `ExplosionTimerHandle` (TimerHandle)

**Event Graph:**

```
[Event BeginPlay]
  [Set Timer by Event → Delay → ExplosionTimerHandle → calls Explode]

[Function: Explode]
  [SphereOverlapActors]
    Center = GetActorLocation
    Radius = ExplosionRadius
    ObjectTypes = [Pawn]
  [ForEach result]
    [Break FItemContext from StoredContext]
    [Make FItemEffectSpec]
      EffectTag = "Item.Effect.Knockback"
      Magnitude = KnockbackForce
      Duration  = 0.0
    [Make FItemContext] ← copy StoredContext, override ImpactPoint = GetActorLocation, bHasImpactPoint = true
    [Execute ApplyItemEffect (CurrentActor, Spec, NewContext)]
  [SpawnNiagaraSystemAtLocation] → explosion VFX
  [PlaySoundAtLocation] → explosion sound
  [DestroyActor]
```

> `BP_TimeBombActor` is a pure Blueprint actor. The manager's pool does not manage it — it destroys itself in `Explode`. This is intentional: the pool architecture is designed for execution actors, not for game-mode-specific world objects.

---

### 5.10 Bombe à Tornade

**What it does:** A thrown projectile that, on impact, spawns a stationary tornado at the impact point. The tornado periodically repels nearby enemies and lasts 8 seconds before disappearing.

#### Data Asset: `DA_BombeTornade`

| Field | Value |
|---|---|
| Display Name | Bombe à Tornade |
| IdentityTags | `Item.Type.Offense`, `Rule.Ignore.Teammates` |
| UsageBlockingTags | `Status.Stunned`, `Status.Frozen` |
| Cooldown | 12.0 |
| ExecutionClass | `BP_Execution_BombeTornade` |
| PayloadClass | `BP_Payload_BombeTornade` |
| PayloadRouting → RoutingPolicy | `TargetingResultOnly` |

#### Blueprint: `BP_Execution_BombeTornade`

Parent: `AExecution_Projectile`

Class Defaults:
- `LaunchMode` = `ArcThrow`
- `Speed` = 1800.0
- `GravityScale` = 0.8
- `TrailVFX` = *(wind trail Niagara)*
- `ImpactVFX` = *(impact burst Niagara)*

#### Blueprint: `BP_Payload_BombeTornade`

Variables:
- `TornadoDuration` (float) = 8.0
- `TornadoRadius` (float) = 500.0
- `RepulsionForce` (float) = 900.0

```
[Event ApplyEffect (Target Actor, Context FItemContext)]

[Spawn Actor from Class: BP_TornadoActor_Static]
  Location = Context.ImpactPoint (if bHasImpactPoint) else Target.GetActorLocation

[Set on spawned actor:]
  → Lifespan = TornadoDuration  (via SetLifeSpan)
  → RepulsionForce = RepulsionForce
  → TornadoRadius = TornadoRadius
  → InstigatorRef = Context.Instigator
  → StoredContext = Context
```

#### Blueprint: `BP_TornadoActor_Static`

Parent: `Actor`

Components:
- `NiagaraComponent` (tornado VFX — looping, auto-activate)
- `SphereComponent` (radius = TornadoRadius, QueryOnly, Pawn channel)

Variables:
- `RepulsionForce` (float) = 900.0
- `TornadoRadius` (float) = 500.0
- `InstigatorRef` (Actor Object Reference)
- `StoredContext` (FItemContext)
- `RepulsionTimerHandle` (TimerHandle)

**Event Graph:**

```
[Event BeginPlay]
  [Set Timer by Event → Rate=0.3, Looping=true → RepulsionTimerHandle → calls ApplyRepulsionTick]

[Function: ApplyRepulsionTick]
  [SphereOverlapActors]
    Center = GetActorLocation
    Radius = TornadoRadius
    ObjectTypes = [Pawn]
  [ForEach result]
    [CurrentActor == InstigatorRef]? → [Branch: True → skip (continue), False → apply]
    [Make FItemEffectSpec]
      EffectTag = "Item.Effect.Knockback"
      Magnitude = RepulsionForce
      Duration  = 0.0
    [Make FItemContext] ← copy StoredContext, set ImpactPoint = GetActorLocation, bHasImpactPoint = true
    [Execute ApplyItemEffect (CurrentActor, Spec, NewContext)]

[Event EndPlay]
  [Clear Timer by Handle: RepulsionTimerHandle]
```

> `SetLifeSpan` in the payload (after spawning the actor) handles destruction after `TornadoDuration`. The `EndPlay` event cleans up the looping timer. Set `NiagaraComponent` to auto-deactivate on `EndPlay` as well.

---

## Summary Table

| Item | Execution | Payload Routing | Effect Tag | Handler sub-function |
|---|---|---|---|---|
| Speed Boost | DirectApply | InstigatorOnly | `Item.Effect.SpeedBoost` | `Handle_SpeedBoost` |
| Bouclier | DirectApply | InstigatorOnly | `Item.Effect.Shield` | `Handle_Shield` |
| Mini Tornade | DirectApply | InstigatorOnly | `Item.Effect.TornadoAura` | `Handle_TornadoAura` |
| Confusion | DirectApply | SearchByRules (AllMatching, Enemy, r=800) | `Item.Effect.Confusion` | `Handle_Confusion` |
| Boule | Projectile (ArcThrow) | TargetingResultOnly | `Item.Effect.Stun` | `Handle_Stun` |
| Mine de Glace | Trap | TargetingResultOnly | `Item.Effect.Freeze` | `Handle_Freeze` |
| Mine de Poison | Trap | TargetingResultOnly | `Item.Effect.Poison` | `Handle_Poison` |
| Mine Explosive | Trap (+ sphere in payload) | TargetingResultOnly | `Item.Effect.Knockback` | `Handle_Knockback` |
| Bombe Retardement | Projectile (ArcThrow) | TargetingResultOnly | `Item.Effect.Knockback` (via BP_TimeBombActor) | `Handle_Knockback` |
| Bombe Tornade | Projectile (ArcThrow) | TargetingResultOnly | `Item.Effect.Knockback` (via BP_TornadoActor_Static) | `Handle_Knockback` |

---

## BP Actors Reference

| Blueprint Actor | Parent | Purpose |
|---|---|---|
| `BP_TornadoActor_Attached` | Actor | Tornado attached to instigator pawn (Mini Tornade) |
| `BP_TornadoActor_Static` | Actor | Tornado placed in world on impact (Bombe Tornade) |
| `BP_TimeBombActor` | Actor | World actor with countdown timer (Bombe Retardement) |
