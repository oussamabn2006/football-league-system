/*
 * Tests for cli.c and app.c: command-line options, menu input, loading /
 * fetching / saving, and complete menu sessions driven by scripted input.
 *
 * The real API is never called. This file defines its own
 * fetch_season_matches(), so it is linked WITHOUT match_service.c,
 * api_client.c and match_parser.c (no network, no libcurl, no cJSON), and
 * every test decides what the "API" answers.
 *
 * Usage:   test_app
 * The menu prints its screens while the tests run, so the output is long;
 * look at the [ OK ] / [ FAIL ] lines and the final summary.
 * Temporary files are created in the current directory and removed again.
 * Exit code: 0 = all checks passed, 1 = at least one failed.
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#endif

#include "app.h"
#include "cli.h"
#include "match.h"
#include "match_service.h"
#include "match_storage.h"
#include "test_util.h"

#define TMP_CSV    "test_tmp_app.csv"
#define TMP_OTHER  "test_tmp_app_other.csv"
#define TMP_BAD    "test_tmp_app_bad.csv"
#define TMP_SCRIPT "test_tmp_app_script.txt"
#define TMP_DIR    "test_tmp_app_dir"

#define CSV_HEADER \
    "game_id,season,date,start_time,home_team,away_team," \
    "home_score,away_score,has_score,status\n"

static const AppConfig g_config = { "epl", 2026, TMP_CSV };

/* ------------------------------------------------------------------ */
/* Fake API                                                           */
/* ------------------------------------------------------------------ */

static int g_api_result = 0;           /* what the fake API returns   */
static Match g_api_matches[8];         /* the matches it hands out    */
static size_t g_api_count = 0;
static int g_api_calls = 0;
static char g_api_league[32];
static int g_api_season = 0;

int fetch_season_matches(
    const char *league,
    int season,
    Match **matches_out,
    size_t *count_out
)
{
    g_api_calls++;
    snprintf(g_api_league, sizeof g_api_league, "%s", league);
    g_api_season = season;

    *matches_out = NULL;
    *count_out = 0;

    if (g_api_result != 0)
        return -1;

    if (g_api_count > 0)
    {
        Match *copy = malloc(g_api_count * sizeof *copy);
        if (copy == NULL)
            return -1;

        memcpy(copy, g_api_matches, g_api_count * sizeof *copy);
        *matches_out = copy;
    }

    *count_out = g_api_count;
    return 0;
}

static void reset_api(void)
{
    g_api_result = 0;
    g_api_count = 0;
    g_api_calls = 0;
    g_api_league[0] = '\0';
    g_api_season = 0;
}

/* ------------------------------------------------------------------ */
/* Helpers                                                            */
/* ------------------------------------------------------------------ */

static Match sample(const char *id, const char *home, const char *away,
                    int hs, int as, bool has_score, const char *status)
{
    return make_match(id, 2026, "2026-08-21", "t", home, away,
                      hs, as, has_score, status);
}

/* Three valid matches: two finished, one upcoming. */
static size_t fill_sample(Match out[3])
{
    out[0] = sample("a", "Arsenal", "Chelsea", 2, 1, true, "final");
    out[1] = sample("b", "Everton", "Fulham", 0, 0, true, "final");
    out[2] = sample("c", "Leeds", "Spurs", 0, 0, false, "scheduled");
    return 3;
}

static void write_sample_csv(const char *path)
{
    Match m[3];
    size_t n = fill_sample(m);
    CHECK(save_matches_csv(path, m, n) == 0, "could not write %s", path);
}

static void write_text(const char *path, const char *text)
{
    FILE *f = fopen(path, "wb");
    if (f == NULL)
    {
        printf("    cannot create %s\n", path);
        exit(2);
    }
    fputs(text, f);
    fclose(f);
}

static bool file_exists(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL)
        return false;
    fclose(f);
    return true;
}

static bool file_has_text(const char *path, const char *text)
{
    char buffer[256];
    FILE *f = fopen(path, "rb");
    if (f == NULL)
        return false;

    size_t got = fread(buffer, 1, sizeof buffer - 1, f);
    fclose(f);
    buffer[got] = '\0';

    return strcmp(buffer, text) == 0;
}

