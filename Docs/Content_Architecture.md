# Content Architecture — Plugin ItemSystem

Ce document décrit la structure complète du dossier `Content/` du plugin.
L'objectif est qu'il soit **autonome** : tout ce qu'il faut pour un fonctionnement
game-ready ou de démo est contenu dans le plugin.

Les Gameplay Tags sont déclarés via un **DataTable asset** (`DT_GameplayTags`) plutôt
que via `Config/DefaultGameplayTags.ini`. Raison : un `.ini` de plugin est fusionné
automatiquement avec le `.ini` du projet hôte par UE, ce qui crée des conflits de merge
dès que le projet hôte a ses propres entrées dans ce même fichier. Un `.uasset` binaire
ne peut pas provoquer de conflit de merge. La seule contrepartie est une action unique
de la part de l'intégrateur : ajouter `DT_GameplayTags` dans
**Project Settings > GameplayTags > Gameplay Tag Table List**.

---

## Arbre des fichiers du plugin

```
Content/ItemSystem/
│
├── Core/                               ← Acteurs Framework (GameMode, GameState, Pawn…)
│   ├── BP_GameMode_ItemDemo
│   ├── BP_GameState_ItemDemo
│   ├── BP_PlayerController_ItemDemo
│   └── BP_Pawn_ItemDemo
│
├── Data/                               ← DataTable registre + configs globales
│   ├── DT_GameplayTags                  ← tags du plugin (FGameplayTagTableRow) — à référencer dans Project Settings
│   ├── DT_ItemRegistry
│   └── DA_SpawnRoutingConfig
│
├── Items/                              ← Un sous-dossier par item (DA + ses stratégies)
│   ├── SkillShot/
│   │   ├── DA_Item_SkillShot
│   │   ├── BP_Execution_Projectile_SkillShot
│   │   └── BP_Payload_Explosion_SkillShot
│   │
│   ├── SpeedBoost_Projectile/
│   │   ├── DA_Item_SpeedBoost
│   │   └── BP_Payload_ModifySpeed_Boost
│   │
│   ├── SpeedSlow_Projectile/
│   │   ├── DA_Item_SpeedSlow
│   │   └── BP_Payload_ModifySpeed_Slow
│   │
│   ├── Trap_Explosion/
│   │   ├── DA_Item_Trap_Explosion
│   │   ├── BP_Execution_Trap_Explosion
│   │   └── BP_Payload_Explosion_Trap
│   │
│   ├── SelfSpeedBoost/
│   │   ├── DA_Item_SelfSpeedBoost
│   │   └── BP_Payload_SelfSpeedBoost
│   │
│   └── EnemySlow/
│       ├── DA_Item_EnemySlow
│       ├── BP_Targeting_Raycast_EnemySlow
│       └── BP_Payload_EnemySlow
│
├── UI/                                 ← Widgets HUD
│   ├── WBP_HUD_Demo
│   ├── WBP_ItemSlot
│   └── WBP_EffectBar
│
└── Maps/
    └── L_Demo
```

---

## Principe d'organisation

| Règle | Raison |
|---|---|
| **`Data/DT_GameplayTags`** | `.uasset` binaire → pas de conflit de merge ; une seule action projet : l'ajouter dans Project Settings > GameplayTag Table List |
| **Un item = un dossier** dans `Items/` | Supprimer ou copier un item = une seule opération dans le Content Browser |
| **Stratégies à côté de leur DA** | Pas de dossier `Payloads/` global — chaque item est indépendant |
| **Framework dans `Core/`** | Séparation claire entre infrastructure et contenu |
| **Data assets globaux dans `Data/`** | Le DataTable et la config de routing sont partagés par tous les items |

---

## Ordre de création recommandé

