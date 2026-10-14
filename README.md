# LastSignal

Jeu d'horreur post-apocalyptique **réaliste en vue bodycam** (dans l'esprit de *Bodycam*), fait avec **Unreal Engine 5** en C++.

> Nuit du 4 novembre 2031, 03:17. L'unité K-7 est envoyée dans la **zone de quarantaine – secteur 4**.
> Mission : récupérer du matériel dans les bâtiments abandonnés, puis rejoindre le point d'extraction au check-point militaire.

---

## Ce qui est fait (version 0.1 : map + mécaniques de base)

| Partie | Contenu |
|---|---|
| **Caméra bodycam** | Caméra à hauteur de poitrine, grand angle déformé (fish-eye), vignette, grain, aberration chromatique, flou de mouvement, balancement de la marche, respiration (plus forte quand on est essoufflé), inertie quand on tourne, secousses (tir, chute, dégâts) |
| **Déplacements** | Marche lente et lourde, sprint avec endurance, accroupi, se pencher à gauche/droite (sans traverser les murs), saut limité, dégâts de chute |
| **Lampe torche** | Fixée à l'épaule, suit le regard avec un léger retard, faisceau visible dans le brouillard, faux contacts de temps en temps |
| **Arme** | Pistolet 9mm, 8 balles par chargeur, **munitions rares**, rechargement de 2,3 s, dispersion plus grande sans viser ou en bougeant, recul |
| **Objets** | Ramasser des objets (sac avec poids), boîtes de munitions |
| **Objectif** | Trouver 3 objets (jerrican, batterie radio, trousse de secours) puis rejoindre l'extraction |
| **Map** | Rue de 120 m entourée d'un mur de quarantaine : immeubles (certains explorables, d'autres en ruine), voitures abandonnées/brûlées/retournées, lampadaires qui grésillent, arbres morts, débris, sacs mortuaires, barils en feu, check-point militaire avec mirador, projecteur et hélisurface |
| **Ambiance** | Nuit, lune, brouillard volumétrique, image désaturée façon « found footage », exposition sombre |
| **HUD** | Incrustation bodycam (REC, date/heure, n° d'unité), santé, endurance, munitions, poids du sac, objectifs. Pas de réticule (réalisme) |
| **Fin de partie** | « EXTRACTION RÉUSSIE » ou « SIGNAL PERDU », puis la map redémarre |

**Prévu pour plus tard** (déjà préparé dans le code) :
- le **poids du sac qui ralentit** : il suffit de cocher `bWeightAffectsSpeed` sur l'inventaire du joueur
- les **ennemis** (infectés + créature) : l'arme inflige déjà des dégâts et le joueur peut déjà en recevoir

> La map est un **blockout** : des formes simples pour valider le level design et le gameplay.
> On remplacera ensuite les blocs par de vrais modèles réalistes (gratuits sur Fab / Quixel Megascans).

---

## Contrôles

| Action | AZERTY | QWERTY (par défaut) |
|---|---|---|
| Avancer / reculer / gauche / droite | Z / S / Q / D | W / S / A / D (par défaut) |
| Regarder | Souris | Souris |
| Sprint | Maj gauche | Maj gauche |
| S'accroupir | Ctrl gauche ou C | Ctrl gauche ou C |
| Se pencher à gauche / à droite | A / E | Q / E |
| Sauter | Espace | Espace |
| Tirer / viser | Clic gauche / clic droit | Clic gauche / clic droit |
| Recharger | R | R |
| Ramasser / interagir | F | F |
| Lampe torche | T | T |

Pour passer en AZERTY : dans l'éditeur, cocher `bAzertyLayout` sur le personnage (`BodycamCharacter`).

---

## Installation

### Sous Windows (conseillé pour jouer et travailler à deux)

1. **Installer Unreal Engine 5** (version 5.8.3, la même pour tout le monde) avec l'**Epic Games Launcher**.
2. **Installer Visual Studio 2022 Community** (gratuit). Pendant l'installation, cocher :
   - « **Développement Desktop en C++** »
   - « **Développement de jeux en C++** » (et, à droite, les outils pour Unreal Engine)
3. Cloner ce dépôt (avec **Git LFS** installé : `git lfs install`).
4. **Clic droit sur `LastSignal.uproject` → « Switch Unreal Engine version… »** et choisir ta version d'Unreal.
5. **Double-cliquer sur `LastSignal.uproject`**. Unreal demande de recompiler les modules → **Oui**.
   (Si ça échoue : clic droit → « Generate Visual Studio project files », ouvrir `LastSignal.sln`,
   choisir *Development Editor* et faire **Générer**, puis relancer le `.uproject`.)

### Sous Linux

1. Télécharger Unreal Engine pour Linux (zip sur le site d'Epic, compte Epic requis) et le décompresser.
2. Compiler le projet :
   ```bash
   <dossier_UE>/Engine/Build/BatchFiles/Linux/Build.sh LastSignalEditor Linux Development -Project="$PWD/LastSignal.uproject" -WaitMutex
   ```
3. Lancer l'éditeur :
   ```bash
   <dossier_UE>/Engine/Binaries/Linux/UnrealEditor "$PWD/LastSignal.uproject"
   ```

---

## Créer la map et jouer

**Méthode rapide (1 clic)** : dans l'éditeur, menu **Outils → Exécuter un script Python…** (*Tools → Execute Python Script*)
et choisir `Tools/create_zone_map.py`. La map `Content/Maps/L_Zone01` est créée et sauvegardée. Appuie sur **Play**.

**Méthode manuelle** : *Fichier → Nouveau niveau → Niveau vide*, puis dans *Placer des acteurs* chercher **ZoneBuilder**
et le glisser dans la scène (position 0,0,0). Sauvegarder dans `Content/Maps/L_Zone01`.

> Astuce : même dans un niveau complètement vide, le jeu génère la zone tout seul au lancement.

Pour que la map s'ouvre au démarrage de l'éditeur : *Modifier → Paramètres du projet → Maps & Modes → Editor Startup Map*.

### Modifier la map

Sélectionner le **ZoneBuilder** dans la scène, puis dans le panneau *Détails* :
- **Seed** : change la disposition (immeubles, voitures, débris…)
- **Street Length** : longueur de la rue
- **Num Cars**, **Num Debris**, **Num Ammo Pickups**
- **Moon Intensity**, **Exposure Bias**, **Fog Density**, **Fog Color** : régler l'obscurité et l'ambiance

Tu peux aussi placer tes propres objets à ramasser (**ItemPickup**, cocher *Is Objective*) et ta propre **ExtractionZone** :
le générateur n'ajoute alors plus les siens.

---

## Organisation du code

```
Source/LastSignal/
├── Player/
│   ├── BodycamCharacter        joueur : caméra bodycam, mouvements, endurance, lampe, vie
│   └── BodycamPlayerController déformation de l'objectif (fish-eye)
├── Weapons/
│   └── BodycamWeaponComponent  arme : tir, munitions, rechargement, recul
├── Items/
│   ├── InventoryComponent      sac + poids (ralentissement prêt mais désactivé)
│   ├── ItemPickup              objet ramassable (objectif ou non)
│   └── AmmoPickup              boîte de munitions
├── Interaction/
│   └── Interactable            interface « on peut interagir avec ça »
├── World/
│   ├── ZoneBuilder             génère toute la map
│   └── ExtractionZone          point d'extraction (fumigène rouge)
├── Game/
│   └── ZoneGameMode            règles : objectifs, victoire, mort, redémarrage
└── UI/
    └── BodycamHUD              interface bodycam
```

Les réglages importants sont exposés dans l'éditeur (panneau *Détails*), rangés dans les catégories *Bodycam*, *Arme*, *Zone*, etc.

---

## Prochaines étapes proposées

1. **Tester et ajuster les sensations** (vitesse, balancement, obscurité, recul)
2. **Ennemis** : infectés (réagissent au bruit et à la lumière), puis la créature spéciale
3. **Poids du sac** qui ralentit + gestion de l'inventaire
4. **Sons** : pas, respiration, tir, ambiance (crucial pour l'horreur)
5. **Vrais modèles 3D** : remplacer le blockout par des assets réalistes (Megascans), une arme et des mains animées
6. Menus (titre, pause, options de sensibilité / disposition clavier)
