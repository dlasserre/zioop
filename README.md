# Zioop

Une arène C++ où des modèles de langage récoltent, s'appellent et se battent.

Zioop est un jeu multi-agents inspiré de Zappy. Un serveur C++ fait autorité sur une
grille où apparaissent des ressources. Des agents s'y connectent en TCP pour se
déplacer, observer, récolter, monter de niveau par des rituels et attaquer leurs
adversaires. Les agents peuvent être pilotés par un LLM (Codex ou Claude, via le
client TypeScript) ou par un bot C++ à règles. Une interface 3D raylib affiche la
partie en temps réel.

```
 clients/ai  (TypeScript : Codex, Claude) ──┐
 zioop_ai    (bot C++ à règles)          ───┼── TCP 127.0.0.1:9090 ──► zioop_server
 zioop_gui   (rendu 3D raylib)           ───┘                          (core::Game)
```

## Composants

| Cible / dossier | Rôle |
|---|---|
| `zioop_core` (`src/zioop/core`) | Règles du jeu : grille (`Matrix`, `Cell`), agents, ressources, rituels, attaques. `core::Game` protège l'état par un mutex. |
| `zioop_protocol` (`src/zioop/network/Protocol.cpp`) | Encodage et décodage des trames, partagés par le serveur, la GUI et le bot. |
| `zioop_network` (`src/zioop/network`) | Serveur TCP Winsock : connexions, distribution des commandes, sérialisation JSON de l'état. |
| `zioop_server` (`app/server_main.cpp`) | Exécutable serveur. Boucle de jeu à 100 ms. |
| `zioop_gui` (`app/gui_main.cpp`, `src/zioop/gui`) | Client d'observation 3D (raylib) qui interroge le serveur avec `GET_MATRIX`. |
| `zioop_ai` (`app/ai_main.cpp`, `src/zioop/ai`) | Bot C++ à règles (`ai::Claude`, malgré son nom, n'appelle aucun modèle). |
| `clients/ai` | Client TypeScript qui fait jouer Codex ou Claude via leurs SDK officiels. |
| `tests` | Tests des rituels (CTest) et test réseau de bout en bout (Python). |

## Règles du jeu

- **Monde** : une grille de taille choisie au lancement du serveur. Chaque case
  accueille au plus 4 agents et fait apparaître des ressources (pomme, gâteau,
  viande, eau magique) qui pourrissent avec le temps.
- **Agents** : chaque `HELLO` crée un agent placé dans un coin de la grille, au
  niveau 1 avec 100 points de vie. L'agent disparaît quand son client se déconnecte.
- **Vision** : `LOOK` renvoie un cône dans la direction demandée. Sa profondeur
  est égale au niveau de l'agent.
- **Rituel** : consomme des ressources qui dépendent du niveau (pommes et viande,
  puis gâteaux aux niveaux supérieurs) et dure 30 s × le niveau visé. Pendant le
  rituel, l'agent ne peut ni se déplacer, ni ramasser, ni attaquer. Le serveur
  répond `OK` à la fin du rituel avec le nouveau niveau. Voir
  [docs/rituel.md](docs/rituel.md) et `m_requiredResources` dans
  [Agent.hpp](includes/zioop/core/Agent.hpp).
- **Attaque** : la cible doit se trouver sur la même case, appartenir à un autre
  client et avoir un niveau inférieur ou égal à celui de l'attaquant. Les dégâts
  valent 2 × le niveau de l'attaquant ; à 0 point de vie, la cible est éliminée.

## Prérequis

- **Windows** : le réseau repose sur Winsock (`ws2_32`). Le projet est développé
  avec CLion et MSVC.
- **CMake 3.20+** et un compilateur **C++23**.
- Les dépendances sont téléchargées automatiquement par `FetchContent` dans
  `.deps/` : [raylib 5.5](https://github.com/raysan5/raylib) (GUI uniquement) et
  [nlohmann/json 3.12.0](https://github.com/nlohmann/json).
- **Node.js 22.9+** pour les agents LLM de `clients/ai`.
- **Python 3** pour le test réseau (facultatif).

## Compiler

Depuis un terminal où MSVC est disponible (Developer PowerShell ou `vcvars64.bat`) :

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

Les exécutables sont placés dans `build/bin/<Config>/`, par exemple
`build/bin/Debug/zioop_server.exe`. Les assets sont copiés à côté de
`zioop_gui.exe` après chaque compilation.

| Option CMake | Défaut | Effet |
|---|---|---|
| `ZIOOP_BUILD_GUI` | `ON` | Compile la GUI et télécharge raylib. |
| `ZIOOP_BUILD_AI` | `ON` | Compile la bibliothèque du bot C++. |
| `BUILD_TESTING` | `ON` | Compile les tests des rituels et leur serveur de test. |

> **MSVC en français :** si vous compilez avec Ninja depuis un terminal plutôt
> que depuis CLion, exécutez `chcp 850` dans la même session `cmd` que
> `vcvars64.bat`. Sinon Ninja ne reconnaît pas le préfixe localisé de
> `/showIncludes` et n'enregistre aucune dépendance d'en-tête : modifier un
> `.hpp` ne déclenche alors plus de recompilation.

## Lancer une partie

1. **Serveur** : largeur et hauteur de la grille en arguments. Il écoute sur
   `127.0.0.1:9090` ; Ctrl+C l'arrête proprement.

   ```powershell
   .\build\bin\Debug\zioop_server.exe 20 20
   ```

2. **GUI** (facultatif) : se connecte au même serveur et rafraîchit l'état
   régulièrement. Caméra libre raylib (clavier et souris) ; `R` recentre la vue.

   ```powershell
   .\build\bin\Debug\zioop_gui.exe
   ```

3. **Agents** :

   - Agents LLM : voir [clients/ai/README.md](clients/ai/README.md) pour
     l'installation et les clés API.

     ```powershell
     cd clients/ai
     npm ci
     npm run build
     npm run start:claude -- --host 127.0.0.1 --port 9090
     ```

   - Bot C++ à règles :

     ```powershell
     .\build\bin\Debug\zioop_ai.exe
     ```

## Protocole

Chaque trame commence par un en-tête de 6 octets en ordre réseau (big-endian) :
le type sur `uint16`, puis la longueur du payload sur `uint32`. Le payload qui suit
est du JSON. Un client envoie une commande et attend sa réponse avant d'envoyer
la suivante, car les trames ne portent pas d'identifiant de requête.

| Id | Commande | Payload | Réponse `OK` |
|---|---|---|---|
| 0 | `HELLO` | — | `{agentID, position: {x, y}}` |
| 1 | `MOVE` | `{agentID, direction}` | nouvelle position |
| 3 | `LOOK` | `{agentID, direction}` | `{cells: [{position, resources, agents}]}` |
| 4 | `TAKE` | `{agentID, type}` | inventaire |
| 6 | `ATTACK` | `{agentID, targetId}` | `{targetId, killed}` |
| 8 | `INVENTORY` | `{agentID}` | `{resources: [{type, quantity}]}` |
| 9 | `RITUAL` | `{agentID}` | différée : `{agentID, level}` à la fin du rituel |
| 10 | `DISCONNECT` | — | — |
| 11 | `GET_MATRIX` | — | instantané complet (grille, ressources, agents) |

- Réponses : `OK` = 0, `KO` = 1 (le payload explique le refus).
- Directions : `NORTH` = 0, `SOUTH` = 90, `EAST` = 180, `WEST` = 270.
- Ressources : `APPLE` = 0, `CAKE` = 1, `MEAT` = 2, `MAGIC_WATER` = 3.
- `ROTATION` (2), `DROP` (5), `BROADCAST` (7) et `GET_CLIENTS` (12) sont déclarés
  dans [Protocol.hpp](includes/zioop/network/Protocol.hpp) mais pas encore
  traités par le serveur.

## Tests

```powershell
# Tests unitaires des rituels (CTest)
ctest --test-dir build --output-on-failure

# Test réseau de bout en bout (~60 s, port local choisi automatiquement)
python tests/ritual_network.py build/bin/Debug/zioop_ritual_server.exe

# Client TypeScript : serveur simulé, sans appel aux API
cd clients/ai
npm test
```

## Structure du dépôt

```
app/          points d'entrée : serveur, GUI, bot C++
includes/     en-têtes publics (zioop/core, network, gui, ai)
src/zioop/    implémentations et CMakeLists par module
clients/ai/   agents LLM en TypeScript (Codex, Claude)
assets/       modèles glTF, textures et icônes de la GUI
cmake/        récupération des dépendances
docs/         notes de conception et modèle UML StarUML
tests/        tests des rituels et test réseau Python
```

## Documentation

- [docs/rituel.md](docs/rituel.md) : déroulement d'un rituel côté serveur.
- [docs/architecture-client-serveur.md](docs/architecture-client-serveur.md) :
  threads, connexions et choix réseau.
- [docs/uml/README.md](docs/uml/README.md) : diagrammes de classes à ouvrir
  avec StarUML.
- [clients/ai/README.md](clients/ai/README.md) : agents LLM, historique des
  parties et tests.

## Limites actuelles

- Windows uniquement, et adresse et port codés en dur (`127.0.0.1:9090`).
- Un agent est supprimé quand son client se déconnecte ; il n'y a pas de reprise.
- `network::Client` est déclaré mais pas encore implémenté.
- `zioop_bot` est une cible de brouillon.
