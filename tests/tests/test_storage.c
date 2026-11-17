/*
 * Tests for match_storage.c (CSV save / load).
 *
 * Usage:   test_storage [path/to/matches.csv]
 *
 * If a CSV path is given (default: data/matches_epl_2026.csv) the test also
 * loads that real file, saves it again, loads the copy and compares every
 * field. If the file does not exist that one test is skipped.
 *
 * The test writes temporary files into the current directory and removes
 * them at the end. Exit code: 0 = all checks passed, 1 = at least one failed.
 */

#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "match.h"
#include "match_storage.h"
#include "match_validation.h"

#define TMP_IN  "test_tmp_in.csv"
#define TMP_OUT "test_tmp_out.csv"

#define HEADER \
    "game_id,season,date,start_time,home_team,away_team," \
    "home_score,away_score,has_score,status\n"

/* One valid row, used as a base for the malformed variants below. */
#define GOOD_ROW \
    "\"id1\",2026,\"2026-08-21\",\"2026-08-21T19:00:00Z\"," \
    "\"Arsenal\",\"Coventry City\",3,0,1,\"final\"\n"

static int g_checks = 0;
static int g_failed = 0;

#define CHECK(cond, ...)                                  \
    do {                                                  \
        g_checks++;                                       \
        if (!(cond)) {                                    \
            g_failed++;                                   \
            printf("    FAIL line %d: ", __LINE__);       \
            printf(__VA_ARGS__);                          \
            printf("\n");                                 \
        }                                                 \
    } while (0)

/* ------------------------------------------------------------------ */
/* Helpers                                                            */
/* ------------------------------------------------------------------ */

static void write_text(const char *path, const char *text)
{
    /* "wb": write the exact bytes, so CRLF tests mean what they say. */
    FILE *f = fopen(path, "wb");
    if (f == NULL)
    {
        printf("    cannot create %s\n", path);
        exit(2);
    }
    fputs(text, f);
    fclose(f);
}

static int load_text(const char *text, Match **m, size_t *n)
{
    write_text(TMP_IN, text);
    return load_matches_csv(TMP_IN, m, n);
}

static Match make_match(
    const char *id, int season, const char *date, const char *start,
    const char *home, const char *away,
    int hs, int as, bool has_score, const char *status)
{
    Match m;
    memset(&m, 0, sizeof m);
    snprintf(m.game_id, sizeof m.game_id, "%s", id);
    m.season = season;
    snprintf(m.date, sizeof m.date, "%s", date);
    snprintf(m.start_time, sizeof m.start_time, "%s", start);
    snprintf(m.home_team, sizeof m.home_team, "%s", home);
    snprintf(m.away_team, sizeof m.away_team, "%s", away);
    m.home_score = hs;
    m.away_score = as;
    m.has_score = has_score;
    snprintf(m.status, sizeof m.status, "%s", status);
    return m;
}

/* Field-by-field comparison (never memcmp: padding bytes are undefined). */
static void expect_same(const Match *a, const Match *b, size_t i)
{
    CHECK(strcmp(a->game_id, b->game_id) == 0,
          "row %zu game_id '%s' != '%s'", i, a->game_id, b->game_id);
    CHECK(a->season == b->season,
          "row %zu season %d != %d", i, a->season, b->season);
    CHECK(strcmp(a->date, b->date) == 0,
          "row %zu date '%s' != '%s'", i, a->date, b->date);
    CHECK(strcmp(a->start_time, b->start_time) == 0,
          "row %zu start_time '%s' != '%s'", i, a->start_time, b->start_time);
    CHECK(strcmp(a->home_team, b->home_team) == 0,
          "row %zu home_team '%s' != '%s'", i, a->home_team, b->home_team);
    CHECK(strcmp(a->away_team, b->away_team) == 0,
          "row %zu away_team '%s' != '%s'", i, a->away_team, b->away_team);
    CHECK(a->home_score == b->home_score,
          "row %zu home_score %d != %d", i, a->home_score, b->home_score);
    CHECK(a->away_score == b->away_score,
          "row %zu away_score %d != %d", i, a->away_score, b->away_score);
    CHECK(a->has_score == b->has_score,
          "row %zu has_score %d != %d", i, a->has_score, b->has_score);
    CHECK(strcmp(a->status, b->status) == 0,
          "row %zu status '%s' != '%s'", i, a->status, b->status);
}