```
1. Data/DT_GameplayTags              ← créer en premier ; ajouter dans Project Settings avant d'ouvrir les DA
2. Data/DT_ItemRegistry              ← doit exister avant d'être référencé par BP_GameState
3. Data/DA_SpawnRoutingConfig        ← idem
4. Items/**/BP_Payload_*             ← les payloads n'ont pas de dépendance sur le reste
5. Items/**/BP_Execution_*           ← les exécutions n'ont pas de dépendance sur les DA
6. Items/**/BP_Targeting_*           ← idem
7. Items/**/DA_Item_*                ← référencent les BPs ci-dessus + les remplir
8. DT_ItemRegistry                   ← ajouter les rows pointant vers les DA
9. Core/BP_Pawn_ItemDemo             ← dépend de rien, mais fait référence à InventoryComponent
10. Core/BP_PlayerController_ItemDemo ← dépend de rien
11. Core/BP_GameState_ItemDemo       ← référence DT_ItemRegistry + DA_SpawnRoutingConfig
12. Core/BP_GameMode_ItemDemo        ← référence les 3 BPs Core ci-dessus
13. UI/WBP_ItemSlot                  ← bind à InventoryComponent.OnInventoryChanged
14. UI/WBP_EffectBar                 ← bind à ItemEffectComponent.OnEffectsChanged
15. UI/WBP_HUD_Demo                  ← compose WBP_ItemSlot + WBP_EffectBar
16. Maps/L_Demo                      ← assigne BP_GameMode_ItemDemo en WorldSettings
```

---

## Fiche détaillée — Data/DT_GameplayTags

**Emplacement** : `Content/ItemSystem/Data/DT_GameplayTags`
**Type** : `DataTable`
**Row Structure** : `GameplayTagTableRow` *(struct built-in UE5, module `GameplayTags`)*

### Pourquoi un DataTable plutôt que Config/DefaultGameplayTags.ini ?

Un plugin peut déclarer ses tags via `Config/DefaultGameplayTags.ini`, mais UE fusionne
ce fichier avec le `DefaultGameplayTags.ini` du **projet hôte**. Dès que le projet hôte
a ses propres entrées dans ce fichier, tout merge de branche ou mise à jour du plugin
crée un conflit de merge sur un fichier texte généré — difficile à résoudre sans perdre
des tags.

Un `.uasset` binaire ne peut pas provoquer de conflit de merge : Git le traite comme un
fichier opaque, LFS le versionne sans diff ligne à ligne. La seule contrepartie est une
**action unique** de la part de l'intégrateur.

| | `Config/DefaultGameplayTags.ini` | `DT_GameplayTags` (DataTable) |
|---|---|---|
| Conflits de merge | ⚠️ Oui, sur fichier texte partagé | ✅ Non, binaire opaque |
| Config projet requise | ✅ Aucune | ⚠️ Une fois : ajouter dans Project Settings |
| Lisible sans l'éditeur | ✅ Oui | ❌ Non |
| Rechargeable à chaud | ✅ Oui | ✅ Oui (DataTable reimport) |

### Setup — action unique côté projet hôte

```
Project Settings
  └─ GameplayTags
       └─ Gameplay Tag Table List
            └─ [+] /ItemSystem/Data/DT_GameplayTags
```

Cette référence est sauvegardée dans `Config/DefaultEngine.ini` du projet hôte,
**pas** dans le fichier du plugin — aucun conflit possible.

### Contenu de la DataTable

| Row Name | Tag | DevComment |
|---|---|---|
| `Item_Test_SkillShot` | `Item.Test.SkillShot` | QA — projectile à effet d'explosion |
| `Item_Test_Guided` | `Item.Test.Guided` | QA — projectile guidé |
| `Item_Test_Speed_Boost` | `Item.Test.Speed.Boost` | QA — boost de vitesse via projectile |
| `Item_Test_Speed_Slow` | `Item.Test.Speed.Slow` | QA — slow de vitesse via projectile |
| `Item_Test_Speed_Boost_Self` | `Item.Test.Speed.Boost.Self` | QA — buff vitesse sur soi (DirectApply, InstigatorOnly) |
| `Item_Test_Speed_Slow_Enemy` | `Item.Test.Speed.Slow.Enemy` | QA — slow ennemi ciblé (DirectApply + Raycast) |
| `Item_Test_Trap_Explosion` | `Item.Test.Trap.Explosion` | QA — mine à explosion |
| `Item_Test_Debug` | `Item.Test.Debug` | QA — item de debug générique |
| `Item_Effect_ModifySpeed` | `Item.Effect.ModifySpeed` | Parent tag — utilisé avec HasParentTag dans le handler |
| `Item_Effect_ModifySpeed_Boost` | `Item.Effect.ModifySpeed.Boost` | Boost de vitesse actif (SpeedMultiplier > 1) |
| `Item_Effect_ModifySpeed_Slow` | `Item.Effect.ModifySpeed.Slow` | Slow de vitesse actif (SpeedMultiplier < 1) |
| `Rule_Ignore_Teammates` | `Rule.Ignore.Teammates` | L'item ne peut pas affecter les alliés de l'instigateur |
| `Rule_Block_Usage` | `Rule.Block.Usage` | Bloque l'utilisation d'item sur le pawn porteur |
| `State_Status_Stunned` | `State.Status.Stunned` | Pawn étourdi — bloque l'activation via CanUseItem |
| `Socket_Mount_HandRight` | `Socket.Mount.HandRight` | Socket main droite pour les projectiles |
| `Socket_Mount_Muzzle` | `Socket.Mount.Muzzle` | Socket canon pour le spawn de projectile |
| `Socket_Mount_Roof` | `Socket.Mount.Roof` | Socket toit (ex : véhicule) |
| `Routing_BuffReceiver` | `Routing.BuffReceiver` | Acteur éligible à recevoir un buff via SearchByRules |

