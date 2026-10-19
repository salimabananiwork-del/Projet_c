/**
 * Implémentation des commandes.
 * Chaque commande est décrite dans un tableau (nom, nombre d'arguments,
 * fonction à appeler) : ajouter une commande = ajouter une ligne au tableau.
 */
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "commands.h"
#include "shape_list.h"
#include "file_io.h"
#include "geometry.h"

/** Nombre maximal de mots sur une ligne. */
#define MAX_ARGS 512

/** Valeur absolue maximale d'une coordonnée (évite les débordements). */
#define COORD_MAX 100000

/** Séparateurs entre les mots (la virgule permet d'écrire "10,20"). */
#define SEPARATORS " \t\r\n,"

/** Fonction associée à une commande. Renvoie false en cas d'erreur. */
typedef bool (*CmdHandler)(Canvas *c, int argc, char **argv, bool verbose);

/** Description d'une commande. */
typedef struct {
    const char *name;
    const char *usage;
    const char *desc;
    int min_args;          /**< nombre minimal d'arguments */
    int max_args;          /**< nombre maximal (-1 = illimité) */
    bool interactive_only; /**< interdite dans un fichier chargé */
    CmdHandler handler;    /**< NULL pour "quitter" */
} Command;

/* ------------------------------------------------------------------ */
/*  Outils de lecture des arguments                                    */
/* ------------------------------------------------------------------ */

/**
 * Convertit un texte en entier, avec vérification.
 * @param s texte
 * @param out entier lu
 * @return false (avec message) si le texte n'est pas un entier valide
 */
static bool parse_int(const char *s, int *out)
{
    char *end;
    errno = 0;
    long v = strtol(s, &end, 10);
    if (end == s || *end != '\0' || errno == ERANGE
        || v < -COORD_MAX || v > COORD_MAX) {
        fprintf(stderr, "Erreur : '%s' n'est pas un entier valide "
                        "(entre %d et %d).\n", s, -COORD_MAX, COORD_MAX);
        return false;
    }
    *out = (int)v;
    return true;
}

/**
 * Lit plusieurs entiers d'un coup.
 * @param argv textes
 * @param n nombre d'entiers à lire
 * @param out tableau résultat
 * @return false si l'un d'eux est invalide
 */
static bool parse_ints(char **argv, int n, int *out)
{
    for (int i = 0; i < n; i++) {
        if (!parse_int(argv[i], &out[i])) {
            return false;
        }
    }
    return true;
}

/**
 * Vérifie qu'une valeur est strictement positive.
 * @param v valeur
 * @param what nom de la valeur pour le message
 * @return false (avec message) si v <= 0
 */
static bool check_positive(int v, const char *what)
{
    if (v <= 0) {
        fprintf(stderr, "Erreur : %s doit être strictement positif.\n", what);
        return false;
    }
    return true;
}

/**
 * Vérifie qu'une couleur est acceptable : un nom en lettres (red, none...)
 * ou un code '#' suivi de 3 ou 6 chiffres hexadécimaux (#f00, #ff0000).
 * @param color couleur
 * @return false (avec message) si invalide
 */
static bool check_color(const char *color)
{
    size_t len = strlen(color);
    bool ok = len > 0 && len < COLOR_LEN;
    if (ok && color[0] == '#') {
        ok = (len == 4 || len == 7);
        for (size_t i = 1; ok && i < len; i++) {
            ok = isxdigit((unsigned char)color[i]);
        }
    } else {
        for (size_t i = 0; ok && i < len; i++) {
            ok = isalpha((unsigned char)color[i]);
        }
    }
    if (!ok) {
        fprintf(stderr, "Erreur : couleur '%s' invalide (ex : red, none, #ff0000).\n",
                color);
    }
    return ok;
}

/**
 * Lit un id et cherche la forme correspondante (même dans un groupe).
 * @param c dessin
 * @param arg texte de l'id
 * @return la forme, ou NULL (avec message)
 */
static Shape *find_shape(Canvas *c, const char *arg)
{
    int id;
    if (!parse_int(arg, &id)) {
        return NULL;
    }
    Shape *s = list_find(&c->shapes, id);
    if (s == NULL) {
        fprintf(stderr, "Erreur : aucune forme avec l'id %d.\n", id);
    }
    return s;
}

