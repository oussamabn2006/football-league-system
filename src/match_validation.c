#include "match_validation.h"

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

/* Accepted match statuses from the API. */
static bool is_valid_status(const char *status)
{
    const char *valid_statuses[] = {
        "scheduled",
        "in_progress",
        "final",
        "postponed",
        "canceled",
        "suspended",
        "delayed"
    };

    size_t count = sizeof(valid_statuses) / sizeof(valid_statuses[0]);

    for (size_t i = 0; i < count; i++)
    {
        if (strcmp(status, valid_statuses[i]) == 0)
            return true;
    }

    return false;
}

/* Check whether a string contains at least one non-space character. */
static bool is_not_empty(const char *value)
{
    if (value == NULL)
        return false;

    while (*value)
    {
        if (!isspace((unsigned char)*value))
            return true;

        value++;
    }

    return false;
}

/* Validate YYYY-MM-DD and check that the date exists. */
static bool is_valid_date(const char *date)
{
    if (date == NULL || strlen(date) != 10)
        return false;

    /*
     * The text must be exactly YYYY-MM-DD: digits everywhere except the
     * two dashes. (sscanf with %d is not enough: it also accepts a sign
     * or leading spaces, so "2026-+8-21" would pass.)
     */
    for (int i = 0; i < 10; i++)
    {
        if (i == 4 || i == 7)
        {
            if (date[i] != '-')
                return false;
        }
        else if (!isdigit((unsigned char)date[i]))
        {
            return false;
        }
    }

    int year  = (date[0] - '0') * 1000 + (date[1] - '0') * 100 +
                (date[2] - '0') * 10   + (date[3] - '0');
    int month = (date[5] - '0') * 10 + (date[6] - '0');
    int day   = (date[8] - '0') * 10 + (date[9] - '0');

    if (year < 1900 || month < 1 || month > 12)
        return false;

    int days_per_month[] = {
        31, 28, 31, 30, 31, 30,
        31, 31, 30, 31, 30, 31
    };

    bool leap_year =
        (year % 400 == 0) ||
        (year % 4 == 0 && year % 100 != 0);

    if (leap_year)
        days_per_month[1] = 29;

    return day >= 1 && day <= days_per_month[month - 1];
}

static bool is_valid_match(const Match *match)
{
    if (match == NULL)
        return false;

    if (!is_not_empty(match->game_id))
        return false;

    if (!is_not_empty(match->home_team) ||
        !is_not_empty(match->away_team))
        return false;

    if (strcmp(match->home_team, match->away_team) == 0)
        return false;

    if (!is_valid_date(match->date))
        return false;

    if (!is_valid_status(match->status))
        return false;

    if (match->season < 1900)
        return false;

    /* A final match must have both scores. */
    if (strcmp(match->status, "final") == 0)
    {
        if (!match->has_score)
            return false;
    }

    /* Scores must never be negative. */
    if (match->has_score)
    {
        if (match->home_score < 0 || match->away_score < 0)
            return false;
    }

    return true;
}

static bool is_duplicate(
    const Match matches[],
    size_t accepted_count,
    const char *game_id
)
{
    for (size_t i = 0; i < accepted_count; i++)
    {
        if (strcmp(matches[i].game_id, game_id) == 0)
            return true;
    }

    return false;
}

int validate_and_deduplicate_matches(
    Match matches[],
    size_t *count,
    ValidationReport *report
)
{
    if (matches == NULL || count == NULL || report == NULL)
        return -1;

    report->valid_count = 0;
    report->invalid_count = 0;
    report->duplicate_count = 0;

    size_t original_count = *count;
    size_t accepted_count = 0;

    for (size_t i = 0; i < original_count; i++)
    {
        if (!is_valid_match(&matches[i]))
        {
            report->invalid_count++;
            continue;
        }

        if (is_duplicate(matches, accepted_count, matches[i].game_id))
        {
            report->duplicate_count++;
            continue;
        }

        /* Keep this valid, unique match in the array. */
        matches[accepted_count] = matches[i];
        accepted_count++;
    }

    *count = accepted_count;
    report->valid_count = accepted_count;

    return 0;
}