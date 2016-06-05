/**
 * @file db_delete.c
 * @brief pictDB library: do_delete implementation.
 *
 * @author Cédric Viaccoz
 * @author Matteo Giorla
 * @date 11 Avr 2016
 */

#include "pictDB.h"

#include <string.h>
#include <stdio.h> // for fseek and fwrite

/********************************************************************//*
 * Deletes the image picID on the struct and the disk file.
 */
int do_delete(const char* pictID, struct pictdb_file* pictdb_file)
{
    if (pictdb_file->header.num_files == 0) {
        //renvoie d'une erreur si la base de données est vide
        return ERR_INVALID_PICID;
    }
    //recherche de la référence à l'image qui a le même nom dans la base de donnée
    int ID_not_found = 1; //pour renvoyer une erreur si aucune image dans la base de donnée n'a cet identifiant
    size_t pictNumber = 0; //pour se placer à la bonne image dans la metadata
    for (int i = 0; i < pictdb_file->header.max_files; ++i) {
        size_t size_of_pictid = strlen(pictID) + 1;
        if(!strncmp(pictdb_file->metadata[i].pict_id, pictID, size_of_pictid) && pictdb_file->metadata[i].is_valid == NON_EMPTY) {
            ID_not_found = 0;
            pictNumber = i;
            //invalidation de la référence en écrivant la valeur 0 dans is_valid
            pictdb_file->metadata[i].is_valid = EMPTY;

            //pour s'assurer que do_read detectera l'absence de thumb/small même si une image fut dans cette métadata précdemment
            pictdb_file->metadata[i].offset[RES_THUMB] = 0;
            pictdb_file->metadata[i].offset[RES_SMALL] = 0;

        }
    }
    if (ID_not_found) {
        //renvoie d'une erreur si l'image n'est pas trouvée
        return ERR_INVALID_PICID;
    } else {

        //écriture des métadonnées sur le disque
        //positionnement
        long initialOffset = sizeof(struct pictdb_header) + pictNumber * sizeof(struct pict_metadata);
        int fseekStatus = fseek(pictdb_file->fpdb, initialOffset, SEEK_SET); //on se place au bon pictID dans la metadata
        if (fseekStatus != 0) {
            return ERR_IO;
        }
        //écriture
        int numberOfItems = fwrite(&(pictdb_file->metadata[pictNumber]), sizeof(struct pict_metadata), 1, pictdb_file->fpdb);
        if (numberOfItems != 1) {
            return ERR_IO;
        }

        //mise à jour du header en cas de succès
        pictdb_file->header.num_files -= 1; //on diminue de 1 le nombre d'images dans la base de donnée
        pictdb_file->header.db_version += 1; //on augmente de 1 le numéro de version de la base de donnée

        //écriture du header sur le disque
        //positionnement
        fseekStatus = fseek(pictdb_file->fpdb, 0, SEEK_SET); //on se place au premier pict_id dans la metadata
        if (fseekStatus != 0) {
            return ERR_IO;
        }
        //écriture
        numberOfItems = fwrite(&(pictdb_file->header), sizeof(struct pictdb_header), 1, pictdb_file->fpdb);
        if (numberOfItems != 1 || ferror(pictdb_file->fpdb)) {
            return ERR_IO;
        }
    }

    return 0;
}
