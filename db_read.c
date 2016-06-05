/**
 * @file db_read.c
 * @brief pictDB library: do_read implementation.
 *
 * @author Cédric Viaccoz
 * @author Matteo Giorla
 * @date Apr 2016
 */

#include "pictDB.h"
#include "image_content.h" //for lazily_resize

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#define found db_file->metadata[index]


int do_read(const char* pict_id, const int resolution_code, char** image_buffer, uint32_t * const image_size, struct pictdb_file * const db_file)
{

    //we first need to locate the good metadata corresponding to the name of the image.
    int iter = 0;
    int index = -1;
    while(index == -1 && iter < db_file->header.max_files) {
        if(strncmp(pict_id, db_file->metadata[iter].pict_id, strlen(pict_id)+1) == 0) {
            index = iter ;
        }
        ++iter;
    }

    //test wether the image was found, and if found, wether the original image is correctly referenced in the metadata
    if(index < 0) {
        return ERR_FILE_NOT_FOUND;
    } else if(found.is_valid == EMPTY || found.offset[RES_ORIG] == 0|| found.size[RES_ORIG] == 0) {
        return ERR_FILE_NOT_FOUND;
    }

    if(found.offset[resolution_code] == 0) {
        //the resolution of the image we seek doesn't exist, so we need to create it.
        int errorReceived = lazily_resize(resolution_code, db_file, index);
        if(errorReceived) {
            return errorReceived;
        }
    }

    //checks if the size and the offset of the image we want is correctly initialized
    if(found.size[resolution_code] == 0 || found.offset[resolution_code] == 0) {
        return ERR_FILE_NOT_FOUND;
    }

    //calloc of the content of image_buffer, since it is the pointer to memory where the image is stored.
    char* actual_image = calloc(found.size[resolution_code], sizeof(char)); //we save place on the heap to store the image
    if(actual_image == NULL) {
        return ERR_OUT_OF_MEMORY;
    }

    //checking integrity of the filepath
    if(db_file->fpdb == NULL) {
        free_the_buffer(&actual_image);
        return ERR_IO;
    }

    //placement
    int errorSeek = fseek(db_file->fpdb, found.offset[resolution_code], SEEK_SET);
    if(errorSeek == -1) {
        free_the_buffer(&actual_image);
        return ERR_IO;
    }

    //reading and loading the image
    size_t actual_size = fread(actual_image, sizeof(char), found.size[resolution_code], db_file->fpdb);
    if(actual_size == 0) {
        free_the_buffer(&actual_image);
        return ERR_IO;
    }

    *image_size = actual_size;
    *image_buffer = actual_image;
    return 0;
}
