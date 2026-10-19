/**
 * Création, libération, style et affichage des formes.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "shape.h"
#include "shape_list.h"

/**
 * Alloue une forme avec le style par défaut (trait noir, pas de fond).
 * @param id identifiant
 * @param type type de forme
 * @return la forme, ou NULL si la mémoire manque
 */
static Shape *shape_alloc(int id, ShapeType type)
{
    Shape *s = calloc(1, sizeof(Shape)); /* calloc met tout à zéro */
    if (s == NULL) {
        return NULL;
    }
    s->id = id;
    s->type = type;
    strcpy(s->style.stroke, "black");
    strcpy(s->style.fill, "none");
    return s;
}

Shape *shape_new_circle(int id, int cx, int cy, int r)
{
    Shape *s = shape_alloc(id, SHAPE_CIRCLE);
    if (s != NULL) {
        s->data.circle.c.x = cx;
        s->data.circle.c.y = cy;
        s->data.circle.r = r;
    }
    return s;
}

Shape *shape_new_ellipse(int id, int cx, int cy, int rx, int ry)
{
    Shape *s = shape_alloc(id, SHAPE_ELLIPSE);
    if (s != NULL) {
        s->data.ellipse.c.x = cx;
        s->data.ellipse.c.y = cy;
        s->data.ellipse.rx = rx;
        s->data.ellipse.ry = ry;
    }
    return s;
}

Shape *shape_new_square(int id, int x, int y, int side)
{
    Shape *s = shape_alloc(id, SHAPE_SQUARE);
    if (s != NULL) {
        s->data.square.p.x = x;
        s->data.square.p.y = y;
        s->data.square.side = side;
    }
    return s;
}

Shape *shape_new_rect(int id, int x, int y, int w, int h)
{
    Shape *s = shape_alloc(id, SHAPE_RECT);
    if (s != NULL) {
        s->data.rect.p.x = x;
        s->data.rect.p.y = y;
        s->data.rect.w = w;
        s->data.rect.h = h;
    }
    return s;
}

Shape *shape_new_line(int id, Point a, Point b)
{
    Shape *s = shape_alloc(id, SHAPE_LINE);
    if (s != NULL) {
        s->data.line.a = a;
        s->data.line.b = b;
    }
    return s;
}

Shape *shape_new_poly(int id, ShapeType type, const Point *pts, int n)
{
    Shape *s = shape_alloc(id, type);
    if (s == NULL) {
        return NULL;
    }
    s->data.poly.pts = malloc(n * sizeof(Point));
    if (s->data.poly.pts == NULL) {
        free(s);
        return NULL;
    }
    memcpy(s->data.poly.pts, pts, n * sizeof(Point));
    s->data.poly.n = n;
    return s;
}

Shape *shape_new_path(int id, const PathCmd *cmds, int n)
{
    Shape *s = shape_alloc(id, SHAPE_PATH);
    if (s == NULL) {
        return NULL;
    }
    s->data.path.cmds = malloc(n * sizeof(PathCmd));
    if (s->data.path.cmds == NULL) {
        free(s);
        return NULL;
    }
    memcpy(s->data.path.cmds, cmds, n * sizeof(PathCmd));
    s->data.path.n = n;
    return s;
}

Shape *shape_new_group(int id)
{
    Shape *s = shape_alloc(id, SHAPE_GROUP);
    if (s != NULL) {
        list_init(&s->data.group);
    }
    return s;
}

void shape_free(Shape *s)
{
    if (s == NULL) {
        return;
    }
    /* on libère d'abord ce que la forme a alloué elle-même */
    switch (s->type) {
    case SHAPE_POLYLINE:
    case SHAPE_POLYGON:
        free(s->data.poly.pts);
        break;
    case SHAPE_PATH:
        free(s->data.path.cmds);
        break;
    case SHAPE_GROUP:
        list_free(&s->data.group);
        break;
    default:
        break;
    }
    free(s);
}

const char *shape_type_name(ShapeType type)
{
    static const char *names[] = {
        "cercle", "ellipse", "carre", "rectangle", "ligne",
        "polyligne", "polygone", "chemin", "groupe"
    };
    return names[type];
}

/**
 * Copie une couleur sans dépasser la taille du tableau.
 * @param dest destination (taille COLOR_LEN)
 * @param color couleur source
 */