### Règles de maintenance

| Règle | Raison |
|---|---|
| Ajouter une row ici avant d'utiliser le tag dans un DA ou un BP | Évite les warnings `Unknown Tag` au chargement |
| Ne jamais re-déclarer ces tags dans `Config/DefaultGameplayTags.ini` | Double déclaration = warning UE à chaque démarrage |
| Préfixer les tags de démo par `Item.Test.*` | Facilite le nettoyage si le projet hôte veut ses propres items |
| Mettre à jour la DataTable en même temps que les `IdentityTags` d'un `DA_Item_*` | Cohérence garantie |

---

## Fiches détaillées — Core/

### BP_GameMode_ItemDemo
**Parent** : `GameModeBase`

| Propriété | Valeur |
|---|---|
| Default Pawn Class | `BP_Pawn_ItemDemo` |
| Player Controller Class | `BP_PlayerController_ItemDemo` |
| Game State Class | `BP_GameState_ItemDemo` |
| HUD Class | *(optionnel — le HUD peut être créé par le PlayerController)* |

> Le GameMode ne fait que câbler les classes. Aucune logique métier dedans.

---

### BP_GameState_ItemDemo
**Parent** : `GameStateBase`

**Composant à ajouter** : `UItemSystemManager` (nommé `ItemSystemManager`)

Configuration du composant `ItemSystemManager` :

| Propriété | Valeur |
|---|---|
| `ItemRegistryDataTable` | `DT_ItemRegistry` |
| `bEnableWorldSpawnManagement` | `true` |
| `bAutoDiscoverSpawnPoints` | `true` |
| `WorldSpawnDistributionPolicy` | `DistributionPolicy_Random` (instancier inline) |
| `WorldSpawnRefreshMode` | `TimedInfinite` |
| `WorldSpawnRefreshInterval` | `30.0` |
| `bRefreshOnConsume` | `true` |
| `SpawnPointPickupRoutingConfig` | `DA_SpawnRoutingConfig` |

> `UItemSystemManager::Get(WorldContextObject)` retrouve ce composant depuis n'importe où
> via `GetWorld()->GetGameState()->GetComponentByClass<UItemSystemManager>()`.

---

### BP_PlayerController_ItemDemo
**Parent** : `PlayerController`

| Propriété | Valeur |
|---|---|
| `Cheat Manager Class` | `ItemCheatManager` |

**Event Graph** — Brancher l'input d'activation :

```
InputAction "UseItem" (pressed)
  └─ Get Controlled Pawn
       └─ Get Component by Class (UInventoryComponent)
            └─ Server_TryActivateItem()
```

> Créer un Input Action `IA_UseItem` dans le projet ou dans le plugin (dossier `Input/`).
> Mapper sur la touche souhaitée (ex : `E` ou `Left Mouse Button`).

---

### BP_Pawn_ItemDemo
**Parent** : `Character`

**Interfaces à implémenter** : `IItemInterface`

**Composants à ajouter** :

| Nom du composant | Classe C++ | Notes |
|---|---|---|
| `InventoryComponent` | `UInventoryComponent` | — |
| `ItemEffectHandlerComponent` | `UItemEffectHandlerComponent` | Configurer `SpeedEffectParentTag = Item.Effect.ModifySpeed` |
| `ItemEffectComponent` | `UItemEffectComponent` | Optionnel — pour le suivi UI des effets actifs |

**Implémentation de IItemInterface** (onglet *My Blueprint > Interfaces*) :

