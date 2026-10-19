/**
 * Implémentation de la sauvegarde, du chargement et de l'export SVG.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "file_io.h"
#include "commands.h"
#include "geometry.h"

/* ------------------------------------------------------------------ */
/*  Partie commune                                                     */
/* ------------------------------------------------------------------ */

/**
 * Écrit les points sous la forme "x y x y ..." (sep = ' ') ou
 * "x,y x,y ..." (sep = ',').
 */
static void write_points(FILE *f, const Point *pts, int n, char sep)
{
    for (int i = 0; i < n; i++) {
        fprintf(f, "%s%d%c%d", (i == 0) ? "" : " ", pts[i].x, sep, pts[i].y);
    }
}

/**
 * Écrit les instructions d'un chemin : "M 10 10 L 20 20 Z".
 */
static void write_path(FILE *f, const PathCmd *cmds, int n)
{
    for (int i = 0; i < n; i++) {
        if (i > 0) {
            fputc(' ', f);
        }
        if (cmds[i].cmd == 'Z') {
            fputc('Z', f);
        } else {
            fprintf(f, "%c %d %d", cmds[i].cmd, cmds[i].p.x, cmds[i].p.y);
        }
    }
}

/* ------------------------------------------------------------------ */
/*  Sauvegarde                                                         */
/* ------------------------------------------------------------------ */

/**
 * Écrit les commandes qui recréent une forme.
 * Au chargement, les id sont redonnés dans l'ordre de création : on simule
 * donc ce compteur avec next_id pour connaître l'id de chaque forme.
 * @param f fichier
 * @param s forme
 * @param next_id compteur d'id simulé
 * @return l'id qu'aura la forme au chargement, ou -1 si rien n'est écrit
 */
static int save_shape(FILE *f, const Shape *s, int *next_id)
{
    int id;

    if (s->type == SHAPE_GROUP) {
        /* on écrit d'abord les enfants, puis la commande groupe */
        int *ids = malloc((s->data.group.count + 1) * sizeof(int));
        if (ids == NULL) {
            return -1;
        }
        int n = 0;
        for (const Shape *c = s->data.group.head; c != NULL; c = c->next) {
            int child = save_shape(f, c, next_id);
            if (child >= 0) {
                ids[n++] = child;
            }
        }
        if (n == 0) {
            free(ids); /* groupe vide : rien à recréer */
            return -1;
        }
        fprintf(f, "groupe");
        for (int i = 0; i < n; i++) {
            fprintf(f, " %d", ids[i]);
        }
        fputc('\n', f);
        free(ids);
        id = (*next_id)++;
    } else {
        fprintf(f, "%s ", shape_type_name(s->type));
        switch (s->type) {
        case SHAPE_CIRCLE:
            fprintf(f, "%d %d %d", s->data.circle.c.x, s->data.circle.c.y,
                    s->data.circle.r);
            break;
        case SHAPE_ELLIPSE:
            fprintf(f, "%d %d %d %d", s->data.ellipse.c.x, s->data.ellipse.c.y,
                    s->data.ellipse.rx, s->data.ellipse.ry);
            break;
        case SHAPE_SQUARE:
            fprintf(f, "%d %d %d", s->data.square.p.x, s->data.square.p.y,
                    s->data.square.side);
            break;
        case SHAPE_RECT:
            fprintf(f, "%d %d %d %d", s->data.rect.p.x, s->data.rect.p.y,
                    s->data.rect.w, s->data.rect.h);
            break;
        case SHAPE_LINE:
            fprintf(f, "%d %d %d %d", s->data.line.a.x, s->data.line.a.y,
                    s->data.line.b.x, s->data.line.b.y);
            break;
        case SHAPE_POLYLINE:
        case SHAPE_POLYGON:
            write_points(f, s->data.poly.pts, s->data.poly.n, ' ');
            break;
        case SHAPE_PATH:
            write_path(f, s->data.path.cmds, s->data.path.n);
            break;
        default:
            break;
        }
        fputc('\n', f);
        id = (*next_id)++;

        /* couleurs (seulement si différentes des valeurs par défaut) */
        if (strcmp(s->style.stroke, "black") != 0) {
            fprintf(f, "trait %d %s\n", id, s->style.stroke);
        }
        if (strcmp(s->style.fill, "none") != 0) {
            fprintf(f, "fond %d %s\n", id, s->style.fill);
        }
    }

    /* transformations */
    if (s->style.tx != 0 || s->style.ty != 0) {
        fprintf(f, "deplacer %d %d %d\n", id, s->style.tx, s->style.ty);
    }
    if (s->style.angle != 0) {
        fprintf(f, "pivoter %d %d\n", id, s->style.angle);
    }
    if (s->style.flip_x) {
        fprintf(f, "inverser %d x\n", id);
    }
    if (s->style.flip_y) {
        fprintf(f, "inverser %d y\n", id);
    }
    return id;
}

