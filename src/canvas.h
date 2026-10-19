/**
 * Le dessin : un viewport et la liste des formes.
 */
#ifndef CANVAS_H
#define CANVAS_H

#include "shape.h"

/** Un dessin complet. */
typedef struct {
    int vx;           /**< abscisse du coin haut-gauche du viewport */
    int vy;           /**< ordonnée du coin haut-gauche du viewport */
    int vw;           /**< largeur du viewport */
    int vh;           /**< hauteur du viewport */
    ShapeList shapes; /**< formes, dans l'ordre de dessin */
    int next_id;      /**< id donné à la prochaine forme créée */
} Canvas;

/**
 * Initialise un dessin vide avec un viewport 0 0 100 100.
 * @param c dessin
 */
void canvas_init(Canvas *c);

/**
 * Libère toutes les formes du dessin.
 * @param c dessin
 */
void canvas_free(Canvas *c);

#endif
