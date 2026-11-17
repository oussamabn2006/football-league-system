
#ifndef MATCH_VALIDATION_H
#define MATCH_VALIDATION_H

#include <stddef.h>
#include "match.h"

typedef struct
{
    size_t valid_count;
    size_t invalid_count;
    size_t duplicate_count;
} ValidationReport;

/*
 * Validates matches and removes invalid or duplicate records.
 * The array is compacted in place.
 *
 * *count is updated to the number of retained matches.
 */
int validate_and_deduplicate_matches(
    Match matches[],
    size_t *count,
    ValidationReport *report
);

#endif
