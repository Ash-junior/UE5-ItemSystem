# CLAUDE.md — ItemSystem Plugin

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

---

## Building

This is a UE5 C++ plugin (`ItemSystem.uplugin`, type: Runtime, LoadingPhase: Default). Build via Unreal Editor (**Build > Build Solution** or use the toolbar hammer icon) or with UnrealBuildTool directly:

```
UnrealBuildTool.exe <ProjectName>Editor Win64 Development "<path>/MK_ItemSystem_CPP.uproject"
```

The plugin module is `ItemSystem`. Dependencies declared in `ItemSystem.Build.cs`:
- **Public**: `Core`, `GameplayTags`, `GameplayTasks`
- **Private**: `CoreUObject`, `Engine`, `Slate`, `SlateCore`, `Niagara`

---

## QA / Testing

Testing is done in PIE (Play-in-Editor), not via automated tests. Use the `ItemCheatManager` console commands:

```
ItemSystem.QA 1              # Enable verbose QA logs
ItemSystem.QA 0              # Disable

Cheat_GiveItem Item.Test.SkillShot             # Give item by tag query
Cheat_GiveItem "Item.Test.SkillShot AND NOT Item.Test.Debug"
Cheat_SimulateImpact Item.Test.SkillShot       # Apply payload to camera raycast hit
Cheat_ClearInventory                           # Clear current inventory
```

Set `CheatManagerClass = ItemCheatManager` on the PlayerController BP to enable these.

---

## Architecture

### Pipeline Overview

```
UInventoryComponent (Pawn)
  └─ Server_TryActivateItem()
       └─ FItemContext (data bag)
            └─ UItemSystemManager::SpawnItemExecution()
                 └─ AItemExecutionStrategy (pooled world actor)
                      ├─ UItemTargetingStrategy (finds target)
                      └─ UItemPayloadStrategy::ApplyEffect() (applies effect to target)
```

### Core Types (`Public/Core/`, `Public/Data/`)

- **`UItemDefinition`** — Primary data asset per item. Holds identity tags, blocking tags, cooldown, visuals (`FItemVisuals`), `MaxStack`, `PickupGrantAmount`, and soft class refs to execution/targeting/payload. Also stores `FItemPayloadRoutingSettings` for direct-apply routing. Audio: `Sound_OnEquip` (played on all clients when item is granted) and `Sound_OnActivate` (played on all clients when item is used).
- **`FItemContext`** — Universal struct flowing through the entire pipeline: instigator actor/controller, target actor, origin transform, item definition, context tags, random seed, invocation GUID.
- **`FItemEffectSpec`** — Lightweight effect descriptor: `EffectTag`, `Magnitude`, `Duration`.

### Manager (`Public/Core/ItemSystemManager.h`)

`UItemSystemManager` lives on the **GameState** as a component. Key responsibilities:
1. **Item registry** — loaded from `ItemRegistryDataTable` (`UDataTable` of `UItemDefinitionTableRow`) via `LoadItemRegistryFromDataTable()`. Items can also be registered manually via `RegisterItems()`.
2. **Querying** — `GetItemByQuery(FGameplayTagQuery)` and `GetItemByPolicy(UItemDistributionPolicy*, AActor*)`.
3. **Execution spawning + pooling** — `SpawnItemExecution(FItemContext)` resolves the soft class, reuses pooled actors, and calls `ReleaseExecutionActor()` to return them.
4. **World spawn management** — Discovers/registers `AItemSpawnPoint` actors, runs timed refresh (`WorldSpawnRefreshMode`), propagates `FItemSpawnPointPickupRoutingSettings` to all spawn points.

Static accessor: `UItemSystemManager::Get(WorldContextObject)`.

### Execution Strategies (`Public/Strategies/`)

`AItemExecutionStrategy` — abstract base Actor, pooled by the manager. Subclasses:
- `Execution_Projectile` — Moving projectile with collision (ArcThrow / Drop / ExternalVelocity launch modes).
- `Execution_Trap` — Static trigger (mine/trap) with lifespan.
- `Execution_DirectApply` — No physical actor; applies payload immediately using `FItemPayloadRoutingSettings` to resolve recipients. VFX/SFX play on the instigator pawn: looks for a pre-placed `UNiagaraComponent` on the pawn and activates it (so the effect follows the pawn). Falls back to spawning at pawn location if no component is found.
- `Execution_Instant` — Simple immediate apply.

