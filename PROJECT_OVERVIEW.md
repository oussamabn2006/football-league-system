# PROJECT OVERVIEW & TECHNICAL HANDOFF
# Football Analytics & Match Outcome Prediction Platform (C11)

**Project type:** University group project  
**Language:** C11  
**Project vision:** A SofaScore-inspired football information, statistics, analytics, and prediction platform.

---

# 1. Project Vision and Objectives

The objective is to develop a modular football platform inspired by applications such as SofaScore.

The long-term goal is to provide a unified application where users can explore football competitions, browse matches, follow team performance, consult statistics and standings, analyze historical results, and obtain machine-learning-based match predictions.

The application is initially focused on the English Premier League (EPL), with the possibility of expanding to other competitions such as La Liga.

The project is being developed incrementally. The current interface is command-line based, but the architecture should remain modular enough to support future interface improvements.

## Main functional areas

1. **Football data:** Retrieve, parse, validate, and store match information.
2. **Match center:** Browse fixtures, completed results, and match information.
3. **Team analytics:** Search teams and analyze their performance, including home/away records and recent form.
4. **Competition management:** Calculate league standings and organize season data.
5. **Predictive analytics:** Analyze historical performance and predict match outcomes.
6. **Application interface:** Provide a clear way to explore the available football information.

## Important scope limitation

The current API Ninjas free plan does not provide full live-score information. The existing implementation works with downloaded season data. Do not claim that live match tracking, live match events, or real-time statistics have been implemented.

---

# 2. Technology Stack and Development Environment

| Component | Technology |
|---|---|
| Programming language | C11 |
| C standard | Strict C11 |
| Build system | CMake |
| Build generator | Ninja |
| HTTP requests | libcurl |
| JSON parsing | cJSON |
| Data storage | CSV |
| Football data provider | API Ninjas |
| Automated tests | CTest |
| Development environment | Windows, MSYS2 UCRT64 |
| Additional verification | Linux with AddressSanitizer and UBSan |

Compiler configuration:
- `CMAKE_C_STANDARD 11`
- `CMAKE_C_STANDARD_REQUIRED ON`
- `CMAKE_C_EXTENSIONS OFF`

Code should compile cleanly with:

`-Wall -Wextra -Wpedantic`

---

# 3. Project Architecture and Directory Structure

Current project structure:

```text
football-league-system/
│
├── CMakeLists.txt
├── README.md
├── PROJECT_OVERVIEW.md
├── .gitignore
├── .env
│
├── data/
│   └── matches_epl_2026.csv
│
├── include/
│   ├── api_client.h
│   ├── match.h
│   ├── match_parser.h
│   ├── match_service.h
│   ├── standings.h
│   ├── match_validation.h
│   ├── match_storage.h
│   ├── cli.h
│   ├── app.h
│   ├── match_display.h
│   └── match_query.h
│
├── src/
│   ├── main.c
│   ├── api_test.c
│   ├── api_client.c
│   ├── match_parser.c
│   ├── match_service.c
│   ├── standings.c
│   ├── match_validation.c
│   ├── match_storage.c
│   ├── storage_test.c
│   ├── app.c
│   ├── cli.c
│   ├── match_display.c
│   └── match_query.c
│
├── tests/
│   └── ...
│
└── build/
```

The exact test-file paths should be checked in the repository. The build directory is generated and must not be committed.

## Architectural style

The project follows a **modular, procedural C architecture**, with responsibilities separated into modules.

The intention is to avoid placing the entire application inside `main.c` or mixing API communication, business logic, storage, and display code.

The general dependency flow is:

```text
              main.c
                 |
                 v
               app.c
                 |
          +------+------+
          |             |
         cli.c      Application logic
                        |
       +----------------+----------------+
       |                |                |
       v                v                v
  match_service.c   match_query.c   standings.c
       |                |                |
       v                v                |
  api_client.c          |                |
       |                |                |
       v                v                v
  match_parser.c   match_validation.c
       |                |
       +----------------+
                |
                v
          match_storage.c
                |
                v
              CSV
```

This diagram represents the logical responsibilities, not necessarily every direct function call.

---

# 4. Core Data Model and Storage Format

## Match structure

Defined in `include/match.h`:

```c
typedef struct {
    char game_id[64];
    int season;
    char date[16];
    char start_time[32];
    char home_team[100];
    char away_team[100];
    int home_score;
    int away_score;
    bool has_score;
    char status[32];
} Match;
```

Important design decision:

`has_score` distinguishes an unplayed match from a completed 0–0 draw.

A match is considered finished only when:

```c
match.has_score && strcmp(match.status, "final") == 0
```

Do not determine whether a match is finished simply by checking whether its scores are zero.

## CSV format

File:

`data/matches_epl_2026.csv`

Header:

```csv
game_id,season,date,start_time,home_team,away_team,home_score,away_score,has_score,status
```

Example:

```csv
g1,2026,2026-08-02,15:00,Arsenal,Chelsea,1,1,1,final
```

CSV rules:
- `has_score` is exactly `1` or `0`.
- Fields containing commas or quotes are quoted and escaped.
- UTF-8 BOM is accepted on the header.
- The loader rejects malformed headers and records.
- Numeric overflow, unterminated quotes, extra fields, junk after closing quotes, and overlong fields are rejected.
- Fields containing line breaks are not accepted when saving.

Do not change the CSV header or `Match` structure without updating the parser, storage, validation, and related tests.

---

# 5. Implemented Modules and Their Responsibilities

## 5.1 API client — `api_client.c`

Files:
- `include/api_client.h`
- `src/api_client.c`

Responsibilities:
- Perform HTTP GET requests using libcurl.
- Communicate with API Ninjas.
- Add the `X-Api-Key` header.
- Read the API key from the environment or `.env`.
- Dynamically allocate the response buffer.
- Return the HTTP status and response body.

Main API:

```c
int api_get(
    const char *endpoint,
    char **response_out,
    long *http_status
);
```

Return convention:
- `0`: success.
- `-1`: error.

The caller owns and frees the allocated response.

## 5.2 Match parser — `match_parser.c`

Files:
- `include/match_parser.h`
- `src/match_parser.c`

Responsibilities:
- Parse API Ninjas JSON responses using cJSON.
- Convert JSON match objects into `Match` structures.
- Extract match IDs, season, date, kickoff time, teams, scores, and status.
- Distinguish completed matches from matches without scores.

Main API:

```c
int parse_matches(
    const char *json,
    Match **matches_out,
    size_t *count_out
);
```

The returned array is dynamically allocated and must be freed by the caller.

## 5.3 Match service — `match_service.c`

Files:
- `include/match_service.h`
- `src/match_service.c`

Responsibilities:
- Fetch an entire season.
- Handle API pagination.
- Request pages of 50 matches.
- Combine the results into one dynamically allocated array.
- Handle request and allocation failures.

Main API:

```c
int fetch_season_matches(
    const char *league,
    int season,
    Match **matches_out,
    size_t *count_out
);
```

The EPL 2026 fetch was verified to return 380 matches over multiple pages.

The implementation also includes retry handling for eligible failures. Check the current source before modifying retry behavior.

## 5.4 Match validation — `match_validation.c`

Files:
- `include/match_validation.h`
- `src/match_validation.c`

Responsibilities:
- Validate match fields.
- Validate dates in strict `YYYY-MM-DD` format.
- Validate teams, scores, season, and status.
- Reject malformed records.
- Remove duplicate matches based on `game_id`.

Main API:

```c
typedef struct {
    size_t valid_count;
    size_t invalid_count;
    size_t duplicate_count;
} ValidationReport;

int validate_and_deduplicate_matches(
    Match matches[],
    size_t *count,
    ValidationReport *report
);
```

Validation is performed in place. Accepted matches are compacted into the original array.

## 5.5 CSV storage — `match_storage.c`

Files:
- `include/match_storage.h`
- `src/match_storage.c`

Responsibilities:
- Save matches to CSV.
- Load matches from CSV.
- Handle quoted fields and escaped quotes.
- Reject malformed input and invalid numeric values.
- Preserve the distinction between unplayed matches and 0–0 results.
- Avoid replacing an existing file when the input data is invalid.

Main APIs:

```c
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
```

## 5.6 Standings — `standings.c`

Files:
- `include/standings.h`
- `src/standings.c`

Responsibilities:
- Calculate league standings from completed matches.
- Count played matches, wins, draws, losses, goals for, and goals against.
- Calculate goal difference and points.
- Sort teams by the implemented tie-breakers.

Points:
- Win: 3 points.
- Draw: 1 point.
- Loss: 0 points.

Implemented sorting:
1. Points.
2. Goal difference.
3. Goals scored.
4. Wins.

Main APIs:

```c
int calculate_standings(
    const Match matches[],
    size_t match_count,
    TeamStanding **standings_out,
    size_t *team_count_out
);

void display_standings(
    const TeamStanding standings[],
    size_t team_count
);
```

Note: This is the implemented simplified tie-break system, not a complete implementation of every official EPL tie-break rule.

## 5.7 Application and CLI — `app.c`, `cli.c`, `main.c`

Responsibilities:

**main.c**
- Application entry point.
- Parse command-line arguments.
- Load data and start the application.

**cli.c**
- Command-line option parsing.
- Menu input handling.
- Strict menu-number parsing.

**app.c**
- Application state.
- Application workflow.
- Fetching and loading data.
- Saving data.
- Main menu and sub-menu coordination.

