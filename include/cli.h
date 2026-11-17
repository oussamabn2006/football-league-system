#ifndef CLI_H
#define CLI_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

/* What the user asked for on the command line. */
typedef struct
{
    bool fetch;              /* --fetch: download the season from the API   */
    const char *load_path;   /* --load FILE: CSV to load (points into argv) */
    bool no_menu;            /* --no-menu: print the standings and exit     */
    bool help;               /* -h / --help                                 */
} CliOptions;

/*
 * Reads the command-line arguments.
 *
 * Returns:
 *   0  = success, *options is filled in
 *  -1  = invalid arguments; a message for the user is written to `error`
 *        (error may be NULL)
 */
int cli_parse(
    int argc,
    char **argv,
    CliOptions *options,
    char *error,
    size_t error_size
);

/* Prints the list of options. */
void cli_print_usage(FILE *out, const char *program);

/*
 * Reads a menu answer such as "3" or "  3 ".
 *
 * Returns the number if the whole text is a number between min and max
 * (inclusive), otherwise -1.
 */
int parse_menu_choice(const char *line, int min, int max);

#endif