All carry `FItemContext` (replicated), trail/impact VFX (`UNiagaraSystem`), and impact SFX. `FinishExecution()` returns the actor to the pool. `ShouldAffectActor()` enforces team filtering (`Rule.Ignore.Teammates` identity tag) and immunity (`ITargetableInterface`).

### Payload Strategies (`Public/Strategies/`)

`UItemPayloadStrategy` — abstract UObject, instanced per execution. Single entry point: `ApplyEffect(AActor* Target, FItemContext)`. Concrete implementations: `Payload_Damage`, `Payload_Explosion`, `Payload_ModifySpeed`, `Payload_Debug`.

### Targeting Strategies (`Public/Strategies/`)

`UItemTargetingStrategy` — abstract UObject, instanced per execution. Implementations: `Targeting_Raycast`, `Targeting_FindNearest`.

### Pawn Integration

Pawns must implement **`IItemInterface`**:
- `GetTeamID()` — team for friendly-fire filtering.
- `HasGameplayTag(FGameplayTag)` — checked against `UsageBlockingTags`.
- `GetSocketByTag(FGameplayTag, FName&)` — maps tag (e.g. `Socket.Mount.HandRight`) to a mesh + socket name for projectile spawn origin.
- `GetItemInstigatorController()` — for scoring/auth.
- `ApplyItemEffect(FItemEffectSpec, FItemContext)` — receives an effect from a payload. Forward to `UItemEffectHandlerComponent::HandleEffect` (or any custom component).

### Inventory (`Public/Core/InventoryComponent.h`)

Added to the Pawn. Holds one item + ammo count (both replicated). Server RPCs: `Server_GrantItem`, `Server_TryActivateItem`, `Server_ClearInventory`. Fires `OnInventoryChanged` delegate for UI. Manages `HeldMeshComponent` (item preview mesh on pawn) via `OnRep_CurrentItem`.

Sound behaviour:
- `Sound_OnEquip` plays on all clients via `OnRep_CurrentItem` (RepNotify fires on grant).
- `Sound_OnActivate` plays on all clients via `Multicast_PlayActivateSound` (called from server on successful activation).

### World Spawn System (`Public/Core/ItemSpawnPoint.h`)

`AItemSpawnPoint` — placed in level, registered to the manager. Contains:
- `USphereComponent` (PickupTrigger) — overlap-based pickup.
- `UStaticMeshComponent` (ItemPreviewMesh) — shows assigned item mesh.
- `UNiagaraComponent` (SpawnVFXComponent) — optional looping VFX (e.g. pickup aura). Assign a Niagara system in the Blueprint subclass or per-actor Details panel.
- Editor-only `UBillboardComponent` + `UTextRenderComponent` for identification.

Pickup routing is controlled by `FItemSpawnPointPickupRoutingSettings` propagated from the manager's `UItemSpawnPointRoutingConfig` data asset. Grant amount priority: **SpawnPoint local override > manager routing config override > `UItemDefinition::PickupGrantAmount`**. `RecipientPolicy` options: `OverlappingActorOnly`, `OverlapActorIfHasTagElseDesignated`, `DesignatedActorOnly`.

### Effect System (`Public/Components/`)

Two-component design for timed stat effects:
1. **`UItemEffectHandlerComponent`** — Pre-attached to the pawn. Entry point: `HandleEffect(FItemEffectSpec, FItemContext)` — a `BlueprintNativeEvent` that returns `false` by default. Create a Blueprint subclass and override `HandleEffect` to implement project-specific effect types (speed, shields, etc.). Optionally reads a sibling `UItemEffectComponent` for UI.
2. **`UItemEffectComponent`** — Optional, for UI tracking only. Holds a replicated `TArray<FItemActiveEffect>`. Fires `OnEffectsChanged` delegate on server and clients.

