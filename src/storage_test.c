
#include <stdio.h>
#include <stdlib.h>

#include "match_storage.h"
#include "match_validation.h"

int main(void)
{
    Match *matches = NULL;
    size_t count = 0;

    if (load_matches_csv(
            "data/matches_epl_2026.csv",
            &matches,
            &count) != 0)
    {
        printf("CSV loading failed.\n");
        return EXIT_FAILURE;
    }

    printf("Loaded %zu matches from CSV.\n", count);

    ValidationReport report;

    if (validate_and_deduplicate_matches(
            matches, &count, &report) != 0)
    {
        free(matches);
        return EXIT_FAILURE;
    }

    printf("Valid matches: %zu\n", report.valid_count);

    free(matches);
    return EXIT_SUCCESS;
}
