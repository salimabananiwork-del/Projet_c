/**
 * Point d'entrée : boucle qui affiche le menu et exécute les choix.
 * On peut répondre au menu avec un numéro, ou taper directement une commande.
 * Usage : ./svg_cli [fichier]   (le fichier est chargé au démarrage)
 */
#include <stdio.h>
#include "canvas.h"
#include "commands.h"
#include "file_io.h"
#include "menu.h"

/**
 * Programme principal.
 * @param argc nombre d'arguments
 * @param argv arguments (argv[1] : fichier à charger, facultatif)
 * @return 0 si tout s'est bien passé
 */
int main(int argc, char **argv)
{
    if (argc > 2) {
        fprintf(stderr, "Usage : %s [fichier]\n", argv[0]);
        return 1;
    }

    Canvas canvas;
    canvas_init(&canvas);

    if (argc == 2) {
        if (file_load(&canvas, argv[1])) {
            printf("Dessin chargé depuis '%s'.\n", argv[1]);
        }
    }

    printf("=== svg_cli : éditeur de dessins vectoriels ===\n");

    char line[LINE_SIZE];
    bool show_menu = true;
    while (1) {
        if (show_menu) {
            menu_print();
        }
        printf("> ");
        fflush(stdout);
        if (!read_line(line, sizeof(line))) {
            printf("\n"); /* fin de l'entrée (Ctrl+D) */
            break;
        }
        CmdResult res;
        int choice = menu_choice(line);
        if (choice >= 0) {
            res = menu_run(&canvas, choice);
            show_menu = true; /* on réaffiche le menu après une action du menu */
        } else {
            res = execute_line(&canvas, line, false);
            show_menu = false; /* en mode commandes, le menu gênerait */
        }
        if (res == CMD_QUIT) {
            break;
        }
        menu_update_preview(&canvas);
    }

    canvas_free(&canvas);
    printf("Au revoir !\n");
    return 0;
}