/* Number of rows in a saved CSV, or (size_t)-1 if it cannot be loaded. */
static size_t rows_in_csv(const char *path)
{
    Match *m = NULL;
    size_t n = 0;

    if (load_matches_csv(path, &m, &n) != 0)
        return (size_t)-1;

    free(m);
    return n;
}

/* Runs the menu with `script` as the user's typing. */
static void run_menu(AppState *state, const char *script)
{
    write_text(TMP_SCRIPT, script);

    FILE *in = fopen(TMP_SCRIPT, "r");
    CHECK(in != NULL, "cannot open the script");
    if (in == NULL)
        return;

    CHECK(app_run_menu(state, &g_config, in) == 0, "menu did not return 0");
    fclose(in);

    printf("\n");    /* the menu ends on a prompt without a line break */
}

static void cleanup(void)
{
    remove(TMP_CSV);
    remove(TMP_OTHER);
    remove(TMP_BAD);
    remove(TMP_SCRIPT);
    remove(TMP_DIR "/out.csv");
#ifdef _WIN32
    _rmdir(TMP_DIR);
#else
    remove(TMP_DIR);
#endif
}

/* ------------------------------------------------------------------ */
/* Command line                                                       */
/* ------------------------------------------------------------------ */

static void test_cli_valid(void)
{
    CliOptions o;
    char err[160];

    char *none[] = { "prog" };
    CHECK(cli_parse(1, none, &o, err, sizeof err) == 0, "no options rejected");
    CHECK(!o.fetch && o.load_path == NULL && !o.no_menu && !o.help, "defaults wrong");

    CHECK(cli_parse(0, NULL, &o, err, sizeof err) == 0, "argc 0 rejected");

    char *fetch[] = { "prog", "--fetch" };
    CHECK(cli_parse(2, fetch, &o, err, sizeof err) == 0 && o.fetch && !o.no_menu, "--fetch");

    char *load[] = { "prog", "--load", "data/x.csv" };
    CHECK(cli_parse(3, load, &o, err, sizeof err) == 0, "--load rejected");
    CHECK(o.load_path != NULL && strcmp(o.load_path, "data/x.csv") == 0 && !o.fetch,
          "--load path not stored");

    char *no_menu[] = { "prog", "--fetch", "--no-menu" };
    CHECK(cli_parse(3, no_menu, &o, err, sizeof err) == 0 && o.fetch && o.no_menu,
          "--fetch --no-menu");

    char *load_no_menu[] = { "prog", "--no-menu", "--load", "x.csv" };
    CHECK(cli_parse(4, load_no_menu, &o, err, sizeof err) == 0 && o.no_menu && o.load_path != NULL,
          "--no-menu --load");

    char *help[] = { "prog", "--help" };
    CHECK(cli_parse(2, help, &o, err, sizeof err) == 0 && o.help, "--help");

    char *help_short[] = { "prog", "-h" };
    CHECK(cli_parse(2, help_short, &o, err, sizeof err) == 0 && o.help, "-h");

    /* Help always wins, even next to options that would conflict. */
    char *help_wins[] = { "prog", "--help", "--fetch", "--load", "x.csv" };
    CHECK(cli_parse(5, help_wins, &o, err, sizeof err) == 0 && o.help,
          "--help next to conflicting options");
}

