# Agents Codex et Claude pour Zioop

Client autonome TypeScript avec les SDK officiels `@openai/codex-sdk` et
`@anthropic-ai/claude-agent-sdk`. Chaque agent se connecte au serveur TCP existant,
récolte les fruits et choisit quand lancer un rituel. Les deux moteurs partagent
les commandes et la gestion du contexte. Aucun changement du serveur ou du
protocole n'est nécessaire.

## Installation et lancement

Prérequis : Node.js 22.9 ou plus récent, npm, un serveur Zioop démarré et une clé API
du fournisseur choisi : OpenAI pour Codex, Anthropic pour Claude.

```powershell
cd C:\Users\damie\CLionProjects\zioop\clients\ai
npm ci
npm run build
Copy-Item .env.example .env
```

Renseigner les clés nécessaires dans `.env`, ou dans l'environnement du processus :

```dotenv
OPENAI_API_KEY=ta_cle_openai
ANTHROPIC_API_KEY=ta_cle_anthropic
```

Une seule clé suffit pour lancer le fournisseur correspondant. Le fichier `.env`
est ignoré par Git. La clé va uniquement au SDK concerné, jamais dans le prompt
ni dans les messages TCP envoyés à Zioop. Les appels utilisent la facturation API
du fournisseur.

Lancer l'un des deux agents :

```powershell
npm run start:codex -- --host 127.0.0.1 --port 9090 --max-decisions 100
npm run start:claude -- --host 127.0.0.1 --port 9090 --max-decisions 100
```

`CODEX_MODEL` ou `CLAUDE_MODEL` dans `.env`, ou `--model NOM`, permet de choisir un
modèle accessible avec la clé correspondante. Si ce paramètre est absent, le SDK
choisit son modèle par défaut. `CODEX_API_KEY` est également accepté si
`OPENAI_API_KEY` est absent. `npm start -- --provider claude` fonctionne aussi ;
sans option, `npm start` utilise `ZIOOP_PROVIDER`, puis Codex par défaut.

La limite par défaut est de 100 décisions (les consultations d'historique comptent
aussi). Ctrl+C interrompt l'appel au modèle ou l'attente réseau et déconnecte l'agent.

## Commandes comprises par l'agent

| Commande | Effet |
|---|---|
| `MOVE` | Avancer d'une case dans une direction. |
| `LOOK` | Consulter les cellules visibles : positions, ressources et autres agents (`id`, `level`). |
| `TAKE` | Prendre un fruit sur la cellule actuelle. |
| `ATTACK` | Éliminer un agent de sa cellule, de niveau inférieur ou égal. |
| `INVENTORY` | Consulter les quantités transportées. |
| `RITUAL` | Dépenser 10 fruits et attendre le nouveau niveau. |
| `HISTORY` | Consulter une page de l'historique, côté client uniquement. |
| `STOP` | Arrêter le client. |

Les directions sont traduites vers les valeurs réelles du protocole :
NORTH=0, SOUTH=90, EAST=180, WEST=270. L'identifiant de l'agent provient du `HELLO`
du serveur ; il est ajouté par le client, pas choisi par le modèle.
Les décisions sont produites sous schéma JSON puis validées localement.
Pour `ATTACK`, le modèle choisit seulement `target`, l'identifiant d'un agent vu par
`LOOK` ; le client l'envoie dans le champ `targetId` attendu par le serveur.
L'état transmis au modèle garde la dernière position et le niveau de chaque agent
vu (`agents`), ainsi que le nombre d'éliminations (`kills`). Le serveur ne prévient
pas un agent éliminé : ses commandes suivantes répondent KO, et le modèle a pour
consigne de s'arrêter avec `STOP`.
`DROP`, `BROADCAST` et `ROTATION` ne sont pas exposées car le serveur
ne les traite pas encore.

Le client n'a qu'une requête TCP en attente. Pour `RITUAL`, le délai autorisé
est de `30 s × niveau cible + 15 s` ; les autres requêtes ont un délai de 10 s.
Il ne relance jamais une commande après une coupure ou un timeout, car son résultat
pourrait être inconnu. Une déconnexion termine la partie locale.

## Contexte et historique

Chaque partie crée un dossier `runs/<fournisseur-date-identifiant>/` contenant :

- `history.jsonl` : actions, intentions courtes, paramètres, réponses réelles
  `OK`/`KO`, numéros de séquence, dates et durées ;
- `session.json` : fournisseur, état connu, notes de mémoire et identifiant de conversation ;
- `codex-home/` ou `claude-home/` : conversation persistée par le SDK, isolée de ta configuration personnelle.

Les décisions utilisent la même conversation pendant toute la connexion :
un même thread Codex ou une session Claude reprise via son identifiant `session_id`.
Chaque tour reçoit aussi l'état connu, le dernier résultat, les douze dernières
entrées du journal, les positions visitées et les notes de mémoire du modèle.
`HISTORY` donne accès aux événements plus anciens : `historyLimit` de 1 à 20,
`historyBefore` exclusif, ou `null` pour la dernière page. Les gros résultats sont
tronqués dans le prompt pour borner sa taille ; le journal conserve les réponses complètes.

Pour consulter toi-même le journal :

```powershell
npm run history -- --file runs/<session>/history.jsonl --last 20
```

Une nouvelle connexion démarre un **nouvel agent** et une **nouvelle conversation**.
Le serveur actuel supprime l'agent à la déconnexion et ne propose pas de reprise
d'identité : réutiliser aveuglément un ancien état serait incorrect. Les anciens
journaux restent consultables avec la commande ci-dessus.

Codex travaille dans un dossier de contexte en lecture seule, avec le shell,
la recherche web et les sous-agents désactivés. Claude utilise une liste vide
d'outils, de skills et de serveurs MCP ; il ne charge pas les réglages ni les
plugins personnels. Le client assure lui-même les commandes du jeu et l'écriture
du journal.

## Tests

```powershell
npm test
```

Les tests utilisent un serveur TCP simulé et des décisions déterministes :
ils ne consomment aucun crédit API. Ils vérifient les trames fragmentées,
l'historique, le contexte des deux moteurs, les commandes invalides, le rituel
différé, les attaques, les timeouts et l'interruption. Les tests de l'adaptateur Claude simulent
le SDK pour vérifier la reprise de session, le JSON structuré et les erreurs API.
Un essai réel des modèles nécessite les clés correspondantes.

Références officielles : [SDK Codex](https://learn.chatgpt.com/docs/codex-sdk),
[authentification OpenAI](https://learn.chatgpt.com/docs/auth),
[sessions Claude](https://code.claude.com/docs/en/agent-sdk/sessions),
[sorties structurées Claude](https://code.claude.com/docs/en/agent-sdk/structured-outputs).
