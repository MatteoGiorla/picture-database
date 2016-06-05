/**
 * @file db_insert.c
 * @brief pictDB library: do_insert implementation.
 *
 * @author Cédric Viaccoz
 * @author Matteo Giorla
 * @date Apr 2016
 */

#include "pictDB.h"
#include "error.h"
#include "dedup.h" //for do_name_and_content_dedup
#include "image_content.h" //for get_resolution

#include <stdint.h> // for uint32_t, uint64_t
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <openssl/sha.h> // for SHA

/********************************************************************//*
 * Function used to insert an image in the database
 */
int do_insert(const char* const image, size_t image_size, char* pict_id, struct pictdb_file* db_file)
{
    /* ====== recherche d'une position libre dans l'index ====== */

    //si le nombre actuel d'images dans la base de donnée n'est pas inférieur à max_files
    if (!(db_file->header.num_files < db_file->header.max_files)) {
        return ERR_FULL_DATABASE;
    }

    //recherche d'une entrée vide dans la metadata
    int found = 0;
    int i = 0;
    while(!found) {
        if (db_file->metadata[i].is_valid == EMPTY) {
            //placement de la valeur hash SHA256 de l’image dans le champ SHA
            unsigned char sha[SHA256_DIGEST_LENGTH];
            (void)SHA256((unsigned char *)image, image_size, sha);

            for(int j = 0; j < SHA256_DIGEST_LENGTH; ++j) {
                db_file->metadata[i].SHA[j] = sha[j];
            }

            //copie de la chaîne de caractères pict_id dans le champs correspondant
            strncpy(db_file->metadata[i].pict_id, pict_id, MAX_PIC_ID+1);

            //stockage de la taille de l’image (passée en paramètre) dans le champs RES_ORIG
            if (image_size > UINT32_MAX) {
                /*on teste s'il y a un overflow (lors du stockage d'une valeur de type size_t
                dans un uint32_t) et on renvoie une erreur le cas échéant*/
                return ERR_RESOLUTIONS;
            }
            db_file->metadata[i].size[RES_ORIG] = image_size;
            db_file->metadata[i].is_valid = NON_EMPTY;
            //pour s'assurer que do_read detectera l'absence de thumb/small même si une image fut dans cette métadata précdemment
            db_file->metadata[i].offset[RES_THUMB] = 0;
            db_file->metadata[i].offset[RES_SMALL] = 0;

            found = 1;
        } else {
            ++i;
        }
    }

    /* ====== déduplication de l'image ====== */
    int dedup_status = do_name_and_content_dedup(db_file, i);
    if(dedup_status) {
        return dedup_status;
    }

    /* ====== écriture de l'image sur le disque ====== */

    //si l'image à la position i n'a pas de doublon, écriture de son contenu à la fin du fichier
    if (db_file->metadata[i].offset[RES_ORIG] == 0) {

        int fseek_status = fseek(db_file->fpdb, 0, SEEK_END);
        if (fseek_status != 0) {
            return ERR_IO;
        }

        //enregistrement de l'offset (fin du fichier) dans la metadata
        db_file->metadata[i].offset[RES_ORIG] = ftell(db_file->fpdb);

        int write_status = fwrite(image, sizeof(char), image_size, db_file->fpdb);
        if (write_status != image_size) {
            return ERR_IO;
        }
    }

    /* ====== mise à jour des données de la base d'images ====== */

    //détermination de la largeur et de la hauteur de l'image
    int get_resolution_status = get_resolution(&(db_file->metadata[i].res_orig[1]), &(db_file->metadata[i].res_orig[0]), image , image_size);
    if (get_resolution_status) {
        return get_resolution_status;
    }

    //mise à jour du header
    db_file->header.num_files += 1;
    db_file->header.db_version += 1;

    //écriture sur le disque

    //metadata
    //positionnement
    long initial_offset_for_metadata = sizeof(struct pictdb_header) + i * sizeof(struct pict_metadata);
    int fseek_status = fseek(db_file->fpdb, initial_offset_for_metadata, SEEK_SET); //on se place au bon pictID dans la metadata
    if (fseek_status != 0) {
        return ERR_IO;
    }

    //écriture
    int num_written = fwrite(&(db_file->metadata[i]), sizeof(struct pict_metadata), 1, db_file->fpdb);
    if (num_written != 1) {
        return ERR_IO;
    }

    //header
    //positionnement
    fseek_status = fseek(db_file->fpdb, 0, SEEK_SET); //on se place au premier pict_id dans la metadata
    if (fseek_status != 0) {
        return ERR_IO;
    }
    //écriture
    num_written = fwrite(&(db_file->header), sizeof(struct pictdb_header), 1, db_file->fpdb);
    if (num_written != 1) {
        return ERR_IO;
    }

    return 0;
}