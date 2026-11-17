#include "cli.h"

#include <ctype.h>
#include <string.h>

/* Writes "prefix" + "detail" into the error buffer (if there is one). */
static int fail(
    char *error,
    size_t error_size,
    const char *prefix,
    const char *detail
)
{
    if (error != NULL && error_size > 0)
    {
        snprintf(error, error_size, "%s%s", prefix, detail);
    }

    return -1;
}

int cli_parse(
    int argc,
    char **argv,
    CliOptions *options,
    char *error,
    size_t error_size
)
{
    if (options == NULL || (argc > 0 && argv == NULL))
    {
        return fail(error, error_size, "Invalid arguments.", "");
    }

    memset(options, 0, sizeof(*options));

    for (int i = 1; i < argc; i++)
    {
        const char *arg = argv[i];

        if (strcmp(arg, "--help") == 0 || strcmp(arg, "-h") == 0)
        {
            options->help = true;
        }
        else if (strcmp(arg, "--fetch") == 0)
        {
            options->fetch = true;
        }
        else if (strcmp(arg, "--no-menu") == 0)
        {
            options->no_menu = true;
        }
        else if (strcmp(arg, "--load") == 0)
        {
            if (options->load_path != NULL)
            {
                return fail(error, error_size,
                    "--load was given more than once.", "");
            }

            /* The next argument must be a file name, not another option. */
            if (i + 1 >= argc ||
                argv[i + 1][0] == '\0' ||
                strncmp(argv[i + 1], "--", 2) == 0)
            {
                return fail(error, error_size,
                    "--load needs a file name.", "");
            }

            options->load_path = argv[++i];
        }
        else if (arg[0] == '-')
        {
            return fail(error, error_size, "Unknown option: ", arg);
        }
        else
        {
            return fail(error, error_size, "Unexpected argument: ", arg);
        }
    }

    /* Asking for help always wins; nothing else is checked. */
    if (options->help)
    {
        return 0;
    }

    if (options->fetch && options->load_path != NULL)
    {
        return fail(error, error_size,
            "--fetch and --load cannot be used together.", "");
    }

    return 0;
}

void cli_print_usage(FILE *out, const char *program)
{
    fprintf(out,
        "Usage: %s [options]\n"
        "\n"
        "With no options the menu opens, and the saved season file is\n"
        "loaded first if it exists.\n"
        "\n"
        "Options:\n"
        "  --fetch       Download the season from the API, validate it and\n"
        "                save it to the CSV file\n"
        "  --load FILE   Load the matches from FILE (a CSV file)\n"
        "  --no-menu     Do not open the menu: print the standings and exit\n"
        "  -h, --help    Show this help\n",
        program
    );
}

int parse_menu_choice(const char *line, int min, int max)
{
    if (line == NULL)
    {
        return -1;
    }

    while (isspace((unsigned char)*line))
    {
        line++;
    }

    if (!isdigit((unsigned char)*line))
    {
        return -1;
    }

    long value = 0;

    while (isdigit((unsigned char)*line))
    {
        value = value * 10 + (*line - '0');

        /* Far beyond any menu; also keeps the number from overflowing. */
        if (value > 1000000)
        {
            return -1;
        }

        line++;
    }

    while (isspace((unsigned char)*line))
    {
        line++;
    }

    if (*line != '\0')
    {
        return -1;
    }

    return (value >= min && value <= max) ? (int)value : -1;
}