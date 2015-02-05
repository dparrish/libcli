/*
 * $Id: Exp $
 * libcli_ex.c - Versa extension to libcli
 *
 * This file contains the definitions for the versa extensions to libcli.
 *
 * Copyright (c) 2013, Versa Networks, Inc.
 * All rights reserved.
 */


/**
 * @file libcli_ex.c
 * Versa extension to libcli
 *
 * This file contains the definitions for the versa extensions to libcli.
 *
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "libcli.h"
#include "libcli_ex.h"



static const char* libcli_arg_type_strings[] = {

    /**
     * unknown type.
     */
    "unknown",

    /**
     * int8_t type.
     */
    "signed-8bit-integer",

    /**
     * int16_t type.
     */
    "signed-16bit-integer",

    /**
     * int32_t type.
     */
    "signed-32bit-integer",

    /**
     * int64_t type.
     */
    "signed-64bit-integer",

    /**
     * uint8_t type.
     */
    "unsigned-8bit-integer",

    /**
     * uint16_t type.
     */
    "unsigned-16bit-integer",

    /**
     * uint32_t type.
     */
    "unsigned-32bit-integer",

    /**
     * uint64_t type.
     */
    "unsigned-64bit-integer",

    /**
     * string type.
     */
    "string",

};




// ----------------------------------------------------------------------------
// * libcli_print_usage
// ----------------------------------------------------------------------------

/**
 * This function prints a discriptive message about the command usage,
 * including help messages on how to use the arguments.
 *
 * @param [in] cli libcli's cli handle
 * @param [in] command command string
 * @param [in] argi argument info of various args of the command
 * @param [in] max_args maximum number of arguments for the command
 *
 * @return none
 */
static void
libcli_print_usage(struct cli_def*            cli,
                   const char*                command,
                   struct libcli_arg_info_s*  argi,
                   int32_t                    max_args)
{
    int32_t ix = 0;

    /* Print command */
    cli_print(cli, " ");
    cli_print(cli, "Usage:");
    cli_print_nonl(cli, "    %s", command);

    /* Print arguments */
    for (ix = 0; ix < max_args; ix++) {
        cli_print_nonl(cli, " <%s>", argi[ix].short_name);
    }
    cli_print(cli, " ");

    /* Print help strings */
    for (ix = 0; ix < max_args; ix++) {
        cli_print_nonl(cli, "        %-22s: %s",
                       argi[ix].short_name,
                       argi[ix].help);
        cli_print(cli, " ");
    }
    cli_print(cli, " ");
}


// ----------------------------------------------------------------------------
// * libcli_arg_select
// ----------------------------------------------------------------------------

/**
 * @brief
 * Default arg select function provided by libcli extension.
 *
 * @param [in] cli libcli's cli handle
 * @param [in] command command string
 * @param [in] opaque opaque value (used only libcli callback functions)
 * @param [in] argi argument info of various args of the command
 * @param [in] argv_processed processed argument values of the command
 * @param [in] argv argument values of the command
 * @param [in] argc number of arguments
 * @param [in] cur_arg_ix index of current argument to be selected
 * @param [io] arg_value value to be filled in for last argument (if any)
 *
 * @return 0 on success; -1 on error.
 */
int
libcli_arg_select(struct cli_def*            cli,
                  const char*                command,
                  void*                      opaque,
                  struct libcli_arg_info_s*  argi,
                  struct libcli_arg_value_s* argv_processed,
                  char**                     argv,
                  int32_t                    argc,
                  int32_t                    cur_arg_ix,
                  struct libcli_arg_value_s* arg_value)
{
    int32_t choice_cnt = 0;
    char* ptr = (char*) opaque;
    int32_t arglen = 0;
    char* cur_arg = argv[cur_arg_ix];
    int32_t is_prefix = 0;

    /* sanity check */
    if (ptr == NULL || strlen(ptr) == 0) {
        return CLI_ERROR;
    }