static void test_cli_invalid(void)
{
    CliOptions o;
    char err[160];

    char *unknown[] = { "prog", "--bogus" };
    CHECK(cli_parse(2, unknown, &o, err, sizeof err) != 0, "unknown option accepted");
    CHECK(strstr(err, "--bogus") != NULL, "message does not name the option: '%s'", err);

    char *no_file[] = { "prog", "--load" };
    CHECK(cli_parse(2, no_file, &o, err, sizeof err) != 0, "--load without a file accepted");
    CHECK(strstr(err, "--load") != NULL, "message: '%s'", err);

    char *empty_file[] = { "prog", "--load", "" };
    CHECK(cli_parse(3, empty_file, &o, err, sizeof err) != 0, "--load with an empty name accepted");

    char *option_as_file[] = { "prog", "--load", "--fetch" };
    CHECK(cli_parse(3, option_as_file, &o, err, sizeof err) != 0, "--load --fetch accepted");

    char *both[] = { "prog", "--fetch", "--load", "x.csv" };
    CHECK(cli_parse(4, both, &o, err, sizeof err) != 0, "--fetch together with --load accepted");

    char *both_reversed[] = { "prog", "--load", "x.csv", "--fetch" };
    CHECK(cli_parse(4, both_reversed, &o, err, sizeof err) != 0, "--load together with --fetch accepted");

    char *twice[] = { "prog", "--load", "a.csv", "--load", "b.csv" };
    CHECK(cli_parse(5, twice, &o, err, sizeof err) != 0, "--load twice accepted");

    char *stray[] = { "prog", "x.csv" };
    CHECK(cli_parse(2, stray, &o, err, sizeof err) != 0, "stray argument accepted");
    CHECK(strstr(err, "x.csv") != NULL, "message: '%s'", err);

    /* The error text is optional. */
    CHECK(cli_parse(2, unknown, &o, NULL, 0) != 0, "unknown option accepted without error buffer");

    /* A tiny buffer must be filled safely (valgrind / ASan would complain). */
    char tiny[4];
    CHECK(cli_parse(2, unknown, &o, tiny, sizeof tiny) != 0 && strlen(tiny) == 3,
          "tiny error buffer: '%s'", tiny);

    char *just_program[] = { "prog" };
    CHECK(cli_parse(1, just_program, NULL, err, sizeof err) != 0, "NULL options accepted");
}

/* ------------------------------------------------------------------ */
/* Menu input                                                         */
/* ------------------------------------------------------------------ */

static void test_parse_choice(void)
{
    CHECK(parse_menu_choice("1", 1, 6) == 1, "'1'");
    CHECK(parse_menu_choice("6", 1, 6) == 6, "'6'");
    CHECK(parse_menu_choice("  4  ", 1, 6) == 4, "'  4  '");
    CHECK(parse_menu_choice("3\n", 1, 6) == 3, "'3\\n'");
    CHECK(parse_menu_choice("3\r", 1, 6) == 3, "'3\\r'");
    CHECK(parse_menu_choice("007", 1, 10) == 7, "'007'");
    CHECK(parse_menu_choice("10", 1, 10) == 10, "'10' in range 1..10");

    CHECK(parse_menu_choice("0", 1, 6) == -1, "'0' is below the range");
    CHECK(parse_menu_choice("7", 1, 6) == -1, "'7' is above the range");
    CHECK(parse_menu_choice("10", 1, 6) == -1, "'10' is above the range");

    CHECK(parse_menu_choice("", 1, 6) == -1, "empty text");
    CHECK(parse_menu_choice("   ", 1, 6) == -1, "only spaces");
    CHECK(parse_menu_choice("abc", 1, 6) == -1, "'abc'");
    CHECK(parse_menu_choice("1a", 1, 6) == -1, "'1a'");
    CHECK(parse_menu_choice("a1", 1, 6) == -1, "'a1'");
    CHECK(parse_menu_choice("1 2", 1, 6) == -1, "'1 2'");
    CHECK(parse_menu_choice("1.5", 1, 6) == -1, "'1.5'");
    CHECK(parse_menu_choice("-1", 1, 6) == -1, "'-1'");
    CHECK(parse_menu_choice("+1", 1, 6) == -1, "'+1'");
    CHECK(parse_menu_choice("99999999999999999999", 1, 6) == -1, "huge number");
    CHECK(parse_menu_choice(NULL, 1, 6) == -1, "NULL");
}

/* ------------------------------------------------------------------ */
/* State                                                              */
/* ------------------------------------------------------------------ */

