/*
 * Tests for standings.c (calculate_standings).
 *
 * Usage:   test_standings [path/to/matches.csv]
 *
 * If a CSV path is given (default: data/matches_epl_2026.csv) the last test
 * also builds the table from that real file and checks it for consistency.
 * If the file does not exist that one test is skipped.
 *
 * Exit code: 0 = all checks passed, 1 = at least one failed.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "match.h"
#include "match_storage.h"
#include "match_validation.h"
#include "standings.h"
#include "test_util.h"

/* Must match MAX_TEAMS in standings.c. */
#define TEAM_LIMIT 30

static const char *g_real_csv = "data/matches_epl_2026.csv";

/* ------------------------------------------------------------------ */
/* Helpers                                                            */
/* ------------------------------------------------------------------ */

static Match final_match(const char *id, const char *home, const char *away,
                         int home_score, int away_score)
{
    return make_match(id, 2026, "2026-08-21", "t", home, away,
                      home_score, away_score, true, "final");
}

static int position_of(const TeamStanding *s, size_t n, const char *name)
{
    for (size_t i = 0; i < n; i++)
    {
        if (strcmp(s[i].team_name, name) == 0)
            return (int)i;
    }
    return -1;
}

static void expect_row(const TeamStanding *s, size_t n, const char *name,
                       int played, int won, int drawn, int lost,
                       int gf, int ga, int points)
{
    int i = position_of(s, n, name);

    CHECK(i >= 0, "team '%s' is missing from the table", name);
    if (i < 0)
        return;

    const TeamStanding *t = &s[i];

    CHECK(t->played == played,  "%s: played %d, expected %d", name, t->played, played);
    CHECK(t->won == won,        "%s: won %d, expected %d", name, t->won, won);
    CHECK(t->drawn == drawn,    "%s: drawn %d, expected %d", name, t->drawn, drawn);
    CHECK(t->lost == lost,      "%s: lost %d, expected %d", name, t->lost, lost);
    CHECK(t->goals_for == gf,   "%s: goals for %d, expected %d", name, t->goals_for, gf);
    CHECK(t->goals_against == ga, "%s: goals against %d, expected %d", name, t->goals_against, ga);
    CHECK(t->goal_difference == gf - ga,
          "%s: goal difference %d, expected %d", name, t->goal_difference, gf - ga);
    CHECK(t->points == points,  "%s: points %d, expected %d", name, t->points, points);
}

/* True when row a must be listed before (or level with) row b. */
static bool correctly_ordered(const TeamStanding *a, const TeamStanding *b)
{
    if (a->points != b->points)
        return a->points > b->points;
    if (a->goal_difference != b->goal_difference)
        return a->goal_difference > b->goal_difference;
    if (a->goals_for != b->goals_for)
        return a->goals_for > b->goals_for;
    return a->won >= b->won;
}

static bool counts_for_table(const Match *m)
{
    return m->has_score && strcmp(m->status, "final") == 0;
}

/*
 * Checks a finished table against the matches it was built from:
 *  - every row is internally consistent,
 *  - every row equals a naive recomputation from the matches,
 *  - the totals add up (every win is somebody's loss, goals scored equal
 *    goals conceded, ...),
 *  - the rows are in the right order.
 */
