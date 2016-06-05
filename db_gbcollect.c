/**
 * @file db_gbcollect.c
 * @brief pictDB library: garbage collector implementation.
 *
 * @author Cédric Viaccoz
 * @author Matteo Giorla
 * @date Mai 2016
 */

#include "pictDB.h"
#include <stdlib.h> //for calloc
#include <stdio.h> //for rename and remove
#include <string.h>

#define current db_file->metadata[i]

int read_and_write_image(struct pictdb_file* db_file, struct pictdb_file* tmp_pictdb_file, int i, const int resolution_code)
{
    //lecture
    char * image_buffer = NULL;
    uint32_t image_size = 0;
    unsigned int errorRead = do_read(current.pict_id, resolution_code, &image_buffer, &image_size, db_file);
    if(errorRead) {
        free_the_buffer(&image_buffer);
        do_close(db_file);
        return errorRead;
    }

    if (resolution_code == RES_ORIG) {
        //écriture
        int errorStatus = do_insert(image_buffer, image_size, current.pict_id, tmp_pictdb_file);
        if(errorStatus) {
            free_the_buffer(&image_buffer);
            do_close(db_file);
            return errorStatus;
        }
    } else {
        //copie du contenu de l'image à la fin du fichier pictDB
        int fseek_status = fseek(tmp_pictdb_file->fpdb, 0, SEEK_END);
        if (fseek_status != 0) {
            free_the_buffer(&image_buffer);
            return ERR_IO;
        }

        uint64_t offset_for_metadata = ftell(tmp_pictdb_file->fpdb);

        int written = fwrite(image_buffer, image_size, 1, tmp_pictdb_file->fpdb);
        if (written != 1) {
            free_the_buffer(&image_buffer);
            return ERR_IO;
        }

        //mise à jour des metadatas en mémoire
        tmp_pictdb_file->metadata[i].offset[resolution_code] = offset_for_metadata; //endroit où l'image réduite est stockée
        tmp_pictdb_file->metadata[i].size[resolution_code] = image_size; //taille de l'image réduite

        //mise à jour des metadatas sur le disque
        //positionnement
        long initial_offset_for_metadata = sizeof(struct pictdb_header) + i * sizeof(struct pict_metadata);
        fseek_status = fseek(tmp_pictdb_file->fpdb, initial_offset_for_metadata, SEEK_SET); //on se place au bon pictID dans la metadata
        if (fseek_status != 0) {
            free_the_buffer(&image_buffer);
            return ERR_IO;
        }

        //écriture
        int num_written = fwrite(&tmp_pictdb_file->metadata[i], sizeof(struct pict_metadata), 1, tmp_pictdb_file->fpdb);
        if (num_written != 1) {
            free_the_buffer(&image_buffer);
            return ERR_IO;
        }
    }

    //libération mémoire
    free_the_buffer(&image_buffer);

    return 0;
}

/********************************************************************//*
 * Performs garbage collecting on pictDB.
 * Requires a temporary filename for copying the pictDB.
 */
int do_gbcollect(struct pictdb_file* db_file, char const* orig_file_name, char* tmp_file_name)
{
    //création d'un pictdb_file temporaire
    struct pictdb_file tmp_pictdb_file;
    //copie du header de la struct
    tmp_pictdb_file.header = db_file->header;
    int create_status = do_create(&tmp_pictdb_file, tmp_file_name);
    if (create_status) {
        return create_status;
    }
    //on remplace le header "vide" créé par do_create
    tmp_pictdb_file.header = db_file->header;

    for (int i = 0; i < tmp_pictdb_file.header.max_files; ++i) {
        if (current.is_valid == NON_EMPTY) {
            //copie de l'image originale dans le fichier temporaire
            int read_and_write_status = read_and_write_image(db_file, &tmp_pictdb_file, i, RES_ORIG);
            if (read_and_write_status) {
                fclose(tmp_pictdb_file.fpdb);
                return read_and_write_status;
            }

            if (current.size[RES_SMALL] != 0) {
                //copie de l'image en résolution small dans le fichier temporaire si elle est présente dans la base originale
                int read_and_write_status = read_and_write_image(db_file, &tmp_pictdb_file, i, RES_SMALL);
                if (read_and_write_status) {
                    fclose(tmp_pictdb_file.fpdb);
                    return read_and_write_status;
                }
            }
            if (current.size[RES_THUMB] != 0) {
                //copie de l'image en résolution thumb dans le fichier temporaire si elle est présente dans la base originale
                int read_and_write_status = read_and_write_image(db_file, &tmp_pictdb_file, i, RES_THUMB);
                if (read_and_write_status) {
                    fclose(tmp_pictdb_file.fpdb);
                    return read_and_write_status;
                }
            }
        }
    }

    //pour que la version soit incrémentée de 1 après le gc.
    tmp_pictdb_file.header.db_version = db_file->header.db_version + 1;

    //écriture du header en mémoire
    int fseek_status = fseek(tmp_pictdb_file.fpdb, 0, SEEK_SET); //on se place au bon pictID dans la metadata
    if (fseek_status != 0) {
        return ERR_IO;
    }
    int items = fwrite(&(tmp_pictdb_file.header), sizeof(struct pictdb_header), 1, tmp_pictdb_file.fpdb);
    if(items != 1) {
        fclose(tmp_pictdb_file.fpdb);
        return ERR_IO; // I/O error.
    }

    //fermeture de la struct temporaire
    do_close(&tmp_pictdb_file);

    //fermeture de la struct originale
    do_close(db_file);

    int remove_status = remove(orig_file_name);
    if (remove_status) {
        return ERR_IO;
    }

    int rename_status = rename(strcat(tmp_file_name, EXTENSION), orig_file_name);
    if (rename_status) {
        return ERR_IO;
    }

    return 0;
}
