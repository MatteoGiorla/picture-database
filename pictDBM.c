/**
 * @file pictDBM.c
 * @brief pictDB Manager: command line interpretor for pictDB core commands.
 *
 * Picture Database Management Tool
 *
 * @author Mia Primorac
 *
 * @author Cédric Viaccoz
 * @author Matteo Giorla
 * @date 21 Mar 2016
 */
#include <vips/vips.h> //for VIPS_INIT

#include "pictDB.h"
#include "pictDBM_tools.h"

#include <stdlib.h>
#include <string.h>
#define MAX_COMMANDS 7 //we can alter this macro according to when new comands are added to the program.
//macros to match the optional arguments of the "create" command.
#define MF_ARGUMENT "-max_files"
#define TR_ARGUMENT "-thumb_res"
#define SR_ARGUMENT "-small_res"
#define MF_DEFAULT 10
#define TR_DEFAULT 64
#define SR_DEFAULT 256


/* déclaration du type command, qui est un pointeur sur
les différentes fonctions appelées par la ligne de commande*/
typedef int (*command)(int args, char* argvs[]);


/********************************************************************//**
 * Opens pictDB file and calls do_list command.
 ********************************************************************** */
int
do_list_cmd (int args, char *argv[])
{
    if (args < 2) {
        return ERR_NOT_ENOUGH_ARGUMENTS;
    }
    struct pictdb_file myfile;

    int openStatus = do_open(argv[1], "r+b", &myfile);
    //traitement de l'erreur renvoyée par do_open
    if (openStatus) {
        return openStatus;
    }

    do_list(&myfile, STDOUT);

    do_close(&myfile);
    return 0;
}

/********************************************************************//**
 * Prepares and calls do_create command.
********************************************************************** */
int
do_create_cmd (int args, char *argv[])
{
    if (args < 2) {
        return ERR_NOT_ENOUGH_ARGUMENTS;
    }

    // default values of the thumbnail and small image resolution, they can be changed according to the options.
    uint32_t max_files =  MF_DEFAULT;
    uint16_t thumb_resX =  TR_DEFAULT;
    uint16_t thumb_resY =  TR_DEFAULT;
    uint16_t small_resX = SR_DEFAULT;
    uint16_t small_resY = SR_DEFAULT;
    //save and test the obligatory argument here.
    const char* dbfilename = argv[1];
    if(dbfilename == NULL) {
        return ERR_INVALID_ARGUMENT;
    }
    puts("Create");

    //iteration on the optionnal arguments.
    for(int i = 2; i <args; ++i) {
        if(strncmp(argv[i], MF_ARGUMENT, strlen(MF_ARGUMENT) + 1) == 0) {
            //test if there is enough argument following
            if(i+2 <= args) {
                max_files = atouint32(argv[i+1]);
                if(max_files > MAX_MAX_FILES || max_files < 1) {
                    return ERR_MAX_FILES;
                }
                ++i; //we need to increment the i of 1 to avoid reading the argument following -max_files in our iteration.
            } else {
                return ERR_NOT_ENOUGH_ARGUMENTS;
            }
        } else if(strncmp(argv[i], TR_ARGUMENT, strlen(TR_ARGUMENT) + 1) == 0) {
            //test if there is enough argument following
            if(i+3 <= args) {
                thumb_resX = atouint16(argv[i+1]);
                thumb_resY = atouint16(argv[i+2]);
                if(thumb_resX > MAX_THUMB_RES || thumb_resX < 1 || thumb_resY > MAX_THUMB_RES || thumb_resY < 1) {
                    return ERR_RESOLUTIONS;
                }
                i += 2; //we need to increment the i of 2 to avoid reading the argument following -thumb_res in our iteration.
            } else {
                return ERR_NOT_ENOUGH_ARGUMENTS;
            }
        } else if(strncmp(argv[i], SR_ARGUMENT, strlen(SR_ARGUMENT) + 1) == 0) {
            //test if there is enough argument following
            if(i+3 <= args) {
                small_resX = atouint16(argv[i+1]);
                small_resY = atouint16(argv[i+2]);
                if(small_resX > MAX_SMALL_RES || small_resX < 1 || small_resY > MAX_SMALL_RES || small_resY < 1) {
                    return ERR_RESOLUTIONS;
                }
                i += 2; //we need to increment the i of 2 to avoid reading the argument following -thumb_res in our iteration.
            } else {
                return ERR_NOT_ENOUGH_ARGUMENTS;
            }
        } else {
            //the argument did not match any of the tree possibilities, so we return an error.
            return ERR_INVALID_ARGUMENT;
        }
    }
    //création d'un pictdb_file
    struct pictdb_file pictdb_file;

    //initialisation des champs de son header
    pictdb_file.header.max_files = max_files;
    pictdb_file.header.res_resized[0][0] = thumb_resX;
    pictdb_file.header.res_resized[0][1] = thumb_resY;
    pictdb_file.header.res_resized[1][0] = small_resX;
    pictdb_file.header.res_resized[1][1] = small_resY;

    int errorStatus = do_create(&pictdb_file, dbfilename); //pour que do_create_cmd retourne le code d'erreur retourné par do_create
    if(pictdb_file.fpdb != NULL) {
        fclose(pictdb_file.fpdb); //On doit fermer le FILE ici puisqu'on ne le fait plus dans create.
    }
    //affichage informatif du header
    print_header(&pictdb_file.header);
    //on doit ensuite libérer la mémoire occupée sur la RAM par les metadata.
    if(pictdb_file.metadata != NULL) {
        free(pictdb_file.metadata);
        pictdb_file.metadata = NULL;
    }

    return errorStatus;
}