static void expect_reject(const char *what, const char *text)
{
    Match *m = NULL;
    size_t n = 123;

    int r = load_text(text, &m, &n);

    CHECK(r != 0, "%s: malformed input was accepted", what);
    CHECK(r == 0 || (m == NULL && n == 0),
          "%s: outputs not reset on failure", what);

    if (r == 0)
        free(m);
}

/* Returns the loaded array (caller frees) or NULL after reporting failure. */
static Match *expect_accept(const char *what, const char *text, size_t rows)
{
    Match *m = NULL;
    size_t n = 0;

    int r = load_text(text, &m, &n);

    CHECK(r == 0, "%s: valid input was rejected", what);
    if (r != 0)
        return NULL;

    CHECK(n == rows, "%s: expected %zu rows, got %zu", what, rows, n);
    return m;
}

static char *read_whole_file(const char *path, size_t *size_out)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL)
        return NULL;

    size_t cap = 4096, len = 0;
    char *buf = malloc(cap);
    if (buf == NULL) { fclose(f); return NULL; }

    size_t got;
    while ((got = fread(buf + len, 1, cap - len, f)) > 0)
    {
        len += got;
        if (len == cap)
        {
            char *bigger = realloc(buf, cap * 2);
            if (bigger == NULL) { free(buf); fclose(f); return NULL; }
            buf = bigger;
            cap *= 2;
        }
    }

    fclose(f);
    *size_out = len;
    return buf;
}

/* ------------------------------------------------------------------ */
/* Round trip: save -> load must give back exactly what was saved     */
/* ------------------------------------------------------------------ */

static void test_roundtrip_special_values(void)
{
    char long_id[GAME_ID_SIZE];
    char long_date[DATE_SIZE];
    char long_time[START_TIME_SIZE];
    char long_team[TEAM_NAME_SIZE];
    char long_status[STATUS_SIZE];

    memset(long_id, 'i', sizeof long_id - 1);         long_id[sizeof long_id - 1] = '\0';
    memset(long_date, 'd', sizeof long_date - 1);     long_date[sizeof long_date - 1] = '\0';
    memset(long_time, 't', sizeof long_time - 1);     long_time[sizeof long_time - 1] = '\0';
    memset(long_team, 'h', sizeof long_team - 1);     long_team[sizeof long_team - 1] = '\0';
    memset(long_status, 's', sizeof long_status - 1); long_status[sizeof long_status - 1] = '\0';

    Match in[] = {
        make_match("m1", 2026, "2026-08-21", "2026-08-21T19:00:00Z",
                   "Arsenal", "Coventry City", 3, 0, true, "final"),
        /* A genuine 0-0 must stay different from "no score yet". */
        make_match("m2", 2026, "2026-08-22", "2026-08-22T15:00:00Z",
                   "Chelsea", "Everton", 0, 0, true, "final"),
        make_match("m3", 2026, "2027-05-23", "2027-05-23T15:00:00Z",
                   "Leeds United", "Fulham", 0, 0, false, "scheduled"),
        make_match("m4", 2026, "2026-09-01", "2026-09-01T15:00:00Z",
                   "Brighton, Hove & Albion", "Spurs", 1, 1, true, "final"),
        make_match("m5", 2026, "2026-09-02", "2026-09-02T15:00:00Z",
                   "Man \"United\"", "Wolves", 2, 1, true, "final"),
        make_match("m6", 2026, "2026-09-03", "2026-09-03T15:00:00Z",
                   "a,\"b\",\"\"", "\"", 0, 2, true, "final"),
        make_match("m7", 2026, "2026-09-04", "",
                   "Empty Time FC", "Other FC", 0, 0, false, "postponed"),
        make_match(long_id, 2026, long_date, long_time,
                   long_team, long_team, 5, 5, true, long_status),
        make_match("m9", INT_MAX, "2026-09-05", "x",
                   "Extremes A", "Extremes B", INT_MAX, INT_MIN, true, "final"),
    };
    /* Second team in the long-name row must differ only for readability. */
    long_team[0] = 'a';
    snprintf(in[7].away_team, sizeof in[7].away_team, "%s", long_team);

    const size_t count = sizeof in / sizeof in[0];

    CHECK(save_matches_csv(TMP_OUT, in, count) == 0, "save failed");

    Match *out = NULL;
    size_t n = 0;

    CHECK(load_matches_csv(TMP_OUT, &out, &n) == 0, "load of saved file failed");
    CHECK(n == count, "expected %zu rows, got %zu", count, n);

    if (out != NULL && n == count)
    {
        for (size_t i = 0; i < count; i++)
            expect_same(&in[i], &out[i], i);
    }

    free(out);
}

