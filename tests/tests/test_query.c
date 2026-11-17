/*
 * Tests for match_query.c: team search, filters, sorting, team summary.
 *
 * Usage:   test_query
 * Exit code: 0 = all checks passed, 1 = at least one failed.
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "match.h"
#include "match_query.h"
#include "standings.h"
#include "test_util.h"

#define FIN(id, date, start, home, away, hs, as) \
    make_match(id, 2026, date, start, home, away, hs, as, true, "final")
#define UPC(id, date, start, home, away) \
    make_match(id, 2026, date, start, home, away, 0, 0, false, "scheduled")

/* ------------------------------------------------------------------ */
/* Status helpers                                                     */
/* ------------------------------------------------------------------ */

static void test_status(void)
{
    Match done = FIN("a", "2026-08-15", "15:00", "X", "Y", 0, 0);
    Match sched = UPC("b", "2026-08-16", "15:00", "X", "Y");
    Match late = make_match("c", 2026, "2026-08-17", "15:00", "X", "Y", 0, 0, false, "postponed");
    Match delayed = make_match("d", 2026, "2026-08-17", "15:00", "X", "Y", 0, 0, false, "delayed");
    Match live = make_match("e", 2026, "2026-08-17", "15:00", "X", "Y", 1, 0, true, "live");
    Match odd = make_match("f", 2026, "2026-08-17", "15:00", "X", "Y", 1, 0, true, "scheduled");

    CHECK(match_is_finished(&done) && !match_is_upcoming(&done), "0-0 final");
    CHECK(!match_is_finished(&sched) && match_is_upcoming(&sched), "scheduled");
    CHECK(match_is_upcoming(&late) && match_is_upcoming(&delayed), "postponed/delayed");
    CHECK(!match_is_finished(&live) && !match_is_upcoming(&live), "live is neither");
    CHECK(!match_is_finished(&odd), "a score alone is not 'finished'");
    CHECK(!match_is_finished(NULL) && !match_is_upcoming(NULL), "NULL");
}

/* ------------------------------------------------------------------ */
/* Team names                                                         */
/* ------------------------------------------------------------------ */

static void league_for_names(Match *m, size_t *n)
{
    static const char *names[] = {
        "Leeds United", "Leeds", "Manchester City", "Manchester United",
        "Arsenal", "Aston Villa"
    };
    size_t k = 0;
    for (size_t i = 0; i + 1 < 6; i += 2)
        m[k++] = FIN("n", "2026-08-15", "15:00", names[i], names[i + 1], 1, 0);
    m[k++] = FIN("n3", "2026-08-16", "15:00", names[4], names[5], 2, 2);
    m[k++] = UPC("n4", "2026-08-17", "15:00", names[0], names[4]);
    *n = k;
}

static void test_find_teams(void)
{
    Match m[8];
    size_t n;
    TeamList t;
    league_for_names(m, &n);

    CHECK(list_teams(m, n, &t) == 6, "list_teams found %zu", t.count);
    CHECK(strcmp(t.names[0], "Arsenal") == 0 && strcmp(t.names[1], "Aston Villa") == 0 &&
          strcmp(t.names[5], "Manchester United") == 0, "not sorted: %s, %s, %s",
          t.names[0], t.names[1], t.names[5]);

    CHECK(find_teams(m, n, "manchester", &t) == 2, "'manchester' should be ambiguous");
    CHECK(strcmp(t.names[0], "Manchester City") == 0, "sorted ambiguity list");

    CHECK(find_teams(m, n, "MAN CITY", &t) == 0, "'MAN CITY' is not a substring");
    CHECK(find_teams(m, n, "city", &t) == 1 && strcmp(t.names[0], "Manchester City") == 0,
          "'city'");
    CHECK(find_teams(m, n, "  ASTON  ", &t) == 1 && strcmp(t.names[0], "Aston Villa") == 0,
          "trim and case");

    /* "Leeds" is a full name and also part of "Leeds United": exact wins. */
    CHECK(find_teams(m, n, "leeds", &t) == 1 && strcmp(t.names[0], "Leeds") == 0,
          "exact match must win");
    CHECK(find_teams(m, n, "leeds u", &t) == 1 && strcmp(t.names[0], "Leeds United") == 0,
          "'leeds u'");

    CHECK(find_teams(m, n, "", &t) == 0, "empty query");
    CHECK(find_teams(m, n, "   ", &t) == 0, "blank query");
    CHECK(find_teams(m, n, "zzz", &t) == 0, "unknown team");
    CHECK(find_teams(m, n, NULL, &t) == 0, "NULL query");

    char huge[400];
    memset(huge, 'a', sizeof huge - 1);
    huge[sizeof huge - 1] = '\0';
    CHECK(find_teams(m, n, huge, &t) == 0, "overlong query");

    CHECK(find_teams(NULL, 0, "x", &t) == 0, "no matches");
    CHECK(find_teams(m, n, "a", NULL) == 0, "NULL result");
    CHECK(list_teams(m, 0, &t) == 0, "empty list");
}

