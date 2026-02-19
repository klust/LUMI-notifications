// C code to push messages to the pushover service without exposing the application key
// and user key on the screen or in process logs, etc.
//
// Written by Kurt Lust
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include "ini.h"
#include <curl/curl.h>

#define ERROR_READ_INI_FILE 1
#define ERROR_NO_APPKEY     2
#define ERROR_NO_USERKEY    3
#define ERROR_NO_ENV_HOME   4
#define ERROR_OUT_OF_MEMORY 5

const char *version = "0.1";
const char *default_config_file = ".LUMI-messenger.ini";
const char *default_message = "Hello from LUMI-messenger";

typedef struct
{
    const char *appkey;
    const char *userkey;
    const char *title;
    const char *message;
    const char *sound;
} pushover_configuration;


//
// Print the help information.
//

void PrintHelp( void ) {

    fprintf( stderr,
        "\n"
        "Command line options for pushover:\n"
        "   -a/--appkey :             Specify the application key for pushover, overwriting\n"
        "                             what is in the configuration file.\n"
        "   -c/--configuration-file : Specify the configuration file to use. The default\n"
        "                             is ~/.LUMI-messenger.ini.\n"
        "   -d/--debug :              Print additional debug info.\n"
        "   -m/--message :            Message to send.\n"
        "   -s/--sound :              Specify the sound to use in pushover from the list of\n"
        "                             pushover-supported sounds."
        "   -t/--title :              Specify a title for the message (e.g., Alert)"
        "   -u/--userkey :            Specify the user key for pushover, overwriting what\n"
        "                             is in the configuration file.\n"
        "   -v/--version :            Print the version of pushover.\n"
        "\n"
        "Sounds known by pushover:\n"
        "Name          Description         Name          Description\n"
        "pushover      Default tone        mechanical    Mechanical click\n"
        "bike          Bicycle bell        pianobar      Piano notes\n"
        "bugle         Bugle call          siren         Emergency siren\n"
        "cashregister  Cha-ching           spacealarm    Sci-fi alarm\n"
        "cosmic        Deep space sound    tugboat       Boat horn\n"
        "falling       Descending whistle  alien         Alien beep\n"
        "gamelan       Percussive chime    climb         Rising scale\n"
        "incoming      Mail has arrived    none          Silent\n"
        "\n"
    );

    return;

}  // End PrintHelp

//
// Handler for the ini tool that we use.
//

static int handler( void* user, const char* section, const char* name,
                    const char* value)
{
    pushover_configuration* pconfig = (pushover_configuration*) user;

    #define MATCH(s, n) strcmp(section, s) == 0 && strcmp(name, n) == 0
    if  (MATCH( "pushover", "appkey" ) ) {
        pconfig->appkey = strdup( value );
    } else if ( MATCH( "pushover", "userkey" ) ) {
        pconfig->userkey = strdup( value );
    } else if ( MATCH( "pushover", "title" ) ) {
        pconfig->title = strdup( value );
    } else if ( MATCH( "pushover", "message" ) ) {
        pconfig->message = strdup( value );
    } else if ( MATCH( "pushover", "sound" ) ) {
        pconfig->sound = strdup( value );
    } else {
        return 0;  /* unknown section/name, error */
    }
    return 1;
}


int main( int argc, char *argv[] ) {

    // Get the message to print

    const char *arg_appkey = (char *) NULL;
    const char *arg_userkey = (char *) NULL;
    const char *arg_config_file = (char *) NULL;
    const char *arg_title = (char *) NULL;
    const char *arg_message = (char *) NULL;
    const char *arg_sound = (char *) NULL;
    int debug = 0;
    int verbose = 0;

    if ( argc == 2 ) {
        arg_message = argv[1];
    }




    // Read data from the INI file.

    pushover_configuration ini_config;
    ini_config.appkey  = NULL;
    ini_config.userkey = NULL;
    ini_config.title   = NULL;
    ini_config.message = NULL;
    ini_config.sound   = NULL;
    char *config_path_file = NULL;

    // First compute the full path the home directory of the user.

    if ( arg_config_file == NULL ) { // No -c argument given, so the default is used.

        char *home_dir = getenv( "HOME" );
        if ( home_dir == NULL ) {
            fprintf( stderr, "Cannot locate the home directory, the environment variable HOME is missing.\n" );
            return ERROR_NO_ENV_HOME;
        }

        const int required_string_length = strlen( home_dir ) + strlen( default_config_file ) + 2; // +2: slash and terminating null

        config_path_file = (char *) malloc( (size_t) (required_string_length * sizeof( char )) );
        if ( config_path_file == NULL ) {
            fprintf( stderr, "Memory allocation failed on line %d of pushover.c.\n", __LINE__ );
            return ERROR_OUT_OF_MEMORY;
        }

        snprintf( config_path_file, required_string_length, "%s/%s", 
                  home_dir, default_config_file );

    }    


    // Now read the actual data from the configuration file.

    if ( ini_parse( config_path_file, handler, &ini_config ) < 0 ) {
        fprintf( stderr, "Failed to load the INI file %s\n", config_path_file );
        return ERROR_READ_INI_FILE;
    }

    pushover_configuration config; // Actual configuration to use to send the message

    // Determine the actual appkey

    if ( arg_appkey != NULL ) {
        config.appkey = arg_appkey;
    } else if ( ini_config.appkey != NULL ) {
        config.appkey = ini_config.appkey;
    } else {
        fprintf( stderr, "No appkey found for pushover in ~/.LUMI-messenger.ini\n" );
        return ERROR_NO_APPKEY;
    }

    // Determine the actual userkey

    if ( arg_userkey != NULL ) {
        config.userkey = arg_userkey;
    } else if ( ini_config.userkey != NULL ) {
        config.userkey = ini_config.userkey;
    } else {
        fprintf( stderr, "No userkey found for pushover in ~/.LUMI-messenger.ini\n" );
        return ERROR_NO_USERKEY;
    }
   
    // Determine the title (if any)

    if ( arg_title != NULL ) {
        config.title = arg_title;
    } else if ( ini_config.title != NULL ) {
        config.title = ini_config.title;
    } else {
        config.title = NULL;
    }
   
    // Determine the message

    if ( arg_message != NULL ) {
        config.message = arg_message;
    } else if ( ini_config.message != NULL ) {
        config.message = ini_config.message;
    } else {
        config.message = default_message;
    }

    // Determine the sound (if any)

    if ( arg_sound != NULL ) {
        config.sound = arg_sound;
    } else if ( ini_config.sound != NULL ) {
        config.sound = ini_config.sound;
    } else {
        config.sound = NULL;
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

        // The title (if any)
        if ( config.title != NULL ) {
            part = curl_mime_addpart( mime );
            curl_mime_name( part, "title" );
            curl_mime_data( part, config.title, CURL_ZERO_TERMINATED );
        }

        // The Message
        part = curl_mime_addpart( mime );
        curl_mime_name( part, "message" );
        curl_mime_data( part, config.message, CURL_ZERO_TERMINATED );

        // The sound (if specified)
        if ( config.sound != NULL ) {
            part = curl_mime_addpart( mime );
            curl_mime_name( part, "sound" );
            curl_mime_data( part, config.sound, CURL_ZERO_TERMINATED );
        }

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

