# Demo Content Guide — Speed Buff & Speed Debuff

Ce guide décrit pas-à-pas la création de deux items de démonstration dans l'éditeur UE5 :

| Item | Comportement | Exécution |
|---|---|---|
| **DA_Item_SelfSpeedBoost** | Buff de vitesse instantané sur soi-même | `Execution_DirectApply` · routing `InstigatorOnly` |
| **DA_Item_EnemySlow** | Slow ciblé sur un ennemi visé au rayon | `Execution_DirectApply` · targeting Raycast · routing `TargetingResultOnly` |

---

## Dossier de référence

Tous les assets de démo vivent dans :
```
Content/ItemSystem/Demo/
├── Payloads/
│   ├── BP_Payload_SelfSpeedBoost
│   └── BP_Payload_EnemySlow
├── Targetings/
│   └── BP_Targeting_Raycast_EnemySlow
└── Items/
    ├── DA_Item_SelfSpeedBoost
    └── DA_Item_EnemySlow
```

---

## Étape 1 — Créer les dossiers

Dans le Content Browser :
1. Clic droit → **New Folder** → `ItemSystem`
2. Dans `ItemSystem/` → `New Folder` → `Demo`
3. Dans `Demo/` → créer trois sous-dossiers : `Payloads`, `Targetings`, `Items`

---

## Étape 2 — BP_Payload_SelfSpeedBoost

**Emplacement** : `Content/ItemSystem/Demo/Payloads/`

1. Clic droit → **Blueprint Class**
2. Chercher `Payload_ModifySpeed` (classe parente) → sélectionner → **Select**
3. Nommer : `BP_Payload_SelfSpeedBoost`
4. Ouvrir le BP → onglet **Class Defaults**

| Propriété | Valeur |
|---|---|
| `SpeedMultiplier` | `1.5` |
| `Duration` | `5.0` |
| `EffectTag` | `Item.Effect.ModifySpeed.Boost` |

5. **Compile & Save**

---

## Étape 3 — BP_Payload_EnemySlow

**Emplacement** : `Content/ItemSystem/Demo/Payloads/`

1. Clic droit → **Blueprint Class** → `Payload_ModifySpeed`
2. Nommer : `BP_Payload_EnemySlow`
3. Ouvrir → **Class Defaults**

| Propriété | Valeur |
|---|---|
| `SpeedMultiplier` | `0.5` |
| `Duration` | `3.0` |
| `EffectTag` | `Item.Effect.ModifySpeed.Slow` |

4. **Compile & Save**

---

## Étape 4 — BP_Targeting_Raycast_EnemySlow

**Emplacement** : `Content/ItemSystem/Demo/Targetings/`

1. Clic droit → **Blueprint Class** → chercher `Targeting_Raycast`
2. Nommer : `BP_Targeting_Raycast_EnemySlow`
3. Ouvrir → **Class Defaults**

| Propriété | Valeur |
|---|---|
| `TraceDistance` | `5000.0` |

4. **Compile & Save**

> **Note** : Le Raycast utilise automatiquement la caméra du contrôleur (ou l'`OriginTransform` fourni par l'`InventoryComponent`). Pas de config supplémentaire nécessaire.

---

## Étape 5 — DA_Item_SelfSpeedBoost

**Emplacement** : `Content/ItemSystem/Demo/Items/`

1. Clic droit → **Miscellaneous > Data Asset**
2. Choisir `ItemDefinition` → **Select**
3. Nommer : `DA_Item_SelfSpeedBoost`
4. Ouvrir et remplir :

### Section Identity

| Propriété | Valeur |
|---|---|
| `IdentityTags` | `Item.Test.Speed.Boost.Self` |
| `UsageBlockingTags` | *(vide)* |

> **Pas** de `Rule.Ignore.Teammates` ici : l'item s'applique à l'instigateur lui-même via `InstigatorOnly` — `ShouldAffectActor` n'est pas évalué dans ce chemin.

### Section Logic

| Propriété | Valeur |
|---|---|
| `ExecutionClass` | `Execution_DirectApply` *(classe C++ directe, pas de BP requis)* |
| `TargetingClass` | *(vide / None)* |
| `PayloadClass` | `BP_Payload_SelfSpeedBoost` |

### Section Payload Routing (`PayloadRoutingSettings`)

| Propriété | Valeur |
|---|---|
| `RoutingPolicy` | `Instigator Only` |
| `bFallbackToInstigatorIfNoRecipient` | `false` *(inutile, déjà Instigator)* |

### Section Inventory

| Propriété | Valeur |
|---|---|
| `MaxStack` | `1` |
| `PickupGrantAmount` | `1` |
| `Cooldown` | `0.0` *(ou à la guise du designer)* |

5. **Save**

---

## Étape 6 — DA_Item_EnemySlow

