
#include "standings.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_TEAMS 30

/*
 * Find a team in the standings array.
 * If the team does not exist, create it.
 *
 * Returns the team's index or -1 on failure.
 */
static int find_or_add_team(
    TeamStanding standings[],
    size_t *team_count,
    const char *team_name
)
{
    /* Search for an existing team. */
    for (size_t i = 0; i < *team_count; i++)
    {
        if (strcmp(
            standings[i].team_name,
            team_name
        ) == 0)
        {
            return (int)i;
        }
    }

    /* Prevent exceeding the maximum number of teams. */
    if (*team_count >= MAX_TEAMS)
    {
        return -1;
    }

    /* Create a new team. */
    size_t index = *team_count;

    memset(
        &standings[index],
        0,
        sizeof(TeamStanding)
    );

    snprintf(
        standings[index].team_name,
        sizeof(standings[index].team_name),
        "%s",
        team_name
    );

    (*team_count)++;

    return (int)index;
}

/*
 * Update a team's statistics after a completed match.
 */
static void update_team_statistics(
    TeamStanding *team,
    int goals_scored,
    int goals_conceded
)
{
    team->played++;

    team->goals_for += goals_scored;
    team->goals_against += goals_conceded;

    team->goal_difference =
        team->goals_for - team->goals_against;

    if (goals_scored > goals_conceded)
    {
        team->won++;
        team->points += 3;
    }
    else if (goals_scored == goals_conceded)
    {
        team->drawn++;
        team->points += 1;
    }
    else
    {
        team->lost++;
    }
}

/*
 * Sort standings:
 * 1. Points (descending)
 * 2. Goal difference (descending)
 * 3. Goals scored (descending)
 * 4. Wins (descending)
 */
static int compare_standings(
    const void *a,
    const void *b
)
{
    const TeamStanding *team_a = a;
    const TeamStanding *team_b = b;

    if (team_a->points != team_b->points)
    {
        return team_b->points - team_a->points;
    }

    if (team_a->goal_difference != team_b->goal_difference)
    {
        return team_b->goal_difference -
               team_a->goal_difference;
    }

    if (team_a->goals_for != team_b->goals_for)
    {
        return team_b->goals_for - team_a->goals_for;
    }

    return team_b->won - team_a->won;
}

/*
 * Calculate the standings from an array of matches.
 */
int calculate_standings(
    const Match matches[],
    size_t match_count,
    TeamStanding **standings_out,
    size_t *team_count_out
)
{
    if ((matches == NULL && match_count > 0) ||
        standings_out == NULL ||
        team_count_out == NULL)
    {
        return -1;
    }

    *standings_out = NULL;
    *team_count_out = 0;

    TeamStanding *standings = calloc(
        MAX_TEAMS,
        sizeof(TeamStanding)
    );

    if (standings == NULL)
    {
        return -1;
    }

    size_t team_count = 0;

    for (size_t i = 0; i < match_count; i++)
    {
        const Match *match = &matches[i];

        /* Ignore invalid team names. */
        if (match->home_team[0] == '\0' ||
            match->away_team[0] == '\0')
        {
            continue;
        }

        /*
         * Register both teams, including teams
         * whose matches have not yet been played.
         */
        int home_index = find_or_add_team(
            standings,
            &team_count,
            match->home_team
        );

        int away_index = find_or_add_team(
            standings,
            &team_count,
            match->away_team
        );

        if (home_index < 0 || away_index < 0)
        {
            free(standings);
            return -1;
        }

        /*
         * Only completed matches with valid scores
         * contribute to league standings.
         */
        if (!match->has_score ||
            strcmp(match->status, "final") != 0)
        {
            continue;
        }

        /* Update home team. */
        update_team_statistics(
            &standings[home_index],
            match->home_score,
            match->away_score
        );

        /* Update away team. */
        update_team_statistics(
            &standings[away_index],
            match->away_score,
            match->home_score
        );

        
    }

    /* Sort the table by league ranking. */
    qsort(
        standings,
        team_count,
        sizeof(TeamStanding),
        compare_standings
    );

    *standings_out = standings;
    *team_count_out = team_count;

    return 0;
}

/*
 * Display the calculated league table.
 */
void display_standings(
    const TeamStanding standings[],
    size_t team_count
)
{
    printf("\n");
    printf("==========================================================================\n");
    printf("                       PREMIER LEAGUE TABLE\n");
    printf("==========================================================================\n");

    printf(
        "%-4s %-24s %3s %3s %3s %3s %4s %4s %4s %4s\n",
        "POS",
        "TEAM",
        "P",
        "W",
        "D",
        "L",
        "GF",
        "GA",
        "GD",
        "PTS"
    );

    printf("--------------------------------------------------------------------------\n");

    for (size_t i = 0; i < team_count; i++)
    {
        printf(
            "%-4zu %-24.24s %3d %3d %3d %3d %4d %4d %4d %4d\n",
            i + 1,
            standings[i].team_name,
            standings[i].played,
            standings[i].won,
            standings[i].drawn,
            standings[i].lost,
            standings[i].goals_for,
            standings[i].goals_against,
            standings[i].goal_difference,
            standings[i].points
        );
    }

    printf("==========================================================================\n");
}
