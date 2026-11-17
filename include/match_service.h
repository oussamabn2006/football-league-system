
#ifndef MATCH_SERVICE_H
#define MATCH_SERVICE_H

#include <stddef.h>
#include "match.h"

/*
 * Retrieves every page of matches for a league season.
 *
 * league:       API league code, for example "epl".
 * season:       Starting year of the season, for example 2026.
 * matches_out:  Receives the allocated array of matches.
 * count_out:    Receives the total number of matches.
 *
 * Returns:
 *   0  = success
 *  -1  = error
 *
 * The caller must free(*matches_out).
 */
int fetch_season_matches(
    const char *league,
    int season,
    Match **matches_out,
    size_t *count_out
);

#endif