**Emplacement** : `Content/ItemSystem/Demo/Items/`

1. Clic droit → **Miscellaneous > Data Asset** → `ItemDefinition`
2. Nommer : `DA_Item_EnemySlow`
3. Ouvrir et remplir :

### Section Identity

| Propriété | Valeur |
|---|---|
| `IdentityTags` | `Item.Test.Speed.Slow.Enemy` |
| `IdentityTags` (ajouter) | `Rule.Ignore.Teammates` |
| `UsageBlockingTags` | *(vide)* |

> `Rule.Ignore.Teammates` est lu par `ShouldAffectActor()` dans `AItemExecutionStrategy` pour refuser l'application sur les alliés, même si le raycast en touche un par accident.

### Section Logic

| Propriété | Valeur |
|---|---|
| `ExecutionClass` | `Execution_DirectApply` |
| `TargetingClass` | `BP_Targeting_Raycast_EnemySlow` |
| `PayloadClass` | `BP_Payload_EnemySlow` |

### Section Payload Routing (`PayloadRoutingSettings`)

| Propriété | Valeur |
|---|---|
| `RoutingPolicy` | `Targeting Result Only` |
| `bFallbackToInstigatorIfNoRecipient` | `false` |

### Section Inventory

| Propriété | Valeur |
|---|---|
| `MaxStack` | `1` |
| `PickupGrantAmount` | `1` |
| `Cooldown` | `0.0` |

4. **Save**

---

## Étape 7 — Enregistrer les items dans le DataTable

Si tu utilises un `ItemRegistryDataTable` (`UDataTable` de type `ItemDefinitionTableRow`) :

1. Ouvrir le DataTable existant.
2. Ajouter deux lignes :

| Row Name | ItemDefinition |
|---|---|
| `Row_SelfSpeedBoost` | `DA_Item_SelfSpeedBoost` |
| `Row_EnemySlow` | `DA_Item_EnemySlow` |

3. **Save**

Sinon, appeler `UItemSystemManager::RegisterItems()` manuellement côté code.

---

## Étape 8 — Tags Gameplay à déclarer

Dans **Project Settings > GameplayTags** (ou dans un `.ini` / Data Table dédié), ajouter si absent :

```
Item.Test.Speed.Boost.Self
Item.Test.Speed.Slow.Enemy
Item.Effect.ModifySpeed.Boost     (probablement déjà déclaré)
Item.Effect.ModifySpeed.Slow      (probablement déjà déclaré)
Rule.Ignore.Teammates             (probablement déjà déclaré)
```

---

## Étape 9 — Test en PIE

Activer le `ItemCheatManager` sur ton PlayerController BP (`CheatManagerClass = ItemCheatManager`), puis :

```
ItemSystem.QA 1                                 # Active les logs verbeux

Cheat_GiveItem Item.Test.Speed.Boost.Self       # Donne le buff sur soi
Cheat_GiveItem Item.Test.Speed.Slow.Enemy       # Donne le slow ennemi
Cheat_ClearInventory                            # Vide l'inventaire
```

Pour activer l'item depuis l'inventaire, appeler `Server_TryActivateItem()` via le BP du Pawn ou via un input.

### Checklist de validation

- [ ] `SelfSpeedBoost` — vitesse augmente immédiatement à l'activation, revient à la normale après 5 s
- [ ] `EnemySlow` — slow s'applique uniquement à un ennemi visé au centre de l'écran (TraceDistance 5000 cm)
- [ ] `EnemySlow` + allié dans la ligne de mire — aucun effet appliqué à l'allié (`Rule.Ignore.Teammates`)
- [ ] `EnemySlow` — raycast dans le vide (aucun acteur touché) → aucun effet
- [ ] Les deux items apparaissent dans `ItemSystem.QA 1` logs avec l'InvocationGUID

---

## Flux pipeline rappel

```
InventoryComponent::Server_TryActivateItem()
  └─ FItemContext { Instigator, TargetActor=null, ItemDefinition }
       └─ ItemSystemManager::SpawnItemExecution()
            └─ AExecution_DirectApply::Execute()
                 ├─ [SelfSpeedBoost] RoutingPolicy=InstigatorOnly
                 │    └─ BP_Payload_SelfSpeedBoost::ApplyEffect(Instigator)
                 │         └─ IItemInterface::ApplyItemEffect → UItemEffectHandlerComponent
                 │
                 └─ [EnemySlow] RoutingPolicy=TargetingResultOnly
                      ├─ BP_Targeting_Raycast_EnemySlow::FindTarget() → EnemyActor
                      ├─ ShouldAffectActor(EnemyActor) → Rule.Ignore.Teammates → OK si ennemi
                      └─ BP_Payload_EnemySlow::ApplyEffect(EnemyActor)
                           └─ IItemInterface::ApplyItemEffect → UItemEffectHandlerComponent
```
