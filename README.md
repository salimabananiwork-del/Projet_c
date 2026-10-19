# svg_cli

Projet C — CODA B1

`svg_cli` est un petit éditeur de dessins qui s'utilise dans le terminal. Un
menu propose les formes (cercles, rectangles, polygones...) et pose les questions
une par une. On peut ensuite les colorier, les déplacer, les faire pivoter ou les
inverser, sauvegarder le dessin, le recharger, et l'exporter en vrai fichier
`.svg` pour le voir dans un navigateur.

J'ai pris le format SVG comme modèle, mais en le simplifiant : le but était de
garder le principe (un viewport + une liste de formes stylisées) sans recoder
tout le format.


## Compiler et lancer

```bash
make build      # compile le programme
make clean      # supprime les fichiers générés
make test       # lance les tests (tests/scenario.txt et tests/menu.txt)
```

```bash
./svg_cli                           # démarre avec un dessin vide
./svg_cli exemples/formes.dessin    # démarre avec les 9 formes du sujet
```

Pour voir le dessin, on choisit `5` dans le menu : le navigateur s'ouvre et
montre le dessin. Ensuite, chaque modification apparaît toute seule, sans rien
refaire.

Le projet compile avec `gcc -std=c99 -Wall -Wextra -pedantic` sans aucun warning.
J'ai utilisé seulement des bibliothèques standard : `stdio`, `stdlib`, `string`,
`stdbool`, `stddef`, `ctype`, `errno` et `math` (pour `sin`/`cos` dans les
rotations, d'où le `-lm`).


## Le menu

Au lancement, le programme affiche :

```
===== Menu =====
  1. Ajouter une forme
  2. Modifier une forme
  3. Voir la liste des formes
  4. Dessin (nouveau, viewport, sauver, charger, exporter)
  5. Afficher le dessin (dans le navigateur)
  0. Quitter
```

On tape un numéro, puis le programme propose la suite. Par exemple pour un
cercle :

```
> 1
--- Quelle forme ? ---
  1. Cercle
  2. Ellipse
  ...
Choix : 1
  Centre X : 30
  Centre Y : 30
  Rayon : 15
Forme 'cercle' créée (id 1).
```

Pour modifier une forme, le menu 2 affiche la liste des formes avec leur id,
demande l'id, puis propose : couleur du trait, couleur de fond, déplacer,
pivoter, inverser, supprimer ou dégrouper.

Le choix 5 ouvre le dessin dans le navigateur. La page se met à jour toute
seule : si on ajoute ou modifie une forme, elle apparaît une seconde plus tard.

Si on se trompe, une réponse vide annule l'action, et `0` revient en arrière.


## Les commandes (pour aller plus vite)

Au lieu d'un numéro, on peut aussi taper directement une commande (`aide`
les liste toutes). C'est plus rapide quand on connaît déjà les commandes, et
c'est ce format qui est utilisé dans les fichiers de sauvegarde.

**Créer des formes**

| Commande | Ce que ça fait |
|---|---|
| `cercle <cx> <cy> <r>` | un cercle |
| `ellipse <cx> <cy> <rx> <ry>` | une ellipse |
| `carre <x> <y> <cote>` | un carré |
| `rectangle <x> <y> <l> <h>` | un rectangle |
| `ligne <x1> <y1> <x2> <y2>` | une ligne |
| `polyligne <x1> <y1> <x2> <y2> ...` | plusieurs lignes à la suite |
| `polygone <x1> <y1> ...` | un polygone (3 points minimum) |
| `chemin M x y L x y H x V y Z` | un chemin |
| `groupe <id> <id> ...` | met plusieurs formes dans un groupe |
| `degrouper <id>` | défait un groupe |

**Modifier une forme**

| Commande | Ce que ça fait |
|---|---|
| `trait <id> <couleur>` | change la couleur du trait |
| `fond <id> <couleur>` | change la couleur de fond |
| `deplacer <id> <dx> <dy>` | déplace la forme |
| `pivoter <id> <angle>` | fait tourner la forme (en degrés, sens horaire) |
| `inverser <id> x\|y` | effet miroir gauche/droite (`x`) ou haut/bas (`y`) |
| `supprimer <id>` | supprime la forme |

