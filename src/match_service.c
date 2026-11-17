
#include "match_service.h"

#include "api_client.h"
#include "match_parser.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define PAGE_SIZE 50
#define ENDPOINT_SIZE 256

int fetch_season_matches(
    const char *league,
    int season,
    Match **matches_out,
    size_t *count_out
)
{
    /* Validate the arguments. */
    if (league == NULL ||
        matches_out == NULL ||
        count_out == NULL ||
        season <= 0)
    {
        return -1;
    }

    /* Initialize outputs before doing any work. */
    *matches_out = NULL;
    *count_out = 0;

    Match *all_matches = NULL;
    size_t total_count = 0;
    size_t offset = 0;

    while (1)
    {
        char endpoint[ENDPOINT_SIZE];

        /* Build the endpoint for the current page. */
        int written = snprintf(
            endpoint,
            sizeof(endpoint),
            "/v1/soccermatches?league=%s&season=%d&offset=%zu",
            league,
            season,
            offset
        );

        if (written < 0 || (size_t)written >= sizeof(endpoint))
        {
            fprintf(stderr, "Error: API endpoint is too long.\n");
            free(all_matches);
            return -1;
        }

        printf("Fetching matches (offset=%zu)...\n", offset);

        /* Request one page from the API. */
        char *response = NULL;
        long http_status = 0;

        if (api_get(endpoint, &response, &http_status) != 0)
        {
            fprintf(stderr, "Error: API request failed.\n");
            free(all_matches);
            return -1;
        }

        if (http_status != 200)
        {
            fprintf(
                stderr,
                "Error: API returned HTTP status %ld.\n",
                http_status
            );

            free(response);
            free(all_matches);
            return -1;
        }

        /* Convert this page's JSON into Match structures. */
        Match *page_matches = NULL;
        size_t page_count = 0;

        int parse_result = parse_matches(
            response,
            &page_matches,
            &page_count
        );

        free(response);

        if (parse_result != 0)
        {
            fprintf(stderr, "Error: Could not parse API response.\n");
            free(all_matches);
            return -1;
        }

        printf("Received %zu matches.\n", page_count);

        /* Check for size overflow before allocating more memory. */
        if (page_count > SIZE_MAX - total_count)
        {
            fprintf(stderr, "Error: Too many matches.\n");
            free(page_matches);
            free(all_matches);
            return -1;
        }

        size_t new_total = total_count + page_count;

        if (new_total > SIZE_MAX / sizeof(Match))
        {
            fprintf(stderr, "Error: Match array is too large.\n");
            free(page_matches);
            free(all_matches);
            return -1;
        }

        /* Append the current page to the complete match array. */
        if (page_count > 0)
        {
            Match *temporary = realloc(
                all_matches,
                new_total * sizeof(Match)
            );

            if (temporary == NULL)
            {
                fprintf(stderr, "Error: Memory allocation failed.\n");
                free(page_matches);
                free(all_matches);
                return -1;
            }

            all_matches = temporary;

            memcpy(
                all_matches + total_count,
                page_matches,
                page_count * sizeof(Match)
            );
        }

        total_count = new_total;

        free(page_matches);

        /* A short page means there are no more pages. */
        if (page_count < PAGE_SIZE)
        {
            break;
        }

        /* Move to the next page. */
        if (offset > SIZE_MAX - PAGE_SIZE)
        {
            fprintf(stderr, "Error: Pagination offset overflow.\n");
            free(all_matches);
            return -1;
        }

        offset += PAGE_SIZE;
    }

    *matches_out = all_matches;
    *count_out = total_count;

    printf("\nTotal matches retrieved: %zu\n", total_count);

    return 0;
}
