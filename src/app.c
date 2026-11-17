#include "app.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#include <sys/types.h>
#endif

#include "cli.h"
#include "match_display.h"
#include "match_query.h"
#include "match_service.h"
#include "match_storage.h"
#include "match_validation.h"
#include "standings.h"

#define LINE_SIZE 512

/* ------------------------------------------------------------------ */
/* State                                                              */
/* ------------------------------------------------------------------ */

void app_init(AppState *state)
{
    if (state == NULL)
    {
        return;
    }

    state->matches = NULL;
    state->count = 0;
    state->source[0] = '\0';
}

void app_free(AppState *state)
{
    if (state == NULL)
    {
        return;
    }

    free(state->matches);
    app_init(state);
}

bool app_file_exists(const char *path)
{
    if (path == NULL)
    {
        return false;
    }

    FILE *file = fopen(path, "rb");

    if (file == NULL)
    {
        return false;
    }

    fclose(file);
    return true;
}

/*
 * Creates the folder that will contain `path` (one level only), so that
 * saving to "data/matches.csv" works on a fresh copy of the project.
 *
 * This is best effort: if the folder cannot be created, opening the file
 * fails afterwards and the caller reports that.
 */
static void create_parent_folder(const char *path)
{
    const char *separator = strrchr(path, '/');

#ifdef _WIN32
    const char *backslash = strrchr(path, '\\');

    if (backslash != NULL && (separator == NULL || backslash > separator))
    {
        separator = backslash;
    }
#endif

    /* No folder in the name, or the file is directly in the root. */
    if (separator == NULL || separator == path)
    {
        return;
    }

    char folder[LINE_SIZE];
    size_t length = (size_t)(separator - path);

    if (length >= sizeof(folder))
    {
        return;
    }

    memcpy(folder, path, length);
    folder[length] = '\0';

#ifdef _WIN32
    _mkdir(folder);
#else
    mkdir(folder, 0777);
#endif
}

/* Saves the matches, creating the folder first if needed. */
static int save_with_folder(const char *path, const Match matches[], size_t count)
{
    create_parent_folder(path);
    return save_matches_csv(path, matches, count);
}

/* Takes ownership of `matches` and releases the previous ones. */
static void set_matches(
    AppState *state,
    Match *matches,
    size_t count,
    const char *source
)
{
    free(state->matches);

    state->matches = matches;
    state->count = count;
    snprintf(state->source, sizeof(state->source), "%s", source);
}

/* Validation needs at least one match; an empty list needs no cleaning. */
static int clean_matches(
    Match *matches,
    size_t *count,
    ValidationReport *report
)
{
    memset(report, 0, sizeof(*report));

    if (*count == 0)
    {
        return 0;
    }

    return validate_and_deduplicate_matches(matches, count, report);
}

/* ------------------------------------------------------------------ */
/* Actions                                                            */
/* ------------------------------------------------------------------ */

int app_load_csv(AppState *state, const char *path)
{
    if (state == NULL || path == NULL)
    {
        return -1;
    }

    Match *loaded = NULL;
    size_t count = 0;

    if (load_matches_csv(path, &loaded, &count) != 0)
    {
        fprintf(stderr, "Could not load %s.\n", path);
        return -1;
    }

    ValidationReport report;

    if (clean_matches(loaded, &count, &report) != 0)
    {
        fprintf(stderr, "Match validation failed.\n");
        free(loaded);
        return -1;
    }

    /* A file without a single usable match must not wipe the current data. */
    if (count == 0)
    {
        fprintf(stderr,
            "%s contains no valid matches. Current data was not changed.\n",
            path);
        free(loaded);
        return -1;
    }

    set_matches(state, loaded, count, path);

    printf("Loaded %zu matches from %s (%zu invalid, %zu duplicates removed).\n",
           count, path, report.invalid_count, report.duplicate_count);

    return 0;
}

