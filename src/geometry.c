/**
 * Implémentation de la géométrie.
 */
#include <math.h>
#include <stdlib.h>
#include "geometry.h"

/** Valeur de pi (M_PI n'existe pas en C99 strict). */
#define PI 3.14159265358979323846

/** Nombre de points pour approcher un cercle (multiple de 4). */
#define CURVE_STEPS 48

void outline_init(Outline *o)
{
    o->subs = NULL;
    o->n = 0;
    o->cap = 0;
}

void outline_free(Outline *o)
{
    for (int i = 0; i < o->n; i++) {
        free(o->subs[i].pts);
    }
    free(o->subs);
    outline_init(o);
}

/**
 * Ajoute un sous-chemin vide au contour (le tableau double de taille si plein).
 * @param o contour
 * @param closed true si le sous-chemin est fermé
 * @return le sous-chemin, ou NULL si la mémoire manque
 */
static Subpath *outline_add_sub(Outline *o, bool closed)
{
    if (o->n == o->cap) {
        int cap = (o->cap == 0) ? 4 : o->cap * 2;
        Subpath *tmp = realloc(o->subs, cap * sizeof(Subpath));
        if (tmp == NULL) {
            return NULL;
        }
        o->subs = tmp;
        o->cap = cap;
    }
    Subpath *sp = &o->subs[o->n++];
    sp->pts = NULL;
    sp->n = 0;
    sp->cap = 0;
    sp->closed = closed;
    return sp;
}

/**
 * Ajoute un point à un sous-chemin (le tableau double de taille si plein).
 * @param sp sous-chemin
 * @param x abscisse
 * @param y ordonnée
 * @return false si la mémoire manque
 */
static bool sub_push(Subpath *sp, double x, double y)
{
    if (sp->n == sp->cap) {
        int cap = (sp->cap == 0) ? 8 : sp->cap * 2;
        Vec2 *tmp = realloc(sp->pts, cap * sizeof(Vec2));
        if (tmp == NULL) {
            return false;
        }
        sp->pts = tmp;
        sp->cap = cap;
    }
    sp->pts[sp->n].x = x;
    sp->pts[sp->n].y = y;
    sp->n++;
    return true;
}

/**
 * Ajoute une ellipse (ou un cercle si rx == ry) approchée par un polygone.
 * @return false si la mémoire manque
 */
static bool add_ellipse(Outline *o, double cx, double cy, double rx, double ry)
{
    Subpath *sp = outline_add_sub(o, true);
    if (sp == NULL) {
        return false;
    }
    for (int i = 0; i < CURVE_STEPS; i++) {
        double a = 2.0 * PI * i / CURVE_STEPS;
        if (!sub_push(sp, cx + rx * cos(a), cy + ry * sin(a))) {
            return false;
        }
    }
    return true;
}

/**
 * Ajoute un rectangle (4 coins, fermé).
 * @return false si la mémoire manque
 */
static bool add_rect(Outline *o, double x, double y, double w, double h)
{
    Subpath *sp = outline_add_sub(o, true);
    return sp != NULL
        && sub_push(sp, x, y)
        && sub_push(sp, x + w, y)
        && sub_push(sp, x + w, y + h)
        && sub_push(sp, x, y + h);
}

/**
 * Ajoute une suite de points.
 * @return false si la mémoire manque
 */
static bool add_points(Outline *o, const Point *pts, int n, bool closed)
{
    Subpath *sp = outline_add_sub(o, closed);
    if (sp == NULL) {
        return false;
    }
    for (int i = 0; i < n; i++) {
        if (!sub_push(sp, pts[i].x, pts[i].y)) {
            return false;
        }
    }
    return true;
}

/**
 * Ajoute un chemin : chaque 'M' commence un nouveau sous-chemin.
 * @return false si la mémoire manque
 */
static bool add_path(Outline *o, const PathCmd *cmds, int n)
{
    Subpath *cur = NULL;
    Point start = {0, 0}; /* début du sous-chemin courant */
    Point last = {0, 0};  /* dernier point atteint */

    for (int i = 0; i < n; i++) {
        const PathCmd *c = &cmds[i];
        if (c->cmd == 'M') {
            cur = outline_add_sub(o, false);
            if (cur == NULL || !sub_push(cur, c->p.x, c->p.y)) {
                return false;
            }
            start = c->p;
            last = c->p;
        } else if (c->cmd == 'L') {
            if (cur == NULL) {
                /* ligne après un 'Z' : on repart du dernier point */
                cur = outline_add_sub(o, false);
                if (cur == NULL || !sub_push(cur, last.x, last.y)) {
                    return false;
                }
            }
            if (!sub_push(cur, c->p.x, c->p.y)) {
                return false;
            }
            last = c->p;
        } else { /* 'Z' */
            if (cur != NULL) {
                cur->closed = true;
            }
            cur = NULL;
            last = start;
        }
    }
    return true;
}

