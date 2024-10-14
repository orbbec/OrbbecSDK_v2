// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once
#include "libobsensor/h/Frame.h"

#include <cstddef>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ob_priv_filter_t ob_priv_filter;
typedef struct ob_priv_filter_context_t ob_priv_filter_context;

/**
 * @brief Function pointer type for the get config schema function of the filter
 *
 * @brief The returned string must be a csv format string representing the configuration schema of the filter. The format of the string is:
 *  <parameter_name>, <parameter_type: int, float, bool>, <minimum_value>, <maximum_value>, <value_step>, <default_value>, <parameter_description>
 *
 * @param[in] filter The filter object to get the configuration schema for
 * @param[out] error Pointer to an error object that will be set if an error occurs
 *
 * @return A csv format string representing the configuration schema of the filter
 */
typedef const char *(*pfunc_ob_priv_filter_get_config_schema)(const ob_priv_filter *filter, ob_error **error);

/**
 * @brief Function pointer type for the update config function of the filter
 *
 * @attention For users, the passed in argc and argv must match the configuration schema returned by the @ref pfunc_ob_priv_filter_get_config_schema function.
 *
 * @param[in] filter The filter object to update the configuration for
 * @param[in] argc The number of arguments in the argv array
 * @param[in] argv An array of strings representing the configuration values
 * @param[out] error Pointer to an error object that will be set if an error occurs
 */
typedef void (*pfunc_ob_priv_filter_update_config)(ob_priv_filter *filter, size_t argc, const char **argv, ob_error **error);

/**
 * @brief Function pointer type for the reset function of the filter
 * @brief Reset the filter to its initial state (stop threads, clear buffers, etc.)
 *
 * @param[in] filter The filter object to reset
 * @param[out] error Pointer to an error object that will be set if an error occurs
 */
typedef void (*pfunc_ob_priv_filter_reset)(ob_priv_filter *filter, ob_error **error);

/**
 * @brief Function pointer type for the process function of the filter
 *
 * @attention Do not modify the frame object passed to the filter process function, the result of the filter process function should be returned as a new frame
 * object.
 *
 * @param[in] filter The filter object to process the frame with
 * @param[in]  frame The frame to process
 * @param[out] error Pointer to an error object that will be set if an error occurs
 *
 * @return The processed frame
 */
typedef ob_frame *(*pfunc_ob_priv_filter_process)(ob_priv_filter *filter, const ob_frame *frame, ob_error **error);

/**
 * @brief Function pointer type for the destroy function of the filter context object
 *
 * @attention The filter developer must ensure that the filter object in the filter context object is destroyed before the filter context object is destroyed.
 *
 * @param filter_context[in] The filter context object to destroy
 * @param error[out] Pointer to an error object that will be set if an error occurs
 */
typedef void (*pfunc_ob_priv_filter_context_destroy)(ob_priv_filter_context *filter_context, ob_error **error);

/**
 * @brief The filter context object is used to store the filter object and the function pointers to the filter functions.
 *
 */
struct ob_priv_filter_context_t {
    pfunc_ob_priv_filter_context_destroy destroy;  //< Destroy the filter context object

    ob_priv_filter *filter;  //< Pointer to the filter object

    pfunc_ob_priv_filter_reset             reset;              //< Reset the filter to its initial state (stop threads, clear buffers, etc.)
    pfunc_ob_priv_filter_get_config_schema get_config_schema;  //< Get the configuration schema of the filter
    pfunc_ob_priv_filter_update_config     update_config;      //< Update the filter's configuration
    pfunc_ob_priv_filter_process           process;            //< Process a frame through the filter
};

/**
 * @brief Function pointer type for the get filter count function
 *
 * @param[out] error Pointer to an error object that will be set if an error occurs
 *
 * @return The number of filters available within the package (dynamic library)
 */
typedef size_t (*pfunc_ob_get_filter_count)(ob_error **error);

/**
 * @brief Function pointer type for the get filter name function
 *
 * @param[in] index The index of the filter to get the name for
 * @param[out] error Pointer to an error object that will be set if an error occurs
 *
 * @return The name of the filter at the given index
 */
typedef const char *(*pfunc_ob_get_filter_name)(size_t index, ob_error **error);

/**
 * @brief Function pointer type to create a new filter object from the filter at the given index
 *
 * @param[in] index The index of the filter to create a new object for
 * @param[out] error Pointer to an error object that will be set if an error occurs
 *
 * @return A new filter context object created that is containing the filter object and the function pointers to the filter functions
 */
typedef ob_priv_filter_context *(*pfunc_ob_create_filter)(size_t index, ob_error **error);  // Create a new filter object from the filter at the given index

/**
 * @brief Function pointer type for the get vendor specific code function
 * @brief For vendor special purpose, the vendor can provide a code to the user to identify the filter package.
 *
 * @param[out] error Pointer to an error object that will be set if an error occurs
 *
 * @return The vendor specific code of the filter package.
 */
typedef const char *(*pfunc_ob_priv_filter_get_vendor_specific_code)(ob_error **error);

/**
 * @brief Function pointer type for the get activation status function
 * @brief The activation status is used to determine if the filter package is activated or not. If is not activated, the filter package may reject to create a
 * filter object and the process function of filter may not effective. If the filter package dose not require activation, the activation status should always
 * return true.
 *
 * @param[out] error Pointer to an error object that will be set if an error occurs
 *
 * @return true if the filter package is activated, false otherwise.
 */
typedef bool (*pfunc_ob_priv_filter_is_activated)(ob_error **error);

/**
 * @brief Function pointer type for the activate function
 * @brief Activate the filter package. The activation key is used to verify the activation request.
 * @brief The vendor of the filter package have the right to require an activation key to activate the filter package.
 *
 * @param[in] activation_key The activation key to activate the filter package.
 * @param[out] error Pointer to an error object that will be set if an error occurs.
 *
 * return true if the activation is successful, false otherwise.
 */
typedef bool (*pfunc_ob_priv_filter_activate)(const char *activation_key, ob_error **error);  // activate the filter package.

#ifdef __cplusplus
}
#endif