static void test_state(void)
{
    AppState s;
    app_init(&s);
    CHECK(s.matches == NULL && s.count == 0 && s.source[0] == '\0', "init");

    write_sample_csv(TMP_CSV);
    CHECK(app_load_csv(&s, TMP_CSV) == 0, "load");
    CHECK(s.matches != NULL && s.count == 3, "loaded %zu", s.count);

    app_free(&s);
    CHECK(s.matches == NULL && s.count == 0, "free did not reset the state");

    app_free(&s);                    /* freeing twice is harmless */
    app_free(NULL);
    app_init(NULL);

    CHECK(app_file_exists(TMP_CSV), "existing file not found");
    CHECK(!app_file_exists("this_file_does_not_exist.csv"), "missing file found");
    CHECK(!app_file_exists(NULL), "NULL path found");
}

/* ------------------------------------------------------------------ */
/* Loading                                                            */
/* ------------------------------------------------------------------ */

static void test_load_csv(void)
{
    AppState s;
    app_init(&s);

    write_sample_csv(TMP_CSV);

    CHECK(app_load_csv(&s, TMP_CSV) == 0, "load failed");
    CHECK(s.count == 3 && s.matches != NULL, "expected 3 matches, got %zu", s.count);
    CHECK(strstr(s.source, TMP_CSV) != NULL, "source is '%s'", s.source);

    /* A bad file must not touch what is loaded. */
    CHECK(app_load_csv(&s, "this_file_does_not_exist.csv") != 0, "missing file accepted");
    CHECK(s.count == 3, "missing file changed the data");

    write_text(TMP_BAD, "game_id,wrong,header\n");
    CHECK(app_load_csv(&s, TMP_BAD) != 0, "wrong header accepted");
    CHECK(s.count == 3, "wrong header changed the data");

    write_text(TMP_BAD, CSV_HEADER);
    CHECK(app_load_csv(&s, TMP_BAD) != 0, "header-only file accepted");
    CHECK(s.count == 3, "header-only file changed the data");

    CHECK(app_load_csv(&s, "") != 0, "empty file name accepted");
    CHECK(app_load_csv(&s, NULL) != 0, "NULL file name accepted");
    CHECK(app_load_csv(NULL, TMP_CSV) != 0, "NULL state accepted");
    CHECK(s.count == 3, "bad arguments changed the data");

    /* A good file replaces the old data. */
    Match one = sample("z", "Burnley", "Wolves", 1, 0, true, "final");
    CHECK(save_matches_csv(TMP_OTHER, &one, 1) == 0, "setup failed");
    CHECK(app_load_csv(&s, TMP_OTHER) == 0, "second load failed");
    CHECK(s.count == 1 && strcmp(s.matches[0].game_id, "z") == 0,
          "second load did not replace the data");
    CHECK(strstr(s.source, TMP_OTHER) != NULL, "source not updated: '%s'", s.source);

    app_free(&s);
}

static void test_load_drops_invalid_and_duplicates(void)
{
    Match m[5];
    fill_sample(m);
    m[3] = m[0];                                                     /* duplicate of "a"  */
    m[4] = sample("x", "Same", "Same", 1, 0, true, "final");         /* team plays itself */
    CHECK(save_matches_csv(TMP_OTHER, m, 5) == 0, "setup failed");

    AppState s;
    app_init(&s);

    CHECK(app_load_csv(&s, TMP_OTHER) == 0, "load failed");
    CHECK(s.count == 3, "expected 3 usable matches, got %zu", s.count);

    /* A file with nothing usable in it is refused and changes nothing. */
    Match bad = sample("y", "Same", "Same", 1, 0, true, "final");
    CHECK(save_matches_csv(TMP_BAD, &bad, 1) == 0, "setup failed");
    CHECK(app_load_csv(&s, TMP_BAD) != 0, "file with only invalid matches accepted");
    CHECK(s.count == 3, "invalid-only file changed the data");

    app_free(&s);
}

/* ------------------------------------------------------------------ */
/* Fetching                                                           */
/* ------------------------------------------------------------------ */

