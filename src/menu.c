/**
 * Implémentation du menu interactif.
 * Chaque action du menu est décrite dans un tableau (texte affiché,
 * commande à lancer, questions à poser). Les réponses sont assemblées en
 * une ligne de commande, exécutée par execute_line : toutes les
 * vérifications des commandes sont donc réutilisées.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "menu.h"
#include "file_io.h"

/** Fichier de l'aperçu ouvert dans le navigateur. */
#define PREVIEW_FILE "apercu.html"

/** true une fois que l'aperçu a été ouvert (il est alors tenu à jour). */
static bool preview_open = false;

/** Nombre maximal de questions pour une action. */
#define MAX_QUESTIONS 4

/** Une action proposée dans un menu. */
typedef struct {
    const char *label;                      /**< texte affiché dans le menu */
    const char *cmd;                        /**< commande à exécuter */
    const char *questions[MAX_QUESTIONS];   /**< questions posées (NULL = fin) */
} MenuItem;

/** Les formes que l'on peut créer. */
static const MenuItem SHAPE_ITEMS[] = {
    {"Cercle", "cercle", {"Centre X", "Centre Y", "Rayon", NULL}},
    {"Ellipse", "ellipse", {"Centre X", "Centre Y", "Rayon horizontal", "Rayon vertical"}},
    {"Carré", "carre", {"Coin haut-gauche X", "Coin haut-gauche Y", "Côté", NULL}},
    {"Rectangle", "rectangle", {"Coin haut-gauche X", "Coin haut-gauche Y",
                                "Largeur", "Hauteur"}},
    {"Ligne", "ligne", {"Départ X", "Départ Y", "Arrivée X", "Arrivée Y"}},
    {"Polyligne (lignes à la suite)", "polyligne",
     {"Points, 2 minimum (ex : 10,20 30,40 50,20)", NULL, NULL, NULL}},
    {"Polygone", "polygone",
     {"Points, 3 minimum (ex : 10,10 50,10 30,40)", NULL, NULL, NULL}},
    {"Chemin", "chemin",
     {"Instructions M L H V Z (ex : M 10 10 L 50 10 V 40 Z)", NULL, NULL, NULL}},
    {"Groupe", "groupe",
     {"Ids des formes à regrouper (ex : 1 2 3)", NULL, NULL, NULL}},
};

/** Les modifications possibles d'une forme (l'id est demandé avant). */
static const MenuItem EDIT_ITEMS[] = {
    {"Couleur du trait", "trait", {"Couleur (ex : red, #ff0000, none)", NULL, NULL, NULL}},
    {"Couleur de fond", "fond", {"Couleur (ex : red, #ff0000, none)", NULL, NULL, NULL}},
    {"Déplacer", "deplacer", {"Déplacement en X", "Déplacement en Y", NULL, NULL}},
    {"Pivoter", "pivoter", {"Angle en degrés (sens horaire)", NULL, NULL, NULL}},
    {"Inverser", "inverser", {"Axe : x (gauche/droite) ou y (haut/bas)", NULL, NULL, NULL}},
    {"Supprimer", "supprimer", {NULL, NULL, NULL, NULL}},
    {"Dégrouper", "degrouper", {NULL, NULL, NULL, NULL}},
};

/** Les actions sur le dessin entier. */
static const MenuItem FILE_ITEMS[] = {
    {"Nouveau dessin", "nouveau", {"Largeur", "Hauteur", NULL, NULL}},
    {"Changer le viewport", "viewport", {"X", "Y", "Largeur", "Hauteur"}},
    {"Sauvegarder", "sauver", {"Nom du fichier (ex : dessin.dessin)", NULL, NULL, NULL}},
    {"Charger", "charger", {"Nom du fichier (ex : dessin.dessin)", NULL, NULL, NULL}},
    {"Exporter en SVG (pour le voir dans un navigateur)", "exporter",
     {"Nom du fichier (ex : dessin.svg)", NULL, NULL, NULL}},
};