static void test_roundtrip_empty_list(void)
{
    CHECK(save_matches_csv(TMP_OUT, NULL, 0) == 0, "save of 0 matches failed");

    Match *out = NULL;
    size_t n = 99;

    CHECK(load_matches_csv(TMP_OUT, &out, &n) == 0, "load of header-only file failed");
    CHECK(n == 0, "expected 0 rows, got %zu", n);
    free(out);
}

static void test_roundtrip_real_file(const char *path)
{
    FILE *probe = fopen(path, "rb");
    if (probe == NULL)
    {
        printf("    SKIP: %s not found\n", path);
        return;
    }
    fclose(probe);

    Match *a = NULL;
    size_t na = 0;

    CHECK(load_matches_csv(path, &a, &na) == 0, "could not load %s", path);
    if (a == NULL)
        return;

    printf("    loaded %zu rows from %s\n", na, path);

    /* The data must still pass the validation rules. */
    Match *copy = malloc(na * sizeof *copy);
    if (copy != NULL)
    {
        memcpy(copy, a, na * sizeof *copy);
        size_t kept = na;
        ValidationReport rep;
        CHECK(validate_and_deduplicate_matches(copy, &kept, &rep) == 0, "validation error");
        CHECK(kept == na && rep.invalid_count == 0 && rep.duplicate_count == 0,
              "validation: kept %zu of %zu (invalid %zu, duplicate %zu)",
              kept, na, rep.invalid_count, rep.duplicate_count);
        free(copy);
    }

    /* Save the loaded data again, load the copy, compare field by field. */
    CHECK(save_matches_csv(TMP_OUT, a, na) == 0, "re-save failed");

    Match *b = NULL;
    size_t nb = 0;

    CHECK(load_matches_csv(TMP_OUT, &b, &nb) == 0, "load of re-saved file failed");
    CHECK(na == nb, "row count changed: %zu -> %zu", na, nb);

    if (b != NULL && na == nb)
    {
        for (size_t i = 0; i < na; i++)
            expect_same(&a[i], &b[i], i);
    }

    /* Informational only: byte-identical output also shows that nothing
     * is lost. It can differ legitimately (CRLF vs LF after a git checkout,
     * or a hand-edited file), so it is reported but never fails. */
    size_t s1 = 0, s2 = 0;
    char *f1 = read_whole_file(path, &s1);
    char *f2 = read_whole_file(TMP_OUT, &s2);
    if (f1 != NULL && f2 != NULL)
        printf("    re-saved file is %sbyte-identical to the original\n",
               (s1 == s2 && memcmp(f1, f2, s1) == 0) ? "" : "NOT ");
    free(f1);
    free(f2);

    free(a);
    free(b);
}