**Le dessin**

| Commande | Ce que ça fait |
|---|---|
| `nouveau <l> <h>` | nouveau dessin vide |
| `viewport <x> <y> <l> <h>` | change la partie visible du dessin |
| `liste` | liste les formes avec leur id |
| `sauver <fichier>` | sauvegarde le dessin |
| `charger <fichier>` | charge un dessin |
| `exporter <fichier.svg>` | exporte en SVG (pour garder une image du dessin) |
| `quitter` | ferme le programme (comme `0` dans le menu) |

Quelques règles :
- chaque forme a un **id**, affiché quand on la crée et avec `liste` ;
- les coordonnées sont des entiers et l'axe Y va vers le bas, comme en SVG ;
- on peut écrire les points `10 20` ou `10,20` ;
- une couleur, c'est un nom (`red`, `blue`...), un code (`#ff0000`) ou `none` ;
- sur un groupe, `trait` et `fond` colorent toutes ses formes, et
  `deplacer` / `pivoter` / `inverser` bougent le groupe en entier.


## Exemple avec les commandes

```
> nouveau 100 60
> cercle 30 30 15
Forme 'cercle' créée (id 1).
> fond 1 red
> rectangle 55 20 30 20
Forme 'rectangle' créée (id 2).
> pivoter 2 45
> sauver dessin.dessin
> exporter dessin.svg
```


## Comment j'ai organisé le code

J'ai découpé le projet en plusieurs fichiers, chacun avec un rôle précis :

```
src/
  main.c          la boucle principale : affiche le menu et lit le clavier
  menu.c/h        le menu : propose les choix et pose les questions
  commands.c/h    lit une commande, vérifie les arguments et l'exécute
  shape.c/h       la structure Shape : créer, colorier, transformer, afficher
  shape_list.c/h  la liste chaînée de formes
  canvas.c/h      le dessin : le viewport + la liste des formes
  geometry.c/h    calcule le centre des formes (pour la rotation et le miroir)
  file_io.c/h     sauvegarde, chargement et export SVG
tests/scenario.txt   test avec les commandes
tests/menu.txt       test avec le menu
exemples/            formes.dessin (les 9 formes du sujet) et son export SVG
```

Toutes les fonctions sont commentées au format javadoc dans les fichiers `.h`.


## Mes choix

**Une liste chaînée pour les formes.** On peut supprimer une forme au milieu ou
la mettre dans un groupe juste en changeant des pointeurs, sans décaler tout un
tableau. Je garde un pointeur sur la fin de la liste pour ajouter une forme en
O(1). L'ordre de la liste est aussi l'ordre de dessin : la dernière forme est
dessinée au-dessus, comme en SVG. Le défaut, c'est que chercher une forme par
son id prend O(n), mais avec quelques centaines de formes ça ne se sent pas.

**Les groupes forment un arbre.** Un groupe contient sa propre liste de formes,
donc un groupe peut contenir un autre groupe. C'est pour ça que plusieurs
fonctions sont récursives (chercher, libérer, afficher, sauvegarder).

**Une union dans `Shape`.** Un cercle n'a pas besoin des mêmes données qu'un
rectangle ou qu'un polygone. Avec une union, chaque forme ne prend que la place
de ses propres données, et le champ `type` dit quelle partie lire.

**Une table de commandes.** Chaque commande est une ligne d'un tableau : son nom,
son nombre d'arguments et un pointeur vers sa fonction. Les vérifications
communes sont faites une seule fois, et ajouter une commande revient à ajouter
une ligne (c'est comme ça que j'ai ajouté `degrouper`).

**Je ne touche jamais aux coordonnées de départ.** Quand on déplace ou pivote
une forme, je stocke juste la transformation dans son style. Comme ça il n'y a
pas d'erreurs d'arrondi qui s'accumulent, et la sauvegarde est exacte. C'est le
même principe que l'attribut `transform` du SVG.

**Le fichier de sauvegarde contient des commandes.** Pour charger un dessin, je
relis donc ces commandes avec le même code que pour le clavier. Le chargement se
fait dans une copie : si le fichier a une erreur, le dessin en cours n'est pas
perdu.