/********************************************************************//**
 * Displays some explanations.
 ********************************************************************** */
int
help (int args, char *argv[])
{
    printf("pictDBM [COMMAND] [ARGUMENTS]\n");
    printf("  help: displays this help.\n");
    printf("  list <dbfilename>: list pictDB content.\n");
    printf("  create <dbfilename> [options]: create a new pictDB.\n");
    printf("      options are:\n");
    printf("          -max_files <MAX_FILES>: maximum number of files.\n");
    printf("                                  default value is 10\n");
    printf("                                  maximum value is 100000\n");
    printf("          -thumb_res <X_RES> <Y_RES>: resolution for thumbnail images.\n");
    printf("                                  default value is 64x64\n");
    printf("                                  maximum value is 128x128\n");
    printf("          -small_res <X_RES> <Y_RES>: resolution for small images.\n");
    printf("                                  default value is 256x256\n");
    printf("                                  maximum value is 512x512\n");
    printf("  delete <dbfilename> <pictID>: delete picture pictID from pictDB.\n");
    printf("  read <dbfilename> <pictID> [original|orig|thumbnail|thumb|small]:\n");
    printf("      read an image from the pictDB and save it to a file.\n");
    printf("  default resolution is \"original\".\n");
    printf("  insert <dbfilename> <pictID> <filename>: insert a new image in the pictDB.\n");
    printf("  delete <dbfilename> <pictID>: delete picture pictID from pictDB.\n");
    printf("  gc <dbfilename> <tmp dbfilename>: performs garbage collecting on pictDB. Requires a temporary filename for copying the pictDB.\n");
    return 0;
}

/********************************************************************//**
 * Deletes a picture from the database.
 */
int
do_delete_cmd (int args, char *argv[])
{
    if (args < 3) {
        return ERR_NOT_ENOUGH_ARGUMENTS;
    }

    //tests de validité de pictID
    if (argv[2] == NULL || strlen(argv[2]) > MAX_PIC_ID) {
        return ERR_INVALID_PICID;
    }

    //ouverture du fichier
    struct pictdb_file pictdb_file;
    int openStatus = do_open(argv[1], "r+b", &pictdb_file);
    if (openStatus != 0) {
        do_close(&pictdb_file);
        return openStatus;
    }

    //suppression de l'image
    int deleteStatus = do_delete(argv[2], &pictdb_file);

    //fermeture du fichier
    do_close(&pictdb_file);

    return deleteStatus;
}

/********************************************************************//**
 * Insert a picture into the database.
 */
int
do_insert_cmd (int args, char *argv[])
{
    if(args < 4) {
        return ERR_NOT_ENOUGH_ARGUMENTS;
    }

    //tests de validité de pictID
    if (argv[2] == NULL || strlen(argv[2]) > MAX_PIC_ID) {
        return ERR_INVALID_PICID;
    }

    struct pictdb_file pictdb_file;
    int openStatus = do_open(argv[1], "r+b", &pictdb_file);
    if (openStatus != 0) {
        do_close(&pictdb_file);
        return openStatus;
    }

    //check if the database isn't full.
    if(!(pictdb_file.header.num_files < pictdb_file.header.max_files)) {
        do_close(&pictdb_file);
        return ERR_FULL_DATABASE;
    }

    char* image_buffer = NULL;
    size_t * image_size = calloc(1, sizeof(size_t));
    if(image_size == NULL) {
        do_close(&pictdb_file);
        return ERR_OUT_OF_MEMORY;
    }
    *image_size = 0;

    //using read_from_disk we load the image into the RAM.
    int errorRead = read_disk_image(argv[3], image_size, &image_buffer);
    if(errorRead) {
        //since it was dynamically allocated in read_disk_image)
        free_the_buffer(&image_buffer);
        free(image_size);
        image_size = NULL;
        do_close(&pictdb_file);
        return errorRead;
    }

    //then we insert the image into the DB
    int errorStatus = do_insert(image_buffer, *image_size, argv[2], &pictdb_file);

    //mise à jour du header
    pictdb_file.header.num_files += 1;
    pictdb_file.header.db_version += 1;

    free_the_buffer(&image_buffer);
    if (image_size) {
        free(image_size);
        image_size = NULL;
    }
    do_close(&pictdb_file);
    return errorStatus;
}

