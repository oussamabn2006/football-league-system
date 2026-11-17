#include <stdio.h>
#include <curl/curl.h>

int main(void)
{
    // Initialize libcurl
    curl_global_init(CURL_GLOBAL_DEFAULT);

    // Create a CURL handle
    CURL *curl = curl_easy_init();

    if (curl == NULL)
    {
        printf("Failed to initialize CURL.\n");
        curl_global_cleanup();
        return 1;
    }

    // Configure the URL
    curl_easy_setopt(curl, CURLOPT_URL, "https://httpbin.org/get");

    // Display the HTTP response in the terminal
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, NULL);

    // Perform the HTTP request
    CURLcode result = curl_easy_perform(curl);

    if (result != CURLE_OK)
    {
        printf("Request failed: %s\n", curl_easy_strerror(result));
    }

    // Release resources
    curl_easy_cleanup(curl);
    curl_global_cleanup();

    return (result == CURLE_OK) ? 0 : 1;
}