> **Speed handling** — There is no built-in C++ speed implementation. Add a Blueprint component to your pawn to handle `Item.Effect.ModifySpeed` effects and manipulate `CharacterMovementComponent::MaxWalkSpeed` there.

### Distribution Policies (`Public/Distribution/`)

`UItemDistributionPolicy` — instanced UObject base for selecting an item from the registry. Implementations:
- `DistributionPolicy_Random` — random selection.
- `DistributionPolicy_TagQuery` — filters by a `FGameplayTagQuery`.

### Gameplay Tag Conventions

| Prefix | Purpose |
|---|---|
| `Item.Test.*` | QA item identifiers |
| `Item.Effect.*` | Active effect identifiers (e.g. `Item.Effect.ModifySpeed.Boost`) |
| `Rule.Ignore.*` | Behaviour rules on items (e.g. `Rule.Ignore.Teammates`) |
| `State.Status.*` | Pawn states for usage blocking (e.g. `State.Status.Stunned`) |
| `Socket.Mount.*` | Socket mapping tags on pawns |
| `Routing.*` | Pickup/payload routing tags on actors |

### Replication Notes

- All item grant/activation is **server-authoritative** (Server RPCs).
- `UInventoryComponent.CurrentItem` uses `RepNotify` to update visuals and play equip sound on clients.
- `UInventoryComponent.LastActivationTime` is `COND_OwnerOnly` — cooldown UI visible only to the owning player.
- `AItemSpawnPoint.AssignedItem` uses `RepNotify` to refresh preview mesh on clients.
- `UItemEffectComponent.ActiveEffects` is replicated; `OnEffectsChanged` fires on both server and clients after `OnRep`.
- Execution actors carry a replicated `FItemContext`; impact VFX/SFX are fired via `NetMulticast Unreliable`.
- `Execution_DirectApply` VFX use `Multicast_PlayVFXOnInstigator` to find and activate the pawn's pre-placed `UNiagaraComponent` on all clients.

---

## QA Checklist

### Global Setup (PIE Multiplayer)
1. Open the project and the test map.
2. Set up PIE:
   1. Play -> Advanced Settings.
   2. Number of Players = 2.
   3. Net Mode = Play As Listen Server.
   4. Play in New Editor Window (optional but recommended).
3. Open Output Log: Window -> Developer Tools -> Output Log.
4. Ensure required components exist:
   1. Open your GameState BP — add `ItemSystemManager`, assign `ItemRegistryDataTable`.
   2. Open your Pawn BP — add `InventoryComponent`.
   3. Add `ItemEffectHandlerComponent` (or a BP subclass of it) if you have effect payloads.
   4. In the pawn's `ApplyItemEffect`, forward the call to `ItemEffectHandlerComponent::HandleEffect`.
   5. Optional — add `ItemEffectComponent` for UI tracking of active effects.
5. Set `Cheat Manager Class` to `ItemCheatManager` on the PlayerController BP.
6. Optional QA logs: `ItemSystem.QA 1` / `ItemSystem.QA 0`.
7. Optional debug visuals: set `bShowDebugVisuals = true` on Execution BPs.

---

### 1) Usage Validation (Cooldown + Blocking Tags)
**Setup**
1. Open `DA_Item_SkillShot`. Set `Cooldown = 2.0`. Set `UsageBlockingTags` to `State.Status.Stunned`.

**Steps**
1. `Cheat_GiveItem Item.Test.SkillShot`. Use twice quickly. Toggle Stunned. Try again.

**Expected**
1. Second activation blocked by cooldown. Blocked when Stunned.
2. QA logs: `QA: Cooldown blocked item ...` / `QA: Blocking tag ... prevented item ...`

---

### 2) Team Filtering + Immunity
**Setup**
1. Add `Rule.Ignore.Teammates` in `IdentityTags`. Implement `GetTeamID` and `IsImmuneTo` on pawns.

**Steps**
1. Player1 fires at Player2 (same team). Enable immunity on target. Fire again.

**Expected**
1. No payload applied to teammates or immune targets.
2. QA logs: `QA: Team filter blocked actor ...` / `QA: Immunity blocked actor ...`

---

