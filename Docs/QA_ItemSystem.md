# ItemSystem QA Checklist (Detailed)

This checklist validates the features implemented in the ItemSystem plugin.

## Global Setup (PIE Multiplayer)
1. Open the project and the test map.
2. Set up PIE:
   1. Play -> Advanced Settings.
   2. Number of Players = 2.
   3. Net Mode = Play As Listen Server.
   4. Play in New Editor Window (optional but recommended).
3. Open Output Log:
   1. Window -> Developer Tools -> Output Log.
4. Ensure required components exist:
   1. Open your GameState BP (for example `GS_Item`).
   2. Add `ItemSystemManager` component if missing.
   3. Assign `ItemRegistryDataTable` on the manager (rows must reference `UItemDefinition` assets).
   4. Open your Pawn BP (for example `BP_ItemCharacter`).
   5. Add `InventoryComponent` if missing.
5. Ensure cheat manager is wired:
   1. Open your PlayerController BP.
   2. Set `Cheat Manager Class` to `ItemCheatManager`.
6. Optional QA logs:
   1. Open console with `~`.
   2. Run `ItemSystem.QA 1`.
   3. Use `ItemSystem.QA 0` to disable.
7. Optional debug visuals:
   1. Open your Execution BP (Projectile/Trap).
   2. Set `bShowDebugVisuals = true`.

---

## 1) Usage Validation (Cooldown + Blocking Tags)
**Setup**
1. Open `DA_Item_SkillShot`.
2. Set `Cooldown = 2.0`.
3. Set `UsageBlockingTags` to include `State.Status.Stunned`.
4. Ensure your pawn implements `HasGameplayTag`:
   1. Return `true` when you want to simulate Stunned.

**Steps**
1. Console: `Cheat_GiveItem Item.Test.SkillShot`.
2. Use the item twice quickly (same second).
3. Toggle Stunned on the pawn.
4. Try to use the item again.

**Expected**
1. Second activation is blocked by cooldown.
2. Activation is blocked when Stunned.
3. With QA logs, see:
   1. `QA: Cooldown blocked item ...`
   2. `QA: Blocking tag ... prevented item ...`

---

## 2) Team Filtering + Immunity
**Setup**
1. Open `DA_Item_*` used for the test.
2. Add `Rule.Ignore.Teammates` in `IdentityTags`.
3. In the pawn, implement `GetTeamID`:
   1. Example: Player 1 returns 1, Player 2 returns 1 (same team) for test.
4. In target actor, implement `IsImmuneTo`:
   1. Return `true` when `IncomingTags` contains `Item.Test.SkillShot`.

**Steps**
1. Player1 fires at Player2 (same team).
2. Enable immunity on target.
3. Fire again.

**Expected**
1. No payload applied to teammates.
2. No payload applied to immune targets.
3. With QA logs, see:
   1. `QA: Team filter blocked actor ...`
   2. `QA: Immunity blocked actor ...`

---

## 3) Spawn Point via Socket
**Setup**
1. In the Pawn skeletal mesh, create a socket (example `MuzzleSocket`).
2. In Pawn BP `GetSocketByTag`:
   1. Map `Socket.Mount.HandRight` -> `MuzzleSocket` and return the mesh component.
3. In `DA_Item_*`, set `AttachSocketTag = Socket.Mount.HandRight`.

**Steps**
1. Fire the item.

**Expected**
1. Projectile spawns from the socket location, not from actor origin.

---

## 4) Distribution Policies
**Setup**
1. Create a BP derived from `UDistributionPolicy_TagQuery`.
2. Set its `Query` to match a test tag (example `Item.Test.SkillShot`).
3. Ensure `ItemRegistryDataTable` contains rows pointing to items with that tag.

**Steps**
1. In any BP (GameState or Debug BP), call:
   1. `UItemSystemManager::Get(World)->GetItemByPolicy(Policy, Requester)`
2. Print the returned item name.

**Expected**
1. Returned item matches the query.

---