static void test_team_cap(void)
{
    enum { MANY = MAX_LISTED_TEAMS + 10 };
    Match *m = calloc(MANY, sizeof *m);
    CHECK(m != NULL, "alloc");
    if (m == NULL)
        return;

    for (size_t i = 0; i < MANY; i++)
    {
        char home[32], away[32];
        snprintf(home, sizeof home, "Home %03zu", i);
        snprintf(away, sizeof away, "Away %03zu", i);
        m[i] = FIN("x", "2026-08-15", "15:00", home, away, 1, 1);
    }

    TeamList t;
    CHECK(list_teams(m, MANY, &t) == MAX_LISTED_TEAMS, "cap not applied: %zu", t.count);
    CHECK(find_teams(m, MANY, "away", &t) <= MAX_LISTED_TEAMS, "cap on search");
    free(m);
}

/* ------------------------------------------------------------------ */
/* Selecting and sorting                                              */
/* ------------------------------------------------------------------ */

static void test_select_and_sort(void)
{
    Match m[] = {
        FIN("g3", "2026-09-05", "15:00", "A", "B", 1, 0),
        UPC("g5", "2026-10-01", "17:30", "C", "A"),
        FIN("g1", "2026-08-15", "15:00", "B", "C", 0, 0),
        UPC("g4", "2026-10-01", "15:00", "B", "A"),
        FIN("g2", "2026-08-15", "12:30", "C", "A", 2, 1),
        make_match("g6", 2026, "2026-10-02", "15:00", "B", "C", 0, 0, false, "postponed"),
        FIN("g0", "2026-08-15", "12:30", "A", "C", 0, 3),
    };
    size_t n = sizeof m / sizeof m[0];
    Match *out = NULL;
    size_t k = 99;

    CHECK(select_team_matches(m, n, "A", &out, &k) == 0 && k == 5, "A has %zu matches", k);
    free(out);
    CHECK(select_team_matches(m, n, "Nobody", &out, &k) == 0 && out == NULL && k == 0,
          "unknown team gives nothing");
    CHECK(select_team_matches(m, n, NULL, &out, &k) != 0 && out == NULL && k == 0, "NULL team");
    CHECK(select_team_matches(m, n, "A", NULL, &k) != 0, "NULL out");
    CHECK(select_team_matches(m, 0, "A", &out, &k) == 0 && out == NULL && k == 0, "empty input");

    CHECK(select_finished_matches(m, n, &out, &k) == 0 && k == 4, "finished: %zu", k);
    free(out);
    CHECK(select_upcoming_matches(m, n, &out, &k) == 0 && k == 3, "upcoming: %zu", k);

    /* The copy is independent of the original. */
    if (out != NULL)
    {
        out[0].home_score = 77;
        free(out);
    }
    CHECK(m[0].home_score == 1, "the original array was modified");

    /* Sorting: date, then time, then id. */
    sort_matches_by_date(m, n, false);
    static const char *oldest[] = { "g0", "g2", "g1", "g3", "g4", "g5", "g6" };
    for (size_t i = 0; i < n; i++)
        CHECK(strcmp(m[i].game_id, oldest[i]) == 0, "oldest first, position %zu is %s", i, m[i].game_id);

    sort_matches_by_date(m, n, true);
    for (size_t i = 0; i < n; i++)
        CHECK(strcmp(m[i].game_id, oldest[n - 1 - i]) == 0, "newest first, position %zu is %s", i, m[i].game_id);

    sort_matches_by_date(NULL, 0, true);   /* must not crash */
    sort_matches_by_date(m, 1, true);
}

/* ------------------------------------------------------------------ */
/* Team summary                                                       */
/* ------------------------------------------------------------------ */

/* Team T: W D L W W D W (see the numbers in test_summary_by_hand). */
static size_t t_scenario(Match *m)
{
    size_t n = 0;
    /* Deliberately NOT in date order. */
    m[n++] = FIN("t7", "2026-09-26", "15:00", "T", "G", 2, 0);
    m[n++] = FIN("t3", "2026-08-29", "15:00", "T", "C", 0, 2);
    m[n++] = FIN("x1", "2026-09-01", "15:00", "C", "G", 4, 4);   /* not T */
    m[n++] = FIN("t1", "2026-08-15", "15:00", "T", "A", 2, 0);
    m[n++] = FIN("t6", "2026-09-19", "15:00", "F", "T", 2, 2);
    m[n++] = UPC("u1", "2026-10-03", "15:00", "T", "H");         /* not played */
    m[n++] = FIN("t4", "2026-09-05", "15:00", "D", "T", 0, 3);
    m[n++] = make_match("p1", 2026, "2026-10-10", "15:00", "H", "T", 0, 0, false, "postponed");
    m[n++] = FIN("t2", "2026-08-22", "15:00", "B", "T", 1, 1);
    m[n++] = FIN("t5", "2026-09-12", "15:00", "T", "E", 3, 0);
    return n;
}

