/**
 * @file db_create.c
 * @brief pictDB library: do_create implementation.
 *
 * @author Cédric Viaccoz
 * @author Matteo Giorla
 * @date 21 Mar 2016
 */

#include "pictDB.h"
#include <string.h> // for strncpy and strncat
#include <stdlib.h> //for calloc

/* define the length and the name of the database extension. */
#define FILE_NAME_MAX_W_EXT 37

/********************************************************************//*
 * Creates the database called db_filename. Writes the header and the
 * preallocated empty metadata array to database file.
 */
int do_create(struct pictdb_file* db_file, char const* file_name)
{
    // Sets the DB header name
    strncpy(db_file->header.db_name, CAT_TXT,  MAX_DB_NAME);
    db_file->header.db_name[MAX_DB_NAME] = '\0';

    char final_file_name[FILE_NAME_MAX_W_EXT + 1];

    //copie de file_name dans final_file_name pour y ajouter l'extension
    int size_file = 0;
    char c;
    do {
        if(size_file > MAX_DB_NAME + 1) {
            return ERR_INVALID_FILENAME; //filename donné en argument est trop grand
        }
        c = file_name[size_file];
        final_file_name[size_file] = c;
        ++size_file;
    } while(c != '\0');

    strcat(final_file_name, EXTENSION);  //concaténation de l'extension à final_file_name.
    FILE* file_toCreate = NULL;
    file_toCreate = fopen(final_file_name, "wb");
    if(file_toCreate == NULL) {
        return ERR_IO;     //le fichier ne s'est pas ouvert correctement.
    }

    strncpy(db_file->header.db_name, file_name, MAX_DB_NAME);
    //initialisation du db_header: (avec initialisation par défaut à 0 ou '\0' pour les différents champs int et/ou char)
    db_file->header.db_version = 0;
    db_file->header.num_files = 0;
    db_file->header.unused_32 = 0;
    db_file->header.unused_64 = 0;

    db_file->metadata = NULL;
    //On doit d'abord s'assurer de la validité de max files, puis regarder si l'allocation dynamique a marché correctement
    if(db_file->header.max_files > 0 && db_file->header.max_files <= MAX_MAX_FILES) {
        db_file->metadata = calloc(db_file->header.max_files, sizeof(struct pict_metadata));
        if(db_file->metadata == NULL) {
            return ERR_OUT_OF_MEMORY;
        }
        //Initialisation des metadata:
        for(int i = 0; i < db_file->header.max_files; ++i) {

            for(int k = 0; k < MAX_PIC_ID+1; ++k) {
                db_file->metadata[i].pict_id[k] = '\0';
            }

            for(int k = 0; k < SHA256_DIGEST_LENGTH; ++k) {
                db_file->metadata[i].SHA[k] = '\0';
            }
            db_file->metadata[i].res_orig[0] = 0;
            db_file->metadata[i].res_orig[1] = 0;

            for(int k = 0; k < NB_RES; ++k) {
                db_file->metadata[i].size[k] = 0;
            }

            for(int k = 0; k < NB_RES; ++k) {
                db_file->metadata[i].offset[k] = 0;
            }
            db_file->metadata[i].is_valid = EMPTY;
            db_file->metadata[i].unused_16 = 0;
        }
    } else {
        return ERR_MAX_FILES;
    }

    //initialisation du filepath
    db_file->fpdb = file_toCreate;
    file_toCreate = NULL;

    //écriture de chaque struct sur le fichier.
    int items = fwrite(&(db_file->header), sizeof(struct pictdb_header), 1, db_file->fpdb);
    items += fwrite(&(db_file->metadata[0]), sizeof(struct pict_metadata), (size_t)db_file->header.max_files, db_file->fpdb);
    if(ferror(db_file->fpdb)) {
        return ERR_IO; // I/O error.
    }

    printf("%d item(s) written\n", items);
    return 0;
}