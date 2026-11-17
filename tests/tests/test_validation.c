/*
 * Tests for match_validation.c (validate_and_deduplicate_matches).
 *
 * Usage:   test_validation
 * Exit code: 0 = all checks passed, 1 = at least one failed.
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "match.h"
#include "match_validation.h"
#include "test_util.h"

/* A match that passes every rule. Tests change one field at a time. */
static Match good_match(void)
{
    return make_match("g1", 2026, "2026-08-21", "2026-08-21T19:00:00Z",
                      "Arsenal", "Chelsea", 2, 1, true, "final");
}

/* Runs the validator on a single match and says whether it was kept. */
static bool is_accepted(const Match *m)
{
    Match copy = *m;
    size_t count = 1;
    ValidationReport report;

    CHECK(validate_and_deduplicate_matches(&copy, &count, &report) == 0,
          "validator returned an error");
    CHECK(report.valid_count == count,
          "report.valid_count (%zu) != returned count (%zu)",
          report.valid_count, count);
    CHECK(report.valid_count + report.invalid_count + report.duplicate_count == 1,
          "report does not add up to the input size");

    return count == 1;
}

static void expect_accepted(const char *what, const Match *m)
{
    CHECK(is_accepted(m), "%s: should be accepted", what);
}

static void expect_rejected(const char *what, const Match *m)
{
    CHECK(!is_accepted(m), "%s: should be rejected", what);
}

/* ------------------------------------------------------------------ */
/* Single-match rules                                                 */
/* ------------------------------------------------------------------ */

static void test_valid_matches(void)
{
    Match m = good_match();
    expect_accepted("normal final match", &m);

    m = make_match("g2", 2026, "2026-08-22", "t", "Everton", "Fulham",
                   0, 0, true, "final");
    expect_accepted("genuine 0-0 final", &m);

    m = make_match("g3", 2026, "2027-05-23", "", "Leeds", "Spurs",
                   0, 0, false, "scheduled");
    expect_accepted("scheduled match without score, empty start time", &m);

    m = make_match("g4", 2026, "2026-09-01", "t", "A", "B",
                   0, 0, false, "postponed");
    expect_accepted("postponed match without score", &m);
}

static void test_statuses(void)
{
    static const char *accepted[] = {
        "scheduled", "in_progress", "final", "postponed",
        "canceled", "suspended", "delayed"
    };
    static const char *rejected[] = {
        "", "unknown", "finished", " final", "final ", "FINAL", "Final"
    };

    for (size_t i = 0; i < sizeof accepted / sizeof accepted[0]; i++)
    {
        /* A final match needs a score; the other statuses do not. */
        Match m = good_match();
        snprintf(m.status, sizeof m.status, "%s", accepted[i]);
        expect_accepted(accepted[i], &m);
    }

    for (size_t i = 0; i < sizeof rejected / sizeof rejected[0]; i++)
    {
        Match m = good_match();
        snprintf(m.status, sizeof m.status, "%s", rejected[i]);
        expect_rejected(rejected[i][0] ? rejected[i] : "(empty status)", &m);
    }
}

static void test_ids_and_team_names(void)
{
    Match m = good_match();
    m.game_id[0] = '\0';
    expect_rejected("empty game_id", &m);

    m = good_match();
    snprintf(m.game_id, sizeof m.game_id, "   ");
    expect_rejected("game_id with only spaces", &m);

    m = good_match();
    m.home_team[0] = '\0';
    expect_rejected("empty home team", &m);

    m = good_match();
    m.away_team[0] = '\0';
    expect_rejected("empty away team", &m);

    m = good_match();
    snprintf(m.home_team, sizeof m.home_team, " \t ");
    expect_rejected("home team with only whitespace", &m);

    m = good_match();
    snprintf(m.away_team, sizeof m.away_team, "%s", m.home_team);
    expect_rejected("team playing itself", &m);
}

static void expect_date(const char *date, bool should_pass)
{
    Match m = good_match();
    snprintf(m.date, sizeof m.date, "%s", date);

    CHECK(is_accepted(&m) == should_pass,
          "date '%s' should be %s", date, should_pass ? "accepted" : "rejected");
}