/** Taille d'un tableau statique. */
#define COUNT(t) ((int)(sizeof(t) / sizeof((t)[0])))

bool read_line(char *buf, int size)
{
    if (fgets(buf, size, stdin) == NULL) {
        return false;
    }
    char *end = strchr(buf, '\n');
    if (end != NULL) {
        *end = '\0';
    } else if (!feof(stdin)) {
        /* ligne trop longue : on jette la fin */
        int ch;
        while ((ch = getchar()) != '\n' && ch != EOF) {
        }
        fprintf(stderr, "Erreur : ligne trop longue (max %d caractères).\n", size - 2);
        buf[0] = '\0';
    }
    return true;
}

void menu_print(void)
{
    printf("\n===== Menu =====\n");
    printf("  1. Ajouter une forme\n");
    printf("  2. Modifier une forme\n");
    printf("  3. Voir la liste des formes\n");
    printf("  4. Dessin (nouveau, viewport, sauver, charger, exporter)\n");
    printf("  5. Afficher le dessin (dans le navigateur)\n");
    printf("  0. Quitter\n");
    printf("Tapez un numéro (ou une commande, 'aide' pour la liste).\n");
}

/**
 * Convertit une ligne en numéro positif.
 * @param line texte
 * @return le nombre, ou -1 si le texte n'est pas un petit nombre positif
 */
static int parse_choice(const char *line)
{
    int n = 0;
    int digits = 0;
    while (*line == ' ' || *line == '\t') {
        line++;
    }
    for (; *line >= '0' && *line <= '9'; line++) {
        if (++digits > 3) {
            return -1;
        }
        n = n * 10 + (*line - '0');
    }
    while (*line == ' ' || *line == '\t' || *line == '\r') {
        line++;
    }
    return (digits > 0 && *line == '\0') ? n : -1;
}

int menu_choice(const char *line)
{
    return parse_choice(line);
}

/**
 * Pose une question et lit la réponse.
 * @param question texte de la question
 * @param buf tableau où écrire la réponse
 * @param size taille du tableau
 * @return false si l'entrée est terminée (Ctrl+D)
 */
static bool ask(const char *question, char *buf, int size)
{
    printf("  %s : ", question);
    fflush(stdout);
    return read_line(buf, size);
}

/**
 * Affiche un sous-menu et lit le choix de l'utilisateur.
 * @param title titre du sous-menu
 * @param items actions proposées
 * @param n nombre d'actions
 * @param choice numéro choisi (0 = retour)
 * @return false si l'entrée est terminée (Ctrl+D)
 */
static bool choose(const char *title, const MenuItem *items, int n, int *choice)
{
    char line[LINE_SIZE];
    printf("\n--- %s ---\n", title);
    for (int i = 0; i < n; i++) {
        printf("  %d. %s\n", i + 1, items[i].label);
    }
    printf("  0. Retour\n");
    while (1) {
        printf("Choix : ");
        fflush(stdout);
        if (!read_line(line, sizeof(line))) {
            return false;
        }
        *choice = parse_choice(line);
        if (*choice >= 0 && *choice <= n) {
            return true;
        }
        fprintf(stderr, "Erreur : tapez un numéro entre 0 et %d.\n", n);
    }
}

/**
 * Pose les questions d'une action, assemble la commande et l'exécute.
 * @param c dessin
 * @param item action choisie
 * @param id id de la forme visée (NULL si l'action n'en a pas besoin)
 * @return false si l'entrée est terminée (Ctrl+D)
 */