/**
 * Ajoute une forme nouvellement créée au dessin.
 * @param c dessin
 * @param s forme (NULL si la création a échoué)
 * @param verbose true pour afficher un message
 * @return false si s est NULL
 */
static bool add_shape(Canvas *c, Shape *s, bool verbose)
{
    if (s == NULL) {
        fprintf(stderr, "Erreur : mémoire insuffisante.\n");
        return false;
    }
    list_append(&c->shapes, s);
    c->next_id++;
    if (verbose) {
        printf("Forme '%s' créée (id %d).\n", shape_type_name(s->type), s->id);
    }
    return true;
}

/**
 * Lit une liste de coordonnées "x1 y1 x2 y2 ..." en tableau de points.
 * @param argc nombre de textes (doit être pair)
 * @param argv textes
 * @param n nombre de points lus
 * @return tableau alloué (à libérer), ou NULL en cas d'erreur
 */
static Point *parse_points(int argc, char **argv, int *n)
{
    if (argc % 2 != 0) {
        fprintf(stderr, "Erreur : il faut un nombre pair de coordonnées.\n");
        return NULL;
    }
    *n = argc / 2;
    Point *pts = malloc(*n * sizeof(Point));
    if (pts == NULL) {
        fprintf(stderr, "Erreur : mémoire insuffisante.\n");
        return NULL;
    }
    for (int i = 0; i < *n; i++) {
        if (!parse_int(argv[2 * i], &pts[i].x)
            || !parse_int(argv[2 * i + 1], &pts[i].y)) {
            free(pts);
            return NULL;
        }
    }
    return pts;
}

/* ------------------------------------------------------------------ */
/*  Lecture d'un chemin (syntaxe proche de l'attribut "d" du SVG)       */
/* ------------------------------------------------------------------ */

/**
 * Ajoute une instruction au tableau dynamique du chemin.
 * @return false si la mémoire manque
 */
static bool path_push(PathCmd **cmds, int *n, int *cap, char cmd, Point p)
{
    if (*n == *cap) {
        int new_cap = (*cap == 0) ? 8 : *cap * 2;
        PathCmd *tmp = realloc(*cmds, new_cap * sizeof(PathCmd));
        if (tmp == NULL) {
            return false;
        }
        *cmds = tmp;
        *cap = new_cap;
    }
    (*cmds)[*n].cmd = cmd;
    (*cmds)[*n].p = p;
    (*n)++;
    return true;
}

/**
 * Lit un chemin à partir des mots de la ligne.
 * Instructions acceptées (coordonnées absolues) :
 *   M x y (aller à), L x y (ligne), H x (horizontale), V y (verticale), Z (fermer).
 * H et V sont convertis en L. Une lettre peut être collée au nombre : "M10 10".
 * @param argc nombre de mots
 * @param argv mots
 * @param out tableau d'instructions alloué (à libérer)
 * @param n nombre d'instructions
 * @return false (avec message) si le chemin est invalide
 */