### 3) Spawn from Socket
**Setup**
1. In Pawn BP `GetSocketByTag`, map `Socket.Mount.HandRight` → `MuzzleSocket`. Set `AttachSocketTag` on item.

**Expected** — Projectile spawns from socket location, not actor origin.

---

### 4) Distribution Policies
**Setup** — Create BP from `UDistributionPolicy_TagQuery`. Call `GetItemByPolicy(Policy, Requester)`.

**Expected** — Returned item matches the query.

---

### 5) Cheat TagQuery Parsing
1. `Cheat_GiveItem Item.Test.SkillShot` — granted.
2. `Cheat_GiveItem Item.Test.SkillShot AND NOT Item.Test.Debug` — granted.
3. `Cheat_GiveItem (Item.Test.SkillShot OR Item.Test.Speed.Boost)` — granted.
4. Invalid query — warning log.

---

### 6) Pooling of Execution Actors
**Setup** — Use a projectile or trap. Enable QA logs.

**Expected** — After 5+ fires, logs show `QA: Added actor to pool ...` and `QA: Reusing pooled actor ...`.

---

### 7) Impact VFX/SFX
**Setup** — Set `ImpactVFX` and `ImpactSound` on Execution BP.

**Expected** — VFX and SFX play at impact location on all clients.

---

### 8) Cheat_ClearInventory
1. `Cheat_GiveItem Item.Test.SkillShot` then `Cheat_ClearInventory`.

**Expected** — Item removed, ammo = 0.

---

### 9) Cheat_SimulateImpact
**Setup** — Ensure item has a valid `PayloadClass`.

1. Aim at a target. `Cheat_SimulateImpact Item.Test.SkillShot`.

**Expected** — Payload applied to hit actor.

---

### 10) UI Feedback (Active Effects)
**Setup**
1. Add `ItemEffectHandlerComponent` and `ItemEffectComponent` to the pawn.
2. `ApplyItemEffect` forwards to `HandleEffect`. Bind `OnEffectsChanged` in UI BP.

**Steps** — Apply a speed-effect item. Watch UI. Wait for expiry.

**Expected**
1. Effect appears in UI on all clients on apply.
2. Effect clears in UI on all clients on expiry.

---

### 11) SpawnPoint Pickup + Recipient Routing

#### 11.1 Quick Vocabulary
- **Overlap Actor** — the actor entering the trigger.
- **Recipient** — actor whose `InventoryComponent` receives the item.
- **AssignedItem** — item currently on `AItemSpawnPoint`.

#### 11.2 One-Time Setup
1. Place `AItemSpawnPoint` in test level.
2. Create `UItemSpawnPointRoutingConfig` Data Asset; assign on manager.
3. Ensure all candidate pawns have `InventoryComponent` and implement `IItemInterface` (`GetTeamID`, `HasGameplayTag`).
4. Prepare 3 pawns: `P1_Overlap` (Team 1), `P2_Ally` (Team 1), `P3_Enemy` (Team 2).
5. Set routing tags: `Routing.ReceiveSelf` on P1, `Routing.Designated` on P2/P3.
6. Enable QA logs: `ItemSystem.QA 1`.

#### 11.3 Routing Config Reference

**Pickup**
- `PickupMethod` (current: `TriggerOverlap`), `bConsumeOnSuccessfulGrant`, `bRequestImmediateRespawnOnConsume`.
- `RecipientPolicy`: `OverlappingActorOnly` | `OverlapActorIfHasTagElseDesignated` | `DesignatedActorOnly`.

**Grant Amount Priority** — SpawnPoint local override > manager routing config > `UItemDefinition::PickupGrantAmount`.

**Recipient Rules** — `OverlapReceivesItemTag`, `DesignatedRecipientActor`, `DesignatedRecipientTag`, `DesignatedRecipientRelation` (Any / SameTeam / Enemy), `bFallbackToOverlapIfDesignatedNotFound`.

#### 11.4 Baseline Test — Overlap Actor Receives
- `RecipientPolicy = OverlappingActorOnly`, `bConsumeOnSuccessfulGrant = true`, `bRequestImmediateRespawnOnConsume = true`.
- Move P1 into trigger → P1 receives item → spawn consumed → immediate respawn.