static bool run_item(Canvas *c, const MenuItem *item, const char *id)
{
    char cmd[LINE_SIZE];
    char answer[LINE_SIZE];
    int len = snprintf(cmd, sizeof(cmd), "%s", item->cmd);
    if (id != NULL) {
        len += snprintf(cmd + len, sizeof(cmd) - len, " %s", id);
    }

    for (int i = 0; i < MAX_QUESTIONS && item->questions[i] != NULL; i++) {
        if (!ask(item->questions[i], answer, sizeof(answer))) {
            return false;
        }
        if (strspn(answer, " \t\r") == strlen(answer)) {
            printf("Annulé (réponse vide).\n");
            return true;
        }
        /* on vérifie qu'il reste de la place avant d'ajouter la réponse */
        if (len + 1 + (int)strlen(answer) >= (int)sizeof(cmd)) {
            fprintf(stderr, "Erreur : réponses trop longues.\n");
            return true;
        }
        len += snprintf(cmd + len, sizeof(cmd) - len, " %s", answer);
    }
    execute_line(c, cmd, false);
    return true;
}

/**
 * Affiche la liste des formes du dessin.
 * @param c dessin
 */
static void show_list(Canvas *c)
{
    char cmd[] = "liste"; /* execute_line modifie la ligne : il faut un tableau */
    execute_line(c, cmd, false);
}

/**
 * Sous-menu "Modifier une forme" : demande l'id puis la modification.
 * @param c dessin
 * @return false si l'entrée est terminée (Ctrl+D)
 */
static bool edit_shape(Canvas *c)
{
    char id[LINE_SIZE];
    int choice;
    if (c->shapes.head == NULL) {
        printf("Aucune forme à modifier : ajoutez-en une d'abord.\n");
        return true;
    }
    show_list(c);
    if (!ask("Id de la forme à modifier", id, sizeof(id))) {
        return false;
    }
    if (parse_choice(id) < 0) {
        fprintf(stderr, "Erreur : l'id doit être un numéro de la liste.\n");
        return true;
    }
    if (!choose("Que voulez-vous modifier ?", EDIT_ITEMS, COUNT(EDIT_ITEMS), &choice)) {
        return false;
    }
    return choice == 0 || run_item(c, &EDIT_ITEMS[choice - 1], id);
}

/**
 * Écrit l'aperçu et l'ouvre dans le navigateur.
 * @param c dessin
 */
static void open_preview(const Canvas *c)
{
    if (!file_write_preview(c, PREVIEW_FILE)) {
        return;
    }
    preview_open = true;
    /* xdg-open ouvre le fichier avec le navigateur par défaut (Linux) ;
       le '&' rend la main tout de suite au programme */
    if (system("xdg-open " PREVIEW_FILE " > /dev/null 2>&1 &") != 0) {
        printf("Ouvrez le fichier '%s' dans votre navigateur.\n", PREVIEW_FILE);
    }
    printf("Aperçu ouvert dans le navigateur (%s).\n", PREVIEW_FILE);
    printf("Il se met à jour tout seul après chaque modification.\n");
}

void menu_update_preview(const Canvas *c)
{
    if (preview_open) {
        file_write_preview(c, PREVIEW_FILE);
    }
}

CmdResult menu_run(Canvas *c, int choice)
{
    int sub;
    bool ok = true;
    switch (choice) {
    case 1:
        ok = choose("Quelle forme ?", SHAPE_ITEMS, COUNT(SHAPE_ITEMS), &sub);
        if (ok && sub != 0) {
            ok = run_item(c, &SHAPE_ITEMS[sub - 1], NULL);
        }
        break;
    case 2:
        ok = edit_shape(c);
        break;
    case 3:
        show_list(c);
        break;
    case 4:
        ok = choose("Dessin", FILE_ITEMS, COUNT(FILE_ITEMS), &sub);
        if (ok && sub != 0) {
            ok = run_item(c, &FILE_ITEMS[sub - 1], NULL);
        }
        break;
    case 5:
        open_preview(c);
        break;
    case 0:
        return CMD_QUIT;
    default:
        fprintf(stderr, "Erreur : tapez un numéro entre 0 et 5.\n");
        break;
    }
    return ok ? CMD_OK : CMD_QUIT;
}
