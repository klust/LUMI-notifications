// C code to push messages to the pushover service without exposing the application key
// and user key on the screen or in process logs, etc.
//
// Written by Kurt Lust
//

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include "ini.h"
#include <curl/curl.h>

#define ERROR_READ_INI_FILE  1
#define ERROR_NO_APPKEY      2
#define ERROR_NO_USERKEY     3
#define ERROR_NO_ENV_HOME    4
#define ERROR_OUT_OF_MEMORY  5
#define ERROR_WRONG_ARGUMENT 6

const char *version = "0.1";
const char *default_config_file = ".LUMI-notifications.ini";
const char *default_message = "Hello from pushover in LUMI-notifications";

typedef struct
{
    const char *apitoken;
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
        "   -a/--api-token              Specify the API token for pushover, overwriting what\n"
        "                               is in the configuration file.\n"
        "   -c/--configuration-file     Specify the configuration file to use. The default\n"
        "                               is ~/.LUMI-notifications.ini.\n"
        "   -d/--debug                  Print additional debug info.\n"
        "   -h/--help                   Print this help information and quit\n"
        "   -m/--message                Message to send.\n"
        "   -n/--no-configuration-file  Do not read any configuration file, use command \n"
        "                               line arguments instead.\n"
        "   -s/--sound                  Specify the sound to use in pushover from the list of\n"
        "                               pushover-supported sounds.\n"
        "   -t/--title                  Specify a title for the message (e.g., Alert)\n"
        "   -u/--user-key               Specify the user key for pushover, overwriting what\n"
        "                               is in the configuration file.\n"
        "   -v/--version                Print the version of pushover.\n"
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
        "Default configuration file: ~/%s\n\n",
        default_config_file
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
    if  (MATCH( "pushover", "api-token" ) ) {
        pconfig->apitoken = strdup( value );
    } else if ( MATCH( "pushover", "user-key" ) ) {
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

    //
    // Get the command line arguments
    //

    const char *arg_apitoken = (char *) NULL;
    const char *arg_userkey = (char *) NULL;
    const char *arg_config_file = (char *) NULL;
    const char *arg_title = (char *) NULL;
    const char *arg_message = (char *) NULL;
    const char *arg_sound = (char *) NULL;
    int arg_debug = 0;
    int arg_no_configuration_file = 0;

    // Define the long options
    // { "name", has_arg, *flag, val }
    static struct option long_options[] = {
        {"api-token",             required_argument, 0, 'a'},
        {"configuration-file",    required_argument, 0, 'c'},
        {"debug",                 no_argument,       0, 'd'},
        {"help",                  no_argument,       0, 'h'},
        {"message",               required_argument, 0, 'm'},
        {"no-configuration-file", no_argument,       0, 'n'},
        {"sound",                 required_argument, 0, 's'},
        {"title",                 required_argument, 0, 't'},
        {"user-key",              required_argument, 0, 'u'},
        {"version",               no_argument,       0, 'v'},
        {0,                       0,                 0, 0} // Required end of array
    };

    int option_index = 0;
    int opt;
    while ( ( opt = getopt_long( argc, argv, "a:c:dhm:ns:t:u:v", long_options, &option_index) ) != -1 ) {
        switch (opt) {
            case 'a':
                arg_apitoken = optarg;
                break;
            case 'c': 
                arg_config_file = optarg;
                arg_no_configuration_file = 0;
                break;
            case 'd':
                arg_debug = 1;
                break;
            case 'h': 
                PrintHelp();
                return 0;
            case 'm':
                arg_message = optarg;
                break;
            case 'n':
                arg_no_configuration_file = 1;
                break;
            case 's':
                arg_sound = optarg;
                break;
            case 't':
                arg_title = optarg;
                break;
            case 'u':
                arg_userkey = optarg;
                break;
            case 'v':
                printf( "pushover version %s", version );
                return 0;
            case '?':
                if ( (optopt == 'a') || (optopt == 'c') || (optopt == 's')  || (optopt == 't') || (optopt == 'u') )
                    fprintf( stderr, "Option -%c requires an argument.\n", optopt) ;
                else if ( isprint( optopt ) )
                    fprintf( stderr, "Unknown option `-%c'.\n", optopt );
                else
                    fprintf( stderr, "Unknown option character `\\x%x'.\n", optopt);
                return ERROR_WRONG_ARGUMENT;
        } // End switch ( opt )
    } // End while

    if ( arg_message == NULL ) {
        // No message specified with -m
        if ( optind == (argc - 1) ) {
            arg_message = argv[optind];
        } else if ( optind < (argc - 1) ) {
            fprintf( stderr, "Too many arguments that may be a message." );
            return ERROR_WRONG_ARGUMENT;
        }
    } else {
        // Message already specified with -m
        if ( optind < argc ) {
            fprintf( stderr, "Too many arguments that may be a message." );
            return ERROR_WRONG_ARGUMENT;
        }
    }

    if ( arg_debug != 0 ) {
        printf( "\nValues after processing the argument list:\n" );
        if ( arg_apitoken != NULL )    printf( "- API token from argument list: %s\n",   arg_apitoken );
        if ( arg_userkey != NULL )     printf( "- user key from argument list: %s\n",    arg_userkey );
        if ( arg_title != NULL )       printf( "- title from argument list: %s\n",       arg_title );
        if ( arg_message != NULL )     printf( "- message from argument list: %s\n",     arg_message );
        if ( arg_sound != NULL )       printf( "- sound from argument list: %s\n",       arg_sound );
        if ( arg_config_file != NULL ) printf( "- config file from argument list: %s\n", arg_config_file );
        printf( "\n" );
    }


    // Read data from the INI file.

    pushover_configuration ini_config;
    ini_config.apitoken  = NULL;
    ini_config.userkey = NULL;
    ini_config.title   = NULL;
    ini_config.message = NULL;
    ini_config.sound   = NULL;
    char *config_path_file = NULL;

    if ( arg_no_configuration_file == 0 ) {

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

        } else
            config_path_file = (char *) arg_config_file;

        // Now read the actual data from the configuration file.

        if ( ini_parse( config_path_file, handler, &ini_config ) < 0 ) {
            fprintf( stderr, "Failed to load the INI file %s\n", config_path_file );
            return ERROR_READ_INI_FILE;
        }

        if ( arg_debug != 0 ) {
            printf( "Values read from the configuration file %s:\n", config_path_file );
            if ( ini_config.apitoken != NULL ) printf( "- API token from argument list: %s\n", ini_config.apitoken );
            if ( ini_config.userkey != NULL )  printf( "- user key from argument list: %s\n",  ini_config.userkey );
            if ( ini_config.title != NULL )    printf( "- title from argument list: %s\n",     ini_config.title );
            if ( ini_config.message != NULL )  printf( "- message from argument list: %s\n",   ini_config.message );
            if ( ini_config.sound != NULL )    printf( "- sound from argument list: %s\n",     ini_config.sound );
            printf( "\n" );
        }

    } else if ( arg_debug != 0 )
        printf( "No configuration file read.\n\n" );

    pushover_configuration config; // Actual configuration to use to send the message

    // Determine the actual API token

    int source_apitoken = 0; // Variable used to track the source of the API token for debug output
    int source_userkey = 0;  // Variable used to track the source of the user key for debug output
    int source_title = 0;    // Variable used to track the source of the title for debug output
    int source_message = 0;  // Variable used to track the source of the message for debug output
    int source_sound = 0;    // Variable used to track the source of the sound for debug output

    if ( arg_apitoken != NULL ) {
        config.apitoken = arg_apitoken;
        source_apitoken = 1;
    } else if ( ini_config.apitoken != NULL ) {
        config.apitoken = ini_config.apitoken;
        source_apitoken = 2;
    } else {
        if ( config_path_file == NULL )
            fprintf( stderr, "No API token given.\n" );
        else
            fprintf( stderr, "No API token given in either %s (field api-token in [pushover]) or on the command line (--api-token).\n", config_path_file );
        return ERROR_NO_APPKEY;
    }

    // Determine the actual userkey

    if ( arg_userkey != NULL ) {
        config.userkey = arg_userkey;
        source_userkey = 1;
    } else if ( ini_config.userkey != NULL ) {
        config.userkey = ini_config.userkey;
        source_userkey = 2;
    } else {
        if ( config_path_file == NULL )
            fprintf( stderr, "No user key given.\n" );
        else
            fprintf( stderr, "No user key given in either %s (field user-key in [pushover]) or on the command line (--user-key).\n", config_path_file );
        return ERROR_NO_USERKEY;
    }
   
    // Determine the title (if any)

    if ( arg_title != NULL ) {
        config.title = arg_title;
        source_title = 1;
    } else if ( ini_config.title != NULL ) {
        config.title = ini_config.title;
        source_title = 2;
    } else {
        config.title = NULL;
    }
   
    // Determine the message

    if ( arg_message != NULL ) {
        config.message = arg_message;
        source_message = 1;
    } else if ( ini_config.message != NULL ) {
        config.message = ini_config.message;
        source_message = 2;
    } else {
        config.message = default_message;
    }

    // Determine the sound (if any)

    if ( arg_sound != NULL ) {
        config.sound = arg_sound;
        source_sound = 1;
    } else if ( ini_config.sound != NULL ) {
        config.sound = ini_config.sound;
        source_sound = 2;
    } else {
        config.sound = NULL;
    }

    if ( arg_debug != 0 ) {
        const char *source[] = { "default", "command line", "configuration file"};
        printf( "Combined parameters from command line and ini file:\n" );
        if ( config.apitoken != NULL ) printf( "- API token: %s (%s)\n",  config.apitoken, source[source_apitoken] );
        if ( config.userkey != NULL )  printf( "- user key: %s (%s)\n", config.userkey,    source[source_userkey] );
        if ( config.title != NULL )    printf( "- title: %s (%s)\n",   config.title,       source[source_title] );
        if ( config.message != NULL )  printf( "- message: %s (%s)\n", config.message,     source[source_message] );
        if ( config.sound != NULL )    printf( "- sound: %s (%s)\n",   config.sound,       source[source_sound] );
        printf( "\n" );
    }

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
        curl_mime_data( part, config.apitoken, CURL_ZERO_TERMINATED );

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

        // We'll send the output to /dev/null if we are not debugging to avoid bombarding the user
        // with output they cannot understand.
        FILE *devnull;
        if ( arg_debug == 0 ) {
            devnull = fopen( "/dev/null", "w" );
            curl_easy_setopt( curl, CURLOPT_WRITEDATA, devnull );
        }

        // Perform the request
        if ( arg_debug != 0 ) printf( "Calling curl, returns: " );
        res = curl_easy_perform( curl );
        if ( arg_debug != 0 ) printf( "\n\n" );

        if ( arg_debug != 0 ) printf( "\n\n" );
        if( res != CURLE_OK ) {
            fprintf( stderr, "Sending the message failed, cURL returned: %s\n", curl_easy_strerror( res ) );
        } else {
            printf( "Message sent successfully!\n" );
        }

        // Clean up
        curl_easy_cleanup( curl );
        curl_mime_free( mime );
        if ( arg_debug == 0 ) fclose( devnull );
    }

    curl_global_cleanup();

    // It's a puzzle to figure out which memory must be freed, and as this is done
    // automatically when a program terminates, we don't care.

    return 0;
}