int app_fetch(AppState *state, const AppConfig *config)
{
    if (state == NULL || config == NULL)
    {
        return -1;
    }

    printf("Fetching %s %d matches...\n", config->league, config->season);

    Match *fetched = NULL;
    size_t count = 0;

    if (fetch_season_matches(
            config->league,
            config->season,
            &fetched,
            &count) != 0)
    {
        fprintf(stderr, "Failed to retrieve season matches.\n");
        return -1;
    }

    ValidationReport report;

    if (clean_matches(fetched, &count, &report) != 0)
    {
        fprintf(stderr, "Match validation failed.\n");
        free(fetched);
        return -1;
    }

    printf("\n--- DATA VALIDATION ---\n");
    printf("Valid matches:     %zu\n", report.valid_count);
    printf("Invalid matches:   %zu\n", report.invalid_count);
    printf("Duplicate matches: %zu\n", report.duplicate_count);

    /* Never replace good data (or overwrite the CSV) with nothing. */
    if (count == 0)
    {
        fprintf(stderr,
            "The API returned no valid matches. Existing data was not changed.\n");
        free(fetched);
        return -1;
    }

    char source[APP_SOURCE_SIZE];
    snprintf(source, sizeof(source), "API: %s %d", config->league, config->season);

    set_matches(state, fetched, count, source);

    if (save_with_folder(config->csv_path, state->matches, state->count) != 0)
    {
        fprintf(stderr,
            "Could not save the matches to %s.\n"
            "The matches are still loaded in memory.\n",
            config->csv_path);
        return -1;
    }

    printf("Matches saved to %s.\n", config->csv_path);
    return 0;
}

int app_save_csv(const AppState *state, const char *path)
{
    if (state == NULL || path == NULL)
    {
        return -1;
    }

    if (state->count == 0)
    {
        fprintf(stderr, "There are no matches to save.\n");
        return -1;
    }

    if (save_with_folder(path, state->matches, state->count) != 0)
    {
        fprintf(stderr, "Could not save the matches to %s.\n", path);
        return -1;
    }

    printf("Saved %zu matches to %s.\n", state->count, path);
    return 0;
}

void app_show_matches(const AppState *state)
{
    if (state == NULL || state->count == 0)
    {
        printf("No matches loaded. Fetch them from the API or load a CSV file first.\n");
        return;
    }

    display_matches(state->matches, state->count);
}

void app_show_standings(const AppState *state)
{
    if (state == NULL || state->count == 0)
    {
        printf("No matches loaded. Fetch them from the API or load a CSV file first.\n");
        return;
    }

    TeamStanding *standings = NULL;
    size_t team_count = 0;

    if (calculate_standings(
            state->matches,
            state->count,
            &standings,
            &team_count) != 0)
    {
        fprintf(stderr, "Failed to calculate standings.\n");
        return;
    }

    display_standings(standings, team_count);
    free(standings);
}

/* ------------------------------------------------------------------ */
/* Browsing: team search, upcoming matches, latest results            */
/* ------------------------------------------------------------------ */

static bool has_data(const AppState *state)
{
    if (state == NULL || state->count == 0)
    {
        printf("No matches loaded. Fetch them from the API or load a CSV file first.\n");
        return false;
    }

    return true;
}

static void print_team_names(const TeamList *teams)
{
    for (size_t i = 0; i < teams->count; i++)
    {
        printf("  - %s\n", teams->names[i]);
    }
}

/*
 * Turns the user's text into exactly one team name.
 * Returns true and fills `team` on success; otherwise prints why not.
 */
static bool resolve_team(const AppState *state, const char *query,
                         char *team, size_t size)
{
    TeamList found;

    size_t n = find_teams(state->matches, state->count, query, &found);

    if (n == 0 && (query == NULL || query[strspn(query, " \t")] == '\0'))
    {
        printf("Please type a team name (or part of one).\n");
        return false;
    }

    if (n == 1)
    {
        snprintf(team, size, "%s", found.names[0]);
        return true;
    }

    if (n > 1)
    {
        printf("'%s' matches several teams. Please be more specific:\n",
               query != NULL ? query : "");
        print_team_names(&found);
        return false;
    }

    printf("No team matches '%s'. The teams are:\n", query != NULL ? query : "");
    list_teams(state->matches, state->count, &found);
    print_team_names(&found);
    return false;
}

int app_show_team_matches(const AppState *state, const char *query)
{
    char team[TEAM_NAME_SIZE];

    if (!has_data(state) || !resolve_team(state, query, team, sizeof(team)))
    {
        return -1;
    }

    Match *selected = NULL;
    size_t selected_count = 0;

    if (select_team_matches(state->matches, state->count, team,
                            &selected, &selected_count) != 0)
    {
        fprintf(stderr, "Not enough memory.\n");
        return -1;
    }

    sort_matches_by_date(selected, selected_count, false);

    char title[TEAM_NAME_SIZE + 32];
    snprintf(title, sizeof(title), "MATCHES OF %s", team);

    display_match_list(title, selected, selected_count);
    free(selected);
    return 0;
}