static void copy_color(char *dest, const char *color)
{
    strncpy(dest, color, COLOR_LEN - 1);
    dest[COLOR_LEN - 1] = '\0';
}

void shape_set_stroke(Shape *s, const char *color)
{
    if (s->type == SHAPE_GROUP) {
        for (Shape *c = s->data.group.head; c != NULL; c = c->next) {
            shape_set_stroke(c, color);
        }
    } else {
        copy_color(s->style.stroke, color);
    }
}

void shape_set_fill(Shape *s, const char *color)
{
    if (s->type == SHAPE_GROUP) {
        for (Shape *c = s->data.group.head; c != NULL; c = c->next) {
            shape_set_fill(c, color);
        }
    } else {
        copy_color(s->style.fill, color);
    }
}

void shape_move(Shape *s, int dx, int dy)
{
    s->style.tx += dx;
    s->style.ty += dy;
}

void shape_rotate(Shape *s, int angle)
{
    /* on garde l'angle entre 0 et 359 */
    s->style.angle = (s->style.angle + angle) % 360;
    if (s->style.angle < 0) {
        s->style.angle += 360;
    }
}

void shape_flip(Shape *s, char axis)
{
    if (axis == 'x') {
        s->style.flip_x = !s->style.flip_x;
    } else {
        s->style.flip_y = !s->style.flip_y;
    }
}

/**
 * Affiche une liste de points sous la forme (x,y) (x,y) ...
 * @param pts points
 * @param n nombre de points
 */
static void print_points(const Point *pts, int n)
{
    for (int i = 0; i < n; i++) {
        printf(" (%d,%d)", pts[i].x, pts[i].y);
    }
}

void shape_print(const Shape *s, int depth)
{
    printf("%*s[%d] %s", depth * 4, "", s->id, shape_type_name(s->type));

    /* géométrie propre à chaque type */
    switch (s->type) {
    case SHAPE_CIRCLE:
        printf(" centre=(%d,%d) r=%d", s->data.circle.c.x, s->data.circle.c.y,
               s->data.circle.r);
        break;
    case SHAPE_ELLIPSE:
        printf(" centre=(%d,%d) rx=%d ry=%d", s->data.ellipse.c.x,
               s->data.ellipse.c.y, s->data.ellipse.rx, s->data.ellipse.ry);
        break;
    case SHAPE_SQUARE:
        printf(" coin=(%d,%d) cote=%d", s->data.square.p.x, s->data.square.p.y,
               s->data.square.side);
        break;
    case SHAPE_RECT:
        printf(" coin=(%d,%d) %dx%d", s->data.rect.p.x, s->data.rect.p.y,
               s->data.rect.w, s->data.rect.h);
        break;
    case SHAPE_LINE:
        printf(" (%d,%d) -> (%d,%d)", s->data.line.a.x, s->data.line.a.y,
               s->data.line.b.x, s->data.line.b.y);
        break;
    case SHAPE_POLYLINE:
    case SHAPE_POLYGON:
        print_points(s->data.poly.pts, s->data.poly.n);
        break;
    case SHAPE_PATH:
        for (int i = 0; i < s->data.path.n; i++) {
            const PathCmd *c = &s->data.path.cmds[i];
            if (c->cmd == 'Z') {
                printf(" Z");
            } else {
                printf(" %c %d %d", c->cmd, c->p.x, c->p.y);
            }
        }
        break;
    case SHAPE_GROUP:
        printf(" (%d forme(s))", s->data.group.count);
        break;
    }

    /* style (un groupe n'a pas de couleur propre) */
    if (s->type != SHAPE_GROUP) {
        printf(" | trait=%s fond=%s", s->style.stroke, s->style.fill);
    }
    const char *inv = "-";
    if (s->style.flip_x && s->style.flip_y) {
        inv = "XY";
    } else if (s->style.flip_x) {
        inv = "X";
    } else if (s->style.flip_y) {
        inv = "Y";
    }
    printf(" | dep=(%d,%d) rot=%d inv=%s\n", s->style.tx, s->style.ty,
           s->style.angle, inv);

    if (s->type == SHAPE_GROUP) {
        for (const Shape *c = s->data.group.head; c != NULL; c = c->next) {
            shape_print(c, depth + 1);
        }
    }
}