static void test_fetch(void)
{
    reset_api();
    remove(TMP_CSV);

    g_api_matches[0] = sample("a", "Arsenal", "Chelsea", 2, 1, true, "final");
    g_api_matches[1] = sample("b", "Everton", "Fulham", 0, 0, true, "final");
    g_api_matches[2] = sample("a", "Arsenal", "Chelsea", 2, 1, true, "final");     /* duplicate */
    g_api_matches[3] = sample("c", "Leeds", "Spurs", 0, 0, true, "bogus status");   /* invalid   */
    g_api_count = 4;

    AppState s;
    app_init(&s);

    CHECK(app_fetch(&s, &g_config) == 0, "fetch failed");
    CHECK(g_api_calls == 1, "API called %d times", g_api_calls);
    CHECK(strcmp(g_api_league, "epl") == 0 && g_api_season == 2026,
          "API asked for '%s' %d", g_api_league, g_api_season);
    CHECK(s.count == 2, "expected 2 valid unique matches, got %zu", s.count);
    CHECK(file_exists(TMP_CSV), "fetched data was not saved");
    CHECK(rows_in_csv(TMP_CSV) == 2, "saved file has %zu rows", rows_in_csv(TMP_CSV));

    /* API error: keep the old data and the old file. */
    g_api_result = -1;
    CHECK(app_fetch(&s, &g_config) != 0, "API failure reported as success");
    CHECK(s.count == 2, "API failure changed the data");

    /* API answers with nothing: keep the old data and the old file. */
    g_api_result = 0;
    g_api_count = 0;
    CHECK(app_fetch(&s, &g_config) != 0, "empty answer reported as success");
    CHECK(s.count == 2, "empty answer wiped the data");
    CHECK(rows_in_csv(TMP_CSV) == 2, "empty answer overwrote the CSV (%zu rows)", rows_in_csv(TMP_CSV));

    /* API answers with only unusable matches: same. */
    g_api_matches[0] = sample("z", "Same", "Same", 1, 0, true, "final");
    g_api_count = 1;
    CHECK(app_fetch(&s, &g_config) != 0, "unusable answer reported as success");
    CHECK(s.count == 2, "unusable answer wiped the data");
    CHECK(rows_in_csv(TMP_CSV) == 2, "unusable answer overwrote the CSV (%zu rows)", rows_in_csv(TMP_CSV));

    CHECK(app_fetch(NULL, &g_config) != 0, "NULL state accepted");
    CHECK(app_fetch(&s, NULL) != 0, "NULL config accepted");

    app_free(&s);
}

static void test_fetch_save_failure_keeps_data_in_memory(void)
{
    reset_api();
    g_api_matches[0] = sample("a", "Arsenal", "Chelsea", 2, 1, true, "final");
    g_api_count = 1;

    /* Two missing folder levels: only one level is created, so saving fails. */
    const AppConfig impossible = { "epl", 2026, "no_such_a/no_such_b/x.csv" };

    AppState s;
    app_init(&s);

    CHECK(app_fetch(&s, &impossible) != 0, "a failed save must be reported");
    CHECK(s.count == 1, "data lost when saving failed");

    app_free(&s);
}

static void test_fetch_creates_folder(void)
{
    reset_api();
    g_api_matches[0] = sample("a", "Arsenal", "Chelsea", 2, 1, true, "final");
    g_api_count = 1;

    const AppConfig in_folder = { "epl", 2026, TMP_DIR "/out.csv" };

    AppState s;
    app_init(&s);

    CHECK(app_fetch(&s, &in_folder) == 0, "fetch into a new folder failed");
    CHECK(rows_in_csv(TMP_DIR "/out.csv") == 1, "file in the new folder is wrong");

    app_free(&s);
}

/* ------------------------------------------------------------------ */
/* Saving                                                             */
/* ------------------------------------------------------------------ */