| Fonction | Implémentation BP |
|---|---|
| `GetTeamID` | Retourner une variable `TeamID` (integer, exposée) |
| `HasGameplayTag` | Comparer le tag reçu avec un `GameplayTagContainer` local (`OwnedTags`) |
| `GetItemInstigatorController` | `Get Controller` → cast vers `AController` |
| `GetSocketByTag` | Switch/Map sur le tag → retourner le Mesh + `FName` du socket |
| `ApplyItemEffect` | `ItemEffectHandlerComponent → HandleEffect(Effect, Context)` → return `true` |

**`GetSocketByTag` — configuration minimale pour les projectors** :

| SocketTag | Composant | Socket |
|---|---|---|
| `Socket.Mount.HandRight` | `Mesh` (SkeletalMesh) | `hand_r` |
| `Socket.Mount.Muzzle` | `Mesh` | `Muzzle` |

> Si le pawn n'utilise pas de projectile, cette fonction peut retourner `(null, "None")` sans casser le système.

**Event Graph** :
- Lier `InventoryComponent.OnInventoryChanged` → notifier le HUD (broadcast ou référence directe).

---

## Fiches détaillées — Data/

### DT_ItemRegistry
**Type** : `DataTable`
**Row Structure** : `ItemDefinitionTableRow` (struct C++ du plugin)

Chaque ligne = un item disponible dans la session.

| Row Name | ItemDefinition |
|---|---|
| `Row_SkillShot` | `DA_Item_SkillShot` |
| `Row_SpeedBoost` | `DA_Item_SpeedBoost` |
| `Row_SpeedSlow` | `DA_Item_SpeedSlow` |
| `Row_Trap_Explosion` | `DA_Item_Trap_Explosion` |
| `Row_SelfSpeedBoost` | `DA_Item_SelfSpeedBoost` |
| `Row_EnemySlow` | `DA_Item_EnemySlow` |

> Le Row Name n'est pas lu par le système — seul `ItemDefinition` (soft ref) compte.

---

### DA_SpawnRoutingConfig
**Type** : `DataAsset`
**Classe** : `ItemSpawnPointRoutingConfig`

Configuration propagée automatiquement à tous les `AItemSpawnPoint` du niveau via le manager.

| Propriété | Valeur démo |
|---|---|
| `PickupMethod` | `TriggerOverlap` |
| `bConsumeOnSuccessfulGrant` | `true` |
| `bRequestImmediateRespawnOnConsume` | `true` |
| `RecipientPolicy` | `OverlappingActorOnly` |
| `bOverrideItemGrantAmount` | `false` |

---

## Fiches détaillées — Items/

> Pour les valeurs exactes de chaque item, voir `Docs/Presets_ItemDefinitions.md`.
> Ce tableau résume le câblage ExecutionClass / TargetingClass / PayloadClass pour chaque DA.

| DA | ExecutionClass | TargetingClass | PayloadClass | Routing |
|---|---|---|---|---|
| `DA_Item_SkillShot` | `BP_Execution_Projectile_SkillShot` | — | `BP_Payload_Explosion_SkillShot` | *(projectile, pas de routing direct)* |
| `DA_Item_SpeedBoost` | `BP_Execution_Projectile_Speed` | — | `BP_Payload_ModifySpeed_Boost` | *(projectile)* |
| `DA_Item_SpeedSlow` | `BP_Execution_Projectile_Speed` | — | `BP_Payload_ModifySpeed_Slow` | *(projectile)* |
| `DA_Item_Trap_Explosion` | `BP_Execution_Trap_Explosion` | — | `BP_Payload_Explosion_Trap` | *(trap, pas de routing direct)* |
| `DA_Item_SelfSpeedBoost` | `Execution_DirectApply` *(C++)* | — | `BP_Payload_SelfSpeedBoost` | `InstigatorOnly` |
| `DA_Item_EnemySlow` | `Execution_DirectApply` *(C++)* | `BP_Targeting_Raycast_EnemySlow` | `BP_Payload_EnemySlow` | `TargetingResultOnly` |

### Stratégies partagées entre items

`BP_Execution_Projectile_Speed` est utilisé à la fois par `DA_Item_SpeedBoost` et `DA_Item_SpeedSlow`.
Il vit dans `Items/SpeedBoost_Projectile/` — `DA_Item_SpeedSlow` y pointe par soft reference.
Alternative : créer une copie dans `Items/SpeedSlow_Projectile/` pour garder l'indépendance totale.

