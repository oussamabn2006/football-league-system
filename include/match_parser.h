
#ifndef MATCH_PARSER_H
#define MATCH_PARSER_H

#include <stddef.h>
#include "match.h"

/*
 * Converts a JSON array into an allocated array of Match structures.
 *
 * json: JSON response received from API Ninjas.
 * matches_out: receives the allocated Match array.
 * count_out: receives the number of matches.
 *
 * Returns:
 *  0  = success
 * -1  = parsing or memory error
 *
 * The caller must free(*matches_out).
 */
int parse_matches(
    const char *json,
    Match **matches_out,
    size_t *count_out
);

#endif