    /* Find the length of substring of current argument */
    arglen = strlen(cur_arg);
    if (cur_arg[arglen - 1] == '?') {
        is_prefix = 1;
        arglen--;
    }

    /* Calculate number of choices */
    do {
        if (arglen == 0) {
            choice_cnt++;
        }
        else if (strncmp(ptr, argv[cur_arg_ix], arglen) == 0) {
            if ((!is_prefix) &&
                ((ptr[arglen] == '|' && ptr[arglen + 1] == '|') ||
                 (ptr[arglen] == '\0'))) {
                /* XXX-TODO: fill in parsed int values for int types */
                arg_value->v.string_val = argv[cur_arg_ix];
                return CLI_OK;
            }
            else {
                choice_cnt++;
            }
        }
        ptr = strstr(ptr, "||");
        if (ptr != NULL) {
            ptr += 2;
        }
    } while (ptr != NULL);

    /* If argument is not prefix, validate complete argument */
    if (!is_prefix) {
        /* none of the choices have matched */
        cli_print(cli,
                  "   Error: none of the valid choices for "
                  "argument '%s' matches the value '%s'",
                  argi[cur_arg_ix].short_name,
                  cur_arg);
        return CLI_ERROR;
    }

    /* Check if at least one choice exists */
    if (choice_cnt <= 0) {
        if (is_prefix) {
            cur_arg[arglen] = 0;
        }
        cli_print(cli,
                  "   Error: none of the valid choices for "
                  "argument '%s' starts with '%s'",
                  argi[cur_arg_ix].short_name,
                  cur_arg);
        if (is_prefix) {
            cur_arg[arglen] = '?';
        }
        return CLI_ERROR;
    }

    /* Print the available choices */
    cli_print(cli, " ");
    cli_print(cli, "   Please specify one of the following choices:");

    /* Print the choices */
    ptr = (char*) opaque;
    do {
        int32_t do_print = 0;
        char* print_ptr = ptr;
        char print_str[1024];

        if (arglen == 0) {
            do_print = 1;
        }
        else if (strncmp(ptr, argv[cur_arg_ix], arglen) == 0) {
            do_print = 1;
        }

        ptr = strstr(ptr, "||");
        if (ptr != NULL) {
            if (do_print) {
                strncpy(print_str, print_ptr, (ptr - print_ptr));
                print_str[ptr - print_ptr] = 0;
            }
            ptr += 2;
        }
        else if (do_print) {
            snprintf(print_str, sizeof(print_str) - 2, "%s", print_ptr);
        }

        if (do_print) {
            cli_print(cli, "        %s", print_str);
        }
    } while (ptr != NULL);
    cli_print(cli, " ");

    return CLI_ERROR;
}


// ----------------------------------------------------------------------------
// * libcli_process_args
// ----------------------------------------------------------------------------

/**
 * @brief
 * This function validates the arguments based on argument info and fills in
 * the argument values.
 *
 * @param [in] cli libcli's cli handle
 * @param [in] command command string
 * @param [in] opaque opaque value (used only libcli callback functions)
 * @param [in] argi argument info of various args of the command
 * @param [in] argv_processed processed argument values of the command
 * @param [in] argv argument values of the command, as entered by user
 * @param [in] argc number of arguments
 *
 * @return 0 on success; -1 on error.
 */