static void verify_table(const Match *matches, size_t match_count,
                         const TeamStanding *s, size_t n)
{
    long played = 0, won = 0, drawn = 0, lost = 0, gf = 0, ga = 0, points = 0;
    size_t finals = 0;

    for (size_t i = 0; i < match_count; i++)
    {
        if (counts_for_table(&matches[i]))
            finals++;
    }

    for (size_t i = 0; i < n; i++)
    {
        const TeamStanding *t = &s[i];

        CHECK(t->played == t->won + t->drawn + t->lost,
              "%s: played %d != W+D+L %d", t->team_name, t->played, t->won + t->drawn + t->lost);
        CHECK(t->goal_difference == t->goals_for - t->goals_against,
              "%s: goal difference is wrong", t->team_name);
        CHECK(t->points == 3 * t->won + t->drawn,
              "%s: points %d != 3*W+D", t->team_name, t->points);

        /* Independent recomputation straight from the matches. */
        int p = 0, w = 0, d = 0, l = 0, f = 0, a = 0;
        for (size_t k = 0; k < match_count; k++)
        {
            const Match *m = &matches[k];
            int scored, conceded;

            if (!counts_for_table(m))
                continue;

            if (strcmp(m->home_team, t->team_name) == 0)
            {
                scored = m->home_score;
                conceded = m->away_score;
            }
            else if (strcmp(m->away_team, t->team_name) == 0)
            {
                scored = m->away_score;
                conceded = m->home_score;
            }
            else
            {
                continue;
            }

            p++;
            f += scored;
            a += conceded;
            if (scored > conceded) w++;
            else if (scored == conceded) d++;
            else l++;
        }

        CHECK(t->played == p && t->won == w && t->drawn == d && t->lost == l &&
              t->goals_for == f && t->goals_against == a,
              "%s: table says P%d W%d D%d L%d GF%d GA%d, recomputed P%d W%d D%d L%d GF%d GA%d",
              t->team_name, t->played, t->won, t->drawn, t->lost, t->goals_for,
              t->goals_against, p, w, d, l, f, a);

        played += t->played;
        won += t->won;
        drawn += t->drawn;
        lost += t->lost;
        gf += t->goals_for;
        ga += t->goals_against;
        points += t->points;

        if (i + 1 < n)
        {
            CHECK(correctly_ordered(&s[i], &s[i + 1]),
                  "%s (position %zu) is ranked above %s but should not be",
                  s[i].team_name, i + 1, s[i + 1].team_name);
        }
    }

    CHECK(won == lost, "total wins %ld != total losses %ld", won, lost);
    CHECK(drawn % 2 == 0, "total draws %ld is odd", drawn);
    CHECK(gf == ga, "total goals for %ld != goals against %ld", gf, ga);
    CHECK(points == 3 * won + drawn, "total points %ld != 3*W+D", points);
    CHECK(played == 2 * (long)finals,
          "total played %ld != 2 x finished matches (%zu)", played, finals);
    CHECK((size_t)(won + drawn / 2) == finals,
          "wins + draws/2 = %ld, finished matches = %zu", won + drawn / 2, finals);
}

/* ------------------------------------------------------------------ */
/* One match at a time                                                */
/* ------------------------------------------------------------------ */

static void test_home_win(void)
{
    Match m[] = { final_match("1", "A", "B", 2, 0) };
    TeamStanding *s = NULL;
    size_t n = 0;

    CHECK(calculate_standings(m, 1, &s, &n) == 0, "error");
    CHECK(n == 2, "expected 2 teams, got %zu", n);

    expect_row(s, n, "A", 1, 1, 0, 0, 2, 0, 3);
    expect_row(s, n, "B", 1, 0, 0, 1, 0, 2, 0);
    CHECK(position_of(s, n, "A") == 0, "winner must be first");

    free(s);
}

static void test_away_win(void)
{
    Match m[] = { final_match("1", "A", "B", 0, 1) };
    TeamStanding *s = NULL;
    size_t n = 0;

    CHECK(calculate_standings(m, 1, &s, &n) == 0, "error");

    expect_row(s, n, "A", 1, 0, 0, 1, 0, 1, 0);
    expect_row(s, n, "B", 1, 1, 0, 0, 1, 0, 3);
    CHECK(position_of(s, n, "B") == 0, "winner must be first");

    free(s);
}

static void test_draws(void)
{
    Match m[] = {
        final_match("1", "A", "B", 1, 1),
        final_match("2", "C", "D", 0, 0),    /* a 0-0 is a played draw */
    };
    TeamStanding *s = NULL;
    size_t n = 0;

    CHECK(calculate_standings(m, 2, &s, &n) == 0, "error");

    expect_row(s, n, "A", 1, 0, 1, 0, 1, 1, 1);
    expect_row(s, n, "B", 1, 0, 1, 0, 1, 1, 1);
    expect_row(s, n, "C", 1, 0, 1, 0, 0, 0, 1);
    expect_row(s, n, "D", 1, 0, 1, 0, 0, 0, 1);

    free(s);
}

