/**
 * @file image_content.c
 * @brief pictDB library: image_content implementation.
 *
 * @author Cédric Viaccoz
 * @author Matteo Giorla
 * @date Apr 2016
 */

#include "pictDB.h"
#include "image_content.h"

#include <vips/vips.h>
#include <stdlib.h>

/********************************************************************//*
 * @brief Computes the shrinking factor (keeping aspect ratio)
 *
 * @param image The image to be resized.
 * @param max_thumbnail_width The maximum width allowed for thumbnail creation.
 * @param max_thumbnail_height The maximum height allowed for thumbnail creation.
 */
double shrink_value(VipsImage *image, int max_thumbnail_width, int max_thumbnail_height)
{
    const double h_shrink = (double) max_thumbnail_width  / (double) image->Xsize ;
    const double v_shrink = (double) max_thumbnail_height / (double) image->Ysize ;
    return h_shrink > v_shrink ? v_shrink : h_shrink ;
}

/********************************************************************//*
 * Function used to create reduced images (in formats "small" and "thumbnail")
 */
int lazily_resize(int resolution_code, struct pictdb_file* db_file, size_t index)
{
    //si la résolution donnée est la résolution originale, lazily_resize ne fait rien
    if (resolution_code == RES_ORIG) {
        return 0;
    }
    //si la résolution passée en argument est invalide, la fonction retourne un code d'erreur
    if (resolution_code != RES_THUMB && resolution_code != RES_SMALL) {
        return ERR_RESOLUTIONS;
    }
    //si l'image demandée existe déjà dans la résolution demandée, la fonction ne fait rien
    if (db_file->metadata[index].size[resolution_code] != 0) {
        return 0;
    }

    //création d'une nouvelle image et ouverture de l'image originale dans l'image vips créée
    VipsImage* original;

    uint32_t size_of_original = db_file->metadata[index].size[RES_ORIG];
    void* image_memory = calloc(1, size_of_original);

    int fseek_status = fseek(db_file->fpdb, db_file->metadata[index].offset[RES_ORIG], SEEK_SET);
    if (fseek_status != 0) {
        free_the_buffer((char**)&image_memory);
        return ERR_IO;
    }

    int num_read = fread(image_memory, db_file->metadata[index].size[RES_ORIG], 1, db_file->fpdb);
    if (num_read != 1) {
        free_the_buffer((char**)&image_memory);
        return ERR_IO;
    }

    int loadStatus = vips_jpegload_buffer(image_memory, size_of_original, &original, NULL);
    if (loadStatus != 0) {
        free_the_buffer((char**)&image_memory);
        return ERR_FILE_NOT_FOUND;
    }
    if (original == NULL) {
        free_the_buffer((char**)&image_memory);
        return ERR_VIPS;
    }

    //création de la nouvelle variante de l'image dans la résolution spécifiée
    VipsObject* process = VIPS_OBJECT(vips_image_new());

    VipsImage** resized = (VipsImage**) vips_object_local_array(process, 1);

    double ratio = shrink_value(original, db_file->header.res_resized[resolution_code][0], db_file->header.res_resized[resolution_code][1]);

    vips_resize(original, resized, ratio, NULL);

    size_t* length = calloc(sizeof(size_t), 1);
    if(length == NULL) {
        void * nullpointer = NULL; //to use pointer_liberation without having an image buffer initialized.
        pointer_liberation(&original, &image_memory, &length, &nullpointer, &process);
        return ERR_OUT_OF_MEMORY;
    }

    uint32_t size_of_resized = db_file->metadata[index].size[resolution_code];
    void* image_buffer = calloc(1, size_of_resized);

    int saveStatus = vips_jpegsave_buffer(*resized, &image_buffer, length, NULL);
    if (saveStatus != 0) {
        pointer_liberation(&original, &image_memory, &length, &image_buffer, &process);
        return ERR_VIPS;
    }
    uint32_t size_for_metadata = *length;

    //copie du contenu de l'image à la fin du fichier pictDB
    fseek_status = fseek(db_file->fpdb, 0, SEEK_END);
    if (fseek_status != 0) {
        pointer_liberation(&original, &image_memory, &length, &image_buffer, &process);
        return ERR_IO;
    }

    uint64_t offset_for_metadata = ftell(db_file->fpdb);

    int written = fwrite(image_buffer, size_for_metadata, 1, db_file->fpdb);
    if (written != 1) {
        pointer_liberation(&original, &image_memory, &length, &image_buffer, &process);
        return ERR_IO;
    }

    //libération des pointeurs
    pointer_liberation(&original, &image_memory, &length, &image_buffer, &process);
    //mise à jour des metadatas en mémoire
    db_file->metadata[index].offset[resolution_code] = offset_for_metadata; //endroit où l'image réduite est stockée
    db_file->metadata[index].size[resolution_code] = size_for_metadata; //taille de l'image réduite

    //mise à jour des metadatas sur le disque
    //positionnement
    long initial_offset_for_metadata = sizeof(struct pictdb_header) + index * sizeof(struct pict_metadata);
    fseek_status = fseek(db_file->fpdb, initial_offset_for_metadata, SEEK_SET); //on se place au bon pictID dans la metadata
    if (fseek_status != 0) return ERR_IO;

    //écriture
    int num_written = fwrite(&db_file->metadata[index], sizeof(struct pict_metadata), 1, db_file->fpdb);
    if (num_written != 1) return ERR_IO;

    return 0;
}

/********************************************************************//*
 * Returns the resoltion of a given image.
 */
int get_resolution(uint32_t* height, uint32_t* width, const char* image_buffer , size_t image_size)
{
    //création d'une nouvelle image et ouverture de l'image donnée dans l'image vips créée
    VipsImage* vips_image;

    int loadStatus = vips_jpegload_buffer((void*)image_buffer, image_size, &vips_image, NULL);
    if (loadStatus != 0) {
        return ERR_VIPS;
    }

    *height = vips_image_get_height(vips_image);
    *width = vips_image_get_width(vips_image);

    return 0;
}


/********************************************************************//*
 * Take care of freeing all the pointer of lazily resize
 * when wanted or when there is an error.
 */
void
pointer_liberation(VipsImage** original, void** image_memory, size_t** length, void ** image_buffer, VipsObject ** process)
{

    /*if(*original != NULL) {
        free(*original);
        *original = NULL;
    }*/
    if (*image_memory != NULL) {
        free(*image_memory);
        *image_memory = NULL;
    }
    if(*length != NULL) {
        free(*length);
        *length = NULL;
    }
    if(*image_buffer != NULL) {
        free(*image_buffer);
        *image_buffer = NULL;
    }
    if(*process != NULL) {
        g_object_unref(*process);
        *process = NULL;
    }
}
