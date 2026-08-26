# AGENTS.md — ItemSystem

Ce fichier guide Codex sur ce dépôt.

## Documentation

**Toute la documentation du plugin est dans [`README.md`](README.md)** : architecture,
API publique, contenu livré, création d'un item, gestion des effets en Blueprint,
procédures de QA, compilation et packaging.

C'est le seul document du plugin. Le lire avant toute intervention.

## Règles de travail

- **Avant de toucher au C++**, lire *Développement > Le piège des includes*. Le module
  compile avec un PCH partagé en target Editor : du code parfaitement valide dans
  l'éditeur peut faire échouer le packaging sur des dizaines d'erreurs d'includes
  manquants. Inclure explicitement ce qui est utilisé, et placer l'include avant le
  `.generated.h` dans les headers.
- **Les tests se font en PIE**, pas via des tests automatisés. Les commandes console et
  les procédures sont dans *QA et tests*.
- **Aucun effet n'est implémenté en C++.** `UItemEffectHandlerComponent::HandleEffect`
  retourne `false` par défaut ; la logique métier vit dans des Blueprints.
- **Ne pas recréer de documentation dispersée.** Toute mise à jour de doc va dans
  `README.md`, pas dans un nouveau fichier ni dans un dossier `Docs/`.
- **Vérifier avant de documenter.** Les noms d'assets et les configurations décrits dans
  les anciennes docs avaient largement divergé du contenu réel. Confronter au disque
  (`Content/ItemSystem/`) et aux sources avant d'écrire.
