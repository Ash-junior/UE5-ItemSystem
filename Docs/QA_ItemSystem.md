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
   6. Add `ItemEffectHandlerComponent` if missing (required for any payload that routes via `IItemInterface::ApplyItemEffect`).
   7. In the pawn's `ApplyItemEffect` implementation, forward the call to `ItemEffectHandlerComponent::HandleEffect`.
   8. Optional — add `ItemEffectComponent` to the pawn if you want UI tracking of active effects (duration bars, etc.).
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
1. Add `ItemEffectHandlerComponent` to the Pawn (required — handles effect application and timer management).
2. Implement `ApplyItemEffect` on the pawn to forward to `ItemEffectHandlerComponent::HandleEffect`.
3. Add `ItemEffectComponent` to the Pawn (optional — enables UI tracking; automatically updated by `ItemEffectHandlerComponent` when present).
4. In UI BP:
   1. Bind to `UItemEffectComponent::OnEffectsChanged`.
   2. Call `GetActiveEffects()` to read the current list.
5. Assign a `Payload_ModifySpeed`-based item definition for the test.

**Steps**
1. Console: `Cheat_GiveItem Item.Test.SpeedBoost` (or any item using `Payload_ModifySpeed`).
2. Activate the item — payload calls `IItemInterface::ApplyItemEffect` on the target.
3. Observe the UI list while the effect is active.
4. Wait for the duration to expire.

**Expected**
1. `ItemEffectHandlerComponent` applies the speed multiplier immediately on the server.
2. If `ItemEffectComponent` is present, the effect entry appears on all clients (replicated).
3. On expiry, `ItemEffectHandlerComponent` restores the original speed and removes the effect from `ItemEffectComponent`.
4. With QA logs (`ItemSystem.QA 1`):
   1. `QA: Speed effect applied — tag: ..., multiplier: ..., base: ... -> new: ...`
   2. `QA: Speed effect expired — speed restored to ...`

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

---

## 13) ItemEffectHandlerComponent — Effect Management

This section validates `UItemEffectHandlerComponent`, the single-component effect system that replaced the previous three-layer design. Tests cover routing, timer lifecycle, re-application, Blueprint override, UI replication, and edge cases.

### 13.1 Vocabulary

- **Handler** — `UItemEffectHandlerComponent` attached to the pawn.
- **EffectComp** — `UItemEffectComponent` on the same pawn (optional, for UI).
- **BaseSpeed** — `MaxWalkSpeed` captured at the moment of the first effect application.
- **Re-apply** — calling the same effect while it is already active.

### 13.2 One-Time Setup

1. Open your Pawn BP.
2. Add `ItemEffectHandlerComponent`. Verify `SpeedEffectParentTag = Item.Effect.ModifySpeed`.
3. Implement `ApplyItemEffect`:
   ```
   HandleEffect(Effect, Context)  →  return result
   ```
4. Optional — add `ItemEffectComponent` to the same pawn for UI tests (sections 13.7–13.8).
5. Create or locate an item definition that uses `Payload_ModifySpeed`:
   - Set `SpeedMultiplier = 0.5` (slow), `Duration = 5.0`, `EffectTag = Item.Effect.ModifySpeed`.
   - Call it `DA_Item_SpeedSlow` for these tests.
6. Create a second definition `DA_Item_SpeedBoost`:
   - `SpeedMultiplier = 2.0`, `Duration = 5.0`, `EffectTag = Item.Effect.ModifySpeed`.
7. Enable QA logs: `ItemSystem.QA 1`.
8. Note the pawn's default `MaxWalkSpeed` before any test (e.g. `600`). Referenced as **BaseSpeed** below.

---

### 13.3 Basic Apply and Expiry

**Goal** — verify the effect applies and the timer correctly restores the original speed.

**Steps**
1. Console: `Cheat_GiveItem Item.Test.SpeedSlow` (or bind `DA_Item_SpeedSlow`).
2. Activate the item.
3. Immediately check `MaxWalkSpeed` (e.g. via `Print String` in pawn BP or debugger).
4. Wait 5 seconds without re-applying.
5. Check `MaxWalkSpeed` again.