**Un menu qui fabrique des commandes.** Le menu ne crée pas les formes
lui-même : il pose les questions, assemble les réponses en une commande (par
exemple `cercle 30 30 15`) et l'envoie au même code que les commandes tapées.
Toutes les vérifications (nombres valides, rayon positif, couleur correcte...)
ne sont donc écrites qu'une fois. Les choix du menu sont eux aussi rangés dans
des tableaux (texte affiché, commande, questions), comme la table de commandes.

**Voir le dessin dans le navigateur.** Je ne dessine pas les formes dans le
terminal : c'est le navigateur qui affiche le SVG, avec les vraies couleurs (le
viewport devient le `viewBox` du SVG). Le choix 5 écrit une page `apercu.html`
qui contient le dessin, et l'ouvre avec `system("xdg-open ...")` (`system` fait
partie de `stdlib.h`). Après chaque action, le programme réécrit cette page, et
une balise `<meta http-equiv="refresh">` la fait se recharger toute seule chaque
seconde. On voit donc le dessin changer en direct.


## Tests

`make test` lance deux scénarios :
- `tests/scenario.txt` utilise les commandes : toutes les formes, les styles,
  les transformations, les groupes, la sauvegarde, le chargement et l'export ;
- `tests/menu.txt` fait la même chose en passant par le menu.

Ils testent aussi des erreurs de saisie (rayon négatif, lettres à la place de
nombres, couleur invalide, mauvais numéro de menu, réponse vide, fichier
introuvable...) pour vérifier que le programme affiche un message et continue.

J'ai aussi vérifié avec AddressSanitizer qu'il n'y a ni fuite mémoire ni accès
mémoire invalide.


## Les difficultés que j'ai rencontrées

**Pivoter ou inverser un groupe.** Pour faire tourner un groupe, il faut connaître
son centre, mais ce centre dépend des formes qu'il contient, qui peuvent
elles-mêmes être déplacées ou pivotées. J'ai résolu ça en transformant le groupe
en points de façon récursive : chaque forme est d'abord transformée, puis on
calcule le centre du groupe et on applique sa transformation à l'ensemble.

**Ne pas perdre le dessin si le fichier chargé est faux.** Au début, si un
fichier avait une erreur au milieu, le dessin en cours était à moitié remplacé.
Maintenant je charge dans un dessin temporaire, et je ne le garde que si tout le
fichier est correct. Sinon je le libère et le dessin d'avant reste intact.

**Respecter le C99 strict.** Avec `-std=c99 -pedantic`, `M_PI` et `strdup`
n'existent pas. J'ai donc défini ma propre constante `PI`, et les couleurs sont
stockées dans un tableau de taille fixe que je remplis avec `strncpy`, sans
avoir besoin de `strdup`.

**Lire les réponses du menu.** `fgets` garde le retour à la ligne, et une ligne
trop longue reste à moitié dans le clavier et fausse la question suivante. J'ai
écrit une seule fonction `read_line` qui enlève le `\n` et vide la fin d'une
ligne trop longue, utilisée pour tout ce qui est tapé au clavier.

**Dégrouper.** Remettre les formes d'un groupe dans la liste principale n'était
pas compliqué en soi. Le problème, c'est la transformation du groupe : une
rotation autour du centre du groupe ne peut pas devenir une simple rotation de
chaque forme autour de son propre centre. J'ai préféré refuser ce cas avec un
message clair plutôt que de donner un résultat faux.


## Ce que j'améliorerais

- Chercher une forme par son id prend O(n). Avec une table de hachage, ce serait
  plus rapide.
- Après un chargement, les id des formes sont renumérotés.
- `degrouper` refuse un groupe qui a été déplacé, pivoté ou inversé : sa
  rotation se fait autour du centre du groupe, et je ne peux pas la reporter
  exactement sur chaque forme.
- Les chemins ne gèrent que `M L H V Z` (pas de courbes).
- L'ouverture automatique du navigateur utilise `xdg-open`, qui n'existe que
  sous Linux. Ailleurs, il faut ouvrir `apercu.html` à la main.
- La page d'aperçu se recharge toutes les secondes, même quand rien ne change.