int app_show_team_summary(const AppState *state, const char *query)
{
    char team[TEAM_NAME_SIZE];
    TeamSummary summary;

    if (!has_data(state) || !resolve_team(state, query, team, sizeof(team)))
    {
        return -1;
    }

    if (summarize_team(state->matches, state->count, team, &summary) != 0)
    {
        fprintf(stderr, "Could not summarise the team.\n");
        return -1;
    }

    display_team_summary(&summary);
    return 0;
}

int app_show_upcoming(const AppState *state, size_t limit)
{
    if (!has_data(state))
    {
        return -1;
    }

    Match *selected = NULL;
    size_t selected_count = 0;

    if (select_upcoming_matches(state->matches, state->count,
                                &selected, &selected_count) != 0)
    {
        fprintf(stderr, "Not enough memory.\n");
        return -1;
    }

    sort_matches_by_date(selected, selected_count, false);

    char title[96];
    size_t shown = selected_count < limit ? selected_count : limit;

    if (selected_count == 0)
        snprintf(title, sizeof(title), "UPCOMING MATCHES");
    else if (shown == selected_count)
        snprintf(title, sizeof(title), "UPCOMING MATCHES");
    else
        snprintf(title, sizeof(title), "NEXT %zu OF %zu UPCOMING MATCHES",
                 shown, selected_count);

    display_match_list(title, selected, shown);
    free(selected);
    return 0;
}

int app_show_latest_results(const AppState *state, size_t limit)
{
    if (!has_data(state))
    {
        return -1;
    }

    Match *selected = NULL;
    size_t selected_count = 0;

    if (select_finished_matches(state->matches, state->count,
                                &selected, &selected_count) != 0)
    {
        fprintf(stderr, "Not enough memory.\n");
        return -1;
    }

    sort_matches_by_date(selected, selected_count, true);

    char title[96];
    size_t shown = selected_count < limit ? selected_count : limit;

    if (selected_count == 0)
        snprintf(title, sizeof(title), "LATEST RESULTS");
    else if (shown == selected_count)
        snprintf(title, sizeof(title), "RESULTS (NEWEST FIRST)");
    else
        snprintf(title, sizeof(title), "LATEST %zu OF %zu RESULTS",
                 shown, selected_count);

    display_match_list(title, selected, shown);
    free(selected);
    return 0;
}

/* ------------------------------------------------------------------ */
/* Menu                                                               */
/* ------------------------------------------------------------------ */

/*
 * Reads one line without its line break. Returns false at the end of the
 * input. A line longer than the buffer is cut and the rest is discarded,
 * so it cannot be mistaken for the next answer.
 */
static bool read_line(FILE *in, char *buffer, size_t size)
{
    if (fgets(buffer, (int)size, in) == NULL)
    {
        return false;
    }

    size_t length = strlen(buffer);

    if (length > 0 && buffer[length - 1] == '\n')
    {
        length--;
    }
    else if (!feof(in))
    {
        int c;

        while ((c = fgetc(in)) != EOF && c != '\n')
        {
            /* discard */
        }
    }

    while (length > 0 && buffer[length - 1] == '\r')
    {
        length--;
    }

    buffer[length] = '\0';
    return true;
}

/* Removes spaces around a file name and one pair of surrounding quotes. */
static void clean_path(char *path)
{
    size_t length = strlen(path);
    size_t start = 0;

    while (start < length && isspace((unsigned char)path[start]))
    {
        start++;
    }

    while (length > start && isspace((unsigned char)path[length - 1]))
    {
        length--;
    }

    if (length - start >= 2 &&
        path[start] == '"' &&
        path[length - 1] == '"')
    {
        start++;
        length--;
    }

    memmove(path, path + start, length - start);
    path[length - start] = '\0';
}

/* Asks for a file name; an empty answer means the default. */
static bool ask_path(
    FILE *in,
    const char *label,
    const char *default_path,
    char *path,
    size_t size
)
{
    printf("%s [%s]: ", label, default_path);
    fflush(stdout);

    if (!read_line(in, path, size))
    {
        return false;
    }

    clean_path(path);

    if (path[0] == '\0')
    {
        snprintf(path, size, "%s", default_path);
    }

    return true;
}

/* Asks a yes/no question. Anything except y / yes (or end of input) is no. */
static bool confirm(FILE *in, const char *question)
{
    char answer[LINE_SIZE];

    printf("%s [y/N]: ", question);
    fflush(stdout);

    if (!read_line(in, answer, sizeof(answer)))
    {
        return false;
    }

    clean_path(answer);
    return answer[0] == 'y' || answer[0] == 'Y';
}

