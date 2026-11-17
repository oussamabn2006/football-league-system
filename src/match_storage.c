#include "match_storage.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Longest line the loader accepts. The longest valid row is well below
 * this, even if every quote in every field has to be doubled.
 */
#define CSV_LINE_SIZE 1024

/* Room for the text of an int field ("-2147483648" is 11 characters). */
#define CSV_NUMBER_SIZE 32

/* The header is written by save_matches_csv and checked by load_matches_csv. */
#define CSV_HEADER \
    "game_id,season,date,start_time,home_team,away_team," \
    "home_score,away_score,has_score,status"

/* ------------------------------------------------------------------ */
/* Writing                                                            */
/* ------------------------------------------------------------------ */

static bool has_line_break(const char *value)
{
    return strchr(value, '\n') != NULL || strchr(value, '\r') != NULL;
}

/*
 * The loader reads one line per record, so a text field that contains a
 * line break could be saved but never loaded again. Such matches are
 * refused before the file is opened, so an existing file is not destroyed.
 */
static bool is_csv_safe(const Match *m)
{
    return !has_line_break(m->game_id) &&
           !has_line_break(m->date) &&
           !has_line_break(m->start_time) &&
           !has_line_break(m->home_team) &&
           !has_line_break(m->away_team) &&
           !has_line_break(m->status);
}

/* Write a CSV field, escaping quotes by doubling them. */
static int write_csv_field(FILE *file, const char *value)
{
    if (fputc('"', file) == EOF)
        return -1;

    for (const char *p = value; *p; p++)
    {
        if (*p == '"' && fputc('"', file) == EOF)
            return -1;

        if (fputc(*p, file) == EOF)
            return -1;
    }

    return fputc('"', file) == EOF ? -1 : 0;
}

int save_matches_csv(
    const char *filename,
    const Match matches[],
    size_t count
)
{
    if (filename == NULL || (matches == NULL && count > 0))
        return -1;

    for (size_t i = 0; i < count; i++)
    {
        if (!is_csv_safe(&matches[i]))
        {
            fprintf(stderr,
                "CSV error: match %zu contains a line break and cannot be saved.\n",
                i);
            return -1;
        }
    }

    FILE *file = fopen(filename, "w");

    if (file == NULL)
    {
        perror("Could not open CSV file");
        return -1;
    }

    if (fprintf(file, "%s\n", CSV_HEADER) < 0)
    {
        fclose(file);
        return -1;
    }

    for (size_t i = 0; i < count; i++)
    {
        const Match *m = &matches[i];

        if (write_csv_field(file, m->game_id) != 0 ||
            fprintf(file, ",%d,", m->season) < 0 ||
            write_csv_field(file, m->date) != 0 ||
            fputc(',', file) == EOF ||
            write_csv_field(file, m->start_time) != 0 ||
            fputc(',', file) == EOF ||
            write_csv_field(file, m->home_team) != 0 ||
            fputc(',', file) == EOF ||
            write_csv_field(file, m->away_team) != 0 ||
            fprintf(file, ",%d,%d,%d,",
                m->home_score,
                m->away_score,
                m->has_score ? 1 : 0) < 0 ||
            write_csv_field(file, m->status) != 0 ||
            fputc('\n', file) == EOF)
        {
            fclose(file);
            return -1;
        }
    }

    if (fclose(file) != 0)
        return -1;

    return 0;
}

/* ------------------------------------------------------------------ */
/* Reading                                                            */
/* ------------------------------------------------------------------ */

/*
 * Reads one field starting at *cursor and advances *cursor past the
 * delimiter that ended it.
 *
 * On success *terminator is ',' (another field follows) or '\n' (this was
 * the last field of the line).
 *
 * Returns -1 for malformed input: an unterminated quote, text after a
 * closing quote, or a field that does not fit in the output buffer
 * (fields are rejected, never silently truncated).
 */
static int read_csv_field(
    const char **cursor,
    char *output,
    size_t output_size,
    char *terminator
)
{
    const char *p = *cursor;
    size_t length = 0;
    bool quoted = false;

    if (*p == '"')
    {
        quoted = true;
        p++;
    }

    while (*p)
    {
        if (quoted)
        {
            if (*p == '"')
            {
                if (p[1] == '"')
                {
                    if (length + 1 >= output_size)
                        return -1;

                    output[length++] = '"';
                    p += 2;
                    continue;
                }

                p++;            /* closing quote */
                quoted = false;
                break;
            }
        }
        else if (*p == ',' || *p == '\n' || *p == '\r')
        {
            break;
        }

        if (length + 1 >= output_size)
            return -1;

        output[length++] = *p++;
    }

    if (quoted)
        return -1;              /* the line ended inside a quoted field */

    output[length] = '\0';

    if (*p == ',')
    {
        *terminator = ',';
        p++;
    }
    else if (*p == '\0')
    {
        *terminator = '\n';
    }
    else if (*p == '\r' || *p == '\n')
    {
        *terminator = '\n';

        if (*p == '\r' && p[1] == '\n')
            p += 2;
        else
            p++;
    }
    else
    {
        return -1;              /* text after a closing quote */
    }

    *cursor = p;
    return 0;
}

/*
 * Reads one field and checks that the line has the shape we expect:
 * every field except the last must be followed by a comma, and the last
 * one must be followed by the end of the line.
 */
