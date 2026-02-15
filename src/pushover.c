// C code to push messages to the pushover service without exposing the application key
// and user key on the screen or in process logs, etc.
//
// Written by Kurt Lust
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ini.h"
#include <curl/curl.h>

#define ERROR_READ_INI_FILE 1
#define ERROR_NO_APPKEY     2
#define ERROR_NO_USERKEY    3


typedef struct
{
    const char* appkey;
    const char* userkey;
} pushover_configuration;


static int handler( void* user, const char* section, const char* name,
                    const char* value)
{
    pushover_configuration* pconfig = (pushover_configuration*) user;

    #define MATCH(s, n) strcmp(section, s) == 0 && strcmp(name, n) == 0
    if  (MATCH( "pushover", "appkey" ) ) {
        pconfig->appkey = strdup( value );
    } else if ( MATCH( "pushover", "userkey" ) ) {
        pconfig->userkey = strdup( value );
    } else {
        return 0;  /* unknown section/name, error */
    }
    return 1;
}


int main(void) {

    // Read data from the INI file.

    pushover_configuration config;
    config.appkey  = NULL;
    config.userkey = NULL;

    if ( ini_parse( "~/.LUMI-messenger.ini", handler, &config ) < 0 ) {
        printf( "Failed to load the INI file | /.LUMI-messenger.ini" );
        return ERROR_READ_INI_FILE;
    }

    if ( config.appkey == NULL ) {
        printf( "No appkey found for pushover in ~/.LUMI-messenger.ini\n" );
        return ERROR_NO_APPKEY;
    }
    if ( config.userkey == NULL ) {
        printf( "No userkey found for pushover in ~/.LUMI-messenger.ini\n" );
        return ERROR_NO_USERKEY;
    }

    // Debug line, should be removed as it prints the secrets on the screen
    printf( "DEBUG: Configuration loaded from ~/.LUMI-messenger.ini: appkey = %s, userkey = %s\n",
            config.appkey, config.userkey );

    // Now use libcurl to push a message.

    CURL *curl;
    CURLcode res;

    curl_global_init(CURL_GLOBAL_ALL);
    curl = curl_easy_init();

    if(curl) {
        curl_mime *mime;
        curl_mimepart *part;

        // Initialize the mime structure
        mime = curl_mime_init(curl);

        // API Token
        part = curl_mime_addpart(mime);
        curl_mime_name(part, "token");
        curl_mime_data(part, "YOUR_APP_TOKEN", CURL_ZERO_TERMINATED);

        // User Key
        part = curl_mime_addpart(mime);
        curl_mime_name(part, "user");
        curl_mime_data(part, "YOUR_USER_KEY", CURL_ZERO_TERMINATED);

        // The Message
        part = curl_mime_addpart(mime);
        curl_mime_name(part, "message");
        curl_mime_data(part, "Hello from C code!", CURL_ZERO_TERMINATED);

        // Set the URL and the mimepost data
        curl_easy_setopt(curl, CURLOPT_URL, "https://api.pushover.net/1/messages.json");
        curl_easy_setopt(curl, CURLOPT_MIMEPOST, mime);

        // Perform the request
        res = curl_easy_perform(curl);

        if(res != CURLE_OK) {
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        } else {
            printf("Message sent successfully!\n");
        }

        // Clean up
        curl_easy_cleanup(curl);
        curl_mime_free(mime);
    }

    curl_global_cleanup();

    // Clean-up the data read by ini

    if ( config.appkey )  free( (void*) config.appkey );
    if ( config.userkey ) free( (void*) config.userkey );

    return 0;
}