/********************************************************************//**
 * Reads a picture from the database.
 */
int
do_read_cmd (int args, char *argv[])
{
    if(args < 4) {
        return ERR_NOT_ENOUGH_ARGUMENTS;
    }

    //tests de validité de pictID
    if (argv[2] == NULL || strlen(argv[2]) > MAX_PIC_ID) {
        return ERR_INVALID_PICID;
    }

    struct pictdb_file pictdb_file;
    int openStatus = do_open(argv[1], "r+b", &pictdb_file); //then everytime there is an error, we must not forget to do_close
    if (openStatus != 0) {
        do_close(&pictdb_file);
        return openStatus;
    }

    //we get the resolution code corresponding to the third argument given.
    int resolution_code = resolution_atoi(argv[3]);
    if(resolution_code == -1) {
        do_close(&pictdb_file);
        return ERR_INVALID_ARGUMENT;
    }

    //these two pointers are where the image and its length will be stocked in the memory
    char * image_buffer = NULL;
    uint32_t image_size = 0;
    unsigned int errorRead = do_read(argv[2], resolution_code, &image_buffer, &image_size, &pictdb_file);
    if(errorRead) {
        free_the_buffer(&image_buffer);
        do_close(&pictdb_file);
        return errorRead;
    }

    do_close(&pictdb_file);

    //now that we have read and stocked in the RAM the image we're interested in, we can write it on a .jpeg
    char* filename = createname(argv[2], resolution_code);
    if(filename == NULL) {
        free_the_buffer(&image_buffer);
        return ERR_INVALID_ARGUMENT;
    }
    int errorStatus = write_disk_image(filename, &image_buffer, image_size);

    //we need to free here the image_buffer, since it was allocated in do_read.
    free_the_buffer(&image_buffer);
    //we need to free here the filename, since it was allocated in createname.
    if (filename != NULL) {
        free(filename);
        filename = NULL;
    }
    return errorStatus;
}

/********************************************************************//**
 * Performs garbage collecting on pictDB.
 */
int
do_gc_cmd (int args, char *argv[])
{

    if(args < 3) {
        return ERR_NOT_ENOUGH_ARGUMENTS;
    }

    struct pictdb_file pictdb_file;
    int openStatus = do_open(argv[1], "r+b", &pictdb_file);
    if (openStatus != 0) {
        do_close(&pictdb_file);
        return openStatus;
    }

    int errorStatus = do_gbcollect(&pictdb_file, argv[1], argv[2]);
    do_close(&pictdb_file);
    return errorStatus;
}



/*!\struct command_mapping
   \brief Struct représentant l'association commande-nom de fonction.

structure qui associe a la chaine de caractère reçue par
la ligne de commande, la fonction do_COMMAND_cmd associée.
*/
struct command_mapping {
    const char* line_name;
    command line_cmd;
};

/*tableau contenant les relations entre les arguments de la ligne de commande
et les nom de fontions utilisées en interne.*/
const struct command_mapping commands[MAX_COMMANDS] = {
    {"help", help},
    {"list", do_list_cmd},
    {"create", do_create_cmd},
    {"delete", do_delete_cmd},
    {"insert", do_insert_cmd},
    {"read", do_read_cmd},
    {"gc", do_gc_cmd}
};

/********************************************************************//**
 * MAIN
 */
int main (int argc, char* argv[])
{

    int ret = 0;

    if (argc < 2) {
        ret = ERR_NOT_ENOUGH_ARGUMENTS;
    } else {
        if (VIPS_INIT(argv[0])) {
            return ERR_VIPS;
        }

        //to skip the argument ./pictDBM
        --argc;
        ++argv;
        command cmd = NULL;
        for(int i = 0; i < MAX_COMMANDS; ++i) {
            if(strcmp(argv[0], commands[i].line_name) == 0) {
                cmd = commands[i].line_cmd;
            }
        }
        //the strcmp didn't find any match, so we return the corresponding Error.
        if(cmd == NULL) {
            ret = ERR_INVALID_COMMAND;
        } else {
            ret = cmd(argc, argv);
        }
        vips_shutdown();
    }
    if (ret) {
        fprintf(stderr, "ERROR: %s\n", ERROR_MESSAGES[ret]);
        (void)help(argc, argv);
    }

    return ret;
}