**Expected**
1. On apply: `MaxWalkSpeed = BaseSpeed × 0.5` (e.g. `300`).
2. QA log: `QA: Speed effect applied — tag: Item.Effect.ModifySpeed, multiplier: 0.50, duration: 5.00, base: 600.0 -> new: 300.0`.
3. After 5 s: `MaxWalkSpeed` restored to `BaseSpeed` (`600`).
4. QA log: `QA: Speed effect expired — speed restored to 600.0`.

---

### 13.4 Re-Apply Before Expiry (Refresh)

**Goal** — verify that re-applying before expiry updates the multiplier and resets the timer, without double-capturing `BaseSpeed`.

**Steps**
1. Apply `DA_Item_SpeedSlow` (`×0.5`, 5 s).
2. After 2 s, apply `DA_Item_SpeedBoost` (`×2.0`, 5 s).
3. Check `MaxWalkSpeed` immediately after the second apply.
4. Wait 5 s for the refreshed timer to expire.
5. Check `MaxWalkSpeed` after expiry.

**Expected**
1. After second apply: `MaxWalkSpeed = BaseSpeed × 2.0` (e.g. `1200`).
2. QA log shows `base: 600.0` for the second apply (not `300.0`), confirming `BaseSpeed` was not re-captured.
3. After 5 s expiry: `MaxWalkSpeed` restored to `600` (original `BaseSpeed`).
4. No second expiry fires (single timer; the first was cancelled by the re-apply).

---

### 13.5 Permanent Effect (Duration = 0)

**Goal** — verify that `Duration = 0` creates a permanent effect with no timer.

**Steps**
1. Edit `DA_Item_SpeedSlow`: set `Duration = 0`.
2. Apply the item.
3. Wait 10 s.
4. Check `MaxWalkSpeed`.

**Expected**
1. `MaxWalkSpeed = BaseSpeed × 0.5` indefinitely.
2. No expiry log appears.
3. `bHasBaseSpeed` remains true (speed is never restored automatically).

**Cleanup** — restore `Duration = 5.0` before continuing.

---

### 13.6 Tag Routing — Child Tag

**Goal** — verify that child tags under `Item.Effect.ModifySpeed` are routed correctly via `MatchesTag`.

**Steps**
1. Edit `DA_Item_SpeedSlow`: set `EffectTag = Item.Effect.ModifySpeed.Slow` (a child tag).
2. Apply the item.

**Expected**
1. Effect applies normally — `MaxWalkSpeed` modified.
2. QA log shows tag `Item.Effect.ModifySpeed.Slow`.
3. On expiry, speed is restored correctly.

---

### 13.7 Tag Routing — Unknown Tag

**Goal** — verify that an unrecognised tag is silently ignored and `HandleEffect` returns `false`.

**Steps**
1. Create a minimal test: call `HandleEffect` directly from a debug BP with a spec whose `EffectTag = Item.Effect.Damage` (any non-speed tag).
2. Log the return value.

**Expected**
1. Return value is `false`.
2. `MaxWalkSpeed` is unchanged.
3. No crash.

---

### 13.8 Blueprint Override of ApplySpeedEffect

**Goal** — verify that a Blueprint child of `ItemEffectHandlerComponent` can override `ApplySpeedEffect`.

**Steps**
1. Create `BP_ItemEffectHandlerComponent` (child of `UItemEffectHandlerComponent`).
2. Override `ApplySpeedEffect`:
   - Call `Parent: Apply Speed Effect`.
   - Add a `Print String`: `"BP override: multiplier = [Multiplier]"`.
3. Replace the pawn's handler component with this BP version.
4. Apply `DA_Item_SpeedSlow`.

**Expected**
1. `Print String` fires with the correct multiplier.
2. The C++ base implementation still runs (speed is actually modified).
3. Expiry still restores speed correctly.

---

### 13.9 UI Tracking with ItemEffectComponent

**Goal** — verify that `UItemEffectComponent` is updated on apply and cleared on expiry.