static void test_dates(void)
{
    /* Valid, including leap years. */
    expect_date("2026-08-21", true);
    expect_date("2026-01-31", true);
    expect_date("2026-12-31", true);
    expect_date("2028-02-29", true);   /* divisible by 4 */
    expect_date("2000-02-29", true);   /* divisible by 400 */
    expect_date("1900-01-01", true);

    /* Days that do not exist. */
    expect_date("2026-02-29", false);  /* not a leap year */
    expect_date("2100-02-29", false);  /* divisible by 100 but not 400 */
    expect_date("2026-04-31", false);
    expect_date("2026-02-30", false);
    expect_date("2026-08-00", false);
    expect_date("2026-08-32", false);

    /* Months that do not exist. */
    expect_date("2026-00-10", false);
    expect_date("2026-13-01", false);

    /* Years outside the accepted range. */
    expect_date("1899-12-31", false);

    /* Wrong shape. */
    expect_date("", false);
    expect_date("2026-8-21", false);
    expect_date("2026-08-2", false);
    expect_date("21-08-2026", false);
    expect_date("2026/08/21", false);
    expect_date("20260821", false);
    expect_date("2026-08-21T19", false);
    expect_date("2026-08-21 ", false);
    expect_date("abcd-ef-gh", false);
    expect_date("2026-0a-21", false);
    expect_date("2026-08-2x", false);
}

static void test_dates_with_signs_and_spaces(void)
{
    /* sscanf's %d silently accepts a sign or leading spaces, so these
     * ten-character strings look like dates to a naive parser. */
    expect_date("2026-+8-21", false);
    expect_date("2026-08-+1", false);
    expect_date("2026- 8-21", false);
    expect_date("2026-08- 1", false);
    expect_date("+026-08-21", false);
    expect_date("-026-08-21", false);
    expect_date(" 026-08-21", false);
}

static void test_season(void)
{
    Match m = good_match();
    m.season = 1900;
    expect_accepted("season 1900", &m);

    m.season = 1899;
    expect_rejected("season 1899", &m);

    m.season = 0;
    expect_rejected("season 0", &m);

    m.season = -2026;
    expect_rejected("negative season", &m);
}

static void test_scores(void)
{
    Match m = good_match();
    m.has_score = false;
    expect_rejected("final match without a score", &m);

    m = good_match();
    m.home_score = -1;
    expect_rejected("negative home score", &m);

    m = good_match();
    m.away_score = -1;
    expect_rejected("negative away score", &m);

    m = good_match();
    m.home_score = 1000;
    m.away_score = 0;
    expect_accepted("very high but non-negative score", &m);
}

/* ------------------------------------------------------------------ */
/* Duplicates, counts and ordering                                    */
/* ------------------------------------------------------------------ */

static void test_duplicates(void)
{
    Match m[3];
    m[0] = make_match("same", 2026, "2026-08-21", "t", "First", "B", 1, 0, true, "final");
    m[1] = make_match("same", 2026, "2026-08-22", "t", "Second", "B", 2, 0, true, "final");
    m[2] = make_match("same", 2026, "2026-08-23", "t", "Third", "B", 3, 0, true, "final");

    size_t count = 3;
    ValidationReport r;

    CHECK(validate_and_deduplicate_matches(m, &count, &r) == 0, "error");
    CHECK(count == 1, "expected 1 match to remain, got %zu", count);
    CHECK(r.valid_count == 1 && r.invalid_count == 0 && r.duplicate_count == 2,
          "report: valid %zu, invalid %zu, duplicate %zu",
          r.valid_count, r.invalid_count, r.duplicate_count);
    CHECK(strcmp(m[0].home_team, "First") == 0,
          "the first occurrence must be kept, kept '%s'", m[0].home_team);
}

static void test_mixed_input_keeps_order(void)
{
    Match m[6];
    m[0] = make_match("a", 2026, "2026-08-21", "t", "A1", "A2", 1, 0, true, "final");
    m[1] = make_match("bad", 2026, "2026-13-40", "t", "B1", "B2", 1, 0, true, "final");
    m[2] = make_match("b", 2026, "2026-08-22", "t", "B1", "B2", 2, 2, true, "final");
    m[3] = make_match("a", 2026, "2026-08-21", "t", "A1", "A2", 1, 0, true, "final");
    m[4] = make_match("c", 2026, "2026-08-23", "t", "C1", "C2", 0, 0, false, "scheduled");
    m[5] = make_match("", 2026, "2026-08-24", "t", "D1", "D2", 0, 0, false, "scheduled");

    size_t count = 6;
    ValidationReport r;

    CHECK(validate_and_deduplicate_matches(m, &count, &r) == 0, "error");
    CHECK(count == 3, "expected 3 matches to remain, got %zu", count);
    CHECK(r.valid_count == 3 && r.invalid_count == 2 && r.duplicate_count == 1,
          "report: valid %zu, invalid %zu, duplicate %zu",
          r.valid_count, r.invalid_count, r.duplicate_count);

    if (count == 3)
    {
        CHECK(strcmp(m[0].game_id, "a") == 0, "first kept id '%s'", m[0].game_id);
        CHECK(strcmp(m[1].game_id, "b") == 0, "second kept id '%s'", m[1].game_id);
        CHECK(strcmp(m[2].game_id, "c") == 0, "third kept id '%s'", m[2].game_id);
        CHECK(m[1].home_score == 2 && m[1].away_score == 2,
              "kept match was modified while compacting");
    }
}

