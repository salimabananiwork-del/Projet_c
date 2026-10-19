/**
 * Sauvegarde, chargement et export SVG.
 *
 * Format de sauvegarde : un fichier texte contenant la liste des commandes
 * qui recréent le dessin. Charger = exécuter ces commandes une par une.
 */
#ifndef FILE_IO_H
#define FILE_IO_H

#include <stdbool.h>
#include "canvas.h"

/**
 * Sauvegarde le dessin dans un fichier de commandes.
 * @param c dessin
 * @param path chemin du fichier
 * @return false en cas d'erreur
 */
bool file_save(const Canvas *c, const char *path);

/**
 * Charge un fichier de commandes. Si une ligne est invalide, le dessin
 * actuel n'est pas modifié.
 * @param c dessin à remplacer
 * @param path chemin du fichier
 * @return false en cas d'erreur
 */
bool file_load(Canvas *c, const char *path);

/**
 * Exporte le dessin en vrai fichier SVG (ouvrable dans un navigateur).
 * @param c dessin
 * @param path chemin du fichier
 * @return false en cas d'erreur
 */
bool file_export_svg(const Canvas *c, const char *path);

/**
 * Écrit une page HTML d'aperçu qui contient le dessin en SVG et se
 * recharge toute seule dans le navigateur (toutes les secondes).
 * @param c dessin
 * @param path chemin du fichier (ex : apercu.html)
 * @return false en cas d'erreur
 */
bool file_write_preview(const Canvas *c, const char *path);

#endif