static void test_summary_by_hand(void)
{
    Match m[16];
    size_t n = t_scenario(m);
    TeamSummary s;

    CHECK(summarize_team(m, n, "T", &s) == 0, "summarize failed");
    CHECK(strcmp(s.team, "T") == 0, "team name");

    CHECK(s.overall.played == 7 && s.overall.won == 4 && s.overall.drawn == 2 &&
          s.overall.lost == 1, "overall P%d W%d D%d L%d",
          s.overall.played, s.overall.won, s.overall.drawn, s.overall.lost);
    CHECK(s.overall.goals_for == 13 && s.overall.goals_against == 5 && s.overall.points == 14,
          "overall GF%d GA%d PTS%d", s.overall.goals_for, s.overall.goals_against, s.overall.points);

    CHECK(s.home.played == 4 && s.home.won == 3 && s.home.drawn == 0 && s.home.lost == 1 &&
          s.home.goals_for == 7 && s.home.goals_against == 2 && s.home.points == 9,
          "home P%d W%d D%d L%d GF%d GA%d PTS%d", s.home.played, s.home.won, s.home.drawn,
          s.home.lost, s.home.goals_for, s.home.goals_against, s.home.points);

    CHECK(s.away.played == 3 && s.away.won == 1 && s.away.drawn == 2 && s.away.lost == 0 &&
          s.away.goals_for == 6 && s.away.goals_against == 3 && s.away.points == 5,
          "away P%d W%d D%d L%d GF%d GA%d PTS%d", s.away.played, s.away.won, s.away.drawn,
          s.away.lost, s.away.goals_for, s.away.goals_against, s.away.points);

    CHECK(strcmp(s.form, "LWWDW") == 0, "form is '%s'", s.form);
    CHECK(s.streak_result == 'W' && s.streak_length == 1, "run %c x%d", s.streak_result, s.streak_length);
}

/* Builds a team whose finished results (oldest first) are given as letters. */
static size_t results_scenario(Match *m, const char *letters)
{
    size_t n = 0;
    for (size_t i = 0; letters[i] != '\0'; i++)
    {
        char id[16], date[16];
        snprintf(id, sizeof id, "r%zu", i);
        snprintf(date, sizeof date, "2026-09-%02zu", i + 1);
        int gf = letters[i] == 'W' ? 2 : (letters[i] == 'D' ? 1 : 0);
        int ga = letters[i] == 'L' ? 2 : (letters[i] == 'D' ? 1 : 0);
        /* Alternate home and away. */
        if (i % 2 == 0)
            m[n++] = FIN(id, date, "15:00", "T", "O", gf, ga);
        else
            m[n++] = FIN(id, date, "15:00", "O", "T", ga, gf);
    }
    return n;
}

static void test_form_and_run(void)
{
    struct { const char *letters; const char *form; char result; int length; } cases[] = {
        { "",         "",       '\0', 0 },
        { "W",        "W",      'W',  1 },
        { "WWW",      "WWW",    'W',  3 },
        { "LLLLLL",   "LLLLL",  'L',  6 },
        { "WDDDD",    "WDDDD",  'D',  4 },
        { "WLWDLLL",  "WDLLL",  'L',  3 },
        { "DDDDDDDD", "DDDDD",  'D',  8 },
    };

    for (size_t i = 0; i < sizeof cases / sizeof cases[0]; i++)
    {
        Match m[16];
        size_t n = results_scenario(m, cases[i].letters);
        TeamSummary s;

        CHECK(summarize_team(m, n, "T", &s) == 0, "case '%s' failed", cases[i].letters);
        CHECK(strcmp(s.form, cases[i].form) == 0, "'%s': form '%s', expected '%s'",
              cases[i].letters, s.form, cases[i].form);
        CHECK(s.streak_result == cases[i].result && s.streak_length == cases[i].length,
              "'%s': run %c x%d", cases[i].letters, s.streak_result, s.streak_length);
        CHECK(s.overall.played == (int)n, "'%s': played %d", cases[i].letters, s.overall.played);
    }
}