static void test_save_csv(void)
{
    AppState s;
    app_init(&s);

    remove(TMP_OTHER);
    CHECK(app_save_csv(&s, TMP_OTHER) != 0, "saving without data reported success");
    CHECK(!file_exists(TMP_OTHER), "a file was created although there was no data");

    write_sample_csv(TMP_CSV);
    CHECK(app_load_csv(&s, TMP_CSV) == 0, "load failed");

    CHECK(app_save_csv(&s, TMP_OTHER) == 0, "save failed");
    CHECK(rows_in_csv(TMP_OTHER) == 3, "saved file has %zu rows", rows_in_csv(TMP_OTHER));

    CHECK(app_save_csv(&s, "") != 0, "empty file name accepted");
    CHECK(app_save_csv(&s, NULL) != 0, "NULL file name accepted");
    CHECK(app_save_csv(NULL, TMP_OTHER) != 0, "NULL state accepted");
    CHECK(app_save_csv(&s, "no_such_a/no_such_b/x.csv") != 0, "impossible folder accepted");

    /* The folder is created when it is missing (one level). */
    CHECK(app_save_csv(&s, TMP_DIR "/out.csv") == 0, "save into a new folder failed");
    CHECK(rows_in_csv(TMP_DIR "/out.csv") == 3, "file in the new folder is wrong");

    app_free(&s);
}

/* ------------------------------------------------------------------ */
/* Whole menu sessions                                                */
/* ------------------------------------------------------------------ */

static void test_menu_ends_cleanly(void)
{
    reset_api();

    AppState s;
    app_init(&s);

    run_menu(&s, "");                      /* input ends at once          */
    run_menu(&s, "6\n");                   /* Exit                        */
    run_menu(&s, "abc\n\n9\n0\n-1\n6\n");  /* nonsense, then Exit         */
    run_menu(&s, "1 2\n6");                /* no final newline            */
    run_menu(&s, "abc\r\n6\r\n");          /* Windows line endings        */

    CHECK(s.count == 0, "nothing was loaded, but there is data");
    CHECK(g_api_calls == 0, "the API was called");
    app_free(&s);
}

static void test_menu_long_line_is_discarded(void)
{
    reset_api();
    g_api_matches[0] = sample("a", "Arsenal", "Chelsea", 2, 1, true, "final");
    g_api_count = 1;

    /*
     * One very long line that ends in "1" (= fetch), followed by "y".
     * If the end of the long line were read as the next answer, the fetch
     * would run. It must be thrown away instead.
     */
    char script[1024];
    size_t length = 600;
    memset(script, 'x', length);
    snprintf(script + length, sizeof script - length, "1\ny\n6\n");

    AppState s;
    app_init(&s);

    run_menu(&s, script);
    CHECK(g_api_calls == 0, "the end of a long line was treated as an answer");

    app_free(&s);
}

static void test_menu_without_data(void)
{
    reset_api();

    AppState s;
    app_init(&s);

    /* Display matches, standings and save with nothing loaded. */
    remove(TMP_OTHER);
    run_menu(&s, "3\n4\n5\n6\n");

    CHECK(s.count == 0, "data appeared from nowhere");
    CHECK(!file_exists(TMP_OTHER), "a file was created without data");
    CHECK(g_api_calls == 0, "the API was called");

    app_free(&s);
}

static void test_menu_load_display_save(void)
{
    AppState s;
    app_init(&s);

    write_sample_csv(TMP_CSV);
    remove(TMP_OTHER);

    /* load the default (empty answer), browse all matches (sub-menu 1, then
     * Back), display standings, save elsewhere */
    run_menu(&s, "2\n\n3\n1\n6\n4\n5\n" TMP_OTHER "\n6\n");

    CHECK(s.count == 3, "expected 3 matches, got %zu", s.count);
    CHECK(rows_in_csv(TMP_OTHER) == 3, "menu save wrote %zu rows", rows_in_csv(TMP_OTHER));

    /* A quoted name with spaces around it (as pasted from Windows),
     * then a load that fails: the data stays. */
    app_free(&s);
    app_init(&s);
    run_menu(&s, "2\n  \"" TMP_OTHER "\"  \n2\nthis_file_does_not_exist.csv\n6\n");
    CHECK(s.count == 3, "quoted path not loaded, got %zu matches", s.count);

    app_free(&s);
}

/* ------------------------------------------------------------------ */
/* Browsing: team search, upcoming, latest, sub-menu                  */
/* ------------------------------------------------------------------ */