bool file_save(const Canvas *c, const char *path)
{
    FILE *f = fopen(path, "w");
    if (f == NULL) {
        fprintf(stderr, "Erreur : impossible d'ouvrir '%s' en écriture.\n", path);
        return false;
    }
    fprintf(f, "# Dessin sauvegardé par svg_cli\n");
    fprintf(f, "viewport %d %d %d %d\n", c->vx, c->vy, c->vw, c->vh);

    int next_id = 1;
    for (const Shape *s = c->shapes.head; s != NULL; s = s->next) {
        save_shape(f, s, &next_id);
    }
    fclose(f);
    return true;
}

/* ------------------------------------------------------------------ */
/*  Chargement                                                         */
/* ------------------------------------------------------------------ */

bool file_load(Canvas *c, const char *path)
{
    FILE *f = fopen(path, "r");
    if (f == NULL) {
        fprintf(stderr, "Erreur : impossible d'ouvrir '%s'.\n", path);
        return false;
    }

    /* on charge dans un dessin temporaire pour ne rien perdre en cas d'erreur */
    Canvas tmp;
    canvas_init(&tmp);
    char line[LINE_SIZE];
    int num = 0;
    bool ok = true;

    while (ok && fgets(line, sizeof(line), f) != NULL) {
        num++;
        if (strchr(line, '\n') == NULL && !feof(f)) {
            fprintf(stderr, "Erreur ligne %d : ligne trop longue.\n", num);
            ok = false;
        } else if (execute_line(&tmp, line, true) != CMD_OK) {
            fprintf(stderr, "Erreur ligne %d du fichier '%s'.\n", num, path);
            ok = false;
        }
    }
    fclose(f);

    if (ok) {
        canvas_free(c);
        *c = tmp; /* le dessin prend possession des formes chargées */
    } else {
        canvas_free(&tmp);
    }
    return ok;
}

/* ------------------------------------------------------------------ */
/*  Export SVG                                                         */
/* ------------------------------------------------------------------ */

/**
 * Écrit l'attribut transform SVG équivalent à notre transformation :
 * inversion et rotation autour du centre, puis déplacement.
 */
static void write_transform(FILE *f, const Shape *s)
{
    const Style *st = &s->style;
    if (transform_is_identity(st)) {
        return;
    }
    Vec2 c = shape_center(s);
    /* SVG applique les transformations de droite à gauche */
    fprintf(f, " transform=\"translate(%d %d) rotate(%d %.2f %.2f) "
               "translate(%.2f %.2f) scale(%d %d) translate(%.2f %.2f)\"",
            st->tx, st->ty, st->angle, c.x, c.y, c.x, c.y,
            st->flip_x ? -1 : 1, st->flip_y ? -1 : 1, -c.x, -c.y);
}

/**
 * Écrit une forme en SVG (récursif pour les groupes).
 */
