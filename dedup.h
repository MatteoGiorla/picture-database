/**
 * @file dedup.h
 * @brief Header file for dedup
 *
 * @author Cédric Viaccoz
 * @author Matteo Giorla
 * @date Apr 2016
 */

#include "error.h"
#include <stdio.h>
#include <stdint.h>
#include <openssl/sha.h>

/**
 * @brief Prototype of the function do_name_and_content_dedup.
 *
 * @param pictDB_file The file to work on, previously opened.
 * @param index The index of the image in the metadata array.
 */
int do_name_and_content_dedup(struct pictdb_file* pictdb_file, uint32_t index);
