# ItemSystem QA Checklist

This checklist validates the features implemented in the ItemSystem plugin.

## Global Setup (PIE Multiplayer)
1. Play -> Advanced Settings.
2. Number of Players = 2.
3. Net Mode = Play As Listen Server.
4. Open Output Log.
5. Ensure:
   - GameState has `ItemSystemManager`.
   - Pawn has `InventoryComponent`.
6. Optional: enable `bShowDebugVisuals` on execution BPs.

---

## 1) Usage Validation (Cooldown + Blocking Tags)
**Setup**
- In `DA_Item_SkillShot`, set `Cooldown = 2.0`.
- Add `UsageBlockingTags = State.Status.Stunned`.
- Pawn implements `HasGameplayTag`.

**Steps**
1. `Cheat_GiveItem Item.Test.SkillShot`
2. Activate twice quickly.
3. Apply `State.Status.Stunned` to the pawn.
4. Try to activate again.

**Expected**
- Second activation is blocked by cooldown.
- Activation blocked when Stunned.

---

## 2) Team Filtering + Immunity
**Setup**
- Add `Rule.Ignore.Teammates` to the item `IdentityTags`.
- Pawns implement `GetTeamID`.
- Targets implement `IsImmuneTo(ContextTags)`.

**Steps**
1. Player1 fires at Player2 on same team.
2. Enable immunity in target for matching tags.
3. Fire again.

**Expected**
- No payload applied to teammates.
- No payload applied to immune targets.

---

## 3) Spawn Point via Socket
**Setup**
- Pawn implements `GetSocketByTag`.
- `DA_Item_*` uses `AttachSocketTag = Socket.Mount.HandRight` (or equivalent).

**Steps**
1. Fire item.

**Expected**
- Projectile spawns from the socket (not actor origin).

---

## 4) Distribution Policies
**Setup**
- Create a BP from `UDistributionPolicy_TagQuery` with a valid query.
- Ensure `GlobalItemRegistry` has multiple items.

**Steps**
1. Call `GetItemByPolicy` (BP or debug).

**Expected**
- Returns a random item that matches the query.

---

## 5) Cheat TagQuery Parsing
**Steps**
1. `Cheat_GiveItem Item.Test.SkillShot`
2. `Cheat_GiveItem Item.Test.SkillShot AND NOT Item.Test.Debug`
3. `Cheat_GiveItem (Item.Test.SkillShot OR Item.Test.Speed.Boost)`

**Expected**
- Item granted for valid queries.
- Warning for invalid queries.

---

## 6) Pooling of Execution Actors
**Setup**
- Use projectile or trap execution.

**Steps**
1. Fire 5+ times.
2. Observe logs or set breakpoints on `ReleaseExecutionActor`.

**Expected**
- Actors are reused instead of destroyed.
- No crashes or leaks.

---

## 7) Impact VFX/SFX
**Setup**
- Set `ImpactVFX` and `ImpactSound` in the execution BP.

**Steps**
1. Fire and hit a wall/target.

**Expected**
- VFX + SFX play at impact location.

---

## 8) Cheat_ClearInventory
**Steps**
1. `Cheat_GiveItem Item.Test.SkillShot`
2. `Cheat_ClearInventory`

**Expected**
- Item removed, ammo = 0.

---

## 9) Cheat_SimulateImpact
**Setup**
- Item has a payload class.

**Steps**
1. Aim at a target.
2. `Cheat_SimulateImpact Item.Test.SkillShot`

**Expected**
- Payload applied directly to hit actor.

---

## 10) UI Feedback (Active Effects)
**Setup**
- Pawn has `UItemEffectComponent`.
- UI binds to `OnEffectsChanged` and reads `GetActiveEffects()`.
- Use `Payload_ModifySpeed` to add effects.

**Steps**
1. Apply Boost/Slow to target.

**Expected**
- Effect appears on clients.
- Duration counts down and effect disappears at expiry.