static void export_shape(FILE *f, const Shape *s, int depth)
{
    fprintf(f, "%*s", (depth + 1) * 2, "");

    if (s->type == SHAPE_GROUP) {
        fprintf(f, "<g");
        write_transform(f, s);
        fprintf(f, ">\n");
        for (const Shape *c = s->data.group.head; c != NULL; c = c->next) {
            export_shape(f, c, depth + 1);
        }
        fprintf(f, "%*s</g>\n", (depth + 1) * 2, "");
        return;
    }

    switch (s->type) {
    case SHAPE_CIRCLE:
        fprintf(f, "<circle cx=\"%d\" cy=\"%d\" r=\"%d\"", s->data.circle.c.x,
                s->data.circle.c.y, s->data.circle.r);
        break;
    case SHAPE_ELLIPSE:
        fprintf(f, "<ellipse cx=\"%d\" cy=\"%d\" rx=\"%d\" ry=\"%d\"",
                s->data.ellipse.c.x, s->data.ellipse.c.y,
                s->data.ellipse.rx, s->data.ellipse.ry);
        break;
    case SHAPE_SQUARE:
        fprintf(f, "<rect x=\"%d\" y=\"%d\" width=\"%d\" height=\"%d\"",
                s->data.square.p.x, s->data.square.p.y,
                s->data.square.side, s->data.square.side);
        break;
    case SHAPE_RECT:
        fprintf(f, "<rect x=\"%d\" y=\"%d\" width=\"%d\" height=\"%d\"",
                s->data.rect.p.x, s->data.rect.p.y, s->data.rect.w, s->data.rect.h);
        break;
    case SHAPE_LINE:
        fprintf(f, "<line x1=\"%d\" y1=\"%d\" x2=\"%d\" y2=\"%d\"",
                s->data.line.a.x, s->data.line.a.y,
                s->data.line.b.x, s->data.line.b.y);
        break;
    case SHAPE_POLYLINE:
    case SHAPE_POLYGON:
        fprintf(f, "<%s points=\"", s->type == SHAPE_POLYGON ? "polygon" : "polyline");
        write_points(f, s->data.poly.pts, s->data.poly.n, ',');
        fputc('"', f);
        break;
    case SHAPE_PATH:
        fprintf(f, "<path d=\"");
        write_path(f, s->data.path.cmds, s->data.path.n);
        fputc('"', f);
        break;
    default:
        break;
    }
    fprintf(f, " stroke=\"%s\" fill=\"%s\"", s->style.stroke, s->style.fill);
    write_transform(f, s);
    fprintf(f, "/>\n");
}

/**
 * Écrit la balise <svg> complète du dessin (sans l'en-tête XML).
 * @param f fichier ouvert en écriture
 * @param c dessin
 */
static void write_svg(FILE *f, const Canvas *c)
{
    fprintf(f, "<svg xmlns=\"http://www.w3.org/2000/svg\" "
               "viewBox=\"%d %d %d %d\" width=\"%d\" height=\"%d\">\n",
            c->vx, c->vy, c->vw, c->vh, c->vw * 4, c->vh * 4);
    for (const Shape *s = c->shapes.head; s != NULL; s = s->next) {
        export_shape(f, s, 0);
    }
    fprintf(f, "</svg>\n");
}

bool file_export_svg(const Canvas *c, const char *path)
{
    FILE *f = fopen(path, "w");
    if (f == NULL) {
        fprintf(stderr, "Erreur : impossible d'ouvrir '%s' en écriture.\n", path);
        return false;
    }
    fprintf(f, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
    write_svg(f, c);
    fclose(f);
    return true;
}

bool file_write_preview(const Canvas *c, const char *path)
{
    FILE *f = fopen(path, "w");
    if (f == NULL) {
        fprintf(stderr, "Erreur : impossible d'ouvrir '%s' en écriture.\n", path);
        return false;
    }
    /* la balise meta "refresh" recharge la page toutes les secondes :
       chaque modification apparaît sans rien faire dans le navigateur */
    fprintf(f, "<!DOCTYPE html>\n<html>\n<head>\n"
               "<meta charset=\"UTF-8\">\n"
               "<meta http-equiv=\"refresh\" content=\"1\">\n"
               "<title>Aperçu svg_cli</title>\n"
               "<style>body { margin: 0; background: #ddd; } "
               "svg { display: block; margin: 20px auto; background: white; "
               "border: 1px solid #888; max-width: 95vw; height: auto; }</style>\n"
               "</head>\n<body>\n");
    write_svg(f, c);
    fprintf(f, "</body>\n</html>\n");
    fclose(f);
    return true;
}
