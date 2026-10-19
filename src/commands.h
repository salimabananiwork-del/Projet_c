/**
 * Analyse et exécution des commandes tapées par l'utilisateur.
 */
#ifndef COMMANDS_H
#define COMMANDS_H

#include <stdbool.h>
#include "canvas.h"

/** Taille maximale d'une ligne de commande. */
#define LINE_SIZE 4096

/** Résultat de l'exécution d'une commande. */
typedef enum {
    CMD_OK,    /**< commande réussie */
    CMD_ERROR, /**< commande invalide */
    CMD_QUIT   /**< l'utilisateur veut quitter */
} CmdResult;

/**
 * Exécute une ligne de commande (la ligne est modifiée par le découpage).
 * Les lignes vides et celles commençant par '#' sont ignorées.
 * @param c dessin
 * @param line ligne à exécuter
 * @param from_file true si la ligne vient d'un fichier (pas de messages,
 *        et certaines commandes sont interdites)
 * @return le résultat de la commande
 */
CmdResult execute_line(Canvas *c, char *line, bool from_file);

#endif