Supported options:

```text
--fetch
--load FILE
--no-menu
--help
```

Main menu:

```text
1. Fetch matches from the API
2. Load matches from a CSV file
3. Browse matches and teams
4. Display standings
5. Save matches to a CSV file
6. Exit
```

Safety decisions:
- Failed or empty downloads must not replace valid existing data.
- Invalid CSV files must not replace valid in-memory data.
- Fetch and overwrite operations require confirmation.
- Saving failures must not destroy the current application state.
- The data directory is created if necessary.

## 5.8 Match queries and team analytics — `match_query.c`

Files:
- `include/match_query.h`
- `src/match_query.c`

Responsibilities:
- Search for teams.
- List available teams.
- Select matches belonging to a team.
- Select finished or upcoming matches.
- Sort matches chronologically.
- Generate team performance summaries.

Current browsing sub-menu:

```text
1. All matches
2. Matches of one team
3. Next 10 upcoming matches
4. Latest 10 results
5. Team summary
6. Back
```

Team search:
- Case-insensitive substring matching.
- Exact team name takes priority.
- Ambiguous matches display candidates.
- Unknown searches display available teams.

Team summary:
- Overall record.
- Home record.
- Away record.
- Played, wins, draws, losses.
- Goals for, goals against, goal difference, points.
- Last five results.
- Current winning/drawing/losing run.

The form is displayed chronologically from oldest to newest.

These statistics are also intended to provide a foundation for future machine-learning features.

---

# 6. Coding Style and Engineering Conventions

Future contributions must preserve the existing coding style and architecture.

## 6.1 General style

- Use standard C11.
- Prefer simple, readable procedural C.
- Keep modules focused on one responsibility.
- Use headers for public declarations and source files for implementations.
- Use descriptive function and variable names.
- Use `snake_case` for functions and variables.
- Use uppercase names for macros and constants.
- Use `typedef struct` for application data structures where appropriate.
- Avoid unnecessary global variables.
- Avoid unnecessary complexity and overengineering.

## 6.2 Function conventions

Most functions use:

```c
0   /* success */
-1  /* error */
```

For dynamically allocated outputs:
- Initialize output pointers to `NULL`.
- Initialize output counts to `0`.
- Validate arguments before using them.
- Document ownership clearly.
- The caller frees returned allocations.

Example:

```c
int some_function(
    const char *input,
    Match **matches_out,
    size_t *count_out
) {
    if (matches_out == NULL || count_out == NULL) {
        return -1;
    }

    *matches_out = NULL;
    *count_out = 0;

    /* Implementation */

    return 0;
}
```

This is a convention to follow when it fits the function's API; inspect existing functions before changing them.

## 6.3 Memory management

- Check `malloc`, `calloc`, and `realloc` results.
- Avoid memory leaks.
- Free dynamically allocated memory on error paths.
- Do not use pointers after freeing them.
- Check allocation-size arithmetic for overflow.
- Clearly define which function owns an allocation.

## 6.4 Input and file safety

- Validate user input.
- Do not trust external API data.
- Check file operations.
- Avoid silently accepting malformed CSV data.
- Do not overwrite valid data with failed or invalid input.

## 6.5 Testing conventions

- Add tests for new functionality.
- Avoid unnecessary network requests in unit tests.
- Use deterministic test data.
- Check edge cases and error paths.
- Keep all existing tests passing.
- Use AddressSanitizer and UBSan where available.

## 6.6 Security

- Never commit `.env`.
- Never hardcode API keys.
- Never include credentials in logs, test output, or documentation.
- Keep generated build files out of Git.

---

# 7. Testing and Verification Status

Tests are run using:

```bash
ctest --test-dir build --output-on-failure
```

Latest verified Windows/MSYS2 result:

```text
Test #1: app          Passed
Test #2: storage      Passed
Test #3: validation   Passed
Test #4: standings    Passed
Test #5: query        Passed

100% tests passed, 0 tests failed out of 5
```

**Important:** Five tests are currently registered in the verified CTest configuration. Do not report six tests or claim that a service test is registered until its target is added and verified.

Other previously verified behavior:
- API season fetch returned 380 EPL matches.
- CSV storage tests passed 3,971 checks.
- A saved CSV was reloaded and re-saved byte-identically.
- Standings calculations were checked for internal consistency.
- Team browsing and team summaries were manually exercised.

A test passing on another machine or under another LLM's reported execution is not automatically proof that it passed on the current development machine. Always verify the actual build and test output.

---

# 8. Development Roadmap

| Step | Feature | Status |
|---|---|---|
| 1 | API integration and data pipeline | Implemented |
| 2 | Application modes and CLI menu | Implemented |
| 3 | Team search and match browsing | Implemented |
| 4 | Historical multi-season dataset | Next |
| 5 | Leakage-free feature engineering | Planned |
| 6 | Multiclass logistic regression | Planned |
| 7 | Prediction integration into application | Planned |
| 8 | Additional football analytics and statistics | Future |
| 9 | Interface improvements and documentation | Future |