#### 11.5 Tag Gate Test (Self If Tag, Else Designated)
- `RecipientPolicy = OverlapActorIfHasTagElseDesignated`, `OverlapReceivesItemTag = Routing.ReceiveSelf`, `DesignatedRecipientActor = P2_Ally`.
- **A (tag present on P1)** → P1 receives. **B (tag absent)** → P2 receives.

#### 11.6 Designated-Only Test
- `RecipientPolicy = DesignatedActorOnly`, `DesignatedRecipientActor = P2_Ally`.
- P1 enters trigger → P2 receives, P1 does not.

#### 11.7 Designated Lookup by Tag + Team Relation
- `DesignatedRecipientTag = Routing.Designated`. P2 and P3 both have this tag.
- `SameTeamAsOverlappingActor` → P2 receives. `EnemyOfOverlappingActor` → P3 receives.

#### 11.8 Nearest Candidate Selection
- Two valid tagged candidates. Move one closer → closest receives.

#### 11.9 Fallback Behavior
- **Fallback enabled** — designated lookup fails, `bFallbackToOverlapIfDesignatedNotFound = true` → P1 receives.
- **Fallback disabled** — no recipient. Spawn item remains available.

#### 11.10 Grant Amount Priority + Stack Validation
1. First pickup: item definition amount (e.g. 3).
2. Manager override enabled (`OverrideGrantAmount = 2`): grants 2.
3. SpawnPoint local override (`LocalGrantAmountOverride = 4`): grants 4.
4. Stack respects `MaxStack`.

#### 11.11 Pass/Fail Checklist
1. Recipient selection matches configured policy.
2. Tag gate and team relation filter correct.
3. Fallback behavior matches toggle.
4. Grant amount and stack correct.
5. Consume/respawn matches config.
6. Replication consistent across server and clients.

#### 11.12 Common Misconfiguration
- No one receives → check `InventoryComponent`, `DesignatedRecipientTag` presence, relation filter.
- Wrong actor receives → re-check `RecipientPolicy`, `OverlapReceivesItemTag`, fallback toggle.
- Item disappears without respawn → verify manager refresh mode and `bRequestImmediateRespawnOnConsume`.

---

### 12) Direct Payload Routing (`AExecution_DirectApply`)

#### 12.1 Setup
1. Item definition: `ExecutionClass = AExecution_DirectApply`, `PayloadClass = Payload_ModifySpeed`.
2. `PayloadRouting`: `RoutingPolicy = SearchByRules`, `RecipientRelation = SameTeamAsInstigator` / `EnemyOfInstigator`, optional `RequiredRecipientTag`, `SearchSelection`.
3. 3 pawns: P1 instigator (Team 1), P2 ally (Team 1), P3 enemy (Team 2).

#### 12.2 Ally Tagged Route
- `RecipientRelation = SameTeamAsInstigator`, `RequiredRecipientTag = Routing.BuffReceiver`.
- Expected: payload applies to tagged ally; execution finishes immediately.

#### 12.3 Enemy Route By Team
- `RecipientRelation = EnemyOfInstigator`, no tag filter.
- Expected: payload applies to enemy target(s); no projectile/trap spawned.

#### 12.4 Fallbacks
- No valid candidates + `bFallbackToTargetingResultIfNoSearchMatch = true` → targeting strategy result.
- Still no recipient + `bFallbackToInstigatorIfNoRecipient = true` → payload applies to instigator.

#### 12.5 VFX on Instigator (Pawn-Attached)
- Add a `UNiagaraComponent` to the pawn BP.
- Set `ImpactVFX` on the Execution BP.
- Expected: on activation, the pawn's `UNiagaraComponent` is set to `ImpactVFX` and activated — VFX follows the pawn. If no component is found, VFX spawns at pawn location (fallback).

---

### 13) ItemEffectHandlerComponent

> **Note** — The C++ `ItemEffectHandlerComponent` no longer has any built-in effect handlers. `HandleEffect` is a `BlueprintNativeEvent` that returns `false` by default. To handle effects (e.g. speed), create a Blueprint subclass of `UItemEffectHandlerComponent` and override `HandleEffect`, or add a dedicated BP component and call it from `ApplyItemEffect`.