---

## Fiches détaillées — UI/

### WBP_ItemSlot
**Parent** : `UserWidget`

**Variables exposées** :
- `InventoryRef` (UInventoryComponent, `Bind` ou `Set` depuis le HUD parent)

**Widgets** :

| Widget | Binding |
|---|---|
| `Image_Icon` | `InventoryRef.GetCurrentItem().Visuals.Icon` |
| `Text_Ammo` | `InventoryRef.GetCurrentAmmo()` |
| `ProgressBar_Cooldown` | `InventoryRef.GetCooldownProgress()` (0=prêt, 1=rechargé) |

**Logique** :
- Lier `InventoryComponent.OnInventoryChanged` → `SetItemSlotData(Item, Ammo)` pour rafraîchir l'UI.
- Le `ProgressBar_Cooldown` peut être animé par tick ou par binding direct.

> `GetCooldownProgress()`, `GetCooldownRemainingTime()`, `IsOnCooldown()` sont disponibles en BP.

---

### WBP_EffectBar
**Parent** : `UserWidget`

**Variables** :
- `EffectComponentRef` (UItemEffectComponent)

**Logique** :
- Lier `ItemEffectComponent.OnEffectsChanged` → `RefreshEffectList()`
- `RefreshEffectList()` appelle `GetActiveEffects()` → boucle pour créer/détruire des tuiles d'effet.

**Tuile d'effet** (widget enfant optionnel `WBP_EffectTile`) :
- `Image_Icon` (selon EffectTag → table de mapping tag→texture)
- `ProgressBar_Duration` → `FItemActiveEffect.GetNormalizedProgress(GetGameTimeSinceCreation())`
- `Text_Remaining` → `FItemActiveEffect.GetRemainingTime(...)`

> Les helpers `GetNormalizedProgress` et `GetRemainingTime` sont exposés via `UItemEffectBlueprintLibrary`.

---

### WBP_HUD_Demo
**Parent** : `UserWidget`

**Composition** :
- `WBP_ItemSlot` (ancré en bas au centre ou en bas à droite)
- `WBP_EffectBar` (ancré en haut à gauche)

**Event BeginPlay (ou `OnInitialized`)** :
```
Get Owning Player → Get Pawn
  ├─ Get Component (UInventoryComponent) → WBP_ItemSlot.SetInventoryRef(...)
  └─ Get Component (UItemEffectComponent) → WBP_EffectBar.SetEffectComponentRef(...)
```

**Ajout au viewport** : dans `BP_PlayerController_ItemDemo.BeginPlay` :
```
Create Widget (WBP_HUD_Demo) → Add to Viewport
```

---

## Fiches détaillées — Maps/

### L_Demo
**Type** : Level

**WorldSettings** :
- `GameMode Override` → `BP_GameMode_ItemDemo`

**Contenu minimal** :
- 1 `PlayerStart`
- 3-5 `AItemSpawnPoint` placés dans le niveau
  - Chaque spawn point enregistré automatiquement au `BeginPlay` du manager (`bAutoDiscoverSpawnPoints = true`)
  - Aucune config manuelle nécessaire si `DA_SpawnRoutingConfig` est configuré

