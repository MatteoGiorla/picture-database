/**
* @file dedup.c
* @brief pictDB library: dedup implementation.
*
* @author Cédric Viaccoz
* @author Matteo Giorla
* @date Apr 2016
*/

#include "pictDB.h"
#include <string.h>
#include <stdlib.h>
#include <openssl/sha.h>

/********************************************************************//*
 * Compares 2 SHA-hash values. Returns 1 if both are the same, 0 if not.
 */
int same_SHA_hash(const unsigned char* SHA1, const unsigned char* SHA2)
{
    for(int i = 0; i < SHA256_DIGEST_LENGTH; ++i) {
        if(SHA1[i] != SHA2[i]) {
            return 0;
        }
    }
    //we reached the end of the 2 SHA without any difference so they were the same.
    return 1;
}

/********************************************************************//*
 * avoids the same image (same content) to be present several times in the database
 */
int do_name_and_content_dedup(struct pictdb_file* pictdb_file, uint32_t index)
{
    //on teste que l'image à l'index donné est valide
    if (pictdb_file->metadata[index].is_valid == EMPTY) {
        return ERR_IO;
    }

    for (int i = 0; i < pictdb_file->header.max_files; ++i) {
        //pour toutes les images valides autres que celle à la position index
        if (i != index && pictdb_file->metadata[i].is_valid == NON_EMPTY) {
            //si le nom de l’image est identique à celui de l’image à la position index
            size_t size_of_pictid = strlen(pictdb_file->metadata[index].pict_id) + 1;
            if (!strncmp(pictdb_file->metadata[index].pict_id, pictdb_file->metadata[i].pict_id, size_of_pictid)) {
                return ERR_DUPLICATE_ID;
            }

            /*si la valeur SHA de l’image est identique à celle de l’image à la position
            index, on peut alors éviter la duplication de l’image à la position index
            (pour toutes ses résolutions)*/
            if (same_SHA_hash(pictdb_file->metadata[index].SHA, pictdb_file->metadata[i].SHA)) {
                /*modification de l’entrée index des métadonnées pour y référencer
                les attributs de l’autre copie de l’image
                (les trois offset et les deux tailles)
                (la taille d’origine est forcément la même)*/
                pictdb_file->metadata[index].offset[RES_THUMB] = pictdb_file->metadata[i].offset[RES_THUMB];
                pictdb_file->metadata[index].offset[RES_SMALL] = pictdb_file->metadata[i].offset[RES_SMALL];
                pictdb_file->metadata[index].offset[RES_ORIG] = pictdb_file->metadata[i].offset[RES_ORIG];

                pictdb_file->metadata[index].size[RES_THUMB] = pictdb_file->metadata[i].size[RES_THUMB];
                pictdb_file->metadata[index].size[RES_SMALL] = pictdb_file->metadata[i].size[RES_SMALL];

                return 0;
            }
        }
    }

    //Si l’image à la position index n’a pas de doublon
    pictdb_file->metadata[index].offset[RES_ORIG] = 0;

    return 0;
}















