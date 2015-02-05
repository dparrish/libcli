/*
 * $Id: Exp $
 * libcli_ex.h - Versa extension to libcli
 *
 * This file contains the definitions for the versa extensions to libcli.
 *
 * Copyright (c) 2013, Versa Networks, Inc.
 * All rights reserved.
 */


#ifndef __LIBCLI_EX_H__
#define __LIBCLI_EX_H__

#include <stdint.h>

/**
 * @file libcli_ex.h
 * Versa extension to libcli
 *
 * This file contains the definitions for the versa extensions to libcli.
 *
 */


struct cli_def;
struct libcli_arg_value_s;
struct libcli_arg_info_s;


/**
 * Various CLI argument types that are supported by libcli_ex
 */

typedef enum libcli_arg_type_e {

    /**
     * unknown type.
     */
    LIBCLI_ARG_TYPE_UNKNOWN,

    /**
     * int8_t type.
     */
    LIBCLI_ARG_TYPE_INT8_T,

    /**
     * int16_t type.
     */
    LIBCLI_ARG_TYPE_INT16_T,

    /**
     * int32_t type.
     */
    LIBCLI_ARG_TYPE_INT32_T,

    /**
     * int64_t type.
     */
    LIBCLI_ARG_TYPE_INT64_T,

    /**
     * uint8_t type.
     */
    LIBCLI_ARG_TYPE_UINT8_T,

    /**
     * uint16_t type.
     */
    LIBCLI_ARG_TYPE_UINT16_T,

    /**
     * uint32_t type.
     */
    LIBCLI_ARG_TYPE_UINT32_T,

    /**
     * uint64_t type.
     */
    LIBCLI_ARG_TYPE_UINT64_T,

    /**
     * string type.
     */
    LIBCLI_ARG_TYPE_STRING,

} libcli_arg_type_t;


// ----------------------------------------------------------------------------
// * libcli_arg_select_fn_t
// ----------------------------------------------------------------------------

/**
 * @brief
 * Callback function to print and/or return arguments from a list of
 * pre-selected values.
 *
 * @param [in] cli libcli's cli handle
 * @param [in] command command string
 * @param [in] opaque opaque value (used only libcli callback functions)
 * @param [in] argi argument info of various args of the command
 * @param [in] argv argument values of the command
 * @param [in] argc number of arguments
 *
 * @return 0 on success; -1 on error.
 */
typedef int
(*libcli_arg_callback_fn_t)(struct cli_def*            cli,
                            const char*                command,
                            void*                      opaque,
                            struct libcli_arg_info_s*  argi,
                            struct libcli_arg_value_s* argv,
                            int32_t                    argc);


// ----------------------------------------------------------------------------
// * libcli_arg_select_fn_t
// ----------------------------------------------------------------------------

/**
 * @brief
 * Callback function to print and/or return arguments from a list of
 * pre-selected values.
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
typedef int
(*libcli_arg_select_fn_t)(struct cli_def*            cli,
                          const char*                command,
                          void*                      opaque,
                          struct libcli_arg_info_s*  argi,
                          struct libcli_arg_value_s* argv_processed,
                          char**                     argv,
                          int32_t                    argc,
                          int32_t                    cur_arg_ix,
                          struct libcli_arg_value_s* arg_value);


/**
 * Argument value.
 */

typedef struct libcli_arg_info_s {

    /** 
     * Short name of argument.
     */
    char *short_name;

    /** 
     * Type of argument.
     */
    libcli_arg_type_t type;

    /**
     * Minimum value/length
     */
    uint64_t min_val;

    /**
     * Maximum value/length
     */
    uint64_t max_val;

    /** 
     * Help string of argument.
     */
    char *help;

    /** 
     * Select/choose callback function for this argument, if any.
     */
    libcli_arg_select_fn_t arg_select_fn;

    /**
     * Select/choose list of pre-defined values for this argument, if any.
     */
    char* select_values;

} libcli_arg_info_t;


/**
 * Argument value.
 */

typedef struct libcli_arg_value_s {

    union {

        /**
         * int8_t value.
         */
        int8_t int8_val;

        /**
         * int16_t value.
         */
        int16_t int16_val;

        /**
         * int32_t value.
         */
        int32_t int32_val;

        /**
         * int64_t value.
         */
        int64_t int64_val;

        /**
         * uint8_t value.
         */
        uint8_t uint8_val;

        /**
         * uint16_t value.
         */
        uint16_t uint16_val;

        /**
         * uint32_t value.
         */
        uint32_t uint32_val;

        /**
         * uint64_t value.
         */
        uint64_t uint64_val;

        /**
         * string value.
         */
        char* string_val;

    } v;

} libcli_arg_value_t;


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
                    int32_t                    argc);


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
                  struct libcli_arg_value_s* arg_value);



#endif /* __LIBCLI_EX_H__ */