static void test_browse_functions(void)
{
    AppState s;
    app_init(&s);

    /* Nothing loaded. */
    CHECK(app_show_team_matches(&s, "arsenal") != 0, "team matches without data");
    CHECK(app_show_team_summary(&s, "arsenal") != 0, "team summary without data");
    CHECK(app_show_upcoming(&s, 10) != 0, "upcoming without data");
    CHECK(app_show_latest_results(&s, 10) != 0, "latest without data");
    CHECK(app_show_team_matches(NULL, "arsenal") != 0, "NULL state");
    CHECK(app_show_upcoming(NULL, 10) != 0, "NULL state (upcoming)");

    write_sample_csv(TMP_CSV);
    CHECK(app_load_csv(&s, TMP_CSV) == 0, "load failed");

    CHECK(app_show_team_matches(&s, "arsenal") == 0, "exact name (any case)");
    CHECK(app_show_team_matches(&s, "  chel ") == 0, "part of a name");
    CHECK(app_show_team_summary(&s, "LEEDS") == 0, "team with only an upcoming match");
    CHECK(app_show_team_matches(&s, "nobody") != 0, "unknown team");
    CHECK(app_show_team_matches(&s, "") != 0, "empty query");
    CHECK(app_show_team_matches(&s, NULL) != 0, "NULL query");
    CHECK(app_show_team_summary(&s, "e") != 0, "ambiguous query (Arsenal, Chelsea, ...)");

    CHECK(app_show_upcoming(&s, 10) == 0, "upcoming");
    CHECK(app_show_upcoming(&s, 0) == 0, "upcoming with limit 0");
    CHECK(app_show_latest_results(&s, 1) == 0, "latest, limit below the number of results");
    CHECK(app_show_latest_results(&s, 10) == 0, "latest");

    CHECK(s.count == 3, "browsing changed the data");
    app_free(&s);
}

static void test_browse_ambiguous_names(void)
{
    AppState s;
    app_init(&s);

    Match m[3];
    m[0] = sample("a", "Manchester City", "Manchester United", 1, 1, true, "final");
    m[1] = sample("b", "Leeds", "Leeds United", 2, 0, true, "final");
    m[2] = sample("c", "Arsenal", "Leeds", 0, 0, false, "scheduled");
    CHECK(save_matches_csv(TMP_OTHER, m, 3) == 0, "setup failed");
    CHECK(app_load_csv(&s, TMP_OTHER) == 0, "load failed");

    CHECK(app_show_team_matches(&s, "manchester") != 0, "ambiguous name accepted");
    CHECK(app_show_team_matches(&s, "manchester u") == 0, "unique part rejected");
    CHECK(app_show_team_summary(&s, "leeds") == 0, "exact name must win over 'Leeds United'");
    CHECK(app_show_team_summary(&s, "united") != 0, "'united' is ambiguous");

    app_free(&s);
}

static void test_menu_browse(void)
{
    reset_api();

    AppState s;
    app_init(&s);
    write_sample_csv(TMP_CSV);

    /* Every sub-menu entry once, a wrong team, a wrong number, an
     * ambiguous name, then Back and Exit. */
    run_menu(&s,
        "2\n\n"
        "3\n"
        "1\n"
        "2\narsenal\n"
        "2\nnobody\n"
        "2\ne\n"
        "3\n"
        "4\n"
        "5\nchelsea\n"
        "5\n\n"
        "9\nabc\n"
        "6\n"
        "6\n");
    CHECK(s.count == 3, "browsing changed the data (%zu matches)", s.count);

    /* The input ends inside the sub-menu, or at the team question. */
    run_menu(&s, "3\n");
    run_menu(&s, "3\n2\n");
    run_menu(&s, "3\n5\n");
    run_menu(&s, "3\n2\narsenal");    /* no final line break */
    CHECK(s.count == 3, "data lost");

    /* The sub-menu is not reachable without data. */
    AppState empty;
    app_init(&empty);
    run_menu(&empty, "3\n6\n");      /* "3" says no data, "6" exits */
    CHECK(empty.count == 0, "data appeared from nowhere");

    app_free(&s);
    app_free(&empty);
}