## 5) Cheat TagQuery Parsing
**Steps**
1. Console: `Cheat_GiveItem Item.Test.SkillShot`
2. Console: `Cheat_GiveItem Item.Test.SkillShot AND NOT Item.Test.Debug`
3. Console: `Cheat_GiveItem (Item.Test.SkillShot OR Item.Test.Speed.Boost)`

**Expected**
1. Item granted for valid queries.
2. Warning for invalid queries.

---

## 6) Pooling of Execution Actors
**Setup**
1. Use a projectile or trap item.
2. Enable QA logs (`ItemSystem.QA 1`).

**Steps**
1. Fire 5+ times.
2. Observe logs.

**Expected**
1. Logs show pooling:
   1. `QA: Added actor to pool ...`
   2. `QA: Reusing pooled actor ...`
2. No crashes or leaks.

---

## 7) Impact VFX/SFX
**Setup**
1. Open your Execution BP.
2. Set `ImpactVFX` to a Niagara system.
3. Set `ImpactSound` to a sound asset.

**Steps**
1. Fire and hit a wall or target.

**Expected**
1. VFX and SFX play at impact location on all clients.

---

## 8) Cheat_ClearInventory
**Steps**
1. Console: `Cheat_GiveItem Item.Test.SkillShot`
2. Console: `Cheat_ClearInventory`

**Expected**
1. Item removed, ammo = 0.

---

## 9) Cheat_SimulateImpact
**Setup**
1. Ensure `DA_Item_*` has a valid `PayloadClass`.

**Steps**
1. Aim at a target with the camera.
2. Console: `Cheat_SimulateImpact Item.Test.SkillShot`

**Expected**
1. Payload is applied directly to the hit actor.
2. Log confirms application.

---

## 10) UI Feedback (Active Effects)
**Setup**
1. Add `ItemEffectComponent` to the Pawn (BP component).
2. In UI BP:
   1. Bind to `OnEffectsChanged`.
   2. Call `GetActiveEffects()`.
3. Use `Payload_ModifySpeed` to add effects.

**Steps**
1. Apply Boost or Slow to the target.
2. Observe UI list.

**Expected**
1. Effect appears on clients (replicated).
2. Duration decreases and effect disappears at expiry.

---

## 11) SpawnPoint Pickup + Recipient Routing (Detailed)
This section validates the new world spawn pickup behavior:
- overlap-based pickup
- configurable recipient routing (self, designated actor, tagged target)
- optional ally/enemy routing via team relation
- configurable consume/respawn flow

### 11.1 Quick Vocabulary
- `Overlap Actor`: the actor that enters the spawn trigger.
- `Recipient`: the actor whose `InventoryComponent` receives the item.
- `AssignedItem`: the item currently present on `AItemSpawnPoint`.

### 11.2 One-Time Setup (Anyone Can Configure)
1. Open a test level.
2. Place one `AItemSpawnPoint` actor, name it `SP_Routing_Test`.
3. Ensure your `GameState` has `ItemSystemManager`.
4. Create a `UItemSpawnPointRoutingConfig` Data Asset (example: `DA_SpawnRouting_Default`).
5. Assign it on `GameState -> ItemSystemManager -> SpawnPointPickupRoutingConfig`.
6. Configure pickup/routing fields in that Data Asset.
7. If you change settings at runtime in PIE, call `ApplySpawnPointPickupRoutingConfig(true)` on the manager (server).
8. Ensure all candidate pawns/characters:
   1. have `InventoryComponent`
   2. implement `ItemInterface` (`GetTeamID` and `HasGameplayTag` at minimum).
9. Prepare at least 3 pawns:
   1. `P1_Overlap` (the one who walks into the spawn)
   2. `P2_Ally` (same team as `P1_Overlap`)
   3. `P3_Enemy` (different team).
10. Set Team IDs for tests:
   1. `P1_Overlap = Team 1`
   2. `P2_Ally = Team 1`
   3. `P3_Enemy = Team 2`.
