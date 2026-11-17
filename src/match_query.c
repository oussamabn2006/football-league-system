#include "match_query.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

bool match_is_finished(const Match *match)
{
    return match != NULL && match->has_score &&
           strcmp(match->status, "final") == 0;
}

bool match_is_upcoming(const Match *match)
{
    if (match == NULL || match_is_finished(match))
        return false;

    return strcmp(match->status, "scheduled") == 0 ||
           strcmp(match->status, "delayed") == 0 ||
           strcmp(match->status, "postponed") == 0;
}

/* ---------------- team names ---------------- */

static int lower(char c)
{
    return tolower((unsigned char)c);
}

static bool equal_ignore_case(const char *a, const char *b)
{
    while (*a != '\0' && *b != '\0')
    {
        if (lower(*a) != lower(*b))
            return false;
        a++;
        b++;
    }
    return *a == *b;
}

static bool contains_ignore_case(const char *text, const char *part)
{
    size_t length = strlen(part);

    for (; *text != '\0'; text++)
    {
        size_t i = 0;
        while (i < length && text[i] != '\0' && lower(text[i]) == lower(part[i]))
            i++;
        if (i == length)
            return true;
    }
    return false;
}

static int compare_names(const void *a, const void *b)
{
    return strcmp((const char *)a, (const char *)b);
}

static bool has_name(const TeamList *list, const char *name)
{
    for (size_t i = 0; i < list->count; i++)
        if (strcmp(list->names[i], name) == 0)
            return true;
    return false;
}

static void add_name(TeamList *list, const char *name)
{
    if (name[0] == '\0' || list->count >= MAX_LISTED_TEAMS || has_name(list, name))
        return;

    strcpy(list->names[list->count], name);
    list->count++;
}

static void sort_names(TeamList *list)
{
    if (list->count > 1)
        qsort(list->names, list->count, sizeof list->names[0], compare_names);
}

size_t list_teams(const Match matches[], size_t count, TeamList *teams)
{
    if (teams == NULL)
        return 0;

    teams->count = 0;
    if (matches == NULL)
        return 0;

    for (size_t i = 0; i < count; i++)
    {
        add_name(teams, matches[i].home_team);
        add_name(teams, matches[i].away_team);
    }
    sort_names(teams);
    return teams->count;
}

size_t find_teams(const Match matches[], size_t count, const char *query,
                  TeamList *found)
{
    if (found == NULL)
        return 0;

    found->count = 0;
    if (matches == NULL || query == NULL)
        return 0;

    /* Trim spaces around the query. */
    while (isspace((unsigned char)*query))
        query++;

    size_t length = strlen(query);
    while (length > 0 && isspace((unsigned char)query[length - 1]))
        length--;

    if (length == 0 || length >= TEAM_NAME_SIZE)
        return 0;

    char trimmed[TEAM_NAME_SIZE];
    memcpy(trimmed, query, length);
    trimmed[length] = '\0';

    TeamList all;
    list_teams(matches, count, &all);

    for (size_t i = 0; i < all.count; i++)
    {
        if (equal_ignore_case(all.names[i], trimmed))
        {
            found->count = 0;
            add_name(found, all.names[i]);
            return 1;
        }
    }

    for (size_t i = 0; i < all.count; i++)
        if (contains_ignore_case(all.names[i], trimmed))
            add_name(found, all.names[i]);

    sort_names(found);
    return found->count;
}

/* ---------------- selecting and sorting ---------------- */

typedef bool (*Filter)(const Match *match, const void *context);

static int select_where(const Match matches[], size_t count, Filter filter,
                        const void *context, Match **out, size_t *out_count)
{
    if (out == NULL || out_count == NULL)
        return -1;

    *out = NULL;
    *out_count = 0;

    if (matches == NULL && count > 0)
        return -1;

    size_t found = 0;
    for (size_t i = 0; i < count; i++)
        if (filter(&matches[i], context))
            found++;

    if (found == 0)
        return 0;

    Match *copy = malloc(found * sizeof *copy);
    if (copy == NULL)
        return -1;

    size_t next = 0;
    for (size_t i = 0; i < count; i++)
        if (filter(&matches[i], context))
            copy[next++] = matches[i];

    *out = copy;
    *out_count = found;
    return 0;
}