#### 13.1 Blueprint Override of HandleEffect
1. Create `BP_ItemEffectHandlerComponent` (child of `UItemEffectHandlerComponent`).
2. Override `HandleEffect`: check `Spec.EffectTag`, apply logic, return `true` if handled.
3. Replace the pawn's handler component with this BP version.
4. Apply an effect item → verify the override fires and returns `true`.

#### 13.2 Unknown Tag — Returns False
- Call `HandleEffect` with a spec whose `EffectTag` is not handled in the override.
- Expected: returns `false`. No side effect. No crash.

#### 13.3 UI Tracking with ItemEffectComponent
- `ItemEffectHandlerComponent` exposes `FindEffectComponent()` utility. In your BP override, call `FindEffectComponent()` → `AddOrRefreshEffect(Spec)` to populate the UI component.

#### 13.4 Common Misconfiguration
- Effect not applying → verify `ApplyItemEffect` on the pawn calls `HandleEffect` on the correct component instance.
- UI not updating → verify the BP override calls `FindEffectComponent()->AddOrRefreshEffect(Spec)` and `ItemEffectComponent` is present.

---

### 14) Projectile Launch Modes (ArcThrow / Drop / ExternalVelocity)

#### 14.1 Vocabulary
- **ArcThrow** — ballistic parabola solved by `SuggestProjectileVelocity` toward camera aim point.
- **Drop** — minimal forward impulse; gravity handles trajectory.
- **ExternalVelocity** — velocity from `FItemContext::LaunchVelocity`; falls back to ArcThrow if not set.

#### 14.2 Setup
1. Three BP subclasses of `AExecution_Projectile`: `BP_Projectile_Arc` (ArcThrow), `BP_Projectile_Drop` (Drop), `BP_Projectile_External` (ExternalVelocity).
2. `bShowDebugVisuals = true` on all. Enable QA logs.

#### 14.3 ArcThrow — Baseline
- `Speed = 2000`, `GravityScale = 1.0`, `bFavorHighArc = false`.
- Expected: visible parabolic arc toward aim point. No fallback log for reachable targets.

#### 14.4 ArcThrow — High vs Low Arc
- `bFavorHighArc = false` → flatter path. `bFavorHighArc = true` → higher looping path to the same target.

#### 14.5 ArcThrow — Unreachable Fallback
- `Speed = 100`, target very far. Expected: fires toward aim direction, QA log: `QA: ArcThrow could not solve ballistic path to aim point — using direct aim.`

#### 14.6 ArcThrow — GravityScale
- Lower GravityScale → broader arc. Higher → tighter arc. Aim point still reached.

#### 14.7 ArcThrow — No PlayerController
- AI pawn or nulled controller. Expected: fires forward at full speed along pawn forward vector. No crash.

#### 14.8 Drop — Pure Gravity Fall
- `DropForwardImpulse = 0`. Expected: falls straight down near pawn feet.

#### 14.9 Drop — Forward Impulse
- `DropForwardImpulse = 300` then `800`. Expected: forward nudge scales with value; gravity dominant.

#### 14.10 ExternalVelocity — Velocity Provided
- `bHasExternalLaunchVelocity = true`, `LaunchVelocity = (1000, 0, 500)`. Expected: projectile travels in that direction, no ArcThrow.

#### 14.11 ExternalVelocity — Missing Velocity Fallback
- `bHasExternalLaunchVelocity = false`. Expected: falls back to ArcThrow + warning log.

#### 14.12 Pool Reuse — Velocity Reset
- Fire → hit → pool returns actor. Rotate 90°. Fire again. Expected: second shot uses new direction, not stale velocity.

#### 14.13 Lifespan — Auto-Destroy After 10 s
- Fire into open air. Expected: actor destroyed after ~10 s. No crash.

#### 14.14 Instigator Self-Collision
- Fire downward at pawn feet. Expected: no hit/overlap on instigator (`MoveIgnoreActors`).