11. Set gameplay tags for routing tests:
   1. on `P1_Overlap`: optional `Routing.ReceiveSelf`
   2. on `P2_Ally` and/or `P3_Enemy`: optional `Routing.Designated`.
12. Enable verbose logs:
   1. open console `~`
   2. run `ItemSystem.QA 1`.

### 11.3 Authoritative GameState Configuration Reference
Configure these properties in the `UItemSpawnPointRoutingConfig` assigned to `GameState -> ItemSystemManager -> SpawnPointPickupRoutingConfig`:

**Pickup**
1. `PickupMethod`: pickup trigger mode (current implementation: `TriggerOverlap`).
2. `bOverrideItemGrantAmount`: if true, manager config overrides item default pickup quantity.
3. `OverrideGrantAmount`: override value used only when `bOverrideItemGrantAmount = true`.
4. `bConsumeOnSuccessfulGrant`: consumes `AssignedItem` after a successful grant.
5. `bRequestImmediateRespawnOnConsume`: asks manager for immediate refill when consumed.
6. `RecipientPolicy`:
   1. `OverlappingActorOnly`
   2. `OverlapActorIfHasTagElseDesignated`
   3. `DesignatedActorOnly`.

**Item Data Asset (Primary Quantity Source)**
1. In each `UItemDefinition`, set `PickupGrantAmount`.
2. This is the default granted amount when pickup succeeds.
3. Manager and SpawnPoint overrides are optional and only applied when explicitly enabled.

**SpawnPoint Local Quantity Override (Optional, Highest Priority)**
1. `bUseLocalGrantAmountOverride`: if true, this specific spawn point overrides both item default and manager config override.
2. `LocalGrantAmountOverride`: local override amount.

**Recipient Rules**
1. `OverlapReceivesItemTag`:
   1. used only in `OverlapActorIfHasTagElseDesignated`
   2. if overlap actor has this tag, overlap actor receives.
2. `DesignatedRecipientActor`:
   1. explicit actor recipient (set at runtime from GameState via `SetSpawnPointPickupRoutingSettings`, because Data Assets should not reference level actors).
3. `DesignatedRecipientTag`:
   1. optional world lookup filter
   2. nearest matching actor with inventory is selected.
4. `DesignatedRecipientRelation`:
   1. `Any`
   2. `SameTeamAsOverlappingActor`
   3. `EnemyOfOverlappingActor`.
5. `bFallbackToOverlapIfDesignatedNotFound`:
   1. if designated target cannot be resolved, fallback to overlap actor.

### 11.4 Recommended Test Environment
1. Run PIE with `3 players`.
2. Net mode:
   1. first pass: `Play As Listen Server`
   2. second pass: `Play As Client` with dedicated server.
3. Keep `SP_Routing_Test` visible and close to all three pawns.
4. Ensure `SP_Routing_Test` currently has a valid `AssignedItem`.

### 11.5 Baseline Test (Overlap Actor Receives)
**Setup**
1. Edit the manager's `SpawnPointPickupRoutingConfig` Data Asset (or call `SetSpawnPointPickupRoutingSettings` in GameState).
2. On tested item definition, set `PickupGrantAmount = 1`.
3. In manager config, set `bOverrideItemGrantAmount = false`.
4. On `SP_Routing_Test`, set `bUseLocalGrantAmountOverride = false`.
5. `RecipientPolicy = OverlappingActorOnly`
6. `bConsumeOnSuccessfulGrant = true`
7. `bRequestImmediateRespawnOnConsume = true`

**Steps**
1. Move `P1_Overlap` into `SP_Routing_Test`.
2. Observe `P1_Overlap` inventory.
3. Observe spawn item state after pickup.

**Expected**
1. `P1_Overlap` receives the item.
2. Spawn item is consumed.
3. If manager allows, spawn is immediately re-assigned.

### 11.6 Tag Gate Test (Self If Tag, Else Designated)
**Setup**
1. Update authoritative settings from GameState/manager.
1. `RecipientPolicy = OverlapActorIfHasTagElseDesignated`
2. `OverlapReceivesItemTag = Routing.ReceiveSelf`
3. `DesignatedRecipientActor = P2_Ally`
4. `bFallbackToOverlapIfDesignatedNotFound = false`

