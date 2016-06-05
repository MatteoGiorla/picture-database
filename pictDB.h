/**
 * @file pictDB.h
 * @brief Main header file for pictDB core library.
 *
 * Defines the format of the data structures that will be stored on the disk
 * and provides interface functions.
 *
 * The picture database starts with exactly one header structure
 * followed by exactly pictdb_header.max_files metadata
 * structures. The actual content is not defined by these structures
 * because it should be stored as raw bytes appended at the end of the
 * database file and addressed by offsets in the metadata structure.
 *
 * @author Mia Primorac
 *
 * @author Cédric Viaccoz
 * @author Matteo Giorla
 * @date 14 Mar 2016
 */

#ifndef PICTDBPRJ_PICTDB_H
#define PICTDBPRJ_PICTDB_H

#include "error.h" /* not needed here, but provides it as required by
                    * all functions of this lib.
                    */
#include <stdio.h> // for FILE
#include <stdint.h> // for uint32_t, uint64_t
#include <openssl/sha.h> // for SHA256_DIGEST_LENGTH

#define CAT_TXT "EPFL PictDB binary"

/* constraints */
#define MAX_DB_NAME 31  // max. size of a PictDB name
#define MAX_PIC_ID 127  // max. size of a picture id
#define MAX_MAX_FILES 100000
#define MAX_THUMB_RES 128
#define MAX_SMALL_RES 512
/* For is_valid in pictdb_metadata */
#define EMPTY 0
#define NON_EMPTY 1

// pictDB library internal codes for different picture resolutions.
#define RES_THUMB 0
#define RES_SMALL 1
#define RES_ORIG  2
#define NB_RES    3

