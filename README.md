# ItemSystem

Plugin Unreal Engine 5.6 (Runtime, C++) fournissant un système d'items complet et
data-driven : définition des items en Data Asset, exécution déléguée à des stratégies
interchangeables, application des effets via une interface implémentée par le pawn, et
distribution dans le monde par points de spawn configurables. L'ensemble est
server-authoritative et pensé pour le multijoueur.

Le plugin est **autonome** : il embarque son propre `Content/`, avec une carte de démo,
dix items d'exemple et les Blueprints de framework nécessaires pour tester sans rien
écrire.

---

## Sommaire

- [Aperçu](#aperçu)
- [Prérequis et installation](#prérequis-et-installation)
- [Architecture](#architecture)
- [Contenu livré](#contenu-livré)
- [Créer un item](#créer-un-item)
- [Gérer les effets en Blueprint](#gérer-les-effets-en-blueprint)
- [QA et tests](#qa-et-tests)
- [Développement](#développement)
- [Référence des enums](#référence-des-enums)

---

## Aperçu

### Le principe

Un item n'est pas une classe : c'est un `UItemDefinition` (Data Asset) qui pointe vers
trois stratégies remplaçables.

| Stratégie | Rôle | Type |
|---|---|---|
| **Execution** | Ce qui se passe dans le monde — un projectile part, une mine se pose, un effet s'applique instantanément | `AItemExecutionStrategy` (Actor, poolé) |
| **Targeting** | Comment on trouve la cible — raycast caméra, ennemi le plus proche | `UItemTargetingStrategy` (UObject) |
| **Payload** | Ce qu'on applique à la cible — dégâts, explosion, effet temporisé | `UItemPayloadStrategy` (UObject) |

Changer un projectile en mine, c'est changer une référence de classe dans le Data Asset.
Aucune de ces trois stratégies ne connaît les deux autres : elles communiquent par un
unique struct, `FItemContext`, qui traverse tout le pipeline.

### Pipeline d'activation

```
UInventoryComponent (sur le Pawn)
  └─ Server_TryActivateItem()                    ← RPC serveur, vérifie cooldown + tags bloquants
       └─ FItemContext                           ← instigateur, cible, transform d'origine, définition, seed
            └─ UItemSystemManager::SpawnItemExecution()
                 └─ AItemExecutionStrategy       ← acteur monde, repris du pool
                      ├─ UItemTargetingStrategy::FindTarget()
                      └─ UItemPayloadStrategy::ApplyEffect(Target, Context)
                           └─ IItemInterface::ApplyItemEffect()   ← sur le pawn touché
                                └─ UItemEffectHandlerComponent::HandleEffect()
```

### Ce que le plugin fournit

- Définition d'items en Data Asset, avec registre chargé depuis un `UDataTable`
- Quatre exécutions prêtes à l'emploi : projectile balistique, piège, application directe, instantané
- Pooling automatique des acteurs d'exécution
- Filtrage d'équipe et immunité intégrés
- Routage de payload par règles : relation d'équipe, tag requis, plus proche ou tous
- Points de spawn monde avec routage de destinataire et refresh temporisé
- Prévisualisation d'arc de tir côté client (`UItemAimPreviewComponent`)
- Réplication complète : grant, activation, VFX, SFX, effets actifs
- Commandes de triche pour la QA en PIE

---

## Prérequis et installation

### Dépendances

| | Modules |
|---|---|
| **Public** | `Core`, `CoreUObject`, `Engine`, `GameplayTags`, `GameplayTasks` |
| **Privé** | `Slate`, `SlateCore`, `Niagara` |

Le plugin déclare `Niagara` comme dépendance dans `ItemSystem.uplugin` — il doit être
activé dans le projet hôte, ce qui est le cas par défaut.

### Installation

1. Copier le dossier `ItemSystem/` dans `Plugins/` du projet.
2. Régénérer les fichiers de projet, puis compiler.
3. Vérifier que le plugin est activé : **Edit > Plugins > Game > Item System**.

### Configuration du projet hôte — obligatoire

Les Gameplay Tags du plugin sont déclarés dans un **DataTable**, pas dans un `.ini`.
Il faut donc l'enregistrer une fois :

```
Project Settings
  └─ Project > GameplayTags
       └─ Gameplay Tag Table List
            └─ [+] /ItemSystem/Data/DT_Item_GameplayTags
```

Cette référence est écrite dans le `DefaultEngine.ini` du **projet hôte**, jamais dans
un fichier du plugin.

> **Pourquoi un DataTable et pas `Config/DefaultGameplayTags.ini` ?**
> UE fusionne le `.ini` d'un plugin avec celui du projet hôte. Dès que le projet hôte a
> ses propres entrées, chaque merge de branche ou mise à jour du plugin produit un
> conflit sur un fichier texte généré — pénible à résoudre sans perdre des tags. Un
> `.uasset` binaire ne peut pas produire de conflit de merge : Git le traite comme
> opaque et LFS le versionne sans diff ligne à ligne. Le prix à payer est l'action
> unique ci-dessus, et le fait que les tags ne soient pas lisibles hors éditeur.

### Mise en place minimale

| Où | Quoi |
|---|---|
| **GameState** | Ajouter `UItemSystemManager`, assigner `ItemRegistryDataTable` et, si besoin, le `UItemSpawnPointRoutingConfig` |
| **Pawn** | Implémenter `IItemInterface`, ajouter `UInventoryComponent` |
| **Pawn** | Ajouter un composant handler d'effets (voir [Gérer les effets en Blueprint](#gérer-les-effets-en-blueprint)) et y router `ApplyItemEffect` |
| **Pawn** *(optionnel)* | `UItemEffectComponent` pour le suivi UI des effets actifs, `UItemAimPreviewComponent` pour la prévisualisation d'arc |
| **PlayerController** | `Cheat Manager Class = ItemCheatManager` pour les commandes de QA |

Pour démarrer immédiatement, ouvrir `Content/ItemSystem/Maps/L_ItemDemo` : tout y est
déjà câblé.

---

## Architecture

### Types de base — `Public/Core/`, `Public/Data/`

**`UItemDefinition`** — le Data Asset décrivant un item.

| Section | Champs |
|---|---|
| Identity | `IdentityTags`, `DisplayName` |
| Rules | `UsageBlockingTags`, `MaxStack`, `Cooldown` |
| Visuals | `Visuals` (`FItemVisuals`), `AttachSocketTag` |
| Logic | `ExecutionClass`, `TargetingClass`, `PayloadClass` (soft class refs), `PayloadRouting` |
| Pickup | `PickupGrantAmount` |
| Audio | `Sound_OnEquip`, `Sound_OnActivate` |

**`FItemContext`** — le sac de données qui traverse tout le pipeline : acteur et controller instigateurs, acteur cible, transform d'origine, définition de l'item, tags de contexte, seed aléatoire, GUID d'invocation.

**`FItemEffectSpec`** — descripteur d'effet minimal : `EffectTag`, `Magnitude`, `Duration`.

**`FItemAimData`** — résultat de visée transmis au serveur à l'activation : `bIsValid`, `StartLocation`, `AimPoint`, `LaunchVelocity`, `ViewLocation`, `ViewRotation`.

**`FItemProjectileArcResult`** — arc prédit pour l'UI et les FX : `bHasSolution`, `bHit`, `AimPoint`, `LaunchVelocity`, `TracedPosition`, `TracedNormal`, `PathPoints`, `HitResult`.

### `UItemSystemManager` — `Public/Core/ItemSystemManager.h`

Composant posé sur le **GameState**. Accès statique : `UItemSystemManager::Get(WorldContextObject)`.

| Responsabilité | API |
|---|---|
| Registre d'items | `LoadItemRegistryFromDataTable()`, `RegisterItems()` |
| Interrogation | `GetItemByQuery(FGameplayTagQuery)`, `GetItemByPolicy(Policy, Requester)` |
| Exécution et pooling | `SpawnItemExecution(FItemContext)`, `ReleaseExecutionActor()` |
| Points de spawn | `RegisterSpawnPoint()`, `RefreshAllSpawnPoints()`, `RefreshSpawnPoint()`, `NotifySpawnPointItemConsumed()` |
| Config de routage | `ApplySpawnPointPickupRoutingConfig()`, `SetSpawnPointPickupRoutingSettings()` |

Le registre se charge depuis `ItemRegistryDataTable`, un `UDataTable` de `FItemDefinitionTableRow`.

### Stratégies d'exécution — `Public/Strategies/`

`AItemExecutionStrategy` — Actor abstrait, poolé par le manager.

| Classe | Comportement |
|---|---|
| `AExecution_Projectile` | Projectile mobile avec collision. Trois modes de lancement, voir `EItemLaunchMode` |
| `AExecution_Trap` | Déclencheur statique — mine, piège — avec durée de vie |
| `AExecution_DirectApply` | Aucun acteur physique : applique le payload immédiatement en résolvant les destinataires via `FItemPayloadRoutingSettings` |
| `AExecution_Instant` | Application immédiate simple |

Toutes portent un `FItemContext` répliqué, un VFX de traînée et d'impact (`UNiagaraSystem`) et un son d'impact. `FinishExecution()` rend l'acteur au pool. `ShouldAffectActor()` applique le filtrage d'équipe (tag `Rule.Ignore.Teammates` dans les `IdentityTags`) et l'immunité (`ITargetableInterface`).

`AExecution_DirectApply` a un comportement VFX particulier : il cherche un `UNiagaraComponent` **pré-placé sur le pawn instigateur** et l'active, pour que l'effet suive le pawn. Sans composant trouvé, il retombe sur un spawn à la position du pawn.

### Stratégies de payload — `Public/Strategies/`

`UItemPayloadStrategy` — UObject abstrait, instancié par exécution. Point d'entrée unique : `ApplyEffect(AActor* Target, const FItemContext& Context)`.

| Classe | Effet |
|---|---|
| `UPayload_Base` | Dispatch générique via `IItemInterface::ApplyItemEffect` |
| `UPayload_Damage` | Dégâts directs (`UGameplayStatics::ApplyDamage`) |
| `UPayload_Explosion` | Dégâts radiaux (`ApplyRadialDamage`) |
| `UPayload_ModifySpeed` | Construit un `FItemEffectSpec` de modification de vitesse |
| `UPayload_Debug` | Log de diagnostic |

`UPayload_Base` est le point de départ recommandé pour un payload Blueprint : il construit le `FItemEffectSpec` et le transmet au pawn, qui décide quoi en faire.

### Stratégies de ciblage — `Public/Strategies/`

`UItemTargetingStrategy` — UObject abstrait, instancié par exécution. Implémentations : `UTargeting_Raycast` (trace depuis la caméra) et `UTargeting_FindNearest` (acteur le plus proche, filtrable par tag).

### Intégration du pawn — `IItemInterface`

Le pawn doit implémenter cette interface. Toutes les fonctions sont `BlueprintNativeEvent`.

| Fonction | Rôle |
|---|---|
| `GetTeamID()` | Équipe, pour le filtrage tir allié |
| `HasGameplayTag(FGameplayTag)` | Testée contre les `UsageBlockingTags` de l'item |
| `GetSocketByTag(FGameplayTag, FName& OutSocketName)` | Mappe un tag, par exemple `Socket.Mount.HandRight`, vers un composant et un socket, pour l'origine du projectile |
| `GetItemInstigatorController()` | Pour le scoring et l'autorité |
| `ApplyItemEffect(FItemEffectSpec, FItemContext)` | Reçoit un effet ; à router vers le composant handler |
| `AddStatusTag(FGameplayTag)` / `RemoveStatusTag(FGameplayTag)` | Pose et retire un tag d'état sur le pawn : étourdi, gelé… |

### Inventaire — `UInventoryComponent`

Posé sur le pawn. Contient un item et un compteur de munitions, tous deux répliqués.

| API | Note |
|---|---|
| `Server_GrantItem(Item, Amount)` | RPC serveur |
| `Server_TryActivateItem()` | RPC serveur — vérifie cooldown et tags bloquants |
| `Server_TryActivateItemWithAim(FItemAimData)` | Activation avec visée calculée côté client |
| `Server_ClearInventory()` | RPC serveur |
| `GetCurrentItem()`, `GetCooldownProgress()`, `GetCooldownRemainingTime()`, `IsOnCooldown()` | Lecture pour l'UI |
| `OnInventoryChanged` | Delegate pour l'UI |

Le composant gère aussi `HeldMeshComponent`, le mesh d'aperçu de l'item sur le pawn, via `OnRep_CurrentItem`.

**Sons** — `Sound_OnEquip` est joué sur tous les clients par `OnRep_CurrentItem`, le RepNotify se déclenchant au grant. `Sound_OnActivate` est joué sur tous les clients par `Multicast_PlayActivateSound`, appelé depuis le serveur à l'activation réussie.

### Système d'effets — `Public/Components/`

Deux composants, aux rôles distincts.

**`UItemEffectHandlerComponent`** — pré-attaché au pawn. Point d'entrée : `HandleEffect(FItemEffectSpec, FItemContext)`, un `BlueprintNativeEvent` qui **retourne `false` par défaut**. Il n'y a aucune implémentation d'effet en C++ : toute la logique métier se surcharge en Blueprint. `FindEffectComponent()` récupère le composant frère pour l'UI.

**`UItemEffectComponent`** — optionnel, purement pour le suivi UI. Contient un `TArray<FItemActiveEffect>` répliqué et déclenche `OnEffectsChanged` sur le serveur et les clients.

Helpers UI dans `UItemEffectBlueprintLibrary` :

| Fonction | Retour |
|---|---|
| `GetEffectNormalizedProgress(Effect, CurrentTime)` | 0 vers 1 sur la durée de l'effet |
| `GetEffectRemainingTime(Effect, CurrentTime)` | Secondes restantes, `-1` si permanent |
| `GetEffectsByTag(Component, Tag)` | Effets dont le tag correspond, matching hiérarchique |
| `HasActiveEffect(Component, Tag)` | Booléen |

> `CurrentTime` doit venir de `GameState->GetServerWorldTimeSeconds()`, pas de `GetWorld()->GetTimeSeconds()`, sinon les barres de progression décrochent en réseau.

### Visée et trajectoire

`UItemProjectileTrajectoryLibrary` calcule l'arc balistique partagé entre le gameplay et l'UI : `BuildArcParamsForItem()`, `ComputeProjectileArc()`, `MakeAimDataFromArc()`.

`UItemAimPreviewComponent` est un composant **local uniquement** qui prédit l'arc en continu pendant la visée : `StartAiming()`, `StopAiming()`, `UpdateAimPreview()`, `GetLastAimData()`, `GetLastArcResult()`. Il expose `OnItemAimPreviewUpdated` et `OnItemAimPreviewStopped` pour brancher un widget de réticule ou un spline de trajectoire.

### Points de spawn monde — `AItemSpawnPoint`

Acteur placé dans le niveau, enregistré auprès du manager.

| Composant | Rôle |
|---|---|
| `USphereComponent` (PickupTrigger) | Ramassage par overlap |
| `UStaticMeshComponent` (ItemPreviewMesh) | Affiche le mesh de l'item assigné |
| `UNiagaraComponent` (SpawnVFXComponent) | VFX en boucle optionnel, par exemple une aura de ramassage |
| `UBillboardComponent` + `UTextRenderComponent` | Identification en éditeur uniquement |

Le routage de ramassage vient d'un `FItemSpawnPointPickupRoutingSettings` propagé depuis le `UItemSpawnPointRoutingConfig` du manager.

**Priorité de la quantité accordée** : override local du spawn point, puis override de la config de routage du manager, puis `UItemDefinition::PickupGrantAmount`.

### Politiques de distribution — `Public/Distribution/`

`UItemDistributionPolicy` — UObject instancié, sélectionne un item dans le registre. Implémentations : `UDistributionPolicy_Random` et `UDistributionPolicy_TagQuery`, qui filtre par `FGameplayTagQuery`.

### Conventions de gameplay tags

| Préfixe | Usage |
|---|---|
| `Item.Test.*` | Identifiants d'items de QA |
| `Item.Effect.*` | Identifiants d'effets actifs |
| `Rule.Ignore.*` | Règles de comportement sur un item |
| `State.Status.*` | États du pawn, pour le blocage d'usage |
| `Socket.Mount.*` | Mapping de sockets sur le pawn |
| `Routing.*` | Tags de routage de ramassage et de payload |

Seuls quatre tags sont référencés en dur dans le C++ : `Item.Effect.Base`, `Item.Effect.ModifySpeed`, `Item.Effect.ModifySpeed.Boost` et `Rule.Ignore.Teammates`. Tous les autres sont déclarés dans `DT_Item_GameplayTags` et libres de convention.

### Réplication

- Tout grant et toute activation passent par des **RPC serveur** : le système est server-authoritative.
- `UInventoryComponent::CurrentItem` — `RepNotify`, met à jour les visuels et joue le son d'équipement sur les clients.
- `UInventoryComponent::LastActivationTime` — `COND_OwnerOnly` : l'UI de cooldown n'est visible que par le joueur propriétaire. Un non-propriétaire lisant `GetCooldownProgress()` sur un autre pawn obtient `1.0`, par conception.
- `AItemSpawnPoint::AssignedItem` — `RepNotify`, rafraîchit le mesh d'aperçu.
- `UItemEffectComponent::ActiveEffects` — répliqué ; `OnEffectsChanged` se déclenche sur le serveur et sur les clients après `OnRep`.
- Les acteurs d'exécution portent un `FItemContext` répliqué ; les VFX et SFX d'impact partent en `NetMulticast Unreliable`.
- `AExecution_DirectApply` utilise `Multicast_PlayVFXOnInstigator` pour activer le `UNiagaraComponent` pré-placé du pawn sur tous les clients.

---

## Contenu livré

### Arbre des assets

```
Content/ItemSystem/
│
├── Core/                          ← Blueprints de framework
│   ├── GM_ItemDemo                  GameMode
│   ├── GS_ItemDemo                  GameState — porte UItemSystemManager
│   ├── PC_ItemDemo                  PlayerController — CheatManagerClass = ItemCheatManager
│   ├── BP_Pawn_ItemDemo             Pawn — IItemInterface + UInventoryComponent
│   ├── AC_ItemEffectHandler         Composant Blueprint de gestion des effets
│   ├── BPI_ItemEffects              Interface Blueprint des effets
│   └── Structure/S_PlayerEffects    Struct d'état des effets du joueur
│
├── Data/
│   ├── DT_Item_GameplayTags         Tags du plugin — à référencer dans Project Settings
│   ├── DT_ItemRegistry              Registre des items (FItemDefinitionTableRow)
│   └── DA_SpawnPointRoutingConfig   Config de routage des points de spawn
│
├── Items/                         ← un sous-dossier par item
│   ├── _Parent/                     BP_Item_BaseActor, DA_Item_Base — bases mutualisées
│   ├── Confusion/                   BP_Execution_, BP_Payload_, DA_Item_
│   ├── EnemySlow/                   BP_Payload_, BP_Targeting_Raycast_, DA_Item_
│   ├── ExplosiveMine/               BP_Execution_, BP_Payload_, DA_Item_
│   ├── IceMine/                     BP_Execution_, BP_Payload_, DA_Item_
│   ├── PoisonMine/                  BP_Execution_, BP_Payload_, DA_Item_
│   ├── SelfSpeedBoost/              BP_Execution_, BP_Payload_, DA_Item_
│   ├── Shield/                      BP_Execution_, BP_Payload_, DA_Item_
│   ├── Snowball/                    BP_Execution_, BP_Payload_, DA_Item_
│   ├── TornadoAura/                 BP_Execution_, BP_Payload_, DA_Item_, BP_Tornado_Attached
│   └── TornadoBomb/                 BP_Execution_, BP_Payload_, DA_Item_, BP_Tornado_Static
│
├── Input/
│   ├── IMC_ItemDemo                 Mapping context
│   └── InputActions/                IA_Item_Move, _Look, _MouseLook, _Jump, _Aim, IA_UseItem
│
├── UI/
│   ├── WBP_ItemSlot                 Icône, munitions, cooldown
│   └── WBP_ItemCrosshair            Réticule de visée
│
├── VFX/SpeedBoost/NS_Sprint        Niagara
├── Audio/                          SW_Pickup_Item, SW_Activate_Item
├── Maps/L_ItemDemo                 Carte de démo
└── DemoContent/                    Mannequins et animations d'exemple
```

### Catalogue des items

Les dix items de démo. La configuration chiffrée de chacun — classes d'exécution,
routage, magnitudes, durées — vit dans son `DA_Item_*` et se lit dans l'éditeur ; la
colonne « Archétype » indique l'intention de conception.

| Item | Archétype | Portée |
|---|---|---|
| `SelfSpeedBoost` | Application directe | Sur soi |
| `Shield` | Application directe | Sur soi |
| `TornadoAura` | Application directe + acteur attaché (`BP_Tornado_Attached`) | Sur soi |
| `Confusion` | Application directe, recherche par règles | Ennemis dans un rayon |
| `EnemySlow` | Ciblage par raycast | Ennemi visé |
| `Snowball` | Projectile balistique | Cible touchée |
| `IceMine` | Piège | Ce qui déclenche |
| `PoisonMine` | Piège | Ce qui déclenche |
| `ExplosiveMine` | Piège, effet de zone | Autour du point de déclenchement |
| `TornadoBomb` | Projectile + acteur posé (`BP_Tornado_Static`) | Zone à l'impact |

### Principes d'organisation

| Règle | Raison |
|---|---|
| Les tags dans un DataTable, pas un `.ini` | Un `.uasset` binaire ne peut pas produire de conflit de merge |
| Un item = un dossier | Supprimer ou dupliquer un item est une seule opération dans le Content Browser |
| Les stratégies à côté de leur Data Asset | Pas de dossier `Payloads/` global : chaque item reste autonome |
| Le framework dans `Core/` | Sépare l'infrastructure du contenu |
| Les assets partagés dans `Data/` | Registre et config de routage sont communs à tous les items |
| Les bases dans `Items/_Parent/` | Les réglages communs se changent en un endroit |

---

## Créer un item

L'ordre compte : les stratégies doivent exister avant le Data Asset qui les référence.

### 1. Le payload — ce qui est appliqué

Créer un Blueprint dérivant de `UPayload_Base` (ou d'un payload C++ existant).
Surcharger `ApplyEffect` : construire un `FItemEffectSpec` — `EffectTag`, `Magnitude`,
`Duration` — et le transmettre à la cible via `IItemInterface::ApplyItemEffect`.

### 2. L'exécution — ce qui se passe dans le monde

Créer un Blueprint dérivant de l'exécution correspondant à l'archétype voulu :

| Intention | Classe parente |
|---|---|
| Objet lancé qui vole | `AExecution_Projectile` |
| Mine ou piège posé au sol | `AExecution_Trap` |
| Effet immédiat, sans objet physique | `AExecution_DirectApply` |
| Application immédiate simple | `AExecution_Instant` |

> Si vous surchargez `ResetForReuse()`, appelez toujours `Super::ResetForReuse()` :
> sans cela l'acteur repris du pool conserve son état précédent, vélocité comprise.

### 3. Le ciblage — optionnel

Nécessaire seulement si l'exécution doit désigner une cible. Blueprint dérivant de
`UTargeting_Raycast` ou `UTargeting_FindNearest`.

### 4. Le Data Asset

Créer un `DA_Item_*` dérivant de `UItemDefinition` — ou de `DA_Item_Base` pour hériter
des réglages communs. Renseigner :

- **Identity** — `IdentityTags` (au moins un tag identifiant), `DisplayName`
- **Rules** — `Cooldown`, `MaxStack`, `UsageBlockingTags`
- **Logic** — `ExecutionClass`, `PayloadClass`, `TargetingClass` si utilisé
- **Logic > PayloadRouting** — voir le tableau ci-dessous
- **Visuals** — icône, mesh, `AttachSocketTag`
- **Audio** — `Sound_OnEquip`, `Sound_OnActivate`

Le routage de payload détermine qui reçoit l'effet :

| `RoutingPolicy` | Destinataire |
|---|---|
| `TargetingResultOnly` | Uniquement ce que la stratégie de ciblage a trouvé |
| `InstigatorOnly` | L'instigateur — pour les buffs sur soi |
| `SearchByRules` | Recherche par relation d'équipe, tag requis et rayon |

En `SearchByRules`, trois réglages affinent la recherche : `RecipientRelation`
(`Any`, `SameTeamAsInstigator`, `EnemyOfInstigator`), `RequiredRecipientTag` et
`SearchSelection` (`NearestSingle` ou `AllMatching`). Deux replis existent :
`bFallbackToTargetingResultIfNoSearchMatch` et `bFallbackToInstigatorIfNoRecipient`.

### 5. L'enregistrement

Ajouter une ligne dans `DT_ItemRegistry` pointant vers le nouveau Data Asset. Sans cela
le manager ne le connaît pas et `Cheat_GiveItem` ne le trouvera pas.

Si l'item introduit de nouveaux tags, les déclarer dans `DT_Item_GameplayTags`
**avant** de les utiliser, sinon UE émet des avertissements `Unknown Tag` au chargement.

---

## Gérer les effets en Blueprint

Le C++ ne sait appliquer aucun effet. `UItemEffectHandlerComponent::HandleEffect`
retourne `false` par défaut : c'est un point d'extension, pas une implémentation. Toute
la logique — vitesse, bouclier, étourdissement, poison — s'écrit en Blueprint.

### Le composant handler

1. Créer un composant Blueprint dérivant de `UItemEffectHandlerComponent`.
2. Surcharger `HandleEffect`. Aiguiller sur `Spec.EffectTag`, appliquer la logique,
   retourner `true` si l'effet a été traité — `false` sinon, ce qui est le comportement
   correct pour un tag inconnu.
3. Remplacer le composant handler du pawn par cette version Blueprint.

Pour les effets temporisés, poser un timer sur `Spec.Duration` et défaire l'effet à
l'expiration. Pour les effets cumulables, penser au cas où le même tag arrive deux fois
avant expiration.

### Câblage du pawn

Dans l'implémentation de `ApplyItemEffect` du pawn, appeler `HandleEffect` sur le
composant handler et propager sa valeur de retour.

Pour les effets qui changent l'état du pawn plutôt qu'une statistique — étourdissement,
gel, confusion — utiliser `AddStatusTag` et `RemoveStatusTag`, puis tester ces tags là
où le comportement doit changer : blocage du saut, inversion des entrées de déplacement,
blocage d'activation d'item via `UsageBlockingTags`.

### Suivi UI

Si le pawn porte un `UItemEffectComponent`, appeler `FindEffectComponent()` depuis la
surcharge puis `AddOrRefreshEffect(Spec)` pour alimenter l'UI. Le widget se lie à
`OnEffectsChanged` et lit les effets via `GetActiveEffects()`, en rappelant cette
fonction à chaque callback plutôt qu'en gardant une copie.

> **La vitesse n'est pas gérée en C++.** Il n'existe aucune implémentation native de
> modification de vitesse. Pour traiter `Item.Effect.ModifySpeed`, manipuler
> `CharacterMovementComponent::MaxWalkSpeed` dans la surcharge Blueprint.

---

## QA et tests

Les tests se font en PIE, pas via des tests automatisés.

### Commandes console

```
ItemSystem.QA 1                      Active les logs QA détaillés
ItemSystem.QA 0                      Les désactive

Cheat_GiveItem <requête de tags>     Donne un item correspondant à la requête
Cheat_GiveItem <requête> <quantité>  Idem, avec une quantité explicite
Cheat_SimulateImpact <requête>       Applique le payload sur la cible du raycast caméra
Cheat_ClearInventory                 Vide l'inventaire courant
```

`Cheat_GiveItem` accepte une requête de tags complète :

```
Cheat_GiveItem Item.Test.Projectile
Cheat_GiveItem "Item.Test.Projectile AND NOT Item.Test.Debug"
Cheat_GiveItem (Item.Test.Projectile OR Item.Test.Trap)
```

Les commandes exigent `CheatManagerClass = ItemCheatManager` sur le PlayerController.

### Environnement de test

1. Play > Advanced Settings : **Number of Players = 2**, **Net Mode = Play As Listen Server**.
2. Ouvrir Output Log, activer `ItemSystem.QA 1`.
3. Sur les Blueprints d'exécution à inspecter, mettre `bShowDebugVisuals = true`.

### Procédures

Les scénarios ci-dessous sont génériques : ils décrivent le type d'item à utiliser, pas
un item nommé.

| # | Scénario | Mise en place | Attendu |
|---|---|---|---|
| 1 | Cooldown | Un item avec `Cooldown > 0` | La seconde activation immédiate est refusée. Log `QA: Cooldown blocked item` |
| 2 | Tag bloquant | `UsageBlockingTags` renseigné, poser ce tag sur le pawn | Activation refusée. Log `QA: Blocking tag ... prevented item` |
| 3 | Filtrage d'équipe | `Rule.Ignore.Teammates` dans les `IdentityTags`, tirer sur un allié | Aucun payload appliqué. Log `QA: Team filter blocked actor` |
| 4 | Immunité | Cible implémentant `ITargetableInterface` en immunité | Aucun payload appliqué. Log `QA: Immunity blocked actor` |
| 5 | Origine au socket | `AttachSocketTag` renseigné, `GetSocketByTag` mappé sur le pawn | Le projectile part du socket, pas de l'origine de l'acteur |
| 6 | Politique de distribution | Appeler `GetItemByPolicy` avec une policy de requête | L'item retourné satisfait la requête |
| 7 | Requête de tags invalide | `Cheat_GiveItem` avec une syntaxe erronée | Avertissement en log, aucun crash |
| 8 | Pooling | Activer cinq fois ou plus un item à exécution physique | Logs `QA: Added actor to pool` puis `QA: Reusing pooled actor` |
| 9 | Réutilisation du pool | Tirer, attendre le retour au pool, pivoter de 90°, retirer | Le second tir part dans la nouvelle direction |
| 10 | VFX et SFX d'impact | `ImpactVFX` et `ImpactSound` renseignés | Joués au point d'impact sur tous les clients |
| 11 | Auto-collision | Tirer vers ses propres pieds | L'instigateur n'est pas touché |
| 12 | Durée de vie | Tirer dans le vide | L'acteur est détruit après sa durée de vie, sans crash |
| 13 | Vider l'inventaire | `Cheat_ClearInventory` | Item retiré, munitions à zéro, widget vidé |
| 14 | Impact simulé | Viser une cible, `Cheat_SimulateImpact` | Le payload s'applique à l'acteur visé |
| 15 | Décrément des munitions | Item à trois charges, activer trois fois | Décrément à chaque usage, widget vidé à zéro |
| 16 | Cooldown à l'écran | Sonder `GetCooldownProgress()` | Passe de 0 à 1 sur la durée du cooldown |
| 17 | Cooldown non répliqué | Lire `GetCooldownProgress()` sur le pawn d'un autre | Retourne `1.0` — comportement voulu, `COND_OwnerOnly` |
| 18 | Effets à l'écran | Appliquer un effet temporisé | `OnEffectsChanged` se déclenche à l'application et à l'expiration, sur tous les clients |
| 19 | Filtrage par tag parent | Appliquer un effet à tag enfant, filtrer sur le parent | `GetEffectsByTag` retourne l'effet — matching hiérarchique |
| 20 | Tag d'effet inconnu | `HandleEffect` avec un tag non traité | Retourne `false`, sans effet de bord ni crash |

### Points de spawn monde

| # | Scénario | Attendu |
|---|---|---|
| 21 | `OverlappingActorOnly` | L'acteur qui entre dans le trigger reçoit l'item |
| 22 | `OverlapActorIfHasTagElseDesignated` | Avec le tag requis, l'acteur reçoit ; sans lui, le destinataire désigné reçoit |
| 23 | `DesignatedActorOnly` | Seul le destinataire désigné reçoit, jamais celui qui entre |
| 24 | Relation d'équipe | `SameTeamAsOverlap` cible l'allié, `EnemyOfOverlap` cible l'ennemi |
| 25 | Candidat le plus proche | Entre deux candidats valides, le plus proche reçoit |
| 26 | Repli | Avec `bFallbackToOverlapIfDesignatedNotFound`, l'acteur entrant reçoit quand la recherche échoue ; sans lui, personne ne reçoit et l'item reste disponible |
| 27 | Priorité de quantité | Override local, puis config manager, puis `PickupGrantAmount` — dans cet ordre |
| 28 | Consommation et respawn | Conformes à `bConsumeOnSuccessfulGrant` et `bRequestImmediateRespawnOnConsume` |

### Modes de lancement des projectiles

| # | Scénario | Attendu |
|---|---|---|
| 29 | `ArcThrow` | Parabole visible atteignant le point visé |
| 30 | `bFavorHighArc` | À faux, trajectoire tendue ; à vrai, trajectoire haute vers la même cible |
| 31 | `ArcThrow` insoluble | Cible hors de portée : tir dans l'axe de visée. Log `QA: ArcThrow could not solve ballistic path` |
| 32 | `GravityScale` | Une valeur basse élargit l'arc, une valeur haute le resserre |
| 33 | Sans PlayerController | Pawn IA : tir vers l'avant du pawn, sans crash |
| 34 | `Drop` sans impulsion | Chute verticale près des pieds du pawn |
| 35 | `Drop` avec impulsion | Poussée vers l'avant proportionnelle, gravité dominante |
| 36 | `ExternalVelocity` fournie | Le projectile suit `FItemContext::LaunchVelocity` |
| 37 | `ExternalVelocity` absente | Repli sur `ArcThrow` avec un avertissement en log |

### Diagnostics courants

| Symptôme | Vérifier |
|---|---|
| Aucun effet ne s'applique | `ApplyItemEffect` du pawn appelle bien `HandleEffect` sur la bonne instance de composant |
| L'UI ne se met pas à jour | `UItemEffectComponent` présent ; la surcharge appelle `FindEffectComponent()->AddOrRefreshEffect(Spec)` |
| Barre de progression figée | Utiliser `GetServerWorldTimeSeconds()`, pas `GetWorld()->GetTimeSeconds()` |
| Personne ne reçoit l'item au spawn point | `UInventoryComponent` présent, tag de destinataire posé, filtre de relation cohérent |
| L'item disparaît sans réapparaître | Mode de refresh du manager et `bRequestImmediateRespawnOnConsume` |
| Le projectile part de travers | Direction du socket renvoyé par `GetSocketByTag` ; en `ExternalVelocity`, vélocité bien en espace monde |
| `ArcThrow` retombe toujours en repli | Vitesse trop faible, ou cible à la verticale |
| Le tir réutilise l'ancienne direction | `Super::ResetForReuse()` non appelé dans la surcharge |
| Avertissements `Unknown Tag` | Tag utilisé sans avoir été déclaré dans `DT_Item_GameplayTags` |

---

## Développement

### Compiler

Depuis l'éditeur : **Build > Build Solution**, ou l'icône marteau. En ligne de commande :

```
UnrealBuildTool.exe <NomProjet>Editor Win64 Development "<chemin>/<NomProjet>.uproject"
```

### Packager le plugin

```
RunUAT.bat BuildPlugin -Plugin="<chemin>/ItemSystem.uplugin" -Package="<dossier de sortie>" -CreateSubFolder
```

UAT crée un `HostProject/` temporaire dans le dossier de sortie et y recopie le plugin.
Les erreurs de compilation pointent vers ce chemin de staging : **corriger les sources
d'origine**, pas la copie, qui est recréée à chaque exécution.

### Le piège des includes

Le module utilise `PCHUsage = UseExplicitOrSharedPCHs`. La target **Editor** fournit un
PCH partagé large qui masque les includes manquants ; la target **UnrealGame** utilisée
par le packaging ne le fait pas. Résultat : du code qui compile parfaitement dans
l'éditeur peut faire échouer le packaging sur des dizaines d'erreurs.

Tout fichier ajouté doit donc inclure explicitement ce qu'il utilise :

| Utilisation | Include requis |
|---|---|
| `FHitResult` | `Engine/HitResult.h` — **en UE 5.6 `Engine/EngineTypes.h` ne l'inclut plus** |
| `UWorld` déréférencé, ou passé en `const UObject*` | `Engine/World.h` |
| `AActor` déréférencé, `Implements<>()`, converti en `UObject*` | `GameFramework/Actor.h` |
| `GetInstigator()` comparé à un `AActor*` | `GameFramework/Pawn.h` |
| `TSubclassOf<UDamageType>` | `GameFramework/DamageType.h` |

Dans un header UHT, l'include va **avant** le `.generated.h`, qui doit rester en
dernière position.

---

## Référence des enums

**`EItemPayloadRoutingPolicy`** — `TargetingResultOnly`, `InstigatorOnly`, `SearchByRules`

**`EItemPayloadRecipientRelation`** — `Any`, `SameTeamAsInstigator`, `EnemyOfInstigator`

**`EItemPayloadSearchSelection`** — `NearestSingle`, `AllMatching`

**`EItemSpawnPickupMethod`** — `TriggerOverlap`

**`EItemSpawnRecipientPolicy`** — `OverlappingActorOnly`, `OverlapActorIfHasTagElseDesignated`, `DesignatedActorOnly`

**`EItemSpawnRecipientRelation`** — `Any`, `SameTeamAsOverlappingActor`, `EnemyOfOverlappingActor`

**`EItemLaunchMode`** — `ArcThrow`, `Drop`, `ExternalVelocity`