static bool parse_path(int argc, char **argv, PathCmd **out, int *n)
{
    PathCmd *cmds = NULL;
    int cap = 0;
    char cmd = 0;          /* instruction courante */
    Point last = {0, 0};   /* position actuelle */
    Point start = {0, 0};  /* début du sous-chemin */
    *n = 0;

    for (int i = 0; i < argc; i++) {
        const char *tok = argv[i];

        /* une lettre change l'instruction courante */
        if (isalpha((unsigned char)tok[0])) {
            cmd = tok[0];
            if (strchr("MLHVZ", cmd) == NULL) {
                fprintf(stderr, "Erreur : instruction '%c' inconnue "
                                "(majuscules M, L, H, V, Z).\n", tok[0]);
                free(cmds);
                return false;
            }
            if (*n == 0 && cmd != 'M') {
                fprintf(stderr, "Erreur : un chemin doit commencer par M.\n");
                free(cmds);
                return false;
            }
            if (cmd == 'Z') {
                if (tok[1] != '\0' || !path_push(&cmds, n, &cap, 'Z', start)) {
                    fprintf(stderr, "Erreur : 'Z' mal placé ou mémoire insuffisante.\n");
                    free(cmds);
                    return false;
                }
                last = start;
                continue;
            }
            tok++; /* le nombre peut être collé à la lettre */
            if (*tok == '\0') {
                continue;
            }
        }
        if (cmd == 0 || cmd == 'Z') {
            fprintf(stderr, "Erreur : '%s' doit suivre une instruction (M, L, H, V).\n",
                    argv[i]);
            free(cmds);
            return false;
        }

        /* lecture des nombres de l'instruction */
        Point p = last;
        bool ok;
        if (cmd == 'H') {
            ok = parse_int(tok, &p.x);
        } else if (cmd == 'V') {
            ok = parse_int(tok, &p.y);
        } else {
            ok = (i + 1 < argc) && parse_int(tok, &p.x) && parse_int(argv[++i], &p.y);
            if (!ok && i + 1 >= argc) {
                fprintf(stderr, "Erreur : '%c' attend deux nombres.\n", cmd);
            }
        }
        if (!ok) {
            free(cmds);
            return false;
        }

        char stored = (cmd == 'M') ? 'M' : 'L';
        if (!path_push(&cmds, n, &cap, stored, p)) {
            fprintf(stderr, "Erreur : mémoire insuffisante.\n");
            free(cmds);
            return false;
        }
        if (cmd == 'M') {
            start = p;
            cmd = 'L'; /* comme en SVG : après M, les couples suivants sont des L */
        }
        last = p;
    }

    if (*n == 0) {
        fprintf(stderr, "Erreur : chemin vide.\n");
        return false;
    }
    *out = cmds;
    return true;
}

/* ------------------------------------------------------------------ */
/*  Commandes de création                                              */
/* ------------------------------------------------------------------ */

/** cercle cx cy r */
static bool cmd_circle(Canvas *c, int argc, char **argv, bool verbose)
{
    int v[3];
    (void)argc;
    if (!parse_ints(argv, 3, v) || !check_positive(v[2], "le rayon")) {
        return false;
    }
    return add_shape(c, shape_new_circle(c->next_id, v[0], v[1], v[2]), verbose);
}

/** ellipse cx cy rx ry */
static bool cmd_ellipse(Canvas *c, int argc, char **argv, bool verbose)
{
    int v[4];
    (void)argc;
    if (!parse_ints(argv, 4, v) || !check_positive(v[2], "rx")
        || !check_positive(v[3], "ry")) {
        return false;
    }
    return add_shape(c, shape_new_ellipse(c->next_id, v[0], v[1], v[2], v[3]), verbose);
}

/** carre x y cote */
static bool cmd_square(Canvas *c, int argc, char **argv, bool verbose)
{
    int v[3];
    (void)argc;
    if (!parse_ints(argv, 3, v) || !check_positive(v[2], "le côté")) {
        return false;
    }
    return add_shape(c, shape_new_square(c->next_id, v[0], v[1], v[2]), verbose);
}

/** rectangle x y largeur hauteur */
static bool cmd_rect(Canvas *c, int argc, char **argv, bool verbose)
{
    int v[4];
    (void)argc;
    if (!parse_ints(argv, 4, v) || !check_positive(v[2], "la largeur")
        || !check_positive(v[3], "la hauteur")) {
        return false;
    }
    return add_shape(c, shape_new_rect(c->next_id, v[0], v[1], v[2], v[3]), verbose);
}

/** ligne x1 y1 x2 y2 */
static bool cmd_line(Canvas *c, int argc, char **argv, bool verbose)
{
    int v[4];
    (void)argc;
    if (!parse_ints(argv, 4, v)) {
        return false;
    }
    Point a = {v[0], v[1]};
    Point b = {v[2], v[3]};
    return add_shape(c, shape_new_line(c->next_id, a, b), verbose);
}

/**
 * Partie commune à polyligne et polygone.
 */
