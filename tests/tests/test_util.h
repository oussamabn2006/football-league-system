/*
 * Tiny helpers shared by the test programs in this folder.
 *
 *   CHECK(condition, "message %d", value)   records a failure, keeps going
 *   run_test("name", function)              runs one test and prints OK / FAIL
 *   test_summary()                          prints the totals, returns the exit code
 *   make_match(...)                         builds a Match in one line
 *
 * Every test program includes this header once, so the counters below are
 * private to each program.
 */

#ifndef TEST_UTIL_H
#define TEST_UTIL_H

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "match.h"

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

/* Copies text into a fixed-size buffer, cutting it off if it is too long. */
static inline void copy_text(char *destination, size_t size, const char *source)
{
    size_t length = strlen(source);

    if (length >= size)
        length = size - 1;

    memcpy(destination, source, length);
    destination[length] = '\0';
}

static inline Match make_match(
    const char *id, int season, const char *date, const char *start,
    const char *home, const char *away,
    int home_score, int away_score, bool has_score, const char *status)
{
    Match m;
    memset(&m, 0, sizeof m);
    copy_text(m.game_id, sizeof m.game_id, id);
    m.season = season;
    copy_text(m.date, sizeof m.date, date);
    copy_text(m.start_time, sizeof m.start_time, start);
    copy_text(m.home_team, sizeof m.home_team, home);
    copy_text(m.away_team, sizeof m.away_team, away);
    m.home_score = home_score;
    m.away_score = away_score;
    m.has_score = has_score;
    copy_text(m.status, sizeof m.status, status);
    return m;
}

static inline void run_test(const char *name, void (*test)(void))
{
    int before = g_failed;
    printf("[ RUN  ] %s\n", name);
    test();
    printf("[ %s ] %s\n", g_failed == before ? " OK " : "FAIL", name);
}

static inline int test_summary(void)
{
    printf("\n%d checks, %d failed\n", g_checks, g_failed);
    return g_failed == 0 ? 0 : 1;
}

#endif