**Prerequisites** — `ItemEffectComponent` present on the pawn.

**Steps**
1. In UI BP, bind to `OnEffectsChanged`.
2. On `OnEffectsChanged`, call `GetActiveEffects()` and log the count + tag of each entry.
3. Apply `DA_Item_SpeedSlow`.
4. Wait for expiry.

**Expected**
1. On apply: `OnEffectsChanged` fires, `GetActiveEffects()` returns 1 entry with tag `Item.Effect.ModifySpeed`.
2. On expiry: `OnEffectsChanged` fires again, `GetActiveEffects()` returns 0 entries.

---

### 13.10 UI Absent (No ItemEffectComponent)

**Goal** — verify the handler works correctly when `ItemEffectComponent` is absent.

**Steps**
1. Remove `ItemEffectComponent` from the pawn.
2. Apply `DA_Item_SpeedSlow`.
3. Wait for expiry.

**Expected**
1. Speed is modified and restored correctly.
2. No crash or warning related to the missing component.

---

### 13.11 Replication Validation

**Goal** — verify that the speed change is visible on all clients and that the UI replication is consistent.

**Setup** — PIE with 2 players (Listen Server). `ItemEffectComponent` present.

**Steps**
1. Apply effect on the **server pawn** (Player 1).
2. Observe `MaxWalkSpeed` on both windows.
3. Observe the UI on both windows (if bound).
4. Wait for expiry.

**Expected**
1. `MaxWalkSpeed` is modified on the server immediately (`HandleEffect` is server-authoritative).
2. Movement replication reflects the new speed on all clients (handled by `UCharacterMovementComponent` replication).
3. `ItemEffectComponent.ActiveEffects` replication triggers `OnEffectsChanged` on clients — UI updates on all windows.
4. On expiry, UI clears on all clients.

---

### 13.12 Non-Character Target

**Goal** — verify no crash when the handler's owner is not an `ACharacter`.

**Steps**
1. Attach `ItemEffectHandlerComponent` to a non-Character actor (e.g. a `StaticMeshActor` for the test).
2. Call `HandleEffect` with a valid speed spec.

**Expected**
1. No crash.
2. `ApplySpeedEffect` returns early silently (no `UCharacterMovementComponent` found).
3. `HandleEffect` still returns `true` (the tag was recognised; execution of the apply is the handler's concern).

---

### 13.13 Pass/Fail Checklist

Mark **PASS** only when all are true:

| # | Scenario | Result |
|---|---|---|
| 1 | Speed modified correctly on first apply | |
| 2 | Speed restored correctly on expiry | |
| 3 | Re-apply overrides multiplier without re-capturing BaseSpeed | |
| 4 | Single timer — no double-expiry on re-apply | |
| 5 | `Duration = 0` creates permanent effect | |
| 6 | Child tags routed correctly | |
| 7 | Unknown tags return `false`, no side effects | |
| 8 | Blueprint override runs alongside C++ base | |
| 9 | `ItemEffectComponent` updated on apply and cleared on expiry | |
| 10 | System works without `ItemEffectComponent` | |
| 11 | Speed change replicates correctly to clients | |
| 12 | No crash on non-Character owner | |

---

### 13.14 Common Misconfiguration

1. **Speed not modified**
   - Check `ItemEffectHandlerComponent` is on the pawn.
   - Check `ApplyItemEffect` is implemented and calls `HandleEffect`.
   - Check `SpeedEffectParentTag` is set to `Item.Effect.ModifySpeed`.
   - Check `EffectTag` on the payload matches or is a child of `SpeedEffectParentTag`.

2. **Speed not restored after expiry**
   - Check `Duration > 0` on the payload.
   - Verify the pawn was not destroyed before expiry (timer would be orphaned).

3. **BaseSpeed captured incorrectly after a re-apply**
   - This would mean `bHasBaseSpeed` was `false` at the time of the second apply.
   - Cause: a previous expiry reset `bHasBaseSpeed`. Check that the second apply happens *before* the first timer fires.

4. **UI not updating**
   - Check `ItemEffectComponent` is present and `OnEffectsChanged` is bound in the UI BP.
   - On clients: check `ItemEffectComponent` has replication enabled (`SetIsReplicated(true)` in the constructor — already the default).

---

## 14) Projectile Launch Modes (ArcThrow / Drop / ExternalVelocity)

This section validates the three launch modes introduced on `AExecution_Projectile` via `EItemLaunchMode`. Each mode is configured per Blueprint subclass and drives the initial velocity applied on spawn and on pool reuse.

### 14.1 Vocabulary

- **ArcThrow** — ballistic parabola solved by `SuggestProjectileVelocity`, aimed at the player's camera aim point.
- **Drop** — item released with minimal forward impulse; gravity handles the trajectory (intended for mines and floor grenades).
- **ExternalVelocity** — velocity supplied by an external system (animation-driven throw, charge-up, etc.) via `FItemContext::LaunchVelocity`. Falls back to ArcThrow if the field is not filled.
- **GravityScale** — multiplier applied to world gravity for this projectile (affects arc shape).
- **SpawnLoc** — actor location at the moment `ApplyLaunchMode` is called (BeginPlay or ResetForReuse).

### 14.2 One-Time Setup

1. Create three Blueprint subclasses of `AExecution_Projectile`:
   - `BP_Projectile_Arc` — `LaunchMode = ArcThrow`
   - `BP_Projectile_Drop` — `LaunchMode = Drop`
   - `BP_Projectile_External` — `LaunchMode = ExternalVelocity`
2. On all three: set `bShowDebugVisuals = true` to track trajectory visually.
3. Create corresponding item definitions pointing to each execution BP.
4. Enable QA logs: `ItemSystem.QA 1`.
5. Run PIE with 2 players (Listen Server) to also validate replication.
6. Ensure the pawn has a valid `GetSocketByTag` or the projectile will spawn at the actor origin.

---

### 14.3 ArcThrow — Baseline

**Goal** — verify the projectile follows a parabolic arc toward the camera aim point.

**Setup**
1. Open `BP_Projectile_Arc`.
2. Set `Speed = 2000`, `GravityScale = 1.0`, `bFavorHighArc = false`, `ArcTraceDistance = 5000`.

**Steps**
1. Console: `Cheat_GiveItem Item.Test.ArcThrow` (or the matching item tag).
2. Aim at a visible surface 1000–3000 units away.
3. Activate the item.
4. Repeat aiming at targets at different distances (close, mid, far).

**Expected**
1. Projectile leaves the spawn location and curves toward the aim point.
2. At mid/far distance the arc is visually parabolic, not straight.
3. Projectile impacts near the aimed surface.
4. No QA fallback log (`ArcThrow could not solve ballistic path...`) appears for reachable targets.

---

### 14.4 ArcThrow — High Arc vs Low Arc

**Goal** — verify `bFavorHighArc` selects the correct ballistic solution when two exist.

**Steps**
1. With `bFavorHighArc = false`: fire at a mid-distance target. Note trajectory.
2. Change `BP_Projectile_Arc` to `bFavorHighArc = true`. Refire at the same target.

**Expected**
1. `bFavorHighArc = false` — projectile takes the flatter, faster path.
2. `bFavorHighArc = true` — projectile takes the higher, looping path to the same target.

---

### 14.5 ArcThrow — Unreachable Target Fallback

**Goal** — verify graceful fallback when `SuggestProjectileVelocity` cannot find a solution.

**Setup**
1. Set `Speed = 100` (very low) on `BP_Projectile_Arc`.
2. Aim at a target very far away or directly above.

**Steps**
1. Activate the item.
2. Observe trajectory and output log.

**Expected**
1. Projectile still fires — it shoots directly toward the aim direction.
2. QA log: `QA: ArcThrow could not solve ballistic path to aim point — using direct aim.`
3. No crash.

**Cleanup** — restore `Speed = 2000`.

---

### 14.6 ArcThrow — GravityScale

**Goal** — verify `GravityScale` affects arc curvature.

**Steps**
1. Fire with `GravityScale = 1.0`, note arc shape.
2. Change to `GravityScale = 0.3` (floaty), refire at the same distance.
3. Change to `GravityScale = 2.5` (heavy), refire.

**Expected**
1. Lower `GravityScale` → broader, higher arc for the same speed and aim point.
2. Higher `GravityScale` → tighter, lower arc.
3. In all cases the projectile still reaches near the same aim point (the solver compensates).

---

### 14.7 ArcThrow — No Player Controller Fallback

**Goal** — verify behaviour when `InstigatorController` is not a `APlayerController` (AI or missing controller).

**Steps**
1. Temporarily null the controller in a debug BP (or test with an AI pawn that has no `APlayerController`).
2. Fire the item.

**Expected**
1. Projectile fires forward at full `Speed` along the pawn's forward vector.
2. No crash.

---

### 14.8 Drop — Pure Gravity Fall

**Goal** — verify the item falls vertically with no forward drift when `DropForwardImpulse = 0`.

**Setup**
1. Open `BP_Projectile_Drop`.
2. Set `DropForwardImpulse = 0`, `GravityScale = 1.0`.

**Steps**
1. Stand still, look straight ahead.
2. Console: `Cheat_GiveItem Item.Test.Drop`.
3. Activate the item.

**Expected**
1. Projectile spawns at the socket location and falls straight down (no forward motion).
2. Lands near the pawn's feet.
3. Debug sphere drops vertically without drifting.

---

### 14.9 Drop — Forward Impulse

**Goal** — verify `DropForwardImpulse` adds a nudge in the pawn's facing direction.

**Steps**
1. Set `DropForwardImpulse = 300` on `BP_Projectile_Drop`.
2. Face a clear direction. Activate the item.
3. Increase to `DropForwardImpulse = 800`, repeat.

**Expected**
1. Projectile travels slightly forward before falling; distance scales with impulse value.
2. Trajectory remains dominated by gravity — this is not a throw, just a nudge.

---

### 14.10 ExternalVelocity — Velocity Provided

**Goal** — verify the projectile uses `FItemContext::LaunchVelocity` when `bHasExternalLaunchVelocity = true`.

**Setup**
1. In a test Blueprint (GameState or debug actor), build `FItemContext` manually:
   - `bHasExternalLaunchVelocity = true`
   - `LaunchVelocity = FVector(1000, 0, 500)` (forward + upward)
   - Fill `Instigator`, `InstigatorController`, `ItemDefinition`.
2. Call `UItemSystemManager::Get(World)->SpawnItemExecution(Context)` directly (bypassing `Server_TryActivateItem`).

**Steps**
1. Trigger the context injection from the debug BP (server side).
2. Observe projectile direction.

**Expected**
1. Projectile immediately travels in the direction of `LaunchVelocity (1000, 0, 500)` — forward and slightly up.
2. No ArcThrow computation runs.
3. No warning log.

---

### 14.11 ExternalVelocity — Missing Velocity Fallback

**Goal** — verify fallback to ArcThrow when `bHasExternalLaunchVelocity = false`.

**Setup**
1. Same debug BP as 14.10 but set `bHasExternalLaunchVelocity = false` (leave `LaunchVelocity` at zero).

**Steps**
1. Trigger the context.
2. Observe projectile and output log.

**Expected**
1. Projectile fires using ArcThrow logic (ballistic arc toward camera aim point).
2. Output log: `Warning: Execution_Projectile ...: ExternalVelocity mode but context has no launch velocity. Falling back to ArcThrow.`
3. No crash.

---

### 14.12 Pool Reuse — Velocity Reset

**Goal** — verify that a reused pooled projectile acquires a fresh velocity (not the previous one).

**Setup**
1. Use `BP_Projectile_Arc` with `MaxPoolSizePerClass ≥ 1` on the manager.
2. Enable QA logs.

**Steps**
1. Fire once — projectile hits something and returns to the pool.
   - Log: `QA: Added actor to pool ...`
2. Rotate the pawn 90° to face a different direction.
3. Fire again.
   - Log: `QA: Reusing pooled actor ...`
4. Observe the second projectile's direction.

**Expected**
1. Second projectile flies toward the new aim direction, not the previous one.
2. `ResetForReuse` log confirms `StopMovementImmediately` ran and `ApplyLaunchMode` was called again.
3. No residual velocity from the first shot.

---

### 14.13 Lifespan — Auto-Destroy After 10 s

**Goal** — verify the projectile self-destructs after 10 seconds if it never hits anything.

**Steps**
1. Fire `BP_Projectile_Arc` into open air (no target).
2. Wait 10 seconds.

**Expected**
1. Projectile actor is destroyed after ~10 s.
2. Pool entry is not added (actor destroys itself, not returned via `FinishExecution`).
3. No crash.

---

### 14.14 Instigator Self-Collision

**Goal** — verify the projectile does not trigger on its own instigator.

**Steps**
1. Fire `BP_Projectile_Arc` directly downward at the pawn's feet.
2. Observe whether the payload fires on the instigator.

**Expected**
1. Instigator is in `MoveIgnoreActors` — no hit/overlap triggers on them.
2. Payload is not applied to the instigator.

---

### 14.15 Replication

**Goal** — verify projectile movement and impact replicate correctly to all clients.

**Setup** — PIE 2 players, Listen Server. Player 1 is server.

**Steps**
1. Player 1 fires `BP_Projectile_Arc` at Player 2.
2. Observe both windows.

**Expected**
1. Projectile appears and moves on both server and client windows (`SetReplicateMovement(true)`).
2. Impact VFX/SFX play on both sides (NetMulticast Unreliable).
3. Payload applies once — `bHasExploded` guard prevents double application.

---

### 14.16 Pass/Fail Checklist

| # | Scenario | Result |
|---|---|---|
| 1 | ArcThrow reaches aimed target with visible parabolic arc | |
| 2 | `bFavorHighArc` selects correct ballistic solution | |
| 3 | ArcThrow falls back to direct aim for unreachable targets + QA log | |
| 4 | `GravityScale` changes arc curvature while still reaching aim point | |
| 5 | No PlayerController → fires forward, no crash | |
| 6 | Drop `DropForwardImpulse = 0` → pure vertical fall | |
| 7 | Drop with impulse → nudged forward, gravity dominant | |
| 8 | ExternalVelocity uses `LaunchVelocity` when provided | |
| 9 | ExternalVelocity falls back to ArcThrow + warning when field missing | |
| 10 | Pool reuse applies fresh velocity, not stale one | |
| 11 | Projectile auto-destroys after 10 s lifespan | |
| 12 | Instigator not affected by own projectile | |
| 13 | Movement and impact replicate to all clients | |

---

### 14.17 Common Misconfiguration

1. **Projectile flies backward or in wrong direction**
   - Check `GetSocketByTag` returns a socket that points in the correct forward direction.
   - For ExternalVelocity: verify `LaunchVelocity` is in world space, not local space.

2. **ArcThrow always falls back to direct aim**
   - `Speed` is too low for the target distance. Increase `Speed` or reduce `ArcTraceDistance`.
   - Target is directly above the instigator (no ballistic solution exists for vertical shots at low speed).

3. **Drop doesn't fall (floats or drifts)**
   - Check `GravityScale > 0` on the projectile.
   - Check world gravity is not overridden to zero in the level's WorldSettings.

4. **ExternalVelocity projectile moves at wrong speed**
   - `MaxSpeed` is clamped to `Speed` by default. The system auto-raises `MaxSpeed` if `LaunchVelocity.Size() > MaxSpeed`, but verify this ran correctly by checking logs.

5. **Second pool reuse keeps old velocity**
   - `ResetForReuse` calls `StopMovementImmediately()` then `ApplyLaunchMode()`. If a BP child of `Execution_Projectile` overrides `ResetForReuse` without calling `Super`, neither runs. Ensure `Super::ResetForReuse()` is called.
