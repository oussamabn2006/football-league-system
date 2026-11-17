#ifndef MATCH_QUERY_H
#define MATCH_QUERY_H

#include <stdbool.h>
#include <stddef.h>
#include "match.h"

#define MAX_LISTED_TEAMS 64
#define FORM_LENGTH 5

typedef struct
{
    char names[MAX_LISTED_TEAMS][TEAM_NAME_SIZE];
    size_t count;
} TeamList;

/* Results of one team in one group of matches. */
typedef struct
{
    int played;
    int won;
    int drawn;
    int lost;
    int goals_for;
    int goals_against;
    int points;
} Record;

typedef struct
{
    char team[TEAM_NAME_SIZE];
    Record overall;
    Record home;
    Record away;

    /* Last results, oldest first, e.g. "WDLWW" (shorter if fewer matches). */
    char form[FORM_LENGTH + 1];

    /* Current run of identical results: 'W', 'D' or 'L' ('\0' = none). */
    char streak_result;
    int streak_length;
} TeamSummary;

/* A match with a score and status "final". */
bool match_is_finished(const Match *match);

/* A match that is still to be played (scheduled, delayed, postponed). */
bool match_is_upcoming(const Match *match);

/*
 * Finds the teams whose name matches `query` (case-insensitive, ignores
 * spaces around the query). If one name equals the query exactly, only
 * that team is returned. An empty or overlong query finds nothing.
 * Names are sorted alphabetically. Returns the number of teams found.
 */
size_t find_teams(
    const Match matches[],
    size_t count,
    const char *query,
    TeamList *found
);

/* All distinct team names, sorted alphabetically (at most MAX_LISTED_TEAMS). */
size_t list_teams(
    const Match matches[],
    size_t count,
    TeamList *teams
);

/*
 * The select_* functions return 0 on success and -1 on error (bad argument
 * or no memory). They allocate *out (NULL when nothing matches), which the
 * caller must free().
 */
int select_team_matches(const Match matches[], size_t count, const char *team,
                        Match **out, size_t *out_count);
int select_finished_matches(const Match matches[], size_t count,
                            Match **out, size_t *out_count);
int select_upcoming_matches(const Match matches[], size_t count,
                            Match **out, size_t *out_count);

/* Sorts by date, then start time, then game id. */
void sort_matches_by_date(Match matches[], size_t count, bool newest_first);

/* Overall/home/away record, recent form and current run (finished matches). */
int summarize_team(const Match matches[], size_t count, const char *team,
                   TeamSummary *summary);

#endif