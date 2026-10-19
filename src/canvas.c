/**
 * Initialisation et libération d'un dessin.
 */
#include "canvas.h"
#include "shape_list.h"

void canvas_init(Canvas *c)
{
    c->vx = 0;
    c->vy = 0;
    c->vw = 100;
    c->vh = 100;
    c->next_id = 1;
    list_init(&c->shapes);
}

void canvas_free(Canvas *c)
{
    list_free(&c->shapes);
}
