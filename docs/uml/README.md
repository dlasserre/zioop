# UML de Zioop pour StarUML

Ouvrir [zioop.mdj](zioop.mdj) dans **StarUML** (`File > Open`). Le fichier est un projet StarUML natif : classes, attributs, opérations et relations y sont éditables. Il contient cinq diagrammes :

1. Vue d’ensemble
2. Moteur C++
3. Réseau C++
4. GUI et bots C++
5. Client IA TypeScript

Le modèle reflète les fichiers de travail du 24 septembre 2026, modifications locales incluses. Les méthodes affichent leurs noms et types de retour ; certains détails de signatures sont abrégés pour garder les diagrammes lisibles. Dans le client TypeScript, les éléments portant un stéréotype `module` représentent des fichiers de fonctions ; les stéréotypes `type` représentent des types TypeScript.

`network::Client` est déclaré, mais son `.cpp` n'a pas encore d'implémentation. `ai::Test` correspond à du code ancien, non inclus dans `ZIOOP_AI_SOURCES`. `ai::Claude` est le bot C++ à règles ; l'intégration du SDK Claude est dans `clients/ai`.

`Server` possède les `Connection`. Chaque connexion acceptée porte son `clientId`, attribué par le serveur et utilisé pour vérifier à quel client appartient un agent. `ClientSession`, qui ne contenait que cet identifiant, a été supprimé. L'`agentId` est distinct du `clientId` : un client peut posséder plusieurs agents.

Les clients ne dépendent pas de la classe `Server`. Les flèches vers celle-ci ont été retirées : elles confondaient un échange TCP à l'exécution avec une dépendance de code. La GUI et le bot C++ utilisent la bibliothèque `zioop_protocol`, isolée de l'implémentation serveur ; leurs exécutables ne lient plus `zioop_network` ni `zioop_core`. Le bot partage seulement l'enum `core::Direction` définie dans `Types.hpp`. Le client TypeScript implémente le même format de trame dans `clients/ai/src/protocol.ts`. Le serveur reste nécessaire pour jouer à la partie en ligne et fournir l'état officiel du jeu.

Le fichier peut être régénéré avec :

```powershell
node docs/uml/generate-staruml.cjs
```

Validation : le projet `.mdj` a été chargé par la CLI de StarUML, qui a exporté ses cinq diagrammes en SVG et en PNG. Les images de vérification ont été retirées après contrôle ; le fichier à ouvrir est `zioop.mdj`.

Documentation : [gestion des projets StarUML](https://docs.staruml.io/user-guide/managing-project), [interface en ligne de commande](https://docs.staruml.io/v5/user-guide/cli-command-line-interface).