#### 14.15 Replication
- P1 fires at P2 (Listen Server). Expected: projectile visible on both windows; impact VFX/SFX on both; payload applied once (`bHasExploded` guard).

#### 14.16 Pass/Fail Checklist

| # | Scenario |
|---|---|
| 1 | ArcThrow reaches aimed target with visible parabolic arc |
| 2 | `bFavorHighArc` selects correct ballistic solution |
| 3 | ArcThrow falls back to direct aim for unreachable targets |
| 4 | `GravityScale` changes arc curvature |
| 5 | No PlayerController → fires forward, no crash |
| 6 | Drop `DropForwardImpulse = 0` → pure vertical fall |
| 7 | Drop with impulse → nudged forward, gravity dominant |
| 8 | ExternalVelocity uses `LaunchVelocity` when provided |
| 9 | ExternalVelocity falls back to ArcThrow + warning |
| 10 | Pool reuse applies fresh velocity |
| 11 | Projectile auto-destroys after 10 s |
| 12 | Instigator not affected by own projectile |
| 13 | Movement and impact replicate to all clients |

#### 14.17 Common Misconfiguration
- Projectile flies in wrong direction → check `GetSocketByTag` forward direction; for ExternalVelocity verify world-space velocity.
- ArcThrow always falls back → `Speed` too low or target directly above.
- Drop doesn't fall → `GravityScale > 0` and world gravity not zero.
- Pool reuse keeps old velocity → ensure `Super::ResetForReuse()` is called in BP overrides.

---

### 15) Widget UI Reference

#### 15.1 Vocabulary
- **BFL** — `UItemEffectBlueprintLibrary` (static helper functions).
- **EffectComp** — `UItemEffectComponent` on the pawn.
- **InvComp** — `UInventoryComponent` on the pawn.
- **ServerTime** — `GameState->GetServerWorldTimeSeconds()` for synchronised progress.

#### 15.2 Setup
1. Pawn has `InventoryComponent`, `ItemEffectHandlerComponent`, `ItemEffectComponent`.
2. Create `WBP_ItemHUD` with: buff/debuff panel, inventory slot (icon + ammo), cooldown bar.
3. In `Event Construct`: bind `EffectComp.OnEffectsChanged` → `RefreshEffects`, `InvComp.OnInventoryChanged` → `RefreshInventory`.

#### 15.3 Buffs/Debuffs — Display and Expiry
- Apply effect (8 s). In `RefreshEffects`: call `BFL.GetEffectNormalizedProgress(Effect, ServerTime)`.
- Expected: `OnEffectsChanged` fires on apply and expiry. Progress bar goes 0→1 over 8 s.

#### 15.4 GetEffectRemainingTime
- Poll `BFL.GetEffectRemainingTime(Effect, ServerTime)`. Expected: counts down 8→0. Returns `-1` for permanent effects.

#### 15.5 GetEffectsByTag (Parent Tag Filter)
- Apply `EffectTag = Item.Effect.ModifySpeed.Slow`. Call `BFL.GetEffectsByTag(EffectComp, "Item.Effect.ModifySpeed")`.
- Expected: returns 1 entry — child tag matched via hierarchical matching.

#### 15.6 HasActiveEffect
- Before apply: `false`. During: `true`. After expiry: `false`.

#### 15.7 Replication to All Clients
- P1 activates slow effect. Both windows should show the effect and clear on expiry.

#### 15.8 Inventory — Item Icon and Name
- `Cheat_GiveItem Item.Test.Cooldown 3` → icon, name, ammo 3 appear. `Cheat_ClearInventory` → widget blanks.

#### 15.9 Ammo Decrement
- 3 ammo → use 3 times → ammo decrements to 0, widget clears.

#### 15.10 Cooldown — Progress Bar (Owner Client)
- Poll `InvComp.GetCooldownProgress()`. Expected: 0.0 immediately after activation → 1.0 after `Cooldown` seconds.

#### 15.11 Cooldown Not Replicated to Non-Owners
- Non-owner reads `GetCooldownProgress()` on another pawn → returns 1.0 (by design, `LastActivationTime` is owner-only).

#### 15.12 Pass/Fail Checklist

