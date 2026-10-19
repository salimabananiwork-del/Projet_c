/**
 * Opérations sur les listes chaînées de formes.
 */
#ifndef SHAPE_LIST_H
#define SHAPE_LIST_H

#include "shape.h"

/**
 * Initialise une liste vide.
 * @param l liste
 */
void list_init(ShapeList *l);

/**
 * Ajoute une forme en fin de liste (O(1) grâce au pointeur tail).
 * @param l liste
 * @param s forme à ajouter
 */
void list_append(ShapeList *l, Shape *s);

/**
 * Libère toutes les formes de la liste puis la remet à vide.
 * @param l liste
 */
void list_free(ShapeList *l);

/**
 * Cherche une forme par son id, y compris à l'intérieur des groupes.
 * @param l liste
 * @param id identifiant cherché
 * @return la forme, ou NULL si absente
 */
Shape *list_find(const ShapeList *l, int id);

/**
 * Cherche une forme par son id, seulement au premier niveau.
 * @param l liste
 * @param id identifiant cherché
 * @return la forme, ou NULL si absente
 */
Shape *list_find_top(const ShapeList *l, int id);

/**
 * Retire une forme de la liste sans la libérer.
 * @param l liste
 * @param id identifiant de la forme
 * @param recursive true pour chercher aussi dans les groupes
 * @return la forme retirée, ou NULL si absente
 */
Shape *list_detach(ShapeList *l, int id, bool recursive);

#endif
