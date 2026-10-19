/**
 * Géométrie : transforme une forme en liste de points ("contour")
 * et applique les transformations (déplacement, rotation, inversion).
 */
#ifndef GEOMETRY_H
#define GEOMETRY_H

#include <stdbool.h>
#include "shape.h"

/** Un point à coordonnées réelles (utile après une rotation). */
typedef struct {
    double x;
    double y;
} Vec2;

/** Une suite de points reliés (tableau dynamique). */
typedef struct {
    Vec2 *pts;
    int n;       /**< nombre de points */
    int cap;     /**< place allouée */
    bool closed; /**< true si le dernier point rejoint le premier */
} Subpath;

/** Contour d'une forme : un ou plusieurs sous-chemins. */
typedef struct {
    Subpath *subs;
    int n;
    int cap;
} Outline;

/**
 * Initialise un contour vide.
 * @param o contour
 */
void outline_init(Outline *o);

/**
 * Libère la mémoire d'un contour.
 * @param o contour
 */
void outline_free(Outline *o);

/**
 * Ajoute au contour les points d'une forme.
 * Les cercles et ellipses sont approchés par des polygones.
 * @param s forme
 * @param o contour à compléter
 * @param with_transform true pour appliquer la transformation de la forme
 * @return false si la mémoire manque
 */
bool shape_outline(const Shape *s, Outline *o, bool with_transform);

/**
 * Applique une transformation aux sous-chemins à partir de l'indice start.
 * Ordre : inversion, rotation (autour de center), puis déplacement.
 * @param o contour
 * @param start premier sous-chemin concerné
 * @param st style contenant la transformation
 * @param center centre de rotation et d'inversion
 */
void outline_transform(Outline *o, int start, const Style *st, Vec2 center);

/**
 * Calcule le centre d'une forme (centre de sa boîte englobante, sans
 * sa propre transformation).
 * @param s forme
 * @return le centre
 */
Vec2 shape_center(const Shape *s);

/**
 * Indique si le style ne contient aucune transformation.
 * @param st style
 * @return true si aucun déplacement, rotation ou inversion
 */
bool transform_is_identity(const Style *st);

#endif
