#include "match_display.h"

#include <stdio.h>

/*
 * Displays matches in a readable table.
 */
void display_matches(
    const Match matches[],
    size_t count
)
{
    if (matches == NULL && count > 0)
    {
        printf("No matches to show.\n");
        return;
    }

    printf("\n");
    printf("===============================================================\n");
    printf("              ENGLISH PREMIER LEAGUE 2026-27\n");
    printf("===============================================================\n");

    if (matches == NULL || count == 0)
    {
        printf("No matches to show.\n");
        return;
    }

    printf(
        "%-12s %-23s %-23s %-9s %-12s\n",
        "DATE",
        "HOME TEAM",
        "AWAY TEAM",
        "SCORE",
        "STATUS"
    );

    printf("---------------------------------------------------------------\n");

    for (size_t i = 0; i < count; i++)
    {
        char score[16];

        if (matches[i].has_score)
        {
            snprintf(
                score,
                sizeof(score),
                "%d - %d",
                matches[i].home_score,
                matches[i].away_score
            );
        }
        else
        {
            snprintf(score, sizeof(score), "-");
        }

        printf(
            "%-12s %-23.23s %-23.23s %-9s %-12s\n",
            matches[i].date,
            matches[i].home_team,
            matches[i].away_team,
            score,
            matches[i].status
        );
    }

    printf("---------------------------------------------------------------\n");
    printf("Total matches retrieved: %zu\n", count);
}

static void format_score(const Match *match, char *score, size_t size)
{
    if (match->has_score)
    {
        snprintf(score, size, "%d - %d", match->home_score, match->away_score);
    }
    else
    {
        snprintf(score, size, "-");
    }
}

void display_match_list(
    const char *title,
    const Match matches[],
    size_t count
)
{
    printf("\n");
    printf("===============================================================\n");
    printf("  %s\n", title != NULL ? title : "MATCHES");
    printf("===============================================================\n");

    if (count == 0 || matches == NULL)
    {
        printf("No matches to show.\n");
        return;
    }

    printf(
        "%-12s %-23s %-23s %-9s %-12s\n",
        "DATE", "HOME TEAM", "AWAY TEAM", "SCORE", "STATUS"
    );
    printf("---------------------------------------------------------------\n");

    for (size_t i = 0; i < count; i++)
    {
        char score[32];
        format_score(&matches[i], score, sizeof(score));

        printf(
            "%-12s %-23.23s %-23.23s %-9s %-12s\n",
            matches[i].date,
            matches[i].home_team,
            matches[i].away_team,
            score,
            matches[i].status
        );
    }

    printf("---------------------------------------------------------------\n");
    printf("%zu match%s\n", count, count == 1 ? "" : "es");
}

static void print_record_row(const char *label, const Record *r)
{
    printf(
        "%-9s %3d %3d %3d %3d %4d %4d %+4d %4d\n",
        label,
        r->played, r->won, r->drawn, r->lost,
        r->goals_for, r->goals_against,
        r->goals_for - r->goals_against,
        r->points
    );
}

void display_team_summary(const TeamSummary *summary)
{
    if (summary == NULL)
    {
        return;
    }

    printf("\n");
    printf("===============================================================\n");
    printf("  %s\n", summary->team);
    printf("===============================================================\n");

    if (summary->overall.played == 0)
    {
        printf("No finished matches yet.\n");
        return;
    }

    printf("%-9s %3s %3s %3s %3s %4s %4s %4s %4s\n",
           "", "P", "W", "D", "L", "GF", "GA", "GD", "PTS");
    printf("---------------------------------------------------------------\n");
    print_record_row("Overall", &summary->overall);
    print_record_row("Home", &summary->home);
    print_record_row("Away", &summary->away);
    printf("---------------------------------------------------------------\n");

    printf("Form (oldest to newest): %s\n", summary->form);

    if (summary->streak_length > 0)
    {
        const char *word = summary->streak_result == 'W' ? "win"
                         : summary->streak_result == 'D' ? "draw"
                                                         : "defeat";

        if (summary->streak_length == 1)
        {
            printf("Last result: %s\n", word);
        }
        else
        {
            printf("Current run: %d %ss in a row\n", summary->streak_length, word);
        }
    }
}