static bool create_poly(Canvas *c, int argc, char **argv, bool verbose, ShapeType type)
{
    int n;
    Point *pts = parse_points(argc, argv, &n);
    if (pts == NULL) {
        return false;
    }
    bool ok = add_shape(c, shape_new_poly(c->next_id, type, pts, n), verbose);
    free(pts); /* la forme a fait sa propre copie */
    return ok;
}

/** polyligne x1 y1 x2 y2 ... */
static bool cmd_polyline(Canvas *c, int argc, char **argv, bool verbose)
{
    return create_poly(c, argc, argv, verbose, SHAPE_POLYLINE);
}

/** polygone x1 y1 x2 y2 x3 y3 ... */
static bool cmd_polygon(Canvas *c, int argc, char **argv, bool verbose)
{
    return create_poly(c, argc, argv, verbose, SHAPE_POLYGON);
}

/** chemin M x y L x y ... Z */
static bool cmd_path(Canvas *c, int argc, char **argv, bool verbose)
{
    PathCmd *cmds;
    int n;
    if (!parse_path(argc, argv, &cmds, &n)) {
        return false;
    }
    bool ok = add_shape(c, shape_new_path(c->next_id, cmds, n), verbose);
    free(cmds);
    return ok;
}

/** groupe id1 id2 ... : regroupe des formes du premier niveau */
static bool cmd_group(Canvas *c, int argc, char **argv, bool verbose)
{
    int *ids = malloc(argc * sizeof(int));
    if (ids == NULL) {
        fprintf(stderr, "Erreur : mémoire insuffisante.\n");
        return false;
    }
    /* on vérifie tout avant de modifier quoi que ce soit */
    for (int i = 0; i < argc; i++) {
        if (!parse_int(argv[i], &ids[i])) {
            free(ids);
            return false;
        }
        if (list_find_top(&c->shapes, ids[i]) == NULL) {
            fprintf(stderr, "Erreur : l'id %d n'est pas une forme du premier niveau.\n",
                    ids[i]);
            free(ids);
            return false;
        }
        for (int j = 0; j < i; j++) {
            if (ids[j] == ids[i]) {
                fprintf(stderr, "Erreur : l'id %d est donné deux fois.\n", ids[i]);
                free(ids);
                return false;
            }
        }
    }

    Shape *g = shape_new_group(c->next_id);
    if (g == NULL) {
        fprintf(stderr, "Erreur : mémoire insuffisante.\n");
        free(ids);
        return false;
    }
    /* on déplace les formes de la liste principale vers le groupe */
    for (int i = 0; i < argc; i++) {
        list_append(&g->data.group, list_detach(&c->shapes, ids[i], false));
    }
    free(ids);
    return add_shape(c, g, verbose);
}

/** degrouper id : remet les formes d'un groupe du premier niveau à sa place */
static bool cmd_ungroup(Canvas *c, int argc, char **argv, bool verbose)
{
    int id;
    (void)argc;
    if (!parse_int(argv[0], &id)) {
        return false;
    }
    Shape *g = list_find_top(&c->shapes, id);
    if (g == NULL || g->type != SHAPE_GROUP) {
        fprintf(stderr, "Erreur : l'id %d n'est pas un groupe du premier niveau.\n", id);
        return false;
    }
    /* la transformation du groupe ne peut pas être reportée sur chaque forme
       (la rotation se fait autour du centre du groupe, pas de chaque forme) */
    if (!transform_is_identity(&g->style)) {
        fprintf(stderr, "Erreur : le groupe %d est déplacé, pivoté ou inversé, "
                        "impossible de le dégrouper.\n", id);
        return false;
    }

    /* on reconstruit la liste en remplaçant le groupe par ses formes,
       pour garder l'ordre de dessin */
    ShapeList rebuilt;
    list_init(&rebuilt);
    Shape *cur = c->shapes.head;
    while (cur != NULL) {
        Shape *next = cur->next; /* list_append modifie cur->next */
        if (cur == g) {
            Shape *child = g->data.group.head;
            while (child != NULL) {
                Shape *child_next = child->next;
                list_append(&rebuilt, child);
                child = child_next;
            }
        } else {
            list_append(&rebuilt, cur);
        }
        cur = next;
    }
    c->shapes = rebuilt;

    list_init(&g->data.group); /* le groupe ne possède plus ses formes */
    shape_free(g);
    if (verbose) {
        printf("Groupe %d dégroupé.\n", id);
    }
    return true;
}