## Machine-learning design decisions

The prediction task has three classes:

- Home Win.
- Draw.
- Away Win.

Planned model:
- Multiclass logistic regression.
- Softmax function.
- Gradient descent.
- L2 regularization.

Evaluation:
- Chronological train/test split.
- Accuracy.
- Macro-F1.
- Per-class precision and recall.
- Confusion matrix.
- Baselines, including always predicting a home win.

### Critical data-leakage rule

For a match being predicted, every feature must be calculated using information available strictly before that match.

For example, a team's recent form must not include the result of the match being predicted or any future match.

Use chronological evaluation rather than random splitting.

### Open academic decision

Ask the supervisor whether the model must be trained in C or whether training in Python followed by C inference is acceptable.

This decision affects the model implementation and integration architecture.

---

# 9. Build, Run, and Configuration

Run commands from the project root using MSYS2 UCRT64:

```bash
cmake -S . -B build -G Ninja

cmake --build build

ctest --test-dir build --output-on-failure
```

Run the application:

```bash
./build/football_system
```

Load a CSV:

```bash
./build/football_system --load data/matches_epl_2026.csv
```

Fetch a season:

```bash
./build/football_system --fetch
```

Print standings without opening the menu:

```bash
./build/football_system --no-menu --load data/matches_epl_2026.csv
```

The API key must be configured locally and must never be committed.

---

# 10. Instructions for Teammates and AI Assistants

Before making changes:

1. Read this file completely.
2. Inspect the actual repository and current source code.
3. Check the existing CMake configuration.
4. Understand the relevant module APIs.
5. Run the existing test suite.
6. Identify which functionality is implemented versus planned.

When implementing a feature:

1. Follow the existing modular architecture.
2. Avoid unnecessary changes to unrelated modules.
3. Preserve the `Match` structure and CSV format unless a change is necessary and coordinated.
4. Add or update automated tests.
5. Compile with strict C11 warnings.
6. Run CTest.
7. Report exactly which files were modified and which commands were executed.
8. Update this overview when architecture, implementation status, or project decisions change.

Do not:
- Rewrite working modules without a clear reason.
- Claim that an unimplemented feature exists.
- Claim full live-score functionality.
- Introduce random train/test splitting for match prediction.
- Use future match information in prediction features.
- Remove existing tests simply to make the build pass.
- Assume another developer's or AI assistant's test claims are verified.

---

# 11. Ready-to-Paste Context for Another AI Assistant

Project: Football Analytics & Match Outcome Prediction Platform, inspired by SofaScore, written in strict C11.

The long-term objective is a football platform with match information, competition standings, team analytics, historical analysis, and three-class machine-learning match predictions.

Technology:
- C11, CMake, Ninja.
- libcurl for HTTP.
- cJSON for JSON parsing.
- API Ninjas for football match data.
- CSV storage.
- CTest.

Environment:
- Windows MSYS2 UCRT64.
- Linux sanitizer testing has also been used.

Current implementation:
- API client with HTTP requests.
- JSON-to-Match parser.
- Season pagination service.
- Match validation and duplicate removal.
- Strict CSV save/load.
- League standings.
- CLI and interactive application.
- Team search and match browsing.
- Team summaries, home/away records, recent form, and streaks.

Current verified CTest status: 5/5 passing on Windows/MSYS2.

Core Match structure is defined in `include/match.h`.

A finished match means:

`has_score && status == "final"`

CSV header:

`game_id,season,date,start_time,home_team,away_team,home_score,away_score,has_score,status`

Architecture:
- Modular procedural C.
- `main.c` is the entry point.
- `app.c` handles application workflows.
- `cli.c` handles command-line and menu input.
- API, parsing, validation, storage, standings, and querying are separated into modules.

Engineering rules:
- Strict C11.
- `-Wall -Wextra -Wpedantic`.
- Functions generally return 0 on success and -1 on error.
- Caller frees allocated outputs.
- Check allocation failures and error paths.
- Do not break the CSV format or Match structure without updating dependent modules and tests.
- Never commit API keys or `.env`.

Next development stage:
1. Build a reliable historical multi-season dataset.
2. Engineer pre-match features without data leakage.
3. Implement multiclass logistic regression.
4. Evaluate using chronological splitting, baselines, accuracy, macro-F1, and a confusion matrix.
5. Integrate predictions into the application.

Important:
- Full live-score functionality is not available through the current free API plan.
- Verify the actual source code and build before making assumptions.
- Preserve the existing architecture and tests.
- Read `PROJECT_OVERVIEW.md` before modifying the project.