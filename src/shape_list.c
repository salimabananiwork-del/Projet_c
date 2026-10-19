/**
 * Implémentation des listes chaînées de formes.
 */
#include <stddef.h>
#include "shape_list.h"

void list_init(ShapeList *l)
{
    l->head = NULL;
    l->tail = NULL;
    l->count = 0;
}

void list_append(ShapeList *l, Shape *s)
{
    s->next = NULL;
    if (l->tail != NULL) {
        l->tail->next = s;
    } else {
        l->head = s;
    }
    l->tail = s;
    l->count++;
}

void list_free(ShapeList *l)
{
    Shape *cur = l->head;
    while (cur != NULL) {
        Shape *next = cur->next; /* on garde la suite avant de libérer */
        shape_free(cur);
        cur = next;
    }
    list_init(l);
}

Shape *list_find(const ShapeList *l, int id)
{
    for (Shape *cur = l->head; cur != NULL; cur = cur->next) {
        if (cur->id == id) {
            return cur;
        }
        if (cur->type == SHAPE_GROUP) {
            Shape *found = list_find(&cur->data.group, id);
            if (found != NULL) {
                return found;
            }
        }
    }
    return NULL;
}

Shape *list_find_top(const ShapeList *l, int id)
{
    for (Shape *cur = l->head; cur != NULL; cur = cur->next) {
        if (cur->id == id) {
            return cur;
        }
    }
    return NULL;
}

Shape *list_detach(ShapeList *l, int id, bool recursive)
{
    Shape *prev = NULL;
    for (Shape *cur = l->head; cur != NULL; prev = cur, cur = cur->next) {
        if (cur->id == id) {
            /* on relie le précédent au suivant */
            if (prev != NULL) {
                prev->next = cur->next;
            } else {
                l->head = cur->next;
            }
            if (l->tail == cur) {
                l->tail = prev;
            }
            l->count--;
            cur->next = NULL;
            return cur;
        }
        if (recursive && cur->type == SHAPE_GROUP) {
            Shape *found = list_detach(&cur->data.group, id, true);
            if (found != NULL) {
                return found;
            }
        }
    }
    return NULL;
}
