/**
 * @file db_list.c
 * @brief pictDB: do_list implementation
 *
 * @author Mia Primorac
 * @author Cédric Viaccoz
 * @author Matteo Giorla
 * @date 14 Mar 2016
 */

#include "pictDB.h"

#include <stdio.h>
#include <json-c/json.h>

/********************************************************************//**
 * Do List.
 */
const char* do_list(struct pictdb_file const* pictdb_file, enum do_list_mode do_list_mode)
{
	if (do_list_mode == STDOUT)
	{
		print_header(&pictdb_file->header);

	    if(pictdb_file->header.num_files != 0) {
	        for (int i = 0; i < pictdb_file->header.max_files; ++i) {
	            if (pictdb_file->metadata[i].is_valid == NON_EMPTY) {
	                print_metadata(&pictdb_file->metadata[i]);
	            }
	        }
	    } else {
	        printf("<< empty database >>\n");
	    }
	    return NULL;
	}
	else if(do_list_mode == JSON){
		//Create the array to store the pict_id's
		struct json_object* json_array = json_object_new_array();

		if(pictdb_file->header.num_files != 0) {
			for (int i = 0; i < pictdb_file->header.max_files; ++i) {
				if (pictdb_file->metadata[i].is_valid == NON_EMPTY) {
					const char* picture_id = pictdb_file->metadata[i].pict_id;
					//Create the json string to add to the array
					struct json_object* value_to_add = json_object_new_string(picture_id);
					//add the pict_id to the array
					json_object_array_add(json_array, value_to_add);	
				}
			}
		}
		//Create the json object to return (in the form of a string)
		struct json_object* json_object = json_object_new_object();
		//Add the pict_id array to the json_object
		json_object_object_add(json_object, "Pictures", json_array);
		//Transform the object into a string to be able to return it
		const char* char_to_return = json_object_to_json_string(json_object);
		//??? utiliser json_object_put ?
		return char_to_return;
	} else {
		return "unimplemented do_list mode";
	}
}