
#include "match_parser.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cjson/cJSON.h>

/*
 * Safely copies a JSON string into a fixed-size C array.
 * If the JSON field is missing or is not a string,
 * the destination receives an empty string.
 */
static void copy_json_string(
    char *destination,
    size_t destination_size,
    const cJSON *object,
    const char *field_name
)
{
    if (destination == NULL || destination_size == 0)
    {
        return;
    }

    destination[0] = '\0';

    const cJSON *field =
        cJSON_GetObjectItemCaseSensitive(object, field_name);

    if (!cJSON_IsString(field) || field->valuestring == NULL)
    {
        return;
    }

    snprintf(
        destination,
        destination_size,
        "%s",
        field->valuestring
    );
}

/*
 * Reads the name of a team from a nested JSON object.
 *
 * Example:
 * "home_team": {
 *     "name": "Arsenal"
 * }
 */
static void copy_team_name(
    char *destination,
    size_t destination_size,
    const cJSON *match_object,
    const char *team_field
)
{
    const cJSON *team =
        cJSON_GetObjectItemCaseSensitive(
            match_object,
            team_field
        );

    if (!cJSON_IsObject(team))
    {
        destination[0] = '\0';
        return;
    }

    copy_json_string(
        destination,
        destination_size,
        team,
        "name"
    );
}

/*
 * Converts one JSON match object into a Match structure.
 */
static void parse_one_match(
    const cJSON *json_match,
    Match *match
)
{
    memset(match, 0, sizeof(Match));

    copy_json_string(
        match->game_id,
        sizeof(match->game_id),
        json_match,
        "game_id"
    );

    copy_json_string(
        match->date,
        sizeof(match->date),
        json_match,
        "date"
    );

    copy_json_string(
        match->start_time,
        sizeof(match->start_time),
        json_match,
        "start_time"
    );

    copy_json_string(
        match->status,
        sizeof(match->status),
        json_match,
        "status"
    );

    copy_team_name(
        match->home_team,
        sizeof(match->home_team),
        json_match,
        "home_team"
    );

    copy_team_name(
        match->away_team,
        sizeof(match->away_team),
        json_match,
        "away_team"
    );

    /* Read the season. */
    const cJSON *season =
        cJSON_GetObjectItemCaseSensitive(
            json_match,
            "season"
        );

    if (cJSON_IsNumber(season))
    {
        match->season = season->valueint;
    }

    /*
     * Only consider the score available if BOTH
     * home_score and away_score are numbers.
     */
    const cJSON *home_score =
        cJSON_GetObjectItemCaseSensitive(
            json_match,
            "home_score"
        );

    const cJSON *away_score =
        cJSON_GetObjectItemCaseSensitive(
            json_match,
            "away_score"
        );

    if (cJSON_IsNumber(home_score) &&
        cJSON_IsNumber(away_score))
    {
        match->home_score = home_score->valueint;
        match->away_score = away_score->valueint;

        match->has_score = true;
    }
    else
    {
        match->has_score = false;
    }
}

/*
 * Parses the complete JSON array returned by API Ninjas.
 */
int parse_matches(
    const char *json,
    Match **matches_out,
    size_t *count_out
)
{
    if (json == NULL ||
        matches_out == NULL ||
        count_out == NULL)
    {
        return -1;
    }

    *matches_out = NULL;
    *count_out = 0;

    /* Convert JSON text into a cJSON tree. */
    cJSON *root = cJSON_Parse(json);

    if (root == NULL)
    {
        fprintf(stderr, "Error: Invalid JSON response.\n");
        return -1;
    }

    /*
     * API Ninjas returns a JSON array directly,
     * not an object containing a "response" field.
     */
    if (!cJSON_IsArray(root))
    {
        fprintf(stderr, "Error: Expected a JSON array.\n");
        cJSON_Delete(root);
        return -1;
    }

    int array_size = cJSON_GetArraySize(root);

    if (array_size < 0)
    {
        cJSON_Delete(root);
        return -1;
    }

    if (array_size == 0)
    {
        cJSON_Delete(root);
        return 0;
    }

    /* Allocate memory for all matches. */
    Match *matches = calloc(
        (size_t)array_size,
        sizeof(Match)
    );

    if (matches == NULL)
    {
        fprintf(stderr, "Error: Memory allocation failed.\n");
        cJSON_Delete(root);
        return -1;
    }

    /* Convert every JSON object into a Match. */
    for (int i = 0; i < array_size; i++)
    {
        const cJSON *json_match =
            cJSON_GetArrayItem(root, i);

        if (!cJSON_IsObject(json_match))
        {
            fprintf(stderr, "Error: Invalid match at index %d.\n", i);

            free(matches);
            cJSON_Delete(root);
            return -1;
        }

        parse_one_match(json_match, &matches[i]);
    }

    *matches_out = matches;
    *count_out = (size_t)array_size;

    cJSON_Delete(root);

    return 0;
}