int
libcli_process_args(struct cli_def*            cli,
                    const char*                command,
                    void*                      opaque,
                    struct libcli_arg_info_s*  argi,
                    struct libcli_arg_value_s* argv_processed,
                    char*                      argv[],
                    int32_t                    argc)
{
    int32_t max_args = 0;
    int32_t ix = 0;

    /* Calculate the maximum number of arguments */
    while (argi != NULL && argi[ix].short_name != NULL) {
        ix++;
    }
    max_args = ix;

    /* If the number of arguments is more than max, print usage and return */
    if (argc == (max_args + 1) && argv[argc - 1][0] == '?') {
        cli_print(cli, "    Please press <return> to complete command.");
        return CLI_ERROR;
    }
    else if (argc > max_args) {
        libcli_print_usage(cli, command, argi, max_args);
        return CLI_ERROR;
    }

    /* Validate the value of each argument and save it to the processed
     * values.
     */
    for (ix = 0; ix < argc; ix++) {
        int32_t len = strlen(argv[ix]);

        /* If there is a select function, call it - the select function is
         * supposed to do multiple things- auto-complete or prompt the user
         * to select from a list of available choices and also validate the
         * argument value, if a complete choice value was entered.
         */
        if (argi[ix].arg_select_fn != NULL) {
            if ((*argi[ix].arg_select_fn)(cli,
                                          command,
                                          argi[ix].select_values,
                                          argi,
                                          argv_processed,
                                          argv,
                                          argc,
                                          ix,
                                          &argv_processed[ix]) != 0) {
                return CLI_ERROR;
            }
        }
        else if (argi[ix].select_values != NULL) {
            if (libcli_arg_select(cli,
                                  command,
                                  argi[ix].select_values,
                                  argi,
                                  argv_processed,
                                  argv,
                                  argc,
                                  ix,
                                  &argv_processed[ix]) != 0) {
                return CLI_ERROR;
            }
        }
        else {

            /* Check if the user is looking for help entering the arg val */
            if (argv[ix][len - 1] == '?') {
                cli_print(cli,
                          "    [%s]   %s",
                          libcli_arg_type_strings[argi[ix].type],
                          argi[ix].help);
                return CLI_ERROR;
            }

            /* Validate string argument values */
            if (argi[ix].type == LIBCLI_ARG_TYPE_STRING) {
                uint64_t len = (uint64_t) strlen(argv[ix]);

                if (len < argi[ix].min_val || len > argi[ix].max_val) {
                    cli_print(cli,
                              "\nError: value of '%s' should be between %lu "
                              "and %lu bytes long",
                              argi[ix].short_name,
                              argi[ix].min_val,
                              argi[ix].max_val);
                    return CLI_ERROR;
                }
                argv_processed[ix].v.string_val = argv[ix];
            }

            /* Validate integer argument values */
            else if ((argi[ix].type >= LIBCLI_ARG_TYPE_INT8_T) &&
                     (argi[ix].type <= LIBCLI_ARG_TYPE_UINT64_T)) {
                uint64_t parsed_val = 0;
                char* end_ptr = NULL;

                parsed_val = strtoull(argv[ix], &end_ptr, 0);
                if (end_ptr != NULL && *end_ptr != '\0') {
                    cli_print(cli,
                              "\nError: value of '%s' should be a valid "
                              "integer value",
                              argi[ix].short_name);
                    return CLI_ERROR;
                }

                switch (argi[ix].type) {
                    case LIBCLI_ARG_TYPE_INT8_T:
                        if ((((int8_t) parsed_val) <
                                         ((int8_t) argi[ix].min_val)) ||
                            (((int8_t) parsed_val) >
                                         ((int8_t) argi[ix].max_val))) {
                            cli_print(cli,
                                      "\nError: value of '%s' should be a "
                                      "valid integer value between %d and %d",
                                      argi[ix].short_name,
                                      (int8_t) argi[ix].min_val,
                                      (int8_t) argi[ix].max_val);
                            return CLI_ERROR;
                        }
                        break;

                    case LIBCLI_ARG_TYPE_INT16_T:
                        if ((((int16_t) parsed_val) <
                                         ((int16_t) argi[ix].min_val)) ||
                            (((int16_t) parsed_val) >
                                         ((int16_t) argi[ix].max_val))) {
                            cli_print(cli,
                                      "\nError: value of '%s' should be a "
                                      "valid integer value between %d and %d",
                                      argi[ix].short_name,
                                      (int16_t) argi[ix].min_val,
                                      (int16_t) argi[ix].max_val);
                            return CLI_ERROR;
                        }
                        break;

                    case LIBCLI_ARG_TYPE_INT32_T:
                        if ((((int32_t) parsed_val) <
                                         ((int32_t) argi[ix].min_val)) ||
                            (((int32_t) parsed_val) >
                                         ((int32_t) argi[ix].max_val))) {
                            cli_print(cli,
                                      "\nError: value of '%s' should be a "
                                      "valid integer value between %d and %d",
                                      argi[ix].short_name,
                                      (int32_t) argi[ix].min_val,
                                      (int32_t) argi[ix].max_val);
                            return CLI_ERROR;
                        }
                        break;

                    case LIBCLI_ARG_TYPE_INT64_T:
                        if ((((int64_t) parsed_val) <
                                         ((int64_t) argi[ix].min_val)) ||
                            (((int64_t) parsed_val) >
                                         ((int64_t) argi[ix].max_val))) {
                            cli_print(cli,
                                      "\nError: value of '%s' should be a "
                                      "valid integer value between %ld and %ld",
                                      argi[ix].short_name,
                                      (int64_t) argi[ix].min_val,
                                      (int64_t) argi[ix].max_val);
                            return CLI_ERROR;
                        }
                        break;

                    case LIBCLI_ARG_TYPE_UINT8_T:
                        if ((((uint8_t) parsed_val) <
                                         ((uint8_t) argi[ix].min_val)) ||
                            (((uint8_t) parsed_val) >
                                         ((uint8_t) argi[ix].max_val))) {
                            cli_print(cli,
                                      "\nError: value of '%s' should be a "
                                      "valid integer value between %u and %u",
                                      argi[ix].short_name,
                                      (uint8_t) argi[ix].min_val,
                                      (uint8_t) argi[ix].max_val);
                            return CLI_ERROR;
                        }
                        break;

                    case LIBCLI_ARG_TYPE_UINT16_T:
                        if ((((uint16_t) parsed_val) <
                                         ((uint16_t) argi[ix].min_val)) ||
                            (((uint16_t) parsed_val) >
                                         ((uint16_t) argi[ix].max_val))) {
                            cli_print(cli,
                                      "\nError: value of '%s' should be a "
                                      "valid integer value between %u and %u",
                                      argi[ix].short_name,
                                      (uint16_t) argi[ix].min_val,
                                      (uint16_t) argi[ix].max_val);
                            return CLI_ERROR;
                        }
                        break;

                    case LIBCLI_ARG_TYPE_UINT32_T:
                        if ((((uint32_t) parsed_val) <
                                         ((uint32_t) argi[ix].min_val)) ||
                            (((uint32_t) parsed_val) >
                                         ((uint32_t) argi[ix].max_val))) {
                            cli_print(cli,
                                      "\nError: value of '%s' should be a "
                                      "valid integer value between %u and %u",
                                      argi[ix].short_name,
                                      (uint32_t) argi[ix].min_val,
                                      (uint32_t) argi[ix].max_val);
                            return CLI_ERROR;
                        }
                        break;

                    case LIBCLI_ARG_TYPE_UINT64_T:
                        if ((parsed_val < argi[ix].min_val) ||
                            (parsed_val > argi[ix].max_val)) {
                            cli_print(cli,
                                      "\nError: value of '%s' should be a "
                                      "valid integer value between %lu and %lu",
                                      argi[ix].short_name,
                                      argi[ix].min_val,
                                      argi[ix].max_val);
                            return CLI_ERROR;
                        }
                        break;

                    default:
                        return CLI_ERROR;
                }
                argv_processed[ix].v.uint64_val = parsed_val;
            }

            /* Invalid argument type */
            else {
                return CLI_ERROR;
            }
        }
    }

    /* Make sure that all arguments have been specified */
    if (argc != max_args) {
        libcli_print_usage(cli, command, argi, max_args);
        return CLI_ERROR;
    }

    /* Looks like all arguments have been specified and are valid */
    return CLI_OK;
}