static void test_invalid_record_does_not_hide_valid_one(void)
{
    /* An invalid record is dropped, so it must not make a later valid
     * record with the same id look like a duplicate. */
    Match m[2];
    m[0] = make_match("x", 2026, "2026-08-21", "t", "Same", "Same", 1, 0, true, "final");
    m[1] = make_match("x", 2026, "2026-08-21", "t", "A", "B", 1, 0, true, "final");

    size_t count = 2;
    ValidationReport r;

    CHECK(validate_and_deduplicate_matches(m, &count, &r) == 0, "error");
    CHECK(count == 1 && r.invalid_count == 1 && r.duplicate_count == 0,
          "kept %zu, invalid %zu, duplicate %zu",
          count, r.invalid_count, r.duplicate_count);
    CHECK(count == 1 && strcmp(m[0].home_team, "A") == 0, "wrong record kept");
}

static void test_invalid_duplicate_counts_as_invalid(void)
{
    /* Documents the current rule: validity is checked before duplicates. */
    Match m[2];
    m[0] = make_match("y", 2026, "2026-08-21", "t", "A", "B", 1, 0, true, "final");
    m[1] = make_match("y", 2026, "2026-08-21", "t", "A", "B", -1, 0, true, "final");

    size_t count = 2;
    ValidationReport r;

    CHECK(validate_and_deduplicate_matches(m, &count, &r) == 0, "error");
    CHECK(count == 1 && r.invalid_count == 1 && r.duplicate_count == 0,
          "kept %zu, invalid %zu, duplicate %zu",
          count, r.invalid_count, r.duplicate_count);
}

static void test_report_adds_up(void)
{
    enum { N = 380 };
    Match *m = malloc(N * sizeof *m);
    if (m == NULL)
        return;

    for (int i = 0; i < N; i++)
    {
        char id[32], home[32], away[32];
        snprintf(id, sizeof id, "g%d", i);
        snprintf(home, sizeof home, "H%d", i);
        snprintf(away, sizeof away, "A%d", i);
        m[i] = make_match(id, 2026, "2026-08-21", "t", home, away, 1, 0, true, "final");
    }

    /* Spoil a few of them. */
    m[10].has_score = false;                        /* final without score */
    snprintf(m[20].date, sizeof m[20].date, "bad");
    snprintf(m[30].game_id, sizeof m[30].game_id, "g31");   /* duplicate of m[31] is below */
    m[31] = m[30];

    size_t count = N;
    ValidationReport r;

    CHECK(validate_and_deduplicate_matches(m, &count, &r) == 0, "error");
    CHECK(r.valid_count + r.invalid_count + r.duplicate_count == N,
          "report does not add up: %zu + %zu + %zu != %d",
          r.valid_count, r.invalid_count, r.duplicate_count, (int)N);
    CHECK(r.invalid_count == 2, "expected 2 invalid, got %zu", r.invalid_count);
    CHECK(r.duplicate_count == 1, "expected 1 duplicate, got %zu", r.duplicate_count);
    CHECK(count == N - 3, "expected %d kept, got %zu", (int)(N - 3), count);

    free(m);
}

static void test_empty_and_null_arguments(void)
{
    Match m = good_match();
    size_t count = 0;
    ValidationReport r;

    r.valid_count = r.invalid_count = r.duplicate_count = 99;
    CHECK(validate_and_deduplicate_matches(&m, &count, &r) == 0, "count 0 failed");
    CHECK(count == 0 && r.valid_count == 0 && r.invalid_count == 0 && r.duplicate_count == 0,
          "report not reset for an empty input");

    count = 1;
    CHECK(validate_and_deduplicate_matches(NULL, &count, &r) != 0, "NULL matches accepted");
    CHECK(validate_and_deduplicate_matches(&m, NULL, &r) != 0, "NULL count accepted");
    CHECK(validate_and_deduplicate_matches(&m, &count, NULL) != 0, "NULL report accepted");
}

int main(void)
{
    run_test("valid matches",                         test_valid_matches);
    run_test("statuses",                              test_statuses);
    run_test("game ids and team names",               test_ids_and_team_names);
    run_test("dates",                                 test_dates);
    run_test("dates with signs and spaces",           test_dates_with_signs_and_spaces);
    run_test("season",                                test_season);
    run_test("scores",                                test_scores);
    run_test("duplicates keep the first record",      test_duplicates);
    run_test("mixed input keeps order",               test_mixed_input_keeps_order);
    run_test("invalid record does not hide valid one", test_invalid_record_does_not_hide_valid_one);
    run_test("invalid duplicate counts as invalid",   test_invalid_duplicate_counts_as_invalid);
    run_test("report adds up (380 matches)",          test_report_adds_up);
    run_test("empty input and NULL arguments",        test_empty_and_null_arguments);

    return test_summary();
}