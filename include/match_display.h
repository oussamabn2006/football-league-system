#ifndef MATCH_DISPLAY_H
#define MATCH_DISPLAY_H

#include <stddef.h>
#include "match.h"
#include "match_query.h"

/* Prints the matches as a table. */
void display_matches(
    const Match matches[],
    size_t count
);

/* Prints a titled list of matches ("No matches to show." when empty). */
void display_match_list(
    const char *title,
    const Match matches[],
    size_t count
);

/* Prints the overall/home/away record, recent form and current run. */
void display_team_summary(const TeamSummary *summary);

#endif