static int read_field(
    const char **cursor,
    char *output,
    size_t output_size,
    bool is_last
)
{
    char terminator = 0;

    if (read_csv_field(cursor, output, output_size, &terminator) != 0)
        return -1;

    if (is_last)
        return (terminator == '\n' && **cursor == '\0') ? 0 : -1;

    return terminator == ',' ? 0 : -1;
}

/*
 * Converts text to an int. Unlike a bare strtol call this rejects empty
 * text, trailing characters and values outside the range of an int.
 */
static int parse_int(const char *text, int *value)
{
    if (!(isdigit((unsigned char)text[0]) || text[0] == '-' || text[0] == '+'))
        return -1;              /* also rejects the empty string */

    char *end = NULL;

    errno = 0;
    long long number = strtoll(text, &end, 10);

    if (errno == ERANGE || end == text || *end != '\0')
        return -1;

    if (number < INT_MIN || number > INT_MAX)
        return -1;

    *value = (int)number;
    return 0;
}

/* Parses one data line into *m. Returns 0 on success, -1 if malformed. */
static int parse_row(const char *line, Match *m)
{
    char season[CSV_NUMBER_SIZE];
    char home_score[CSV_NUMBER_SIZE];
    char away_score[CSV_NUMBER_SIZE];
    char has_score[CSV_NUMBER_SIZE];
    int flag = 0;

    const char *cursor = line;

    memset(m, 0, sizeof(*m));

    if (read_field(&cursor, m->game_id,    sizeof m->game_id,    false) != 0 ||
        read_field(&cursor, season,        sizeof season,        false) != 0 ||
        read_field(&cursor, m->date,       sizeof m->date,       false) != 0 ||
        read_field(&cursor, m->start_time, sizeof m->start_time, false) != 0 ||
        read_field(&cursor, m->home_team,  sizeof m->home_team,  false) != 0 ||
        read_field(&cursor, m->away_team,  sizeof m->away_team,  false) != 0 ||
        read_field(&cursor, home_score,    sizeof home_score,    false) != 0 ||
        read_field(&cursor, away_score,    sizeof away_score,    false) != 0 ||
        read_field(&cursor, has_score,     sizeof has_score,     false) != 0 ||
        read_field(&cursor, m->status,     sizeof m->status,     true)  != 0)
    {
        return -1;
    }

    if (m->game_id[0] == '\0')
        return -1;

    if (parse_int(season, &m->season) != 0 ||
        parse_int(home_score, &m->home_score) != 0 ||
        parse_int(away_score, &m->away_score) != 0 ||
        parse_int(has_score, &flag) != 0 ||
        (flag != 0 && flag != 1))
    {
        return -1;
    }

    m->has_score = (flag == 1);
    return 0;
}

static bool is_blank_line(const char *line)
{
    return line[0] == '\n' ||
           (line[0] == '\r' && (line[1] == '\n' || line[1] == '\0'));
}

/* Compares the first line of the file with the expected header. */
static bool is_expected_header(const char *line)
{
    /* Spreadsheet programs may put a UTF-8 byte-order mark in front. */
    if ((unsigned char)line[0] == 0xEF &&
        (unsigned char)line[1] == 0xBB &&
        (unsigned char)line[2] == 0xBF)
    {
        line += 3;
    }

    size_t length = strlen(line);

    while (length > 0 && (line[length - 1] == '\n' || line[length - 1] == '\r'))
        length--;

    return length == strlen(CSV_HEADER) &&
           memcmp(line, CSV_HEADER, length) == 0;
}

int load_matches_csv(
    const char *filename,
    Match **matches_out,
    size_t *count_out
)
{
    if (filename == NULL || matches_out == NULL || count_out == NULL)
        return -1;

    *matches_out = NULL;
    *count_out = 0;

    FILE *file = fopen(filename, "r");

    if (file == NULL)
    {
        perror("Could not open CSV file");
        return -1;
    }

    char line[CSV_LINE_SIZE];
    Match *matches = NULL;
    size_t count = 0;
    size_t line_number = 1;

    if (fgets(line, sizeof(line), file) == NULL)
    {
        fprintf(stderr, "CSV error: %s is empty.\n", filename);
        goto fail;
    }

    if (!is_expected_header(line))
    {
        fprintf(stderr, "CSV error: %s does not start with the expected header.\n",
                filename);
        goto fail;
    }

    while (fgets(line, sizeof(line), file) != NULL)
    {
        line_number++;

        size_t length = strlen(line);

        /* No newline and not at end of file: the line did not fit. */
        if (length > 0 && line[length - 1] != '\n' && !feof(file))
        {
            fprintf(stderr, "CSV error: line %zu is too long.\n", line_number);
            goto fail;
        }

        if (is_blank_line(line))
            continue;

        Match m;

        if (parse_row(line, &m) != 0)
        {
            fprintf(stderr, "CSV error: line %zu is malformed.\n", line_number);
            goto fail;
        }

        Match *temporary = realloc(matches, (count + 1) * sizeof(Match));

        if (temporary == NULL)
        {
            fprintf(stderr, "CSV error: out of memory.\n");
            goto fail;
        }

        matches = temporary;
        matches[count++] = m;
    }

    if (ferror(file))
    {
        fprintf(stderr, "CSV error: could not read %s.\n", filename);
        goto fail;
    }

    fclose(file);

    *matches_out = matches;
    *count_out = count;

    return 0;

fail:
    free(matches);
    fclose(file);
    return -1;
}