static void test_menu_overwrite_asks_first(void)
{
    AppState s;
    app_init(&s);
    write_sample_csv(TMP_CSV);

    /* The target exists: "n" keeps it, anything else than y keeps it too. */
    write_text(TMP_OTHER, "KEEP");
    run_menu(&s, "2\n\n5\n" TMP_OTHER "\nn\n6\n");
    CHECK(file_has_text(TMP_OTHER, "KEEP"), "file overwritten although the user said no");

    run_menu(&s, "5\n" TMP_OTHER "\n\n6\n");
    CHECK(file_has_text(TMP_OTHER, "KEEP"), "file overwritten on an empty answer");

    run_menu(&s, "5\n" TMP_OTHER "\n");   /* input ends at the question */
    CHECK(file_has_text(TMP_OTHER, "KEEP"), "file overwritten when the input ended");

    run_menu(&s, "5\n" TMP_OTHER "\ny\n6\n");
    CHECK(rows_in_csv(TMP_OTHER) == 3, "file not overwritten after 'y'");

    app_free(&s);
}

static void test_menu_fetch(void)
{
    reset_api();
    g_api_matches[0] = sample("a", "Arsenal", "Chelsea", 2, 1, true, "final");
    g_api_matches[1] = sample("b", "Everton", "Fulham", 0, 0, true, "final");
    g_api_count = 2;

    AppState s;
    app_init(&s);
    remove(TMP_CSV);

    run_menu(&s, "1\nn\n6\n");
    CHECK(g_api_calls == 0, "fetch ran although the user said no");

    run_menu(&s, "1\n\n6\n");
    CHECK(g_api_calls == 0, "fetch ran on an empty answer");

    run_menu(&s, "1\n");    /* input ends at the question */
    CHECK(g_api_calls == 0, "fetch ran when the input ended");

    run_menu(&s, "1\ny\n6\n");
    CHECK(g_api_calls == 1, "fetch did not run after 'y'");
    CHECK(s.count == 2, "expected 2 matches, got %zu", s.count);
    CHECK(file_exists(TMP_CSV), "fetched data was not saved");

    run_menu(&s, "1\nYES\n6\n");
    CHECK(g_api_calls == 2, "fetch did not run after 'YES'");

    /* A failing API must not end the menu or lose the data. */
    g_api_result = -1;
    run_menu(&s, "1\ny\n6\n");
    CHECK(g_api_calls == 3, "fetch did not run");
    CHECK(s.count == 2, "failed fetch changed the data");

    app_free(&s);
}

int main(void)
{
    cleanup();

    run_test("command line: valid options",          test_cli_valid);
    run_test("command line: invalid options",        test_cli_invalid);
    run_test("menu choice parsing",                  test_parse_choice);
    run_test("state: init, free, file check",        test_state);

    run_test("load: good and bad files",             test_load_csv);
    run_test("load: invalid and duplicate records",  test_load_drops_invalid_and_duplicates);
    run_test("fetch: success and every failure",     test_fetch);
    run_test("fetch: saving fails, data stays",      test_fetch_save_failure_keeps_data_in_memory);
    run_test("fetch: creates the output folder",     test_fetch_creates_folder);
    run_test("save",                                 test_save_csv);

    run_test("menu: always ends cleanly",            test_menu_ends_cleanly);
    run_test("menu: long line is discarded",         test_menu_long_line_is_discarded);
    run_test("menu: actions without data",           test_menu_without_data);
    run_test("menu: load, display, save",            test_menu_load_display_save);
    run_test("browse: team search and lists",        test_browse_functions);
    run_test("browse: ambiguous team names",         test_browse_ambiguous_names);
    run_test("menu: browse sub-menu",                test_menu_browse);
    run_test("menu: overwrite asks first",           test_menu_overwrite_asks_first);
    run_test("menu: fetch with confirmation",        test_menu_fetch);

    cleanup();
    return test_summary();
}