static void test_same_day_order(void)
{
    /* Two matches on one day: the start time decides which is the latest. */
    Match m[] = {
        FIN("late", "2026-09-01", "20:00", "T", "A", 0, 1),    /* T loses  */
        FIN("early", "2026-09-01", "12:00", "B", "T", 0, 3),   /* T wins   */
    };
    TeamSummary s;
    CHECK(summarize_team(m, 2, "T", &s) == 0, "summarize failed");
    CHECK(strcmp(s.form, "WL") == 0, "form '%s', expected 'WL'", s.form);
    CHECK(s.streak_result == 'L' && s.streak_length == 1, "run %c x%d", s.streak_result, s.streak_length);
}

static void test_summary_edge_cases(void)
{
    Match m[] = { UPC("u", "2026-10-03", "15:00", "T", "H") };
    TeamSummary s;

    CHECK(summarize_team(m, 1, "T", &s) == 0, "team without finished matches");
    CHECK(s.overall.played == 0 && s.overall.points == 0 && s.form[0] == '\0' &&
          s.streak_length == 0 && s.streak_result == '\0', "expected all zeros");

    CHECK(summarize_team(m, 1, "Nobody", &s) == 0 && s.overall.played == 0, "unknown team");
    CHECK(summarize_team(m, 1, NULL, &s) != 0, "NULL team");
    CHECK(summarize_team(m, 1, "", &s) != 0, "empty team");
    CHECK(summarize_team(m, 1, "T", NULL) != 0, "NULL summary");
    CHECK(summarize_team(NULL, 0, "T", &s) == 0 && s.overall.played == 0, "no matches");
    CHECK(summarize_team(NULL, 3, "T", &s) != 0, "NULL matches with a count");
}

/* ------------------------------------------------------------------ */
/* Cross-check with standings.c on a generated season                 */
/* ------------------------------------------------------------------ */

static void test_agrees_with_standings(void)
{
    enum { TEAMS = 20 };
    Match *m = calloc(TEAMS * (TEAMS - 1), sizeof *m);
    CHECK(m != NULL, "alloc");
    if (m == NULL)
        return;

    size_t n = 0;
    unsigned seed = 12345;

    for (int h = 0; h < TEAMS; h++)
    {
        for (int a = 0; a < TEAMS; a++)
        {
            if (h == a)
                continue;

            char home[32], away[32], id[32], date[16];
            snprintf(home, sizeof home, "Team %02d", h);
            snprintf(away, sizeof away, "Team %02d", a);
            snprintf(id, sizeof id, "g%zu", n);
            snprintf(date, sizeof date, "2026-%02d-%02d", 8 + (int)(n / 28) % 9, 1 + (int)(n % 28));

            seed = seed * 1103515245u + 12345u;
            int hs = (int)((seed >> 16) % 5);
            seed = seed * 1103515245u + 12345u;
            int as = (int)((seed >> 16) % 4);

            if (n % 7 == 3)
                m[n] = UPC(id, date, "15:00", home, away);   /* a few unplayed */
            else
                m[n] = FIN(id, date, "15:00", home, away, hs, as);
            n++;
        }
    }

    TeamStanding *table = NULL;
    size_t teams = 0;
    CHECK(calculate_standings(m, n, &table, &teams) == 0 && teams == TEAMS, "standings failed");

    for (size_t i = 0; table != NULL && i < teams; i++)
    {
        TeamSummary s;
        CHECK(summarize_team(m, n, table[i].team_name, &s) == 0, "summary failed");
        CHECK(s.overall.played == table[i].played && s.overall.won == table[i].won &&
              s.overall.drawn == table[i].drawn && s.overall.lost == table[i].lost &&
              s.overall.goals_for == table[i].goals_for &&
              s.overall.goals_against == table[i].goals_against &&
              s.overall.points == table[i].points,
              "%s differs from the standings", table[i].team_name);

        CHECK(s.home.played + s.away.played == s.overall.played &&
              s.home.points + s.away.points == s.overall.points &&
              s.home.goals_for + s.away.goals_for == s.overall.goals_for &&
              s.home.goals_against + s.away.goals_against == s.overall.goals_against,
              "%s: home + away != overall", table[i].team_name);
    }

    free(table);
    free(m);
}

int main(void)
{
    run_test("status helpers",                     test_status);
    run_test("team names: search",                 test_find_teams);
    run_test("team names: list cap",               test_team_cap);
    run_test("select and sort",                    test_select_and_sort);
    run_test("summary: worked example",            test_summary_by_hand);
    run_test("summary: form and current run",      test_form_and_run);
    run_test("summary: same-day order",            test_same_day_order);
    run_test("summary: edge cases",                test_summary_edge_cases);
    run_test("summary agrees with standings.c",    test_agrees_with_standings);

    return test_summary();
}