static bool has_matches(const AppState *state)
{
    if (state->count == 0)
    {
        printf("No matches loaded. Fetch them from the API or load a CSV file first.\n");
        return false;
    }

    return true;
}

static void print_menu(const AppState *state)
{
    printf("\n=== FOOTBALL LEAGUE SYSTEM ===\n");

    if (state->count > 0)
    {
        printf("Data: %zu matches (%s)\n", state->count, state->source);
    }
    else
    {
        printf("Data: none loaded\n");
    }

    printf("1. Fetch matches from the API\n");
    printf("2. Load matches from a CSV file\n");
    printf("3. Browse matches and teams\n");
    printf("4. Display standings\n");
    printf("5. Save matches to a CSV file\n");
    printf("6. Exit\n");
    printf("Choice: ");
    fflush(stdout);
}

static void menu_fetch(AppState *state, const AppConfig *config, FILE *in)
{
    printf("This downloads the whole %s %d season from the API "
           "(several requests, which count against your quota).\n",
           config->league, config->season);

    if (confirm(in, "Continue?"))
    {
        app_fetch(state, config);
    }
    else
    {
        printf("Cancelled.\n");
    }
}

static void menu_load(AppState *state, const AppConfig *config, FILE *in)
{
    char path[LINE_SIZE];

    if (ask_path(in, "CSV file to load", config->csv_path, path, sizeof(path)))
    {
        app_load_csv(state, path);
    }
}

static void menu_save(AppState *state, const AppConfig *config, FILE *in)
{
    if (!has_matches(state))
    {
        return;
    }

    char path[LINE_SIZE];

    if (!ask_path(in, "Save to", config->csv_path, path, sizeof(path)))
    {
        return;
    }

    if (app_file_exists(path) && !confirm(in, "That file exists. Overwrite it?"))
    {
        printf("Cancelled.\n");
        return;
    }

    app_save_csv(state, path);
}

/* Asks for a line of text (a team name). Returns false at the end of input. */
static bool ask_text(FILE *in, const char *label, char *text, size_t size)
{
    printf("%s: ", label);
    fflush(stdout);

    if (!read_line(in, text, size))
    {
        return false;
    }

    clean_path(text);
    return true;
}

static void print_browse_menu(void)
{
    printf("\n--- Browse matches and teams ---\n");
    printf("1. All matches\n");
    printf("2. Matches of one team\n");
    printf("3. Next %d upcoming matches\n", APP_LIST_LIMIT);
    printf("4. Latest %d results\n", APP_LIST_LIMIT);
    printf("5. Team summary (record, home/away, form)\n");
    printf("6. Back\n");
    printf("Choice: ");
    fflush(stdout);
}

static void menu_browse(AppState *state, FILE *in)
{
    if (!has_matches(state))
    {
        return;
    }

    char line[LINE_SIZE];

    for (;;)
    {
        print_browse_menu();

        if (!read_line(in, line, sizeof(line)))
        {
            printf("\n");
            return;
        }

        int choice = parse_menu_choice(line, 1, 6);

        if (choice == 2 || choice == 5)
        {
            char query[LINE_SIZE];

            if (!ask_text(in, "Team name (or part of it)", query, sizeof(query)))
            {
                printf("\n");
                return;
            }

            if (choice == 2)
                app_show_team_matches(state, query);
            else
                app_show_team_summary(state, query);
            continue;
        }

        switch (choice)
        {
            case 1: app_show_matches(state);                       break;
            case 3: app_show_upcoming(state, APP_LIST_LIMIT);      break;
            case 4: app_show_latest_results(state, APP_LIST_LIMIT); break;
            case 6: return;
            default:
                printf("Please enter a number from 1 to 6.\n");
                break;
        }
    }
}

int app_run_menu(AppState *state, const AppConfig *config, FILE *in)
{
    if (state == NULL || config == NULL || in == NULL)
    {
        return -1;
    }

    char line[LINE_SIZE];

    for (;;)
    {
        print_menu(state);

        if (!read_line(in, line, sizeof(line)))
        {
            printf("\n");
            return 0;
        }

        switch (parse_menu_choice(line, 1, 6))
        {
            case 1: menu_fetch(state, config, in); break;
            case 2: menu_load(state, config, in);  break;
            case 3: menu_browse(state, in);       break;
            case 4: app_show_standings(state);     break;
            case 5: menu_save(state, config, in);  break;
            case 6: return 0;
            default:
                printf("Please enter a number from 1 to 6.\n");
                break;
        }
    }
}