| # | Scenario |
|---|---|
| 1 | `OnEffectsChanged` fires on apply and expiry |
| 2 | `GetEffectNormalizedProgress` returns 0→1 over duration |
| 3 | `GetEffectRemainingTime` counts down; -1 for permanent |
| 4 | `GetEffectsByTag` parent tag matches child-tagged effects |
| 5 | `HasActiveEffect` correct before/during/after |
| 6 | `OnEffectsChanged` replicates to all clients |
| 7 | `OnInventoryChanged` delivers icon, name, ammo |
| 8 | Ammo decrements and widget clears on depletion |
| 9 | `GetCooldownProgress` transitions 0→1 over cooldown |
| 10 | Cooldown correct on owner; not visible on non-owners |

#### 15.13 Common Misconfiguration
- `OnEffectsChanged` never fires → check `ItemEffectComponent` present; check `ApplyItemEffect` calls `HandleEffect` which calls `EffectComp->AddOrRefreshEffect`.
- Effect stays in widget after expiry → verify `OnEffectsChanged` is bound; call `GetActiveEffects()` fresh on each callback.
- Progress bar stuck → use `GameState->GetServerWorldTimeSeconds()`, not `GetWorld()->GetTimeSeconds()`.
- Cooldown not updating on client → `LastActivationTime` is `COND_OwnerOnly`; verify pawn ownership.

---

## Item Presets (DA_Item_*)

Reference configuration for core gameplay tests.

### DA_Item_SkillShot
**Use**: skill-shot projectile with explosion payload.
- `IdentityTags`: `Item.Test.SkillShot`
- `ExecutionClass`: `BP_Execution_Projectile_SkillShot` — Speed=2000, GravityScale=1.0, bShowDebugVisuals=true
- `PayloadClass`: `BP_Payload_Explosion_SkillShot` — DamageAmount=20, DamageRadius=300, bDoFullDamage=false, bIgnoreInstigator=true

### DA_Item_SpeedBoost
**Use**: projectile applies speed boost.
- `IdentityTags`: `Item.Test.Speed.Boost`
- `ExecutionClass`: `BP_Execution_Projectile_Speed`
- `PayloadClass`: `BP_Payload_ModifySpeed_Boost` — SpeedMultiplier=1.3, Duration=3.0, EffectTag=`Item.Effect.ModifySpeed.Boost`

### DA_Item_SpeedSlow
**Use**: projectile applies speed slow.
- `IdentityTags`: `Item.Test.Speed.Slow`
- `ExecutionClass`: `BP_Execution_Projectile_Speed`
- `PayloadClass`: `BP_Payload_ModifySpeed_Slow` — SpeedMultiplier=0.6, Duration=3.0, EffectTag=`Item.Effect.ModifySpeed.Slow`

### DA_Item_Trap_Explosion
**Use**: mine/trap with explosion payload.
- `IdentityTags`: `Item.Test.Trap.Explosion`
- `ExecutionClass`: `BP_Execution_Trap_Explosion` — TriggerRadius=150, LifeSpanSeconds=30, bShowDebugVisuals=true
- `PayloadClass`: `BP_Payload_Explosion_Trap`

### DA_Item_SpeedBoost_DirectRouted
**Use**: direct (non-projectile) speed boost routed by rules.
- `IdentityTags`: `Item.Test.Speed.Boost.Direct`
- `ExecutionClass`: `AExecution_DirectApply` (or BP subclass)
- `PayloadClass`: `BP_Payload_ModifySpeed_Boost`
- `PayloadRouting`:
  - `RoutingPolicy`: `SearchByRules`
  - `RequiredRecipientTag`: `Routing.BuffReceiver` (optional)
  - `RecipientRelation`: `SameTeamAsInstigator` (ally buff) or `EnemyOfInstigator` (debuff)
  - `SearchSelection`: `NearestSingle` or `AllMatching`
  - `SearchRadius`: 2000
  - `bFallbackToInstigatorIfNoRecipient`: true for self-safe buffs

### Tag Notes
- Add `Rule.Ignore.Teammates` in `IdentityTags` to prevent friendly fire.
- Use `UsageBlockingTags` for stun or other blocking states.
