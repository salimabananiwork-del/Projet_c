/**
 * Définition des formes graphiques et de leur style.
 * Une forme est un nœud de liste chaînée : elle pointe vers la forme suivante.
 */
#ifndef SHAPE_H
#define SHAPE_H

#include <stdbool.h>

/** Taille maximale d'un nom de couleur (avec le '\0'). */
#define COLOR_LEN 32

/** Les différents types de formes gérés. */
typedef enum {
    SHAPE_CIRCLE,
    SHAPE_ELLIPSE,
    SHAPE_SQUARE,
    SHAPE_RECT,
    SHAPE_LINE,
    SHAPE_POLYLINE,
    SHAPE_POLYGON,
    SHAPE_PATH,
    SHAPE_GROUP
} ShapeType;

/** Un point à coordonnées entières. */
typedef struct {
    int x;
    int y;
} Point;

/** Une instruction de chemin : 'M' (aller à), 'L' (ligne vers) ou 'Z' (fermer). */
typedef struct {
    char cmd;
    Point p;
} PathCmd;

/** Style et transformation appliqués à une forme. */
typedef struct {
    char stroke[COLOR_LEN]; /**< couleur du trait */
    char fill[COLOR_LEN];   /**< couleur de fond */
    int tx;                 /**< déplacement en X */
    int ty;                 /**< déplacement en Y */
    int angle;              /**< rotation en degrés, entre 0 et 359 */
    bool flip_x;            /**< miroir gauche/droite */
    bool flip_y;            /**< miroir haut/bas */
} Style;

typedef struct Shape Shape;

/** Liste chaînée de formes (avec pointeur de fin pour ajouter en O(1)). */
typedef struct {
    Shape *head;
    Shape *tail;
    int count;
} ShapeList;

/** Une forme. L'union ne garde en mémoire que les données de son type. */
struct Shape {
    int id;
    ShapeType type;
    Style style;
    union {
        struct { Point c; int r; } circle;
        struct { Point c; int rx; int ry; } ellipse;
        struct { Point p; int side; } square;
        struct { Point p; int w; int h; } rect;
        struct { Point a; Point b; } line;
        struct { Point *pts; int n; } poly;      /**< polyligne et polygone */
        struct { PathCmd *cmds; int n; } path;
        ShapeList group;                          /**< formes du groupe */
    } data;
    Shape *next; /**< forme suivante dans la liste */
};

/**
 * Crée un cercle.
 * @param id identifiant de la forme
 * @param cx abscisse du centre
 * @param cy ordonnée du centre
 * @param r rayon
 * @return la forme créée, ou NULL si la mémoire manque
 */
Shape *shape_new_circle(int id, int cx, int cy, int r);

/**
 * Crée une ellipse.
 * @param id identifiant
 * @param cx abscisse du centre
 * @param cy ordonnée du centre
 * @param rx rayon horizontal
 * @param ry rayon vertical
 * @return la forme créée, ou NULL
 */
Shape *shape_new_ellipse(int id, int cx, int cy, int rx, int ry);

/**
 * Crée un carré.
 * @param id identifiant
 * @param x abscisse du coin haut-gauche
 * @param y ordonnée du coin haut-gauche
 * @param side longueur du côté
 * @return la forme créée, ou NULL
 */
Shape *shape_new_square(int id, int x, int y, int side);

/**
 * Crée un rectangle.
 * @param id identifiant
 * @param x abscisse du coin haut-gauche
 * @param y ordonnée du coin haut-gauche
 * @param w largeur
 * @param h hauteur
 * @return la forme créée, ou NULL
 */
Shape *shape_new_rect(int id, int x, int y, int w, int h);

/**
 * Crée une ligne entre deux points.
 * @param id identifiant
 * @param a premier point
 * @param b second point
 * @return la forme créée, ou NULL
 */
Shape *shape_new_line(int id, Point a, Point b);

/**
 * Crée une polyligne ou un polygone (les points sont copiés).
 * @param id identifiant
 * @param type SHAPE_POLYLINE ou SHAPE_POLYGON
 * @param pts tableau de points
 * @param n nombre de points
 * @return la forme créée, ou NULL
 */
Shape *shape_new_poly(int id, ShapeType type, const Point *pts, int n);

/**
 * Crée un chemin (les instructions sont copiées).
 * @param id identifiant
 * @param cmds tableau d'instructions
 * @param n nombre d'instructions
 * @return la forme créée, ou NULL
 */
Shape *shape_new_path(int id, const PathCmd *cmds, int n);

/**
 * Crée un groupe vide.
 * @param id identifiant
 * @return la forme créée, ou NULL
 */
Shape *shape_new_group(int id);

/**
 * Libère une forme et tout ce qu'elle contient (enfants compris).
 * @param s forme à libérer (NULL accepté)
 */
void shape_free(Shape *s);

/**
 * Donne le nom d'un type de forme (c'est aussi le nom de la commande).
 * @param type type de forme
 * @return nom en français
 */
const char *shape_type_name(ShapeType type);

/**
 * Change la couleur du trait. Sur un groupe, s'applique à tous ses enfants.
 * @param s forme
 * @param color nouvelle couleur
 */
void shape_set_stroke(Shape *s, const char *color);

/**
 * Change la couleur de fond. Sur un groupe, s'applique à tous ses enfants.
 * @param s forme
 * @param color nouvelle couleur
 */
void shape_set_fill(Shape *s, const char *color);

/**
 * Ajoute un déplacement à la forme.
 * @param s forme
 * @param dx déplacement en X
 * @param dy déplacement en Y
 */
void shape_move(Shape *s, int dx, int dy);

/**
 * Ajoute une rotation (autour du centre de la forme).
 * @param s forme
 * @param angle angle en degrés (sens horaire)
 */
void shape_rotate(Shape *s, int angle);

/**
 * Inverse la forme selon un axe (un 2e appel annule le 1er).
 * @param s forme
 * @param axis 'x' (miroir gauche/droite) ou 'y' (miroir haut/bas)
 */
void shape_flip(Shape *s, char axis);

/**
 * Affiche la forme (et ses enfants si c'est un groupe).
 * @param s forme
 * @param depth niveau d'indentation
 */
void shape_print(const Shape *s, int depth);

#endif