**Steps A (Tag Present)**
1. Ensure `P1_Overlap` has `Routing.ReceiveSelf`.
2. `P1_Overlap` enters the trigger.

**Expected A**
1. Recipient is `P1_Overlap`.

**Steps B (Tag Missing)**
1. Remove `Routing.ReceiveSelf` from `P1_Overlap`.
2. `P1_Overlap` enters the trigger again.

**Expected B**
1. Recipient is `P2_Ally`.

### 11.7 Designated-Only Test
**Setup**
1. Update authoritative settings from GameState/manager.
1. `RecipientPolicy = DesignatedActorOnly`
2. `DesignatedRecipientActor = P2_Ally`
3. `bFallbackToOverlapIfDesignatedNotFound = false`

**Steps**
1. `P1_Overlap` enters trigger.

**Expected**
1. `P2_Ally` receives item.
2. `P1_Overlap` does not receive item.

### 11.8 Designated Lookup by Tag + Team Relation
**Setup**
1. Update authoritative settings from GameState/manager.
1. `RecipientPolicy = DesignatedActorOnly`
2. `DesignatedRecipientActor = None`
3. `DesignatedRecipientTag = Routing.Designated`
4. Set both `P2_Ally` and `P3_Enemy` with `Routing.Designated`.

**Case A: Ally**
1. `DesignatedRecipientRelation = SameTeamAsOverlappingActor`
2. `P1_Overlap` enters trigger.

**Expected A**
1. Recipient is `P2_Ally`.

**Case B: Enemy**
1. `DesignatedRecipientRelation = EnemyOfOverlappingActor`
2. `P1_Overlap` enters trigger.

**Expected B**
1. Recipient is `P3_Enemy`.

### 11.9 Nearest Candidate Selection (Tag Lookup)
**Setup**
1. Keep `DesignatedRecipientActor = None`.
2. Keep `DesignatedRecipientTag = Routing.Designated`.
3. Place 2 valid candidates with same relation and same tag.

**Steps**
1. Move one candidate very close to spawn, keep one farther away.
2. Trigger pickup with `P1_Overlap`.

**Expected**
1. Closest valid tagged actor receives item.

### 11.10 Fallback Behavior
**Case A: Fallback Enabled**
1. Make designated lookup impossible (clear designated actor and remove designated tag from all actors).
2. Set `bFallbackToOverlapIfDesignatedNotFound = true`.
3. Trigger pickup with `P1_Overlap`.

**Expected A**
1. `P1_Overlap` receives item (if it has inventory).

**Case B: Fallback Disabled**
1. Same setup but `bFallbackToOverlapIfDesignatedNotFound = false`.
2. Trigger pickup.

**Expected B**
1. No recipient receives item.
2. Spawn item remains available.

### 11.11 Grant Amount Priority + Stack Validation
**Setup**
1. Use an item with known `MaxStack` (example `MaxStack = 5`).
2. Set `PickupGrantAmount = 3` on the item definition.
3. Ensure manager config `bOverrideItemGrantAmount = false`.
4. Ensure spawn point `bUseLocalGrantAmountOverride = false`.

**Steps**
1. Trigger pickup once.
2. Enable manager override: `bOverrideItemGrantAmount = true`, `OverrideGrantAmount = 2`.
3. Trigger pickup once.
4. Disable manager override and enable spawnpoint local override: `bUseLocalGrantAmountOverride = true`, `LocalGrantAmountOverride = 4`.
5. Trigger pickup once.
6. Repeat pickups to hit stack cap.

**Expected**
1. First pickup grants item definition amount (`3`).
2. Second pickup grants manager override amount (`2`).
3. Third pickup grants spawn point local override amount (`4`).
4. Priority is: SpawnPoint local override > manager routing config override > item definition `PickupGrantAmount`.
5. Inventory clamping respects `MaxStack`.

