# AGENTS.md

This file provides guidance to Codex (Codex.ai/code) when working with code in this repository.

## Building

This is a UE5 C++ plugin (`ItemSystem.uplugin`, type: Runtime, LoadingPhase: Default). Build via Unreal Editor (**Build > Build Solution** or use the toolbar hammer icon) or with UnrealBuildTool directly:

```
UnrealBuildTool.exe <ProjectName>Editor Win64 Development "<path>/MK_ItemSystem_CPP.uproject"
```

The plugin module is `ItemSystem`. Dependencies declared in `ItemSystem.Build.cs`:
- **Public**: `Core`, `GameplayTags`, `GameplayTasks`
- **Private**: `CoreUObject`, `Engine`, `Slate`, `SlateCore`, `Niagara`

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

Set `CheatManagerClass = ItemCheatManager` on the PlayerController BP to enable these. See the **QA Checklist** and **Item Presets** sections of `CLAUDE.md` for the full test procedures and reference item configurations (`Docs/QA_ItemSystem.md` and `Docs/Presets_ItemDefinitions.md` were merged into it).

## Content Structure

The plugin ships a self-contained `Content/ItemSystem/` folder. See **`Docs/Content_Architecture.md`** for the complete asset tree, per-asset configuration tables, creation order, and full wiring diagram.

Quick map:
- `Core/` — GameMode, GameState (+`UItemSystemManager`), PlayerController (+`ItemCheatManager`), Pawn (+`IItemInterface`, `UInventoryComponent`, `UItemEffectHandlerComponent`, `UItemEffectComponent`)
- `Data/` — `DT_ItemRegistry` (DataTable of `FItemDefinitionTableRow`), `DA_SpawnRoutingConfig` (`UItemSpawnPointRoutingConfig`)
- `Items/<ItemName>/` — one folder per item: `DA_Item_*` + its `BP_Execution_*` / `BP_Targeting_*` / `BP_Payload_*`
- `UI/` — `WBP_HUD_Demo`, `WBP_ItemSlot` (icon/ammo/cooldown), `WBP_EffectBar` (active effects)
- `Maps/` — `L_Demo` (WorldSettings → `BP_GameMode_ItemDemo`, has `AItemSpawnPoint` actors)

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

- **`UItemDefinition`** — Primary data asset per item. Holds identity tags, blocking tags, cooldown, visuals (`FItemVisuals`), `MaxStack`, `PickupGrantAmount`, and soft class refs to execution/targeting/payload. Also stores `FItemPayloadRoutingSettings` for direct-apply routing. Audio: `Sound_OnEquip` (played on all clients when the item is granted) and `Sound_OnActivate` (played on all clients when the item is used).
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
- `Execution_Projectile` — Moving projectile with collision.
- `Execution_Trap` — Static trigger (mine/trap) with lifespan.
- `Execution_DirectApply` — No physical actor; applies payload immediately using `FItemPayloadRoutingSettings` to resolve recipients (by team relation, tag, nearest/all-matching search). VFX/SFX play on the instigator pawn: it looks for a pre-placed `UNiagaraComponent` on the pawn and activates it, so the effect follows the pawn. Falls back to spawning at the pawn location if no component is found.
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
- `ApplyItemEffect(FItemEffectSpec, FItemContext)` — receives an effect from a payload. Implementation should forward to `UItemEffectHandlerComponent::HandleEffect` on the same pawn.

### Inventory (`Public/Core/InventoryComponent.h`)

Added to the Pawn. Holds one item + ammo count (both replicated). Server RPCs: `Server_GrantItem`, `Server_TryActivateItem`, `Server_ClearInventory`. Fires `OnInventoryChanged` delegate for UI. Manages `HeldMeshComponent` (item preview mesh on pawn) via `OnRep_CurrentItem`.

Sound behaviour:
- `Sound_OnEquip` plays on all clients via `OnRep_CurrentItem` (RepNotify fires on grant).
- `Sound_OnActivate` plays on all clients via `Multicast_PlayActivateSound` (called from the server on successful activation).

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

Payload flow: `Payload_ModifySpeed` builds an `FItemEffectSpec` and calls `IItemInterface::Execute_ApplyItemEffect` on the target. The pawn's implementation forwards to `UItemEffectHandlerComponent::HandleEffect`.

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
- `UInventoryComponent.CurrentItem` uses `RepNotify` to update visuals and play the equip sound on clients.
- `UInventoryComponent.LastActivationTime` is `COND_OwnerOnly` — cooldown UI visible only to the owning player.
- `AItemSpawnPoint.AssignedItem` uses `RepNotify` to refresh preview mesh on clients.
- `UItemEffectComponent.ActiveEffects` is replicated; `OnEffectsChanged` fires on both server and clients after `OnRep`.
- Execution actors carry a replicated `FItemContext`; impact VFX/SFX are fired via `NetMulticast Unreliable`.
- `Execution_DirectApply` VFX use `Multicast_PlayVFXOnInstigator` to find and activate the pawn's pre-placed `UNiagaraComponent` on all clients.
- Execution actors carry a replicated `FItemContext`; impact VFX/SFX are fired via `NetMulticast Unreliable`.