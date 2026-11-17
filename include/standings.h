
#ifndef STANDINGS_H
#define STANDINGS_H

#include <stddef.h>
#include "match.h"

typedef struct
{
    char team_name[TEAM_NAME_SIZE];

    int played;
    int won;
    int drawn;
    int lost;

    int goals_for;
    int goals_against;
    int goal_difference;

    int points;

} TeamStanding;

/*
 * Calculates the league standings from completed matches.
 *
 * matches: array of Match structures.
 * match_count: number of matches.
 * standings_out: receives the allocated standings array.
 * team_count_out: receives the number of teams.
 *
 * Returns:
 *  0 = success
 * -1 = error
 *
 * The caller must free(*standings_out).
 */
int calculate_standings(
    const Match matches[],
    size_t match_count,
    TeamStanding **standings_out,
    size_t *team_count_out
);

void display_standings(
    const TeamStanding standings[],
    size_t team_count
);

#endif