static bool filter_team(const Match *m, const void *context)
{
    const char *team = context;
    return strcmp(m->home_team, team) == 0 || strcmp(m->away_team, team) == 0;
}

static bool filter_finished(const Match *m, const void *context)
{
    (void)context;
    return match_is_finished(m);
}

static bool filter_upcoming(const Match *m, const void *context)
{
    (void)context;
    return match_is_upcoming(m);
}

int select_team_matches(const Match matches[], size_t count, const char *team,
                        Match **out, size_t *out_count)
{
    if (team == NULL || out == NULL || out_count == NULL)
    {
        if (out != NULL) *out = NULL;
        if (out_count != NULL) *out_count = 0;
        return -1;
    }
    return select_where(matches, count, filter_team, team, out, out_count);
}

int select_finished_matches(const Match matches[], size_t count,
                            Match **out, size_t *out_count)
{
    return select_where(matches, count, filter_finished, NULL, out, out_count);
}

int select_upcoming_matches(const Match matches[], size_t count,
                            Match **out, size_t *out_count)
{
    return select_where(matches, count, filter_upcoming, NULL, out, out_count);
}

static int compare_oldest_first(const void *a, const void *b)
{
    const Match *x = a;
    const Match *y = b;
    int c = strcmp(x->date, y->date);
    if (c == 0) c = strcmp(x->start_time, y->start_time);
    if (c == 0) c = strcmp(x->game_id, y->game_id);
    return c;
}

static int compare_newest_first(const void *a, const void *b)
{
    return compare_oldest_first(b, a);
}

void sort_matches_by_date(Match matches[], size_t count, bool newest_first)
{
    if (matches == NULL || count < 2)
        return;

    qsort(matches, count, sizeof *matches,
          newest_first ? compare_newest_first : compare_oldest_first);
}

/* ---------------- team summary ---------------- */

static void add_result(Record *record, int goals_for, int goals_against)
{
    record->played++;
    record->goals_for += goals_for;
    record->goals_against += goals_against;

    if (goals_for > goals_against)
    {
        record->won++;
        record->points += 3;
    }
    else if (goals_for == goals_against)
    {
        record->drawn++;
        record->points += 1;
    }
    else
    {
        record->lost++;
    }
}

int summarize_team(const Match matches[], size_t count, const char *team,
                   TeamSummary *summary)
{
    if (summary == NULL)
        return -1;

    memset(summary, 0, sizeof *summary);

    if (team == NULL || (matches == NULL && count > 0))
        return -1;

    size_t length = strlen(team);
    if (length == 0 || length >= TEAM_NAME_SIZE)
        return -1;

    memcpy(summary->team, team, length + 1);

    Match *played = NULL;
    size_t played_count = 0;

    if (select_team_matches(matches, count, team, &played, &played_count) != 0)
        return -1;

    /* Keep only finished matches, in the order they were played. */
    size_t kept = 0;
    for (size_t i = 0; i < played_count; i++)
        if (match_is_finished(&played[i]))
            played[kept++] = played[i];

    sort_matches_by_date(played, kept, false);

    char results[1024];
    size_t result_count = 0;

    for (size_t i = 0; i < kept; i++)
    {
        const Match *m = &played[i];
        bool is_home = strcmp(m->home_team, team) == 0;
        bool is_away = strcmp(m->away_team, team) == 0;

        int gf = is_home ? m->home_score : m->away_score;
        int ga = is_home ? m->away_score : m->home_score;

        add_result(&summary->overall, gf, ga);
        if (is_home) add_result(&summary->home, gf, ga);
        if (is_away && !is_home) add_result(&summary->away, gf, ga);

        char letter = gf > ga ? 'W' : (gf == ga ? 'D' : 'L');
        if (result_count < sizeof results)
            results[result_count++] = letter;
    }

    free(played);

    /* Form: the last FORM_LENGTH results. */
    size_t start = result_count > FORM_LENGTH ? result_count - FORM_LENGTH : 0;
    size_t form_length = result_count - start;
    memcpy(summary->form, results + start, form_length);
    summary->form[form_length] = '\0';

    /* Current run. */
    if (result_count > 0)
    {
        char last = results[result_count - 1];
        int run = 0;
        for (size_t i = result_count; i > 0 && results[i - 1] == last; i--)
            run++;
        summary->streak_result = last;
        summary->streak_length = run;
    }

    return 0;
}