/* ------------------------------------------------------------------ */
/*  Commandes de style et de transformation                            */
/* ------------------------------------------------------------------ */

/** trait id couleur */
static bool cmd_stroke(Canvas *c, int argc, char **argv, bool verbose)
{
    (void)argc;
    Shape *s = find_shape(c, argv[0]);
    if (s == NULL || !check_color(argv[1])) {
        return false;
    }
    shape_set_stroke(s, argv[1]);
    if (verbose) {
        printf("Trait de la forme %d : %s.\n", s->id, argv[1]);
    }
    return true;
}

/** fond id couleur */
static bool cmd_fill(Canvas *c, int argc, char **argv, bool verbose)
{
    (void)argc;
    Shape *s = find_shape(c, argv[0]);
    if (s == NULL || !check_color(argv[1])) {
        return false;
    }
    shape_set_fill(s, argv[1]);
    if (verbose) {
        printf("Fond de la forme %d : %s.\n", s->id, argv[1]);
    }
    return true;
}

/** deplacer id dx dy */
static bool cmd_move(Canvas *c, int argc, char **argv, bool verbose)
{
    int d[2];
    (void)argc;
    Shape *s = find_shape(c, argv[0]);
    if (s == NULL || !parse_ints(argv + 1, 2, d)) {
        return false;
    }
    /* on évite que le déplacement total devienne énorme */
    if (abs(s->style.tx + d[0]) > COORD_MAX || abs(s->style.ty + d[1]) > COORD_MAX) {
        fprintf(stderr, "Erreur : déplacement total trop grand.\n");
        return false;
    }
    shape_move(s, d[0], d[1]);
    if (verbose) {
        printf("Forme %d déplacée (déplacement total : %d, %d).\n",
               s->id, s->style.tx, s->style.ty);
    }
    return true;
}

/** pivoter id angle */
static bool cmd_rotate(Canvas *c, int argc, char **argv, bool verbose)
{
    int angle;
    (void)argc;
    Shape *s = find_shape(c, argv[0]);
    if (s == NULL || !parse_int(argv[1], &angle)) {
        return false;
    }
    shape_rotate(s, angle);
    if (verbose) {
        printf("Forme %d pivotée (angle total : %d°).\n", s->id, s->style.angle);
    }
    return true;
}

/** inverser id x|y */
static bool cmd_flip(Canvas *c, int argc, char **argv, bool verbose)
{
    (void)argc;
    Shape *s = find_shape(c, argv[0]);
    if (s == NULL) {
        return false;
    }
    char axis = (char)tolower((unsigned char)argv[1][0]);
    if (argv[1][1] != '\0' || (axis != 'x' && axis != 'y')) {
        fprintf(stderr, "Erreur : l'axe doit être x ou y.\n");
        return false;
    }
    shape_flip(s, axis);
    if (verbose) {
        printf("Forme %d inversée selon %c.\n", s->id, axis);
    }
    return true;
}

/** supprimer id */
static bool cmd_delete(Canvas *c, int argc, char **argv, bool verbose)
{
    int id;
    (void)argc;
    if (!parse_int(argv[0], &id)) {
        return false;
    }
    Shape *s = list_detach(&c->shapes, id, true);
    if (s == NULL) {
        fprintf(stderr, "Erreur : aucune forme avec l'id %d.\n", id);
        return false;
    }
    shape_free(s);
    if (verbose) {
        printf("Forme %d supprimée.\n", id);
    }
    return true;
}

/* ------------------------------------------------------------------ */
/*  Commandes générales                                                */
/* ------------------------------------------------------------------ */