**Éclairage** : `DirectionalLight` + `SkyAtmosphere` (niveau par défaut d'UE5 suffit).

---

## Diagramme de câblage global

```
L_Demo (WorldSettings.GameMode = BP_GameMode_ItemDemo)
│
├─ BP_GameMode_ItemDemo
│    ├─ DefaultPawnClass      → BP_Pawn_ItemDemo
│    ├─ PlayerControllerClass → BP_PlayerController_ItemDemo
│    └─ GameStateClass        → BP_GameState_ItemDemo
│
├─ BP_GameState_ItemDemo
│    └─ UItemSystemManager
│         ├─ ItemRegistryDataTable  → DT_ItemRegistry
│         │    └─ rows → DA_Item_* (× 6)
│         │              └─ ExecutionClass / TargetingClass / PayloadClass → BP_*
│         └─ SpawnPointPickupRoutingConfig → DA_SpawnRoutingConfig
│              └─ propagé à → AItemSpawnPoint (× N, placés dans L_Demo)
│
├─ BP_PlayerController_ItemDemo
│    ├─ CheatManagerClass → ItemCheatManager
│    ├─ Input "UseItem" → InventoryComponent.Server_TryActivateItem()
│    └─ BeginPlay → Create WBP_HUD_Demo → Add to Viewport
│
└─ BP_Pawn_ItemDemo  (implements IItemInterface)
     ├─ UInventoryComponent          → Server_GrantItem / Server_TryActivateItem
     │    └─ OnInventoryChanged      → WBP_ItemSlot (refresh icon/ammo/cooldown)
     ├─ UItemEffectHandlerComponent  → HandleEffect() [SpeedEffectParentTag configuré]
     │    └─ ApplySpeedEffect()      → modifie CharacterMovement.MaxWalkSpeed
     └─ UItemEffectComponent         → ActiveEffects (répliqué)
          └─ OnEffectsChanged        → WBP_EffectBar (refresh liste)
```

---

## Gameplay Tags

Tous les tags sont déclarés dans **`Data/DT_GameplayTags`** (voir section dédiée ci-dessus).
L'intégrateur les active en une seule fois en ajoutant ce DataTable dans
**Project Settings > GameplayTags > Gameplay Tag Table List**.

> Ajouter un nouveau tag = ajouter une row dans `DT_GameplayTags`.
> Ne pas déclarer les tags du plugin dans `Config/DefaultGameplayTags.ini` du projet hôte.

---

## Checklist de validation complète

### Tags & Config
- [ ] `Data/DT_GameplayTags` référencé dans **Project Settings > GameplayTags > Gameplay Tag Table List**
- [ ] Aucun tag du plugin déclaré dans `Config/DefaultGameplayTags.ini` du projet hôte
- [ ] Après ajout dans Project Settings, ouvrir le GameplayTag Browser → tous les tags `Item.*`, `Rule.*`, `State.*`, `Socket.*`, `Routing.*` sont listés avec la source `DT_GameplayTags`

### Infrastructure
- [ ] `BP_GameMode_ItemDemo` assigné dans WorldSettings de `L_Demo`
- [ ] `UItemSystemManager` présent sur `BP_GameState_ItemDemo` avec `DT_ItemRegistry` référencé
- [ ] `DT_ItemRegistry` contient les 6 rows pointant vers des DA valides
- [ ] `ItemCheatManager` assigné dans `BP_PlayerController_ItemDemo`

### Pawn
- [ ] `BP_Pawn_ItemDemo` implémente `IItemInterface` (5 fonctions)
- [ ] `UInventoryComponent`, `UItemEffectHandlerComponent`, `UItemEffectComponent` présents
- [ ] `SpeedEffectParentTag = Item.Effect.ModifySpeed` configuré sur `ItemEffectHandlerComponent`

### Items
- [ ] `Cheat_GiveItem Item.Test.SkillShot` → projectile tiré et explosion visible
- [ ] `Cheat_GiveItem Item.Test.Speed.Boost` → projectile qui bouste la cible touchée
- [ ] `Cheat_GiveItem Item.Test.Speed.Slow` → projectile qui slow la cible touchée
- [ ] `Cheat_GiveItem Item.Test.Trap.Explosion` → mine posée, explose au contact
- [ ] `Cheat_GiveItem Item.Test.Speed.Boost.Self` → vitesse augmente immédiatement, revient après 5 s
- [ ] `Cheat_GiveItem Item.Test.Speed.Slow.Enemy` → slow sur ennemi ciblé (pas les alliés)

### Spawn Points
- [ ] Marcher sur un `AItemSpawnPoint` → item accordé à l'inventaire
- [ ] Le spawn point disparaît après pickup, réapparaît après ~30 s (ou immédiatement si `bRequestImmediateRespawnOnConsume = true`)

### UI
- [ ] `WBP_ItemSlot` affiche l'icône, le nombre de charges et la progression du cooldown
- [ ] `WBP_EffectBar` affiche les effets actifs avec leur durée restante
- [ ] L'UI se met à jour sans lag côté client (RepNotify + delegate)

### Réseau (si testé en multi)
- [ ] L'activation d'item depuis un client se répercute bien sur le serveur (`Server_TryActivateItem` RPC)
- [ ] Les effets actifs (`UItemEffectComponent.ActiveEffects`) sont visibles côté client
- [ ] Le changement d'item dans l'inventaire (`OnRep_CurrentItem`) met à jour le mesh tenu côté client
