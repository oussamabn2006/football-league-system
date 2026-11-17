#include <stdio.h>
#include <stdlib.h>

#include "app.h"
#include "cli.h"

#define PROGRAM_NAME "football_system"

/* Settings for the competition we work with. */
static const AppConfig CONFIG = {
    "epl",
    2026,
    "data/matches_epl_2026.csv"
};

/*
 * Application entry point.
 *
 * main only decides what to do at start-up; the work itself is done by
 * the app module (and the modules below it).
 */
int main(int argc, char **argv)
{
    CliOptions options;
    char error[160];

    if (cli_parse(argc, argv, &options, error, sizeof(error)) != 0)
    {
        fprintf(stderr, "Error: %s\n\n", error);
        cli_print_usage(stderr, PROGRAM_NAME);
        return EXIT_FAILURE;
    }

    if (options.help)
    {
        cli_print_usage(stdout, PROGRAM_NAME);
        return EXIT_SUCCESS;
    }

    AppState state;
    app_init(&state);

    bool start_ok = true;

    if (options.fetch)
    {
        start_ok = (app_fetch(&state, &CONFIG) == 0);
    }
    else if (options.load_path != NULL)
    {
        start_ok = (app_load_csv(&state, options.load_path) == 0);
    }
    else if (app_file_exists(CONFIG.csv_path))
    {
        /* In the menu a bad file is not fatal: the user can load another. */
        bool loaded = (app_load_csv(&state, CONFIG.csv_path) == 0);
        start_ok = loaded || !options.no_menu;
    }
    else if (options.no_menu)
    {
        fprintf(stderr,
            "There is no saved data (%s). Run with --fetch first.\n",
            CONFIG.csv_path);
        start_ok = false;
    }
    else
    {
        printf("No saved data found (%s).\n"
               "Choose 1 to fetch the season from the API, or 2 to load a CSV file.\n",
               CONFIG.csv_path);
    }

    int exit_code = EXIT_SUCCESS;

    if (!start_ok)
    {
        exit_code = EXIT_FAILURE;
    }
    else if (options.no_menu)
    {
        app_show_standings(&state);
    }
    else
    {
        app_run_menu(&state, &CONFIG, stdin);
    }

    app_free(&state);
    return exit_code;
}