/** nouveau largeur hauteur */
static bool cmd_new(Canvas *c, int argc, char **argv, bool verbose)
{
    int v[2];
    (void)argc;
    if (!parse_ints(argv, 2, v) || !check_positive(v[0], "la largeur")
        || !check_positive(v[1], "la hauteur")) {
        return false;
    }
    canvas_free(c);
    canvas_init(c);
    c->vw = v[0];
    c->vh = v[1];
    if (verbose) {
        printf("Nouveau dessin %dx%d.\n", v[0], v[1]);
    }
    return true;
}

/** viewport x y largeur hauteur */
static bool cmd_viewport(Canvas *c, int argc, char **argv, bool verbose)
{
    int v[4];
    (void)argc;
    if (!parse_ints(argv, 4, v) || !check_positive(v[2], "la largeur")
        || !check_positive(v[3], "la hauteur")) {
        return false;
    }
    c->vx = v[0];
    c->vy = v[1];
    c->vw = v[2];
    c->vh = v[3];
    if (verbose) {
        printf("Viewport : x=%d y=%d largeur=%d hauteur=%d.\n", v[0], v[1], v[2], v[3]);
    }
    return true;
}

/** liste */
static bool cmd_list(Canvas *c, int argc, char **argv, bool verbose)
{
    (void)argc;
    (void)argv;
    (void)verbose;
    printf("Viewport : x=%d y=%d largeur=%d hauteur=%d\n", c->vx, c->vy, c->vw, c->vh);
    if (c->shapes.head == NULL) {
        printf("Aucune forme.\n");
    }
    for (const Shape *s = c->shapes.head; s != NULL; s = s->next) {
        shape_print(s, 0);
    }
    return true;
}

/** sauver fichier */
static bool cmd_save(Canvas *c, int argc, char **argv, bool verbose)
{
    (void)argc;
    if (!file_save(c, argv[0])) {
        return false;
    }
    if (verbose) {
        printf("Dessin sauvegardé dans '%s'.\n", argv[0]);
    }
    return true;
}

/** charger fichier */
static bool cmd_load(Canvas *c, int argc, char **argv, bool verbose)
{
    (void)argc;
    if (!file_load(c, argv[0])) {
        return false;
    }
    if (verbose) {
        printf("Dessin chargé depuis '%s'.\n", argv[0]);
    }
    return true;
}

/** exporter fichier.svg */
static bool cmd_export(Canvas *c, int argc, char **argv, bool verbose)
{
    (void)argc;
    if (!file_export_svg(c, argv[0])) {
        return false;
    }
    if (verbose) {
        printf("Dessin exporté en SVG dans '%s'.\n", argv[0]);
    }
    return true;
}

static bool cmd_help(Canvas *c, int argc, char **argv, bool verbose);

/** Table de toutes les commandes. */
static const Command COMMANDS[] = {
    {"aide", "aide", "affiche cette aide", 0, 0, true, cmd_help},
    {"nouveau", "nouveau <largeur> <hauteur>", "nouveau dessin vide", 2, 2, false, cmd_new},
    {"viewport", "viewport <x> <y> <largeur> <hauteur>", "change la fenêtre visible",
     4, 4, false, cmd_viewport},
    {"cercle", "cercle <cx> <cy> <r>", "crée un cercle", 3, 3, false, cmd_circle},
    {"ellipse", "ellipse <cx> <cy> <rx> <ry>", "crée une ellipse", 4, 4, false, cmd_ellipse},
    {"carre", "carre <x> <y> <cote>", "crée un carré", 3, 3, false, cmd_square},
    {"rectangle", "rectangle <x> <y> <largeur> <hauteur>", "crée un rectangle",
     4, 4, false, cmd_rect},
    {"ligne", "ligne <x1> <y1> <x2> <y2>", "crée une ligne", 4, 4, false, cmd_line},
    {"polyligne", "polyligne <x1> <y1> <x2> <y2> ...", "crée une suite de lignes",
     4, -1, false, cmd_polyline},
    {"polygone", "polygone <x1> <y1> <x2> <y2> <x3> <y3> ...", "crée un polygone",
     6, -1, false, cmd_polygon},
    {"chemin", "chemin M <x> <y> L <x> <y> H <x> V <y> Z", "crée un chemin",
     1, -1, false, cmd_path},
    {"groupe", "groupe <id> <id> ...", "regroupe des formes", 1, -1, false, cmd_group},
    {"degrouper", "degrouper <id>", "défait un groupe", 1, 1, false, cmd_ungroup},
    {"trait", "trait <id> <couleur>", "couleur du trait (none = aucun)",
     2, 2, false, cmd_stroke},
    {"fond", "fond <id> <couleur>", "couleur de fond (none = aucun)",
     2, 2, false, cmd_fill},
    {"deplacer", "deplacer <id> <dx> <dy>", "déplace une forme", 3, 3, false, cmd_move},
    {"pivoter", "pivoter <id> <angle>", "pivote une forme (degrés, sens horaire)",
     2, 2, false, cmd_rotate},
    {"inverser", "inverser <id> <x|y>", "miroir gauche/droite (x) ou haut/bas (y)",
     2, 2, false, cmd_flip},
    {"supprimer", "supprimer <id>", "supprime une forme", 1, 1, false, cmd_delete},
    {"liste", "liste", "liste les formes", 0, 0, true, cmd_list},
    {"sauver", "sauver <fichier>", "sauvegarde le dessin", 1, 1, true, cmd_save},
    {"charger", "charger <fichier>", "charge un dessin", 1, 1, true, cmd_load},
    {"exporter", "exporter <fichier.svg>", "exporte en SVG (à ouvrir dans un navigateur)", 1, 1, true, cmd_export},
    {"quitter", "quitter", "quitte le programme", 0, 0, true, NULL},
};

