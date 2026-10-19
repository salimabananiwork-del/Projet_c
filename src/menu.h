/**
 * Menu interactif : propose les actions et les formes avec des numéros,
 * puis pose les questions une par une.
 */
#ifndef MENU_H
#define MENU_H

#include <stdbool.h>
#include "canvas.h"
#include "commands.h"

/**
 * Lit une ligne au clavier et enlève le retour à la ligne.
 * Une ligne trop longue est ignorée (avec un message) et rendue vide.
 * @param buf tableau où écrire la ligne
 * @param size taille du tableau
 * @return false si l'entrée est terminée (Ctrl+D)
 */
bool read_line(char *buf, int size);

/**
 * Affiche le menu principal.
 */
void menu_print(void);

/**
 * Indique si la ligne est un numéro (choix du menu) ou une commande.
 * @param line ligne tapée
 * @return le numéro tapé, ou -1 si la ligne n'est pas un numéro
 */
int menu_choice(const char *line);

/**
 * Exécute un choix du menu principal (pose les questions nécessaires).
 * @param c dessin
 * @param choice numéro choisi (renvoyé par menu_choice)
 * @return CMD_QUIT si l'utilisateur quitte ou si l'entrée est terminée
 */
CmdResult menu_run(Canvas *c, int choice);

/**
 * Met à jour l'aperçu dans le navigateur, s'il a été ouvert (choix 5).
 * À appeler après chaque action.
 * @param c dessin
 */
void menu_update_preview(const Canvas *c);

#endif