static void test_unfinished_matches_do_not_count(void)
{
    Match m[] = {
        make_match("s1", 2026, "2026-09-01", "t", "A", "B", 0, 0, false, "scheduled"),
        make_match("s2", 2026, "2026-09-02", "t", "C", "D", 1, 0, true,  "in_progress"),
        make_match("s3", 2026, "2026-09-03", "t", "E", "F", 0, 0, false, "postponed"),
        make_match("s4", 2026, "2026-09-04", "t", "G", "H", 0, 0, false, "canceled"),
        make_match("s5", 2026, "2026-09-05", "t", "I", "J", 2, 1, false, "final"),
        make_match("s6", 2026, "2026-09-06", "t", "K", "L", 2, 1, true,  "suspended"),
    };
    TeamStanding *s = NULL;
    size_t n = 0;

    CHECK(calculate_standings(m, 6, &s, &n) == 0, "error");

    /* The teams are known from the fixtures, so they are listed... */
    CHECK(n == 12, "expected 12 teams, got %zu", n);

    /* ...but nobody has played a point-earning match yet. */
    for (size_t i = 0; i < n; i++)
    {
        CHECK(s[i].played == 0 && s[i].points == 0 &&
              s[i].goals_for == 0 && s[i].goals_against == 0,
              "%s: has statistics from an unfinished match", s[i].team_name);
    }

    free(s);
}

static void test_matches_without_team_names_are_skipped(void)
{
    Match m[] = {
        final_match("1", "", "B", 1, 0),
        final_match("2", "A", "", 1, 0),
        final_match("3", "C", "D", 1, 0),
    };
    TeamStanding *s = NULL;
    size_t n = 0;

    CHECK(calculate_standings(m, 3, &s, &n) == 0, "error");
    CHECK(n == 2, "expected 2 teams, got %zu", n);
    CHECK(position_of(s, n, "B") < 0 && position_of(s, n, "A") < 0,
          "a team from an unnamed fixture was registered");
    expect_row(s, n, "C", 1, 1, 0, 0, 1, 0, 3);

    free(s);
}

/* ------------------------------------------------------------------ */
/* A whole (small) league with hand-computed results                  */
/* ------------------------------------------------------------------ */

static void test_known_four_team_league(void)
{
    /*
     *   A 2-0 B     C 1-1 D     A 1-3 C
     *   B 2-2 D     A 0-0 D     B 1-0 C
     *
     * Hand-computed:      P  W  D  L  GF GA GD Pts
     *   C                 3  1  1  1   4  3 +1   4
     *   A                 3  1  1  1   3  3  0   4
     *   B                 3  1  1  1   3  4 -1   4
     *   D                 3  0  3  0   3  3  0   3
     *
     * A, B and C all have 4 points, so goal difference decides: C, A, B.
     */
    Match m[] = {
        final_match("1", "A", "B", 2, 0),
        final_match("2", "C", "D", 1, 1),
        final_match("3", "A", "C", 1, 3),
        final_match("4", "B", "D", 2, 2),
        final_match("5", "A", "D", 0, 0),
        final_match("6", "B", "C", 1, 0),
    };
    TeamStanding *s = NULL;
    size_t n = 0;

    CHECK(calculate_standings(m, 6, &s, &n) == 0, "error");
    CHECK(n == 4, "expected 4 teams, got %zu", n);

    expect_row(s, n, "C", 3, 1, 1, 1, 4, 3, 4);
    expect_row(s, n, "A", 3, 1, 1, 1, 3, 3, 4);
    expect_row(s, n, "B", 3, 1, 1, 1, 3, 4, 4);
    expect_row(s, n, "D", 3, 0, 3, 0, 3, 3, 3);

    CHECK(position_of(s, n, "C") == 0, "C should be 1st, is %d", position_of(s, n, "C") + 1);
    CHECK(position_of(s, n, "A") == 1, "A should be 2nd, is %d", position_of(s, n, "A") + 1);
    CHECK(position_of(s, n, "B") == 2, "B should be 3rd, is %d", position_of(s, n, "B") + 1);
    CHECK(position_of(s, n, "D") == 3, "D should be 4th, is %d", position_of(s, n, "D") + 1);

    verify_table(m, 6, s, n);
    free(s);
}