/** Nombre de commandes dans la table. */
#define NB_COMMANDS ((int)(sizeof(COMMANDS) / sizeof(COMMANDS[0])))

/** aide */
static bool cmd_help(Canvas *c, int argc, char **argv, bool verbose)
{
    (void)c;
    (void)argc;
    (void)argv;
    (void)verbose;
    printf("Commandes disponibles :\n");
    for (int i = 0; i < NB_COMMANDS; i++) {
        printf("  %-46s %s\n", COMMANDS[i].usage, COMMANDS[i].desc);
    }
    printf("Couleurs : nom (red, blue...), code (#ff0000) ou none.\n");
    printf("Les coordonnées sont des entiers ; l'axe Y va vers le bas (comme en SVG).\n");
    return true;
}

/**
 * Cherche une commande par son nom.
 * @param name nom tapé
 * @return la commande, ou NULL si inconnue
 */
static const Command *find_command(const char *name)
{
    for (int i = 0; i < NB_COMMANDS; i++) {
        if (strcmp(COMMANDS[i].name, name) == 0) {
            return &COMMANDS[i];
        }
    }
    return NULL;
}

CmdResult execute_line(Canvas *c, char *line, bool from_file)
{
    /* découpage de la ligne en mots */
    char *argv[MAX_ARGS];
    int argc = 0;
    for (char *tok = strtok(line, SEPARATORS); tok != NULL; tok = strtok(NULL, SEPARATORS)) {
        if (argc == MAX_ARGS) {
            fprintf(stderr, "Erreur : trop d'arguments (max %d).\n", MAX_ARGS);
            return CMD_ERROR;
        }
        argv[argc++] = tok;
    }

    /* ligne vide ou commentaire */
    if (argc == 0 || argv[0][0] == '#') {
        return CMD_OK;
    }

    const Command *cmd = find_command(argv[0]);
    if (cmd == NULL) {
        fprintf(stderr, "Erreur : commande '%s' inconnue (tapez 'aide').\n", argv[0]);
        return CMD_ERROR;
    }
    if (from_file && cmd->interactive_only) {
        fprintf(stderr, "Erreur : '%s' est interdite dans un fichier.\n", cmd->name);
        return CMD_ERROR;
    }
    int nargs = argc - 1;
    if (nargs < cmd->min_args || (cmd->max_args >= 0 && nargs > cmd->max_args)) {
        fprintf(stderr, "Usage : %s\n", cmd->usage);
        return CMD_ERROR;
    }
    if (cmd->handler == NULL) {
        return CMD_QUIT;
    }
    return cmd->handler(c, nargs, argv + 1, !from_file) ? CMD_OK : CMD_ERROR;
}