/* ------------------------------------------------------------------ */
/* Loading: valid but unusual input                                   */
/* ------------------------------------------------------------------ */

static void test_load_valid_row(void)
{
    Match *m = expect_accept("valid row", HEADER GOOD_ROW, 1);
    if (m == NULL)
        return;

    CHECK(strcmp(m[0].game_id, "id1") == 0, "game_id");
    CHECK(m[0].season == 2026, "season");
    CHECK(strcmp(m[0].date, "2026-08-21") == 0, "date");
    CHECK(strcmp(m[0].start_time, "2026-08-21T19:00:00Z") == 0, "start_time");
    CHECK(strcmp(m[0].home_team, "Arsenal") == 0, "home_team");
    CHECK(strcmp(m[0].away_team, "Coventry City") == 0, "away_team");
    CHECK(m[0].home_score == 3 && m[0].away_score == 0, "scores");
    CHECK(m[0].has_score, "has_score");
    CHECK(strcmp(m[0].status, "final") == 0, "status");
    free(m);
}

static void test_load_quotes_and_commas(void)
{
    Match *m = expect_accept("quotes and commas", HEADER
        "\"id1\",2026,\"2026-08-21\",\"2026-08-21T19:00:00Z\","
        "\"Brighton, Hove & Albion\",\"Man \"\"United\"\"\",1,2,1,\"final\"\n", 1);
    if (m == NULL)
        return;

    CHECK(strcmp(m[0].home_team, "Brighton, Hove & Albion") == 0,
          "home_team '%s'", m[0].home_team);
    CHECK(strcmp(m[0].away_team, "Man \"United\"") == 0,
          "away_team '%s'", m[0].away_team);
    free(m);
}

static void test_load_no_trailing_newline(void)
{
    /* GOOD_ROW without its final '\n'. */
    free(expect_accept("no trailing newline", HEADER
        "\"id1\",2026,\"2026-08-21\",\"2026-08-21T19:00:00Z\","
        "\"Arsenal\",\"Coventry City\",3,0,1,\"final\"", 1));
}

static void test_load_crlf(void)
{
    free(expect_accept("CRLF line endings",
        "game_id,season,date,start_time,home_team,away_team,"
        "home_score,away_score,has_score,status\r\n"
        "\"id1\",2026,\"2026-08-21\",\"2026-08-21T19:00:00Z\","
        "\"Arsenal\",\"Coventry City\",3,0,1,\"final\"\r\n"
        "\"id2\",2026,\"2026-08-22\",\"2026-08-22T19:00:00Z\","
        "\"Chelsea\",\"Everton\",0,0,1,\"final\"\r\n", 2));
}

static void test_load_blank_trailing_lines(void)
{
    free(expect_accept("blank lines at the end", HEADER GOOD_ROW "\n\n", 1));
}

static void test_load_utf8_bom(void)
{
    /* Excel adds a byte-order mark when saving "CSV UTF-8". */
    free(expect_accept("UTF-8 BOM before header",
                       "\xEF\xBB\xBF" HEADER GOOD_ROW, 1));
}

static void test_load_header_only(void)
{
    free(expect_accept("header only", HEADER, 0));
}

/* ------------------------------------------------------------------ */
/* Loading: malformed input must be rejected, not silently accepted   */
/* ------------------------------------------------------------------ */

static void test_load_missing_file(void)
{
    Match *m = NULL;
    size_t n = 5;

    CHECK(load_matches_csv("this_file_does_not_exist_123.csv", &m, &n) != 0,
          "missing file reported success");
    CHECK(m == NULL && n == 0, "outputs not reset");
}

static void test_load_null_arguments(void)
{
    Match *m = NULL;
    size_t n = 0;

    CHECK(load_matches_csv(NULL, &m, &n) != 0, "NULL filename accepted");
    CHECK(load_matches_csv(TMP_IN, NULL, &n) != 0, "NULL matches_out accepted");
    CHECK(load_matches_csv(TMP_IN, &m, NULL) != 0, "NULL count_out accepted");
}

