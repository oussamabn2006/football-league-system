
#ifndef MATCH_STORAGE_H
#define MATCH_STORAGE_H

#include <stddef.h>
#include "match.h"

int save_matches_csv(
    const char *filename,
    const Match matches[],
    size_t count
);

int load_matches_csv(
    const char *filename,
    Match **matches_out,
    size_t *count_out
);

#endif