### 11.12 Consume / No Consume Validation
**Case A: Consume Enabled**
1. `bConsumeOnSuccessfulGrant = true`.
2. Trigger pickup.

**Expected A**
1. Spawn item is consumed.

**Case B: Consume Disabled**
1. `bConsumeOnSuccessfulGrant = false`.
2. Trigger pickup.

**Expected B**
1. Spawn item remains assigned.
2. Repeated overlaps can grant again (by design).

### 11.13 Failure Path Validation
1. Remove `InventoryComponent` from all possible recipients.
2. Trigger pickup.
3. Expected:
   1. no grant
   2. no consume
   3. QA log contains: `no valid inventory recipient found`.

### 11.14 Replication Validation
1. Run with dedicated server and multiple clients.
2. Trigger pickup from one client.
3. Expected:
   1. grant is server-authoritative
   2. `AssignedItem` changes replicate to all clients
   3. preview mesh updates correctly on all clients.

### 11.15 Pass/Fail Checklist
Mark as **PASS** only when all are true:
1. Recipient selection matches configured policy every time.
2. Tag gate behavior is correct (self vs designated).
3. Team relation filter chooses ally/enemy correctly.
4. Fallback behavior matches toggle.
5. Grant amount and stack behavior are correct.
6. Consume/respawn behavior matches configuration.
7. Replication is consistent across server and clients.

### 11.16 Common Misconfiguration Guide
1. No one receives item:
   1. check recipient has `InventoryComponent`
   2. check `DesignatedRecipientTag` actually exists on target actor
   3. check relation (`SameTeam` vs `Enemy`) is not excluding target.
2. Wrong actor receives item:
   1. re-check `RecipientPolicy`
   2. verify `OverlapReceivesItemTag`
   3. verify fallback toggle.
3. Item disappears without expected respawn:
   1. verify manager refresh mode and interval
   2. verify `bRequestImmediateRespawnOnConsume`.

---

## 12) Direct Payload Routing (Non-Projectile / Non-Trap)
This section validates payload routing for direct-use items (shield, speed boost, etc.) using `AExecution_DirectApply`.

### 12.1 Setup
1. Create an item definition using:
   1. `ExecutionClass = AExecution_DirectApply` (or a BP child)
   2. `PayloadClass = Payload_ModifySpeed` (or any payload)
2. Configure `PayloadRouting` on the item:
   1. `RoutingPolicy = SearchByRules`
   2. `RecipientRelation = SameTeamAsInstigator` or `EnemyOfInstigator`
   3. `RequiredRecipientTag` optional (`Routing.BuffReceiver`, `Routing.DebuffReceiver`, etc.)
   4. `SearchSelection = NearestSingle` or `AllMatching`
3. Prepare 3 pawns:
   1. `P1` instigator (Team 1)
   2. `P2` ally (Team 1)
   3. `P3` enemy (Team 2)
4. Add tags for routing tests:
   1. ally/enemy candidates with required routing tag if used.

### 12.2 Ally Tagged Route
1. Set `RecipientRelation = SameTeamAsInstigator`.
2. Set `RequiredRecipientTag = Routing.BuffReceiver`.
3. Use item from `P1`.
4. Expected:
   1. Payload applies to tagged ally candidate(s) according to `SearchSelection`.
   2. Execution actor finishes immediately after apply.

### 12.3 Enemy Route By Team (No Tag)
1. Set `RecipientRelation = EnemyOfInstigator`.
2. Clear `RequiredRecipientTag`.
3. Use item from `P1`.
4. Expected:
   1. Payload applies to enemy target(s) (different `TeamID`).
   2. No projectile/trap is spawned; execution ends immediately.

### 12.4 Fallbacks
1. Make search fail (no valid candidates).
2. If `bFallbackToTargetingResultIfNoSearchMatch = true`, provide a valid targeting strategy and verify fallback apply.
3. If still no recipient and `bFallbackToInstigatorIfNoRecipient = true`, payload applies to instigator.