#define EXTENSION ".pictDB"
#ifdef __cplusplus
extern "C" {
#endif

/*! \enum do_list_mode
  Enum to choose the displaying format (stdout or json)
 */
enum do_list_mode{
	STDOUT, JSON
};

/*!\struct pictdb_header
   \brief Struct représentant le header d'une image.

  Struct pour les headers, comprenant le nom, la version, le nombre de fichier actuel,
  le nombre maximal de fichier et les possibles résolutions de la base  de donnée.
  Deux variables non utilisés de 32 et 64 bits sont également présents pour d'éventuels
  ajouts dans le futur.
*/
struct pictdb_header {
    char db_name[MAX_DB_NAME + 1];
    uint32_t db_version;
    uint32_t num_files;
    uint32_t max_files;
    uint16_t res_resized[NB_RES - 1][NB_RES - 1];
    uint32_t unused_32;
    uint64_t unused_64;
};

/*! \struct pict_metadata
    \brief Struct représentant la metadata d'une image.

 Struct pour les metadata, comprenant l'identificateur, le hashcode et la résolution de l'image.
 Cette struct spécifie également la taille de l'image, son offset (position du fichier dans la DB)
 et une variable indiquant si l'image est encore utilisée ou effacée.
 A nouveau une variable non utilisée de 16 bits est déclarée pour d'éventuels ajouts futurs.
*/
struct pict_metadata {
    char pict_id[MAX_PIC_ID + 1];
    unsigned char SHA[SHA256_DIGEST_LENGTH];
    uint32_t res_orig[2];
    uint32_t size[NB_RES]; // différentes résolutions possibles selon leur taille: « thumbnail », « small » et « original »
    uint64_t offset[NB_RES];
    uint16_t is_valid; // peut prendre deux valeurs : NON_EMPTY ou EMPTY
    uint16_t unused_16;
};

/*! \struct pictdb_file
    \brief Struct représentant une base de données d'images.

 Struct pour les files, comprenant un pointeur sur le fichier de la DataBase,
 les informations générales sous forme d'un header et un tableau de metadata.
*/
struct pictdb_file {
    FILE* fpdb;
    struct pictdb_header header;
    struct pict_metadata * metadata;
};

/**
 * @brief Prints database header informations.
 *
 * @param header The header to be displayed.
 */
void print_header(struct pictdb_header const* header);

/**
 * @brief Prints picture metadata informations.
 *
 * @param metadata The metadata of one picture.
 */
void print_metadata (struct pict_metadata const* metadata);

/**
 * @brief Displays (on stdout or json) pictDB metadata.
 *
 * @param db_file In memory structure with header and metadata.
 * @param do_list_mode enum to choose between the differents formats in the enum do_list_mode
 * @return string to print in the case where the format is JSON, else NULL
 */
const char* do_list(struct pictdb_file const* db_file, enum do_list_mode do_list_mode);


/**
 * @brief Creates the database called db_filename. Writes the header and the
 *        preallocated empty metadata array to database file.
 *
 * @param db_file In memory structure with header and metadata.
 * @param file_name name of the Database to be saved under on the disk.
 * @return error code as defined in error.h if anything went wrong, 0 otherwise.
 */
int do_create(struct pictdb_file* db_file, const char* file_name);


/**
 * @brief Deletes the image from the database given in argument
 *
 * @param pictID the name of the picture wanted to be erased .
 * @param db_file In memory structure with header and metadata.
 * @return error code as defined in error.h if anything went wrong, 0 otherwise.
 */
int do_delete(const char* pictID, struct pictdb_file* db_file);

/**
 * @brief Opens (and only opens) the file indicated by the name, and charges its
 *		  header and metadatas onto the the struct given.
 *
 * @param file_name the name of the file wanted to be open.
 * @param open_mode the mode with which we want to open the file (i.e. "r+", "wb",...).
 * @param pict_file the struct where the header and metadata are stocked.
 * @return error code as defined in error.h if anything went wrong, 0 otherwise.
 */
int do_open(const char* file_name, const char* open_mode, struct pictdb_file* const pict_file);


/**
 * @brief Closes the file associated with struct pictdb_file (which was previously already opened)
 *
 * @param pict_file In memory structure with header and metadata.
 */
void do_close(struct pictdb_file* const pict_file);

/**
 * @brief Transforms a resolution given in form of a character string into the right resolution code
 *
 * @param resolution the given resolution
 * @return error code as defined in error.h if anything went wrong, 0 otherwise.
 */
int resolution_atoi(const char* const resolution);

/**
 * @brief Extracts image from image database and load it
 *
 * @param pict_id name of the picture to find in the db
 * @param resolution_code tells in what resolution we want to read the image (thumbnail, small or original)
 * @param image_buffer adress in memory where the image will be stocked
 * @param image_size adress where the size of the image found will be stocked
 * @param pictdb_file the database to seek the image metadata
 *
 * @return error code as defined in error.h if anything went wrong, 0 otherwise.
 */
int do_read(const char* pict_id, const int resolution_code, char** image_buffer, uint32_t * const image_size, struct pictdb_file * const db_file);

/**
 * @brief Inserts image in image database
 *
 * @param image the image to insert into the database
 * @param image_size the size of the image to insert
 * @param pict_id the id of the image to insert
 * @param db_file the database file into which the image has to be inserted
 *
 * @return error code as defined in error.h if anything went wrong, 0 otherwise.
 */
int do_insert(const char* const image, size_t image_size, char* pict_id, struct pictdb_file* db_file);

/**
 * @brief Performs garbage collecting on pictDB.
 *
 * @param db_file In memory structure with header and metadata.
 * @param orig_file_name name of the original file
 * @param tmp_file_name name of the temporary file to perform the garbage collecting
 *
 * @return error code as defined in error.h if anything went wrong, 0 otherwise.
 */
 int do_gbcollect(struct pictdb_file* db_file, char const* orig_file_name, char* tmp_file_name);

/**
 * @brief Read an image from the disk to store it in the DB
 *
 * @param filename filename of the image on the disk
 * @param length place where the size of the image on the disk will be stored
 * @param image_buffer pointer to the place where the buffered image will be stored on the RAM
 * @return a pointer to the zone in RAM where the image is stored, NULL if an error occured.
 */
int read_disk_image(const char * filename, size_t * const length, char ** image_buffer);

/**
 * @brief Store an image from the RAM into the disk
 *
 * @param filename file name of the image on the disk
 * @param image_buffer place where the image is stored in the RAM
 * @param length size of the image buffered inthe RAM
 * @return error code as defined in error.h if anything went wrong, 0 otherwise.
 */
int write_disk_image(const char * const filename, char** image_buffer, uint32_t length);

/**
 * @brief creates the filename of the image that will be stocked on the disk
 *
 * @param pict_id name of the image
 * @param resolution_code the resolution of the output image
 * @return a pointer to the zone in RAM where the name is stored, NULL if an error occured.
 */
char* createname(const char * const pict_id, const int resolution_code);


/**
 * @brief free the memory of a buffered image.
 *
 * @param image_buffer pointer of the pointer where the data is stocked.
 */
void free_the_buffer(char ** image_buffer);


#ifdef __cplusplus
}
#endif
#endif