static void test_load_rejects_bad_files(void)
{
    expect_reject("empty file", "");

    expect_reject("wrong header",
        "id,season\n" GOOD_ROW);

    expect_reject("empty game_id", HEADER
        "\"\",2026,\"2026-08-21\",\"2026-08-21T19:00:00Z\","
        "\"Arsenal\",\"Coventry City\",3,0,1,\"final\"\n");

    expect_reject("empty season (must not become 0)", HEADER
        "\"id1\",,\"2026-08-21\",\"2026-08-21T19:00:00Z\","
        "\"Arsenal\",\"Coventry City\",3,0,1,\"final\"\n");

    expect_reject("empty home_score (must not become 0)", HEADER
        "\"id1\",2026,\"2026-08-21\",\"2026-08-21T19:00:00Z\","
        "\"Arsenal\",\"Coventry City\",,0,1,\"final\"\n");

    expect_reject("non-numeric score", HEADER
        "\"id1\",2026,\"2026-08-21\",\"2026-08-21T19:00:00Z\","
        "\"Arsenal\",\"Coventry City\",abc,0,1,\"final\"\n");

    expect_reject("number followed by junk", HEADER
        "\"id1\",2026,\"2026-08-21\",\"2026-08-21T19:00:00Z\","
        "\"Arsenal\",\"Coventry City\",3x,0,1,\"final\"\n");

    /* 4294967297 = 2^32 + 1: a plain (int) cast would silently give 1. */
    expect_reject("season does not fit in an int", HEADER
        "\"id1\",4294967297,\"2026-08-21\",\"2026-08-21T19:00:00Z\","
        "\"Arsenal\",\"Coventry City\",3,0,1,\"final\"\n");

    expect_reject("score does not fit in an int", HEADER
        "\"id1\",2026,\"2026-08-21\",\"2026-08-21T19:00:00Z\","
        "\"Arsenal\",\"Coventry City\",-4294967297,0,1,\"final\"\n");

    expect_reject("has_score is 2", HEADER
        "\"id1\",2026,\"2026-08-21\",\"2026-08-21T19:00:00Z\","
        "\"Arsenal\",\"Coventry City\",3,0,2,\"final\"\n");

    expect_reject("too few fields (status missing)", HEADER
        "\"id1\",2026,\"2026-08-21\",\"2026-08-21T19:00:00Z\","
        "\"Arsenal\",\"Coventry City\",3,0,1\n");

    expect_reject("too many fields", HEADER
        "\"id1\",2026,\"2026-08-21\",\"2026-08-21T19:00:00Z\","
        "\"Arsenal\",\"Coventry City\",3,0,1,\"final\",extra\n");

    expect_reject("junk after closing quote", HEADER
        "\"id1\",2026,\"2026-08-21\",\"2026-08-21T19:00:00Z\","
        "\"Arsenal\"x,\"Coventry City\",3,0,1,\"final\"\n");

    expect_reject("unterminated quote", HEADER
        "\"id1\",2026,\"2026-08-21\",\"2026-08-21T19:00:00Z\","
        "\"Arsenal\",\"Coventry City\",3,0,1,\"final\n");

    /* A field longer than its buffer must be rejected, not truncated. */
    {
        char row[512];
        char name[200];
        memset(name, 'x', sizeof name - 1);
        name[sizeof name - 1] = '\0';
        snprintf(row, sizeof row, HEADER
            "\"id1\",2026,\"2026-08-21\",\"t\",\"%s\",\"B\",1,1,1,\"final\"\n", name);
        expect_reject("team name longer than TEAM_NAME_SIZE", row);
    }

    /* A line longer than the read buffer must be rejected. */
    {
        static char big[4096];
        size_t len = (size_t)snprintf(big, sizeof big, "%s\"id1\",2026,\"", HEADER);
        memset(big + len, 'x', 2000);
        strcpy(big + len + 2000, "\",1,1,1,1,1,\"final\"\n");
        expect_reject("line longer than the read buffer", big);
    }

    /* A bad row in the middle must reject the whole file. */
    expect_reject("good row, bad row, good row", HEADER GOOD_ROW
        "\"id2\",oops,\"2026-08-22\",\"t\",\"A\",\"B\",1,1,1,\"final\"\n" GOOD_ROW);
}