static void test_tiebreak_goals_scored(void)
{
    /* X and Y: 3 points, goal difference +1. Z and W: 0 points, GD -1.
     * Goals scored must separate them: X(3) > Y(1), Z(2) > W(0). */
    Match m[] = {
        final_match("1", "X", "Z", 3, 2),
        final_match("2", "Y", "W", 1, 0),
    };
    TeamStanding *s = NULL;
    size_t n = 0;

    CHECK(calculate_standings(m, 2, &s, &n) == 0, "error");
    CHECK(position_of(s, n, "X") == 0, "X should be 1st");
    CHECK(position_of(s, n, "Y") == 1, "Y should be 2nd");
    CHECK(position_of(s, n, "Z") == 2, "Z should be 3rd");
    CHECK(position_of(s, n, "W") == 3, "W should be 4th");

    free(s);
}

static void test_tiebreak_wins(void)
{
    /* P: won 2-1, lost 1-2  -> 3 points, GF 3, GA 3, 1 win.
     * T: three 1-1 draws    -> 3 points, GF 3, GA 3, 0 wins.
     * Everything is level except wins, so P must be above T. */
    Match m[] = {
        final_match("1", "P", "Q", 2, 1),
        final_match("2", "R", "P", 2, 1),
        final_match("3", "T", "U", 1, 1),
        final_match("4", "T", "V", 1, 1),
        final_match("5", "Q", "T", 1, 1),
    };
    TeamStanding *s = NULL;
    size_t n = 0;

    CHECK(calculate_standings(m, 5, &s, &n) == 0, "error");

    expect_row(s, n, "P", 2, 1, 0, 1, 3, 3, 3);
    expect_row(s, n, "T", 3, 0, 3, 0, 3, 3, 3);
    CHECK(position_of(s, n, "P") < position_of(s, n, "T"),
          "P (1 win) must be above T (0 wins)");

    free(s);
}

/* ------------------------------------------------------------------ */
/* A full season, checked against an independent calculation          */
/* ------------------------------------------------------------------ */

/* 20 teams, every pair twice = 380 matches; the first `finished` are final. */
static Match *build_season(size_t finished, size_t *count_out)
{
    enum { TEAMS = 20 };
    Match *m = malloc(TEAMS * (TEAMS - 1) * sizeof *m);
    if (m == NULL)
        return NULL;

    uint32_t seed = 12345u;
    size_t k = 0;

    for (int h = 0; h < TEAMS; h++)
    {
        for (int a = 0; a < TEAMS; a++)
        {
            if (h == a)
                continue;

            char id[32], home[32], away[32];
            snprintf(id, sizeof id, "m%zu", k);
            snprintf(home, sizeof home, "Team%02d", h);
            snprintf(away, sizeof away, "Team%02d", a);

            seed = seed * 1664525u + 1013904223u;
            int hs = (int)((seed >> 16) % 5);
            seed = seed * 1664525u + 1013904223u;
            int as = (int)((seed >> 16) % 4);

            if (k < finished)
                m[k] = make_match(id, 2026, "2026-08-21", "t", home, away, hs, as, true, "final");
            else
                m[k] = make_match(id, 2026, "2027-03-01", "t", home, away, 0, 0, false, "scheduled");
            k++;
        }
    }

    *count_out = k;
    return m;
}

static void test_full_season(void)
{
    static const size_t finished_options[] = { 0, 1, 100, 250, 380 };

    for (size_t i = 0; i < sizeof finished_options / sizeof finished_options[0]; i++)
    {
        size_t count = 0;
        Match *m = build_season(finished_options[i], &count);
        CHECK(m != NULL && count == 380, "could not build the test season");
        if (m == NULL)
            return;

        TeamStanding *s = NULL;
        size_t n = 0;

        CHECK(calculate_standings(m, count, &s, &n) == 0,
              "error with %zu finished matches", finished_options[i]);
        CHECK(n == 20, "expected 20 teams, got %zu", n);

        if (s != NULL)
            verify_table(m, count, s, n);

        free(s);
        free(m);
    }
}

/* ------------------------------------------------------------------ */
/* Edge cases                                                         */
/* ------------------------------------------------------------------ */

