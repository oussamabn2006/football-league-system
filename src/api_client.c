#include "api_client.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <curl/curl.h>

#define API_BASE_URL "https://api.api-ninjas.com"
#define API_KEY_MAX 512
#define URL_MAX 2048

/* Structure used to store the HTTP response. */
typedef struct
{
    char *data;
    size_t size;
} ResponseBuffer;


/* Remove spaces at the beginning and end of a string. */
static char *trim(char *str)
{
    while (isspace((unsigned char)*str))
    {
        str++;
    }

    char *end = str + strlen(str);

    while (end > str && isspace((unsigned char)*(end - 1)))
    {
        end--;
    }

    *end = '\0';

    return str;
}


/* Load API key from environment variable or .env file. */
static const char *load_api_key(void)
{
    static char api_key[API_KEY_MAX];

    /* First, check the environment variable. */
    const char *env_key = getenv("API_NINJAS_KEY");

    if (env_key != NULL && env_key[0] != '\0')
    {
        if (strlen(env_key) >= API_KEY_MAX)
        {
            return NULL;
        }

        snprintf(api_key, sizeof(api_key), "%s", env_key);
        return api_key;
    }

    /* If not found, read the .env file. */
    FILE *file = fopen(".env", "r");

    if (file == NULL)
    {
        return NULL;
    }

    char line[1024];

    while (fgets(line, sizeof(line), file) != NULL)
    {
        char *content = trim(line);

        /* Ignore empty lines and comments. */
        if (*content == '\0' || *content == '#')
        {
            continue;
        }

        char *equals = strchr(content, '=');

        if (equals == NULL)
        {
            continue;
        }

        *equals = '\0';

        char *name = trim(content);
        char *value = trim(equals + 1);

        if (strcmp(name, "API_NINJAS_KEY") == 0)
        {
            /* Remove optional surrounding quotes. */
            size_t len = strlen(value);

            if (len >= 2 &&
                ((value[0] == '"' && value[len - 1] == '"') ||
                 (value[0] == '\'' && value[len - 1] == '\'')))
            {
                value[len - 1] = '\0';
                value++;
            }

            if (strlen(value) > 0 &&
                strlen(value) < sizeof(api_key))
            {
                snprintf(api_key, sizeof(api_key), "%s", value);

                fclose(file);
                return api_key;
            }
        }
    }

    fclose(file);
    return NULL;
}


/* libcurl calls this function when response data arrives. */
static size_t write_callback(
    void *contents,
    size_t size,
    size_t nmemb,
    void *user_data
)
{
    ResponseBuffer *buffer = (ResponseBuffer *)user_data;

    if (size != 0 && nmemb > (size_t)-1 / size)
    {
        return 0;
    }

    size_t received = size * nmemb;

    if (received > (size_t)-1 - buffer->size - 1)
    {
        return 0;
    }

    char *new_data = realloc(
        buffer->data,
        buffer->size + received + 1
    );

    if (new_data == NULL)
    {
        return 0;
    }

    buffer->data = new_data;

    memcpy(
        buffer->data + buffer->size,
        contents,
        received
    );

    buffer->size += received;
    buffer->data[buffer->size] = '\0';

    return received;
}


/* Perform an authenticated GET request. */
int api_get(
    const char *endpoint,
    char **response_out,
    long *http_status
)
{
    if (endpoint == NULL ||
        response_out == NULL ||
        http_status == NULL)
    {
        return -1;
    }

    *response_out = NULL;
    *http_status = 0;

    const char *api_key = load_api_key();

    if (api_key == NULL)
    {
        fprintf(stderr,
            "Error: API key not found. Check your .env file.\n");

        return -1;
    }

    char url[URL_MAX];

    int url_length = snprintf(
        url,
        sizeof(url),
        "%s%s",
        API_BASE_URL,
        endpoint
    );

    if (url_length < 0 || (size_t)url_length >= sizeof(url))
    {
        fprintf(stderr, "Error: URL is too long.\n");
        return -1;
    }

    CURL *curl = curl_easy_init();

    if (curl == NULL)
    {
        fprintf(stderr, "Error: Could not initialize CURL.\n");
        return -1;
    }

    /* Prepare the authentication header. */
    char auth_header[API_KEY_MAX + 32];

    snprintf(
    auth_header,
    sizeof(auth_header),
    "X-Api-Key: %s",
    api_key
);

    struct curl_slist *headers = NULL;

    headers = curl_slist_append(headers, auth_header);

    if (headers == NULL)
    {
        curl_easy_cleanup(curl);
        return -1;
    }

    /* Initialize the response buffer. */
    ResponseBuffer buffer;

    buffer.data = malloc(1);
    buffer.size = 0;

    if (buffer.data == NULL)
    {
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        return -1;
    }

    buffer.data[0] = '\0';

    /* Configure the HTTP request. */
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);

    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);

    /* Execute the request. */
    CURLcode result = curl_easy_perform(curl);

    if (result != CURLE_OK)
    {
        fprintf(stderr, "HTTP request failed: %s\n",
            curl_easy_strerror(result));

        free(buffer.data);
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);

        return -1;
    }

    curl_easy_getinfo(
        curl,
        CURLINFO_RESPONSE_CODE,
        http_status
    );

    /* Return response memory to the caller. */
    *response_out = buffer.data;

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    return 0;
}