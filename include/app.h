#ifndef APP_H
#define APP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#include "match.h"

#define APP_SOURCE_SIZE 260

/* Fixed settings of the application. */
typedef struct
{
    const char *league;     /* API league code, for example "epl"          */
    int season;             /* season to fetch, for example 2026           */
    const char *csv_path;   /* default CSV file for saving and loading     */
} AppConfig;

/* The matches currently held in memory. */
typedef struct
{
    Match *matches;                 /* NULL when nothing is loaded  */
    size_t count;
    char source[APP_SOURCE_SIZE];   /* where the data came from     */
} AppState;

void app_init(AppState *state);

/* Releases the matches. The state can be reused afterwards. */
void app_free(AppState *state);

bool app_file_exists(const char *path);

/*
 * Loads matches from a CSV file, removes invalid and duplicate records
 * and replaces the matches held in the state.
 *
 * If anything goes wrong, or the file has no valid match at all, the
 * state is left unchanged.
 * Returns 0 on success, -1 on error.
 */
int app_load_csv(AppState *state, const char *path);

/*
 * Downloads the season from the API, validates it, replaces the matches
 * held in the state and saves them to config->csv_path.
 *
 * - If the download fails or contains no valid match, nothing changes
 *   (the CSV file is not touched).
 * - If only the saving fails, the matches stay in memory (and -1 is
 *   returned, so that a script notices).
 * - The folder of the CSV file is created if it is missing (one level).
 *
 * Returns 0 on success, -1 on error.
 */
int app_fetch(AppState *state, const AppConfig *config);

/* Saves the matches held in the state. Returns 0 on success, -1 on error. */
int app_save_csv(const AppState *state, const char *path);

void app_show_matches(const AppState *state);
void app_show_standings(const AppState *state);

/* How many matches the "next" and "latest" lists show. */
#define APP_LIST_LIMIT 10

/*
 * Shows all matches of the team that `query` names (part of the name is
 * enough, upper/lower case does not matter). If the query matches no team
 * or several teams, the candidates are listed instead.
 * Returns 0 when matches were shown, -1 otherwise.
 */
int app_show_team_matches(const AppState *state, const char *query);

/* Same team search; shows record, home/away split, form and current run. */
int app_show_team_summary(const AppState *state, const char *query);

/* The next `limit` matches still to be played. Returns 0 or -1 (no data). */
int app_show_upcoming(const AppState *state, size_t limit);

/* The `limit` most recent finished matches. Returns 0 or -1 (no data). */
int app_show_latest_results(const AppState *state, size_t limit);

/*
 * Runs the interactive menu, reading the user's answers from `in`.
 * Returns when the user chooses Exit or when `in` reaches its end.
 */
int app_run_menu(AppState *state, const AppConfig *config, FILE *in);

#endif