static void test_empty_input(void)
{
    TeamStanding *s = NULL;
    size_t n = 99;

    CHECK(calculate_standings(NULL, 0, &s, &n) == 0, "0 matches should not be an error");
    CHECK(n == 0, "expected 0 teams, got %zu", n);
    free(s);
}

static void test_null_arguments(void)
{
    Match m[] = { final_match("1", "A", "B", 1, 0) };
    TeamStanding *s = NULL;
    size_t n = 0;

    CHECK(calculate_standings(NULL, 1, &s, &n) != 0, "NULL matches with count 1 accepted");
    CHECK(calculate_standings(m, 1, NULL, &n) != 0, "NULL standings_out accepted");
    CHECK(calculate_standings(m, 1, &s, NULL) != 0, "NULL team_count_out accepted");
}

static void test_team_limit(void)
{
    /* The table has room for TEAM_LIMIT teams. Exactly that many is fine,
     * one more is reported as an error (not a crash, not silent loss). */
    char names[TEAM_LIMIT + 2][16];
    for (int i = 0; i < TEAM_LIMIT + 2; i++)
        snprintf(names[i], sizeof names[i], "T%02d", i);

    Match ok[TEAM_LIMIT / 2];
    for (int i = 0; i < TEAM_LIMIT / 2; i++)
        ok[i] = final_match("x", names[2 * i], names[2 * i + 1], 1, 0);

    TeamStanding *s = NULL;
    size_t n = 0;

    CHECK(calculate_standings(ok, TEAM_LIMIT / 2, &s, &n) == 0, "exactly %d teams failed", TEAM_LIMIT);
    CHECK(n == TEAM_LIMIT, "expected %d teams, got %zu", TEAM_LIMIT, n);
    free(s);

    Match too_many[TEAM_LIMIT / 2 + 1];
    memcpy(too_many, ok, sizeof ok);
    too_many[TEAM_LIMIT / 2] = final_match("y", names[TEAM_LIMIT], names[0], 1, 0);

    s = NULL;
    n = 7;
    CHECK(calculate_standings(too_many, TEAM_LIMIT / 2 + 1, &s, &n) != 0,
          "%d teams should be reported as an error", TEAM_LIMIT + 1);
    CHECK(s == NULL && n == 0, "outputs must be empty after an error");
}

/* ------------------------------------------------------------------ */
/* The real data file                                                 */
/* ------------------------------------------------------------------ */

static void test_real_file(void)
{
    FILE *probe = fopen(g_real_csv, "rb");
    if (probe == NULL)
    {
        printf("    SKIP: %s not found\n", g_real_csv);
        return;
    }
    fclose(probe);

    Match *m = NULL;
    size_t count = 0;

    CHECK(load_matches_csv(g_real_csv, &m, &count) == 0, "could not load %s", g_real_csv);
    if (m == NULL)
        return;

    ValidationReport report;
    CHECK(validate_and_deduplicate_matches(m, &count, &report) == 0, "validation error");

    TeamStanding *s = NULL;
    size_t n = 0;

    CHECK(calculate_standings(m, count, &s, &n) == 0, "could not calculate standings");

    if (s != NULL)
    {
        size_t finals = 0;
        for (size_t i = 0; i < count; i++)
            finals += counts_for_table(&m[i]) ? 1 : 0;

        printf("    %zu matches, %zu finished, %zu teams in the table\n", count, finals, n);

        verify_table(m, count, s, n);
    }

    free(s);
    free(m);
}

int main(int argc, char **argv)
{
    if (argc > 1)
        g_real_csv = argv[1];

    run_test("home win",                              test_home_win);
    run_test("away win",                              test_away_win);
    run_test("draws (including 0-0)",                 test_draws);
    run_test("unfinished matches do not count",       test_unfinished_matches_do_not_count);
    run_test("matches without team names are skipped", test_matches_without_team_names_are_skipped);
    run_test("known four-team league",                test_known_four_team_league);
    run_test("tie-break: goals scored",               test_tiebreak_goals_scored);
    run_test("tie-break: wins",                       test_tiebreak_wins);
    run_test("full season vs independent calculation", test_full_season);
    run_test("empty input",                           test_empty_input);
    run_test("NULL arguments",                        test_null_arguments);
    run_test("team limit",                            test_team_limit);
    run_test("real data file",                        test_real_file);

    return test_summary();
}