/* ------------------------------------------------------------------ */
/* Saving                                                             */
/* ------------------------------------------------------------------ */

static void test_save_bad_arguments(void)
{
    Match m = make_match("m1", 2026, "2026-08-21", "t", "A", "B", 1, 0, true, "final");

    CHECK(save_matches_csv(NULL, &m, 1) != 0, "NULL filename accepted");
    CHECK(save_matches_csv(TMP_OUT, NULL, 1) != 0, "NULL array with count 1 accepted");
}

static void test_save_bad_directory(void)
{
    Match m = make_match("m1", 2026, "2026-08-21", "t", "A", "B", 1, 0, true, "final");

    CHECK(save_matches_csv("no_such_directory_xyz/out.csv", &m, 1) != 0,
          "saving into a missing directory reported success");
}

static void test_save_rejects_line_breaks(void)
{
    /* The loader reads line by line, so a field containing a line break
     * could be saved but never loaded again. Saving must refuse it, and
     * must not destroy the file that is already there. */
    write_text(TMP_OUT, "KEEP\n");

    Match m = make_match("m1", 2026, "2026-08-21", "t",
                         "Line\nBreak FC", "B", 1, 0, true, "final");

    CHECK(save_matches_csv(TMP_OUT, &m, 1) != 0,
          "field with a line break was saved");

    size_t size = 0;
    char *content = read_whole_file(TMP_OUT, &size);
    CHECK(content != NULL && size == 5 && memcmp(content, "KEEP\n", 5) == 0,
          "existing file was modified by a rejected save");
    free(content);
}

/* ------------------------------------------------------------------ */

static void run(const char *name, void (*test)(void))
{
    int before = g_failed;
    printf("[ RUN  ] %s\n", name);
    test();
    printf("[ %s ] %s\n", g_failed == before ? " OK " : "FAIL", name);
}

int main(int argc, char **argv)
{
    const char *real_csv = argc > 1 ? argv[1] : "data/matches_epl_2026.csv";

    run("round trip: special values",        test_roundtrip_special_values);
    run("round trip: empty list",            test_roundtrip_empty_list);
    printf("[ RUN  ] round trip: real file\n");
    {
        int before = g_failed;
        test_roundtrip_real_file(real_csv);
        printf("[ %s ] round trip: real file\n", g_failed == before ? " OK " : "FAIL");
    }

    run("load: valid row",                   test_load_valid_row);
    run("load: quotes and commas",           test_load_quotes_and_commas);
    run("load: no trailing newline",         test_load_no_trailing_newline);
    run("load: CRLF line endings",           test_load_crlf);
    run("load: blank trailing lines",        test_load_blank_trailing_lines);
    run("load: UTF-8 BOM",                   test_load_utf8_bom);
    run("load: header only",                 test_load_header_only);
    run("load: missing file",                test_load_missing_file);
    run("load: NULL arguments",              test_load_null_arguments);
    run("load: malformed input is rejected", test_load_rejects_bad_files);

    run("save: bad arguments",               test_save_bad_arguments);
    run("save: missing directory",           test_save_bad_directory);
    run("save: line break in a field",       test_save_rejects_line_breaks);

    remove(TMP_IN);
    remove(TMP_OUT);

    printf("\n%d checks, %d failed\n", g_checks, g_failed);
    return g_failed == 0 ? 0 : 1;
}