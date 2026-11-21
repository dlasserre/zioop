# Agents Codex et Claude pour Zioop

Client autonome TypeScript avec les SDK officiels `@openai/codex-sdk` et
`@anthropic-ai/sdk` (API Messages de Claude). Chaque agent se connecte au serveur
TCP existant, récolte les ressources, lance ses rituels et combat. Le client est
conçu pour limiter le nombre de tokens consommés par partie.

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

Le fichier `.env` est ignoré par Git. La clé va uniquement au SDK concerné, jamais
dans le prompt ni dans les messages TCP envoyés à Zioop.

```powershell
npm run start:claude -- --model haiku --max-decisions 50 -v
npm run start:claude -- --model opus --effort medium
npm run start:codex -- --max-decisions 100
```

| Option | Effet |
|---|---|
| `--model`, `-m` | Claude : `haiku`, `sonnet`, `opus` (défaut) ou `fable`, ou un identifiant complet `claude-*`. Codex : identifiant du modèle. Équivalent : `CLAUDE_MODEL` / `CODEX_MODEL`. |
| `--effort` | Claude : `low` (défaut), `medium`, `high`, `xhigh` ou `max`. Plus l'effort est élevé, plus la réflexion coûte de tokens de sortie. Équivalent : `CLAUDE_EFFORT`. |
| `--max-decisions` | Nombre maximal d'appels à l'IA (100 par défaut). Chaque appel produit un plan de plusieurs actions. |
| `-v`, `--verbose` | Affiche le prompt envoyé, les réponses du serveur, les événements et les tokens de chaque appel. |

Alias et tarifs utilisés pour l'estimation des coûts (dollars par million de tokens) :

| Alias | Modèle | Entrée | Sortie | Lecture cache | Écriture cache |
|---|---|---|---|---|---|
| `haiku` | `claude-haiku-5-5` | 0,10 | 0,50 | 0,01 (estimé) | 0,125 |
| `sonnet` | `claude-sonnet-5-5` | 2 | 10 | 0,20 | 2,50 |
| `opus` | `claude-opus-5-5` | 4 | 20 | 0,20 | 5 |
| `fable` | `claude-fable-5-1` | 10 | 50 | 0,25 | 12,50 |

Pour `sonnet`, `opus` et `fable`, le client active le repli automatique côté serveur
(`fallbacks: "default"`) : si le modèle refuse une requête, l'API la rejoue sur un
autre modèle au lieu d'échouer.

## Stratégie d'économie de tokens

1. **Aucun historique de conversation.** Chaque appel est indépendant : le modèle
   reçoit uniquement les règles du jeu et un état compact (environ 500 caractères).
   Le coût d'un appel reste constant pendant toute la partie au lieu de croître.
2. **Mémoire écrite par le modèle.** Le champ `memory` (200 caractères maximum)
   est la seule mémoire entre deux appels. Le modèle y note son plan, les zones
   intéressantes et les menaces ; le client le lui renvoie au tour suivant.
3. **Règles en cache.** Le prompt système est identique d'un appel à l'autre et
   marqué `cache_control` : après le premier appel, il est relu depuis le cache à
   environ 5 % du prix (Opus) au lieu du plein tarif.
4. **Plans de plusieurs actions.** Le modèle renvoie jusqu'à 8 actions exécutées
   à la suite. Il n'est rappelé que si une action échoue, si un agent apparaît sur
   sa case, si un message arrive, après un rituel ou à la fin du plan.
5. **Automatismes sans IA.** Après chaque déplacement, le client fait un `LOOK`,
   prend les ressources utiles au prochain rituel et lance `RITUAL` dès que
   l'inventaire suffit. Il attend la fin du rituel (événement `Ritual`) sans appeler
   le modèle, et s'arrête à la réception de l'événement `Died`.
6. **Réflexion limitée.** L'effort `low` par défaut réduit les tokens de réflexion,
   qui sont facturés comme des tokens de sortie.

Le bilan de fin de partie et `session.json` donnent le nombre d'appels, d'actions
par appel, les tokens (entrée, cache lu, cache écrit, sortie), le taux de cache et
le coût estimé par appel et par action.

## Plan renvoyé par le modèle

```json
{ "plan": [{ "action": "MOVE", "arg": "E" }, { "action": "MOVE", "arg": "E" }], "memory": "...", "why": "..." }
```

| Action | Argument |
|---|---|
| `MOVE`, `LOOK` | `N`, `S`, `E` ou `W` |
| `TAKE` | `a` (pomme), `c` (gâteau), `m` (viande) ou `w` (eau magique) |
| `ATTACK` | identifiant de l'agent ciblé |
| `INVENTORY`, `RITUAL`, `STOP` | vide |

Le client valide chaque action, ajoute l'identifiant de l'agent reçu au `HELLO` et
traduit les directions vers le protocole (NORTH=0, SOUTH=90, EAST=180, WEST=270).
Il n'a qu'une requête TCP en attente et ne relance jamais une commande après une
coupure ou un timeout.

## Journal

Chaque partie crée un dossier `runs/<fournisseur-date-identifiant>/` contenant :

- `history.jsonl` : actions, plans, réponses réelles `OK`/`KO`, tokens et latence de chaque appel ;
- `session.json` : statut, modèle, état final et métriques de coût.

```powershell
npm run history -- --file runs/<session>/history.jsonl --last 20
```

## Tests

```powershell
npm test
```

Les tests utilisent un serveur TCP simulé et des décisions déterministes : ils ne
consomment aucun crédit API. Ils vérifient le protocole, les plans, les
automatismes, les rituels par événement, la mort de l'agent, la construction des
requêtes Claude (cache, effort, sortie structurée, repli) et le calcul des coûts.
