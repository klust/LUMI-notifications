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
#define ERROR_NO_ENV_HOME   4
#define ERROR_OUT_OF_MEMORY 5

const char *config_file = ".LUMI-messenger.ini";
const char *default_message = "Hello from LUMI-messenger";

typedef struct
{
    const char *appkey;
    const char *userkey;
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


int main( int argc, char *argv[] ) {

    // Get the message to print

    const char *message;

    if ( argc == 2 ) {
        message = argv[1];
    } else {
        message = default_message;
    }

    // Read data from the INI file.

    pushover_configuration config;
    config.appkey  = NULL;
    config.userkey = NULL;

    // First compute the full path the home directory of the user.

    char *home_dir = getenv( "HOME" );
    if ( home_dir == NULL ) {
        fprintf( stderr, "Cannot locate the home directory, the environment variable HOME is missing.\n" );
        return ERROR_NO_ENV_HOME;
    }

    const int required_string_length = strlen( home_dir ) + strlen( config_file ) + 2; // +2: slash and terminating null

    char *config_path_file = (char *) malloc( (size_t) (required_string_length * sizeof( char )) );
    if ( config_path_file == NULL ) {
        fprintf( stderr, "Memory allocation failed on line %d of pushover.c.\n", __LINE__ );
        return ERROR_OUT_OF_MEMORY;
    }

    snprintf( config_path_file, required_string_length, "%s/%s", 
              home_dir, config_file );

    // Now read the actual data from the configuration file.

    if ( ini_parse( config_path_file, handler, &config ) < 0 ) {
        fprintf( stderr, "Failed to load the INI file ~/.LUMI-messenger.ini\n" );
        return ERROR_READ_INI_FILE;
    }

    if ( config.appkey == NULL ) {
        fprintf( stderr, "No appkey found for pushover in ~/.LUMI-messenger.ini\n" );
        return ERROR_NO_APPKEY;
    }
    if ( config.userkey == NULL ) {
        fprintf( stderr, "No userkey found for pushover in ~/.LUMI-messenger.ini\n" );
        return ERROR_NO_USERKEY;
    }

    // Debug line, should be removed as it prints the secrets on the screen
    // printf( "DEBUG: Configuration loaded from ~/.LUMI-messenger.ini: appkey = %s, userkey = %s\n",
    //        config.appkey, config.userkey );

    // Now use libcurl to push a message.

    CURL *curl;
    CURLcode res;

    curl_global_init(CURL_GLOBAL_ALL);
    curl = curl_easy_init();

    if( curl ) {
        curl_mime *mime;
        curl_mimepart *part;

        // Initialize the mime structure
        mime = curl_mime_init( curl );

        // API Token
        part = curl_mime_addpart( mime );
        curl_mime_name( part, "token" );
        curl_mime_data( part, config.appkey, CURL_ZERO_TERMINATED );

        // User Key
        part = curl_mime_addpart( mime );
        curl_mime_name( part, "user" );
        curl_mime_data( part, config.userkey, CURL_ZERO_TERMINATED );

        // The Message
        part = curl_mime_addpart( mime );
        curl_mime_name( part, "message" );
        curl_mime_data( part, message, CURL_ZERO_TERMINATED );

        // Set the URL and the mimepost data
        curl_easy_setopt( curl, CURLOPT_URL, "https://api.pushover.net/1/messages.json" );
        curl_easy_setopt( curl, CURLOPT_MIMEPOST, mime );

        // For now we'll send the output to /dev/null as it is only useful for
        // debugging. We can keep it on the screen if we would later add a debug mode.
        FILE *devnull = fopen( "/dev/null", "w" );
        curl_easy_setopt( curl, CURLOPT_WRITEDATA, devnull );

        // Perform the request
        res = curl_easy_perform( curl );

        if( res != CURLE_OK ) {
            fprintf( stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror( res ) );
        } else {
            printf( "Message sent successfully!\n" );
        }

        // Clean up
        curl_easy_cleanup( curl );
        curl_mime_free( mime );
        fclose( devnull );
    }

    curl_global_cleanup();

    // Clean-up the data read by ini

    if ( config.appkey )  free( (void*) config.appkey );
    if ( config.userkey ) free( (void*) config.userkey );
    free( config_path_file );

    return 0;
}

