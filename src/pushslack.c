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
#define ERROR_NO_BOTTOKEN    2
#define ERROR_NO_MEMBERID    3
#define ERROR_NO_WEBHOOKURL  4
#define ERROR_NO_ENV_HOME    5
#define ERROR_OUT_OF_MEMORY  6
#define ERROR_WRONG_ARGUMENT 7
#define ERROR_INTERNAL_ERROR 8

#define MODE_NONE            0
#define MODE_CHATPOSTMESSAGE 1
#define MODE_WEBHOOK         2

const char *version = "0.1";
const char *default_config_file = ".LUMI-messenger.ini";
const char *default_message = "Hello from Slack in LUMI-messenger";

typedef struct
{
    const char *bottoken;
    const char *memberid;
    const char *webhookurl;
    const char *message;
} pushslack_configuration;


//
// Print the help information.
//

void PrintHelp( void ) {

    fprintf( stderr,
        "\n"
        "Command line options for pushover:\n"
        "   -b/--bot-token              Specify the bot token, overwriting what is in the\n"
        "                               configuration file. This has precedence over \n"
        "                               --webhook-url.\n"
        "   -c/--configuration-file     Specify the configuration file to use. The default\n"
        "                               is ~/.LUMI-messenger.ini.\n"
        "   -d/--debug                  Print additional debug info.\n"
        "   -h/--help                   Print this help information and quit\n"
        "   -m/--message                Message to send.\n"
        "   -n/--no-configuration-file  Do not read any configuration file, use command\n"
        "                               line arguments instead.\n"
        "   -u/--member-id              Specify the member ID for slack, overwriting what\n"
        "                               is in the configuration file.\n"
        "   -v/--version                Print the version of pushover.\n"
        "   -w/--webhook-url            Webhook URL, overwriting what is in the configuration\n"
        "                               file. Only use if --bot-token and --member-id are not\n"
        "                               given on either the command line or in the configuration\n"
        "                               file.\n"
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
    pushslack_configuration* pconfig = (pushslack_configuration*) user;

    #define MATCH(s, n) strcmp(section, s) == 0 && strcmp(name, n) == 0
    if  (MATCH( "slack", "bot-token" ) ) {
        pconfig->bottoken = strdup( value );
    } else if ( MATCH( "slack", "member-id" ) ) {
        pconfig->memberid = strdup( value );
    } else if ( MATCH( "slack", "webhook-url" ) ) {
        pconfig->webhookurl = strdup( value );
    } else if ( MATCH( "slack", "message" ) ) {
        pconfig->message = strdup( value );
    } else {
        return 0;  /* unknown section/name, error */
    }
    return 1;
}


int main( int argc, char *argv[] ) {

    //
    // Get the command line arguments
    //

    const char *arg_bottoken = (char *) NULL;
    const char *arg_memberid = (char *) NULL;
    const char *arg_webhookurl = (char *) NULL;
    const char *arg_config_file = (char *) NULL;
    const char *arg_message = (char *) NULL;
    int arg_debug = 0;
    int arg_no_configuration_file = 0;

    // Define the long options
    // { "name", has_arg, *flag, val }
    static struct option long_options[] = {
        {"bot-token",             required_argument, 0, 'b'},
        {"configuration-file",    required_argument, 0, 'c'},
        {"debug",                 no_argument,       0, 'd'},
        {"help",                  no_argument,       0, 'h'},
        {"message",               required_argument, 0, 'm'},
        {"no-configuration-file", no_argument,       0, 'n'},
        {"member-id",             required_argument, 0, 'u'},
        {"version",               no_argument,       0, 'v'},
        {"webhook-url",           required_argument, 0, 'w'},
        {0,                       0,                 0, 0} // Required end of array
    };

    int option_index = 0;
    int opt;
    while ( ( opt = getopt_long( argc, argv, "b:c:dhm:nu:vw:", long_options, &option_index) ) != -1 ) {
        switch (opt) {
            case 'b':
                arg_bottoken = optarg;
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
            case 'u':
                arg_memberid = optarg;
                break;
            case 'v':
                printf( "pushover version %s", version );
                return 0;
            case 'w':
                arg_webhookurl = optarg;
                break;
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
        if ( arg_bottoken != NULL )    printf( "- Bot token from argument list: %s\n",   arg_bottoken );
        if ( arg_memberid != NULL )    printf( "- Member ID from argument list: %s\n",   arg_memberid );
        if ( arg_webhookurl != NULL )  printf( "- Webhook URL from argument list: %s\n", arg_webhookurl );
        if ( arg_message != NULL )     printf( "- message from argument list: %s\n",     arg_message );
        if ( arg_config_file != NULL ) printf( "- config file from argument list: %s\n", arg_config_file );
        printf( "\n" );
    }


    // Read data from the INI file.

    pushslack_configuration ini_config;
    ini_config.bottoken    = NULL;
    ini_config.memberid    = NULL;
    ini_config.webhookurl  = NULL;
    ini_config.message     = NULL;
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
                fprintf( stderr, "Memory allocation failed in file %s at line %d.\n", __FILE__, __LINE__ );
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
            if ( ini_config.bottoken != NULL )   printf( "- Bot token from argument list: %s\n",   ini_config.bottoken );
            if ( ini_config.memberid != NULL )   printf( "- Member ID from argument list: %s\n",   ini_config.memberid );
            if ( ini_config.webhookurl != NULL ) printf( "- Webhook URL from argument list: %s\n", ini_config.webhookurl );
            if ( ini_config.message != NULL )    printf( "- message from argument list: %s\n",     ini_config.message );
            printf( "\n" );
        }

    } else if ( arg_debug != 0 )
        printf( "No configuration file read.\n\n" );

    pushslack_configuration config; // Actual configuration to use to send the message

    int source_bottoken = 0;   // Variable used to track the source of the API token for debug output
    int source_memberid = 0;   // Variable used to track the source of the user key for debug output
    int source_webhookurl = 0; // Variable used to track the source of the title for debug output
    int source_message = 0;    // Variable used to track the source of the message for debug output

    // Determine the actual bot token (if any)

    if ( arg_bottoken != NULL ) {
        config.bottoken = arg_bottoken;
        source_bottoken = 1;
    } else if ( ini_config.bottoken != NULL ) {
        config.bottoken = ini_config.bottoken;
        source_bottoken = 2;
    } else {
        config.bottoken = NULL;
        source_bottoken = 0;
    }
    // Determine the actual memberid (if any)

    if ( arg_memberid != NULL ) {
        config.memberid = arg_memberid;
        source_memberid = 1;
    } else if ( ini_config.memberid != NULL ) {
        config.memberid = ini_config.memberid;
        source_memberid = 2;
    } else {
        config.memberid = NULL;
        source_memberid = 0;
    }
   
    // Determine the webhook URL (if any)

    if ( arg_webhookurl != NULL ) {
        config.webhookurl = arg_webhookurl;
        source_webhookurl = 1;
    } else if ( ini_config.webhookurl != NULL ) {
        config.webhookurl = ini_config.webhookurl;
        source_webhookurl = 2;
    } else {
        config.webhookurl = NULL;
        source_webhookurl = 0;
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
        source_message = 0;
    }

    if ( arg_debug != 0 ) {
        const char *source[] = { "default", "command line", "configuration file"};
        printf( "Combined parameters from command line and ini file:\n" );
        if ( config.bottoken != NULL )   printf( "- bot token: %s (%s)\n",   config.bottoken,   source[source_bottoken] );
        else                             printf( "- bot token not specified.\n" );
        if ( config.memberid != NULL )   printf( "- Member ID: %s (%s)\n",   config.memberid,   source[source_memberid] );
        else                             printf( "- Member ID not specified.\n" );
        if ( config.webhookurl != NULL ) printf( "- Webhook URL: %s (%s)\n", config.webhookurl, source[source_webhookurl] );
        else                             printf( "- Webhook URL not specified\n" );
        if ( config.message != NULL )    printf( "- message: %s (%s)\n",     config.message,    source[source_message] );
        printf( "\n" );
    }

    // Now determine the communication mode: 1 = chat.postMessage, 2 = webhook

    int communication_mode = MODE_NONE;

    if ( ( config.bottoken != NULL ) && ( config.memberid != NULL ) )
        communication_mode = MODE_CHATPOSTMESSAGE;
    else if ( ( config.bottoken != NULL ) || ( config.memberid != NULL ) ) {
        // Either config.bottoken is not NULL or memberid is not NULL but not both
        if ( config.bottoken == NULL ) {
            fprintf( stderr, "No bot token given even though the member ID was given. Either both or none should be given.\n" );
            fprintf( stderr, "The bot token should be given either in %s (field bot-token in [slack]) or on the command line (--bot-token).\n", config_path_file );
            if ( source_memberid == 1 ) fprintf( stderr, "The member ID was taken from the command line\n" );
            else                        fprintf( stderr, "The member ID was found in the configuration file %s.\n", config_path_file );
            return ERROR_NO_BOTTOKEN;
        } else {
            fprintf( stderr, "No member ID given even though the bot token was given. Either both or none should be given.\n" );
            fprintf( stderr, "The member id should be given either in %s (field member-id in [slack]) or on the command line (--member-id).\n", config_path_file );
            if ( source_memberid == 1 ) fprintf( stderr, "The member ID was taken from the command line\n" );
            else                        fprintf( stderr, "The member id was found in the configuration file %s.\n", config_path_file );
            return ERROR_NO_MEMBERID;
        }
    } else if ( config.webhookurl != NULL ) {
        communication_mode = MODE_WEBHOOK;
    } else {
        fprintf( stderr, "Either a bot token and member-id need to be specified, or a webhook URL, but neither were found.\n" );
        return ERROR_NO_WEBHOOKURL;
    }

    if ( arg_debug != 0 ) {
        switch ( communication_mode ) {
            case MODE_CHATPOSTMESSAGE : printf( "Communicating via chat.postMessage.\n\n" ); break;
            case MODE_WEBHOOK         : printf( "Communicating via a webhook URL.\n\n" ); break;
            default :                   fprintf( stderr, "Should never have arrived at this point. File %s at line %d.\n", __FILE__, __LINE__ ); return ERROR_INTERNAL_ERROR;
        }
    }

    // Now use libcurl to push a message.

    CURL *curl;
    CURLcode curl_res;

    curl_global_init( CURL_GLOBAL_ALL );
    curl = curl_easy_init();
    if ( curl == (CURL *) NULL ) {
        fprintf( stderr, "Failed to initialise libcurl properly to send a message in file %s at line %d.\n", __FILE__, __LINE__ );
        return ERROR_INTERNAL_ERROR;
    }

    if( curl ) {
    
        if ( communication_mode == MODE_CHATPOSTMESSAGE ) { // Communicating via chat.postMessage

            struct curl_slist *headers = NULL;

            int required;

            // Header: Authorization: Bearer token
            size_t token_length = 23 + strlen( config.bottoken );
            char *token = (char *) malloc( (size_t) (token_length * sizeof( char )) );
            if ( token == (char *) NULL ) { fprintf( stderr, "Out-of-memory in file %s at line %d.\n", __FILE__, __LINE__ ); return ERROR_OUT_OF_MEMORY; }
            required = snprintf( token, token_length, "Authorization: Bearer %s", config.bottoken );
            if ( required >= (int) token_length ) { fprintf( stderr, "Internal error in file %s at line %d.\n", __FILE__, __LINE__ ); return ERROR_INTERNAL_ERROR; }
            headers = curl_slist_append(headers, token );

            // Header: Content-Type: application/json
            headers = curl_slist_append(headers, "Content-Type: application/json");

            // Make the json message.
            const int bufsize = 25; // Large enough that Cray Clang does not complain that the snprintf buffer will always be truncated.
            char dummybuf[bufsize];
            int json_length = snprintf( dummybuf, (size_t) bufsize, "{\"channel\":\"%s\",\"text\":\"%s\"}", config.memberid, config.message ) + 1;
            char *json = (char *) malloc( (size_t) (json_length * sizeof( char )) );
            if ( json == (char *) NULL ) { fprintf( stderr, "Out-of-memory in file %s at line %d.\n", __FILE__, __LINE__ ); return ERROR_OUT_OF_MEMORY; }
            required = snprintf( json, (size_t) json_length, "{\"channel\":\"%s\",\"text\":\"%s\"}", config.memberid, config.message );
            if ( required >= (int) json_length ) { fprintf( stderr, "Internal error in file %s at line %d.\n", __FILE__, __LINE__ ); return ERROR_INTERNAL_ERROR; }

            curl_easy_setopt( curl, CURLOPT_URL,        "https://slack.com/api/chat.postMessage" );
            curl_easy_setopt( curl, CURLOPT_HTTPHEADER, headers );
            curl_easy_setopt( curl, CURLOPT_POSTFIELDS, json );

            // We'll send the output to /dev/null if we are not debugging to avoid bombarding the user
            // with output they cannot understand.
            FILE *devnull;
            if ( arg_debug == 0 ) {
                devnull = fopen( "/dev/null", "w" );
                curl_easy_setopt( curl, CURLOPT_WRITEDATA, devnull );
            }

            // Perform the request
            if ( arg_debug != 0 ) printf( "Calling curl, returns: " );
            curl_res = curl_easy_perform( curl );
            if ( arg_debug != 0 ) printf( "\n\n" );

            if( curl_res != CURLE_OK ) {
                fprintf( stderr, "Sending the message failed, cURL returned: %s\n", curl_easy_strerror( curl_res ) );
            } else {
                printf( "Message sent successfully!\n" );
            }

            // Clean-up
            curl_slist_free_all( headers );
            curl_easy_cleanup( curl );

            free( token );
            free( json );

            if ( arg_debug == 0 ) fclose( devnull );

        } else { // Communicating via webhook URL

            struct curl_slist *headers = NULL;

            int required;

            // Build the header, in this case only the Content-Type: application/json
            headers = curl_slist_append( headers, "Content-Type: application/json" );

            // Build the json part
            const int bufsize = 12; // Large enough that Clang does not complain that the snprintf buffer will always be truncated.
            char dummybuf[bufsize];
            int json_length = snprintf( dummybuf, (size_t) bufsize, "{\"text\":\"%s\"}", config.message ) + 1;
            char *json = (char *) malloc( (size_t) (json_length * sizeof( char )) );
            if ( json == (char *) NULL ) { fprintf( stderr, "Out-of-memory in file %s at line %d.\n", __FILE__, __LINE__ ); return ERROR_OUT_OF_MEMORY; }
            required = snprintf( json, (size_t) json_length, "{\"text\":\"%s\"}", config.message );
            if ( required >= (int) json_length ) { fprintf( stderr, "Internal error in file %s at line %d.\n", __FILE__, __LINE__ ); return ERROR_INTERNAL_ERROR; }

            // Build the curl call
            curl_easy_setopt( curl, CURLOPT_URL,        config.webhookurl );
            curl_easy_setopt( curl, CURLOPT_HTTPHEADER, headers );
            curl_easy_setopt( curl, CURLOPT_POSTFIELDS, json );

            // We'll send the output to /dev/null if we are not debugging to avoid bombarding the user
            // with output they cannot understand.
            FILE *devnull;
            if ( arg_debug == 0 ) {
                devnull = fopen( "/dev/null", "w" );
                curl_easy_setopt( curl, CURLOPT_WRITEDATA, devnull );
            }

            // Perform the request
            if ( arg_debug != 0 ) printf( "Calling curl, returns: " );
            curl_res = curl_easy_perform( curl );
            if ( arg_debug != 0 ) printf( "\n\n" );

            if( curl_res != CURLE_OK ) {
                fprintf( stderr, "Sending the message failed, cURL returned: %s\n", curl_easy_strerror( curl_res ) );
            } else {
                printf( "Message sent successfully!\n" );
            }

            // Clean-up
            curl_slist_free_all( headers );
            curl_easy_cleanup( curl );

            free( json );

            if ( arg_debug == 0 ) fclose( devnull );

        } // end else-part if ( communication_mode ... )

    curl_global_cleanup();

    // It's a puzzle to figure out which memory must be freed, and as this is done
    // automatically when a program terminates, we don't care.

    return 0;

    } // end if ( curl )

}

