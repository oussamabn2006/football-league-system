
#ifndef MATCH_H
#define MATCH_H

#include <stdbool.h>

#define GAME_ID_SIZE 64
#define DATE_SIZE 16
#define START_TIME_SIZE 32
#define TEAM_NAME_SIZE 100
#define STATUS_SIZE 32

typedef struct
{
    char game_id[GAME_ID_SIZE];

    int season;

    char date[DATE_SIZE];
    char start_time[START_TIME_SIZE];

    char home_team[TEAM_NAME_SIZE];
    char away_team[TEAM_NAME_SIZE];

    int home_score;
    int away_score;

    /*
     * False means the scores are not available yet.
     * This is useful for upcoming matches.
     */
    bool has_score;

    char status[STATUS_SIZE];

} Match;

#endif
