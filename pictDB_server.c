/**
 * @file pictDB_server.c
 * @brief pictDB Server
 *
 * Picture Database Server Management Tool
 *
 * @author Cédric Viaccoz
 * @author Matteo Giorla
 * @date Mai 2016
 */
#include <stdlib.h>
#include <stdint.h> // for uint32_t
#include <string.h>
#include <vips/vips.h>
#include "libmongoose/mongoose.h"
#include "pictDB.h"

#define MAX_QUERY_PARAM 5
#define RES_ARG "res"
#define PIC_ARG "pict_id"

static const char *s_http_port = "8000";
static struct mg_serve_http_opts s_http_server_opts;

//the struct on wihch we work internally with all pictDB commands.
static struct pictdb_file webStruct;

//the title says everything
//FOR THIS ALGORITM TO WORK, file_name SHOULD OBLIGATORY END WITH A \0 !!!
//Actually doesn't remove only ".jpg", remove everything that comes after a point (".")
/********************************************************************//**
 * utilitary function to remove jpg from the file got.
 */
char* remove_jpg(char* file_name)
{
    char* original = file_name;
    while(*file_name != '\0' && *file_name != '.') {
        ++file_name;
    }
    if(*file_name == '.') {
        while(*file_name != '\0') {
            *file_name = '\0';
            ++file_name;
        }
    }
    return original;
}

/********************************************************************//**
 * Split the messsage received into a more pictDBM readable message.
 */
void split (char* result[], char* tmp, const char* src, const char* delim, size_t len)
{
    if (len != 0) {
        char* to_split = calloc(len, sizeof(char));
        char* delims = calloc(strlen(delim), sizeof(char));
        if (to_split != NULL && delims != NULL) {
            strncpy(to_split, src, len);
            strncpy(delims, delim, strlen(delim));

            char* read_word = calloc(MAX_PIC_ID+1, sizeof(char));
            int i = 0;
            read_word = strtok(to_split, delims);
            while(read_word != NULL) {
                result[i] = calloc(MAX_PIC_ID+1, sizeof(char));
                strncpy(result[i], read_word, strlen(read_word));
                strncat(tmp, read_word, (MAX_PIC_ID + 1) * MAX_QUERY_PARAM);
                strncat(tmp, "\0", 2);
                i++;
                read_word = strtok(NULL, delims);
            }
            if (read_word != NULL) {
                free(read_word);
                read_word = NULL;
            }
        }

        //free the memory here, in case one of the two pointers was allocated anyway.
        if (to_split != NULL) {
            free(to_split);
            to_split = NULL;
        }
        if (delims != NULL) {
            free(delims);
            delims = NULL;
        }
    }
}

/********************************************************************//**
 * Simply sends the webpage the error the server encountered.
 */
void mg_error(struct mg_connection* nc, int error)
{
    mg_printf(nc,   "HTTP/1.1 500 "
              "%s", ERROR_MESSAGES[error]);
}

/*utilitary function which takes care of freeing len
 * strings indicated by the pointer stocked in result
 */
static void free_result(char ** result, const size_t len)
{
    for(int i = 0; i < len; ++i) {
        if(result[i] != NULL) {
            free(result[i]);
            result[i] = NULL;
        }

    }
}

/********************************************************************//**
 * Implementation of list call from the webPage
 */
static void handle_list_call(struct mg_connection *nc)
{

    const char* json_list = do_list(&webStruct, JSON);
    if(json_list == NULL) {
        mg_printf(nc, "HTTP/1.1 500 Internal Servor Error\r\n");
    } else {
        //respond to the connection the list of pict in JSON (string) format.
        mg_printf(nc,"HTTP/1.1 200 OK\r\n"
                  "Content-Type: application/json\r\n"
                  "Content-Length: %zu\r\n\r\n"
                  "%s", strlen(json_list), json_list);
    }
    nc->flags |= MG_F_SEND_AND_CLOSE; //"refresh" la connexion à la page index
}

/********************************************************************//**
 * Implementation of read call from the webPage
 */
static void handle_read_call(struct mg_connection *nc, struct http_message * const http_m)
{
    char** result = calloc(MAX_QUERY_PARAM, sizeof(char*));
    char* tmp = calloc((MAX_PIC_ID + 1) * MAX_QUERY_PARAM, sizeof(char));
    if (result == NULL || tmp == NULL) {
        mg_error(nc, ERR_OUT_OF_MEMORY);
    }
    split(result, tmp, http_m->query_string.p, "&=", http_m->query_string.len);
    const char * pictID = NULL;
    const char * reso = NULL;
    //will get the argument of do read in the result Array
    for(int i = 0; i < MAX_QUERY_PARAM-1; ++i) {
        if (result[i] != NULL) {
            if(strncmp(result[i], RES_ARG, strlen(RES_ARG) + 1) == 0) {
                reso = result[i+1];
            } else if(strncmp(result[i], PIC_ARG, strlen(PIC_ARG) + 1) == 0) {
                pictID = result[i+1];
            }
        }
    }
    //these two pointers are where the image and its length will be stocked in the memory
    int resolution_code = resolution_atoi(reso);
    if(resolution_code == -1 || reso == NULL || pictID == NULL) {
        mg_error(nc, ERR_INVALID_ARGUMENT);
    } else {
        char * image_buffer = NULL;
        uint32_t image_size = 0;
        unsigned int read_status = do_read(pictID, resolution_code, &image_buffer, &image_size, &webStruct);//free en trop dans do read.
        if (read_status != 0) {
            mg_error(nc, read_status);
        } else {
            mg_printf(nc,"HTTP/1.1 200 OK\r\n"
                      "Content-Type: image/jpeg \r\n"
                      "Content-Length: %" PRIu32 "\r\n\r\n", image_size);

            mg_send(nc, image_buffer, (int)image_size); //envoi de l'image
        };
        nc->flags |= MG_F_SEND_AND_CLOSE; //"refresh" la connexion à la page index
        //freeing the buffer here regardless of what the output of do_read is.
        free_the_buffer(&image_buffer);
    }
    //libération de toute mémoire allouée précedemment.
    free_result(result, MAX_QUERY_PARAM);
    if (result != NULL) {
        free(result);
        result = NULL;
    }
    if (tmp != NULL) {
        free(tmp);
        tmp = NULL;
    }
}


/********************************************************************//**
 * Implementation of insert call from the webPage
 */
static void handle_insert_call(struct mg_connection *nc, struct http_message * const http_m)
{

    char var_name[100], file_name[MAX_PIC_ID];
    const char *chunk;
    size_t chunk_len, n1, n2;

    n1 = n2 = 0;
    while ((n2 = mg_parse_multipart(http_m->body.p + n1,
                                    http_m->body.len - n1,
                                    var_name, sizeof(var_name),
                                    file_name, sizeof(file_name),
                                    &chunk, &chunk_len)) > 0) {
        n1 += n2;
    }

    char* pict_id = remove_jpg(file_name);
    int insert_status = do_insert(chunk, chunk_len, pict_id, &webStruct);
    if (insert_status != 0) {
        mg_error(nc, insert_status);
    } else {
        mg_printf(nc,   "HTTP/1.1 302 Found\r\n"
                  "Location: http://localhost:%s/index.html\r\n", s_http_port);
    }
    nc->flags |= MG_F_SEND_AND_CLOSE; //"refresh" la connexion à la page index
}

/********************************************************************//**
 * Implementation of delete call from the webPage
 */
static void handle_delete_call(struct mg_connection *nc, struct http_message * const http_m)
{
    char** result = calloc(MAX_QUERY_PARAM, sizeof(char*));
    char* tmp = calloc((MAX_PIC_ID + 1) * MAX_QUERY_PARAM, sizeof(char));
    if (result == NULL || tmp == NULL) {
        mg_error(nc, ERR_OUT_OF_MEMORY);
    }

    split(result, tmp, http_m->query_string.p, "&=", http_m->query_string.len);
    const char * pict_id = NULL;
    //this will get the arguments of do read in the result Array
    for(int i = 0; i < MAX_QUERY_PARAM-1; ++i) {
        if(result[i] != NULL && strncmp(result[i], PIC_ARG, strlen(PIC_ARG) + 1) == 0) {
            pict_id = result[i+1];
        }
    }

    int delete_status = do_delete(pict_id, &webStruct);

    //free all the memory
    free_result(result, MAX_QUERY_PARAM);
    if (result != NULL) {
        free(result);
        result = NULL;
    }
    if (tmp != NULL) {
        free(tmp);
        tmp = NULL;
    }

    if (delete_status != 0) {
        mg_error(nc, delete_status);
    } else {
        mg_printf(nc,   "HTTP/1.1 302 Found\r\n"
                  "Location: http://localhost:%s/index.html\r\n", s_http_port);
    }
    nc->flags |= MG_F_SEND_AND_CLOSE; //"refresh" la connexion à la page index
}

/********************************************************************//**
 * Event_handler for everything the webPage asks the server.
 */
static void ev_handler(struct mg_connection *nc, int ev, void *ev_data)
{
    struct http_message *http_m = (struct http_message *) ev_data;

    if (ev == MG_EV_HTTP_REQUEST) {
        if(mg_vcmp(&http_m->uri, "/pictDB/list") == 0) {
            handle_list_call(nc);
        } else if(mg_vcmp(&http_m->uri, "/pictDB/read") == 0) {
            handle_read_call(nc, http_m);
        } else if(mg_vcmp(&http_m->uri, "/pictDB/insert") == 0) {
            handle_insert_call(nc, http_m);
        } else if(mg_vcmp(&http_m->uri, "/pictDB/delete") == 0) {
            handle_delete_call(nc, http_m);
        } else {
            mg_serve_http(nc, http_m, s_http_server_opts); /*Serve static content*/
        }
    }
}

/********************************************************************//**
 * MAIN for pictDB_server
 */
int main (int argc, char* argv[])
{
    //we first need to correctly open the struct
    int ret = 0;
    if (argc < 2) {
        ret = ERR_NOT_ENOUGH_ARGUMENTS;
    } else {
        ret = do_open(argv[1], "r+b", &webStruct);
        if(!ret) {
            print_header(&webStruct.header);
        }
    }
    if(ret) {
        fprintf(stderr, "ERROR: %s\n", ERROR_MESSAGES[ret]);
    } else {

        //sourcecode of simplest_web_server from mangoose API
        struct mg_mgr mgr;
        struct mg_connection *nc;

        mg_mgr_init(&mgr, NULL);
        nc = mg_bind(&mgr, s_http_port, ev_handler);
        if (nc == NULL) {
            fprintf(stderr, "Error starting server on port %s\n", s_http_port);
            do_close(&webStruct);
            exit(1);
        }

        // Set up HTTP server parameters
        mg_set_protocol_http_websocket(nc);
        s_http_server_opts.document_root = ".";  // Serve current directory
        s_http_server_opts.dav_document_root = ".";  // Allow access via WebDav
        s_http_server_opts.enable_directory_listing = "yes";

        //for VIPS code to work when the server is running indefinitely.
        if(VIPS_INIT(argv[0])) {
            do_close(&webStruct);
            return ERR_VIPS;
        }

        printf("Starting web server on port %s\n", s_http_port);
        for (;;) {
            mg_mgr_poll(&mgr, 1000);
        }
        mg_mgr_free(&mgr);
        /**TODO(when disposing time) : make it close with s_sig_received (cf mongoose/.../coap_server.c)**/

        vips_shutdown();
        //at the end of the webserver, we close the pictdb_file.
        do_close(&webStruct);
    }
    return ret;
}