/**
 * Calcule le centre de la boîte englobante des sous-chemins à partir de start.
 * @return le centre, ou (0,0) s'il n'y a aucun point
 */
static Vec2 outline_center(const Outline *o, int start)
{
    Vec2 c = {0.0, 0.0};
    bool first = true;
    double minx = 0, maxx = 0, miny = 0, maxy = 0;

    for (int i = start; i < o->n; i++) {
        for (int j = 0; j < o->subs[i].n; j++) {
            Vec2 p = o->subs[i].pts[j];
            if (first) {
                minx = maxx = p.x;
                miny = maxy = p.y;
                first = false;
            }
            if (p.x < minx) minx = p.x;
            if (p.x > maxx) maxx = p.x;
            if (p.y < miny) miny = p.y;
            if (p.y > maxy) maxy = p.y;
        }
    }
    if (!first) {
        c.x = (minx + maxx) / 2.0;
        c.y = (miny + maxy) / 2.0;
    }
    return c;
}

void outline_transform(Outline *o, int start, const Style *st, Vec2 center)
{
    double a = st->angle * PI / 180.0;
    double ca = cos(a);
    double sa = sin(a);

    for (int i = start; i < o->n; i++) {
        for (int j = 0; j < o->subs[i].n; j++) {
            Vec2 *p = &o->subs[i].pts[j];
            /* on se place par rapport au centre */
            double x = p->x - center.x;
            double y = p->y - center.y;
            /* 1. inversion */
            if (st->flip_x) x = -x;
            if (st->flip_y) y = -y;
            /* 2. rotation (sens horaire car l'axe Y descend) */
            double rx = x * ca - y * sa;
            double ry = x * sa + y * ca;
            /* 3. retour au centre puis déplacement */
            p->x = rx + center.x + st->tx;
            p->y = ry + center.y + st->ty;
        }
    }
}

bool transform_is_identity(const Style *st)
{
    return st->tx == 0 && st->ty == 0 && st->angle == 0
        && !st->flip_x && !st->flip_y;
}

bool shape_outline(const Shape *s, Outline *o, bool with_transform)
{
    int start = o->n; /* premier sous-chemin de cette forme */
    bool ok = true;

    switch (s->type) {
    case SHAPE_CIRCLE:
        ok = add_ellipse(o, s->data.circle.c.x, s->data.circle.c.y,
                         s->data.circle.r, s->data.circle.r);
        break;
    case SHAPE_ELLIPSE:
        ok = add_ellipse(o, s->data.ellipse.c.x, s->data.ellipse.c.y,
                         s->data.ellipse.rx, s->data.ellipse.ry);
        break;
    case SHAPE_SQUARE:
        ok = add_rect(o, s->data.square.p.x, s->data.square.p.y,
                      s->data.square.side, s->data.square.side);
        break;
    case SHAPE_RECT:
        ok = add_rect(o, s->data.rect.p.x, s->data.rect.p.y,
                      s->data.rect.w, s->data.rect.h);
        break;
    case SHAPE_LINE: {
        Point pts[2] = {s->data.line.a, s->data.line.b};
        ok = add_points(o, pts, 2, false);
        break;
    }
    case SHAPE_POLYLINE:
        ok = add_points(o, s->data.poly.pts, s->data.poly.n, false);
        break;
    case SHAPE_POLYGON:
        ok = add_points(o, s->data.poly.pts, s->data.poly.n, true);
        break;
    case SHAPE_PATH:
        ok = add_path(o, s->data.path.cmds, s->data.path.n);
        break;
    case SHAPE_GROUP:
        /* chaque enfant arrive avec sa propre transformation */
        for (const Shape *c = s->data.group.head; c != NULL && ok; c = c->next) {
            ok = shape_outline(c, o, true);
        }
        break;
    }

    if (ok && with_transform && !transform_is_identity(&s->style)) {
        outline_transform(o, start, &s->style, outline_center(o, start));
    }
    return ok;
}

Vec2 shape_center(const Shape *s)
{
    Vec2 c = {0.0, 0.0};
    Outline o;
    outline_init(&o);
    if (shape_outline(s, &o, false)) {
        c = outline_center(&o, 0);
    }
    outline_free(&o);
    return c;
}
