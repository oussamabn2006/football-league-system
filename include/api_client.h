
#ifndef API_CLIENT_H
#define API_CLIENT_H

/*
 * Sends an authenticated GET request to API Ninjas.
 *
 * endpoint: API endpoint, for example:
 *           "/v1/soccermatches?league=epl&season=2026"
 *
 * response_out: receives the allocated JSON response.
 * http_status: receives the HTTP status code.
 *
 * Returns:
 *  0  = request completed
 * -1  = error
 *
 * The caller must free(response_out) using free().
 */
int api_get(
    const char *endpoint,
    char **response_out,
    long *http_status
);

#endif
