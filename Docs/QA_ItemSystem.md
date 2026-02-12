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
   3. Open your Pawn BP (for example `BP_ItemCharacter`).
   4. Add `InventoryComponent` if missing.
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
3. Ensure `GlobalItemRegistry` has items with that tag.

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

