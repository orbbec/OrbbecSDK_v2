﻿/**
 * @file  Error.hpp
 * @brief This file defines the Error class, which describes abnormal errors within the SDK.
 *        Detailed information about the exception can be obtained through this class.
 */
#pragma once

#include "Types.hpp"
#include "openobsdk/h/Error.h"
#include <memory>

struct ErrorImpl;

namespace ob {
class Error : public std::exception {
private:
    const ob_error *impl_;

    /**
     * @brief Construct a new Error object
     *
     * @attention This constructor should not be called directly, use the handle() function instead.
     *
     * @param error
     */
    explicit Error(ob_error *error) : impl_(error){};

public:
    /**
     * @brief A static function to handle the ob_error and throw an exception if needed.
     *
     * @param error The ob_error pointer to be handled.
     * @param throw_exception A boolean value to indicate whether to throw an exception or not, the default value is true.
     */
    static void handle(ob_error **error, bool throw_exception = true) {
        if(!error || !*error) {  // no error
            return;
        }

        if(throw_exception) {
            throw Error(*error);
            *error = nullptr;
        }
        else {
            ob_delete_error(*error);
            *error = nullptr;
        }
    }

    /**
     * @brief Destroy the Error object
     */
    ~Error() {
        if(impl_) {
            ob_delete_error(impl_);
            impl_ = nullptr;
        }
    }

    /**
     * @brief Returns the error message of the exception.
     *
     * @return const char*  The error message.
     */
    const char *what() const noexcept override {
        return impl_->message;
    }

    /**
     * @brief Returns the exception type of the exception.
     * @brief Read the comments of the OBExceptionType enum in the openobsdk/h/ObTypes.h file for more information.
     *
     * @return OBExceptionType The exception type.
     */
    OBExceptionType Error::getExceptionType() const noexcept {
        return impl_->exception_type;
    }

    /**
     * @brief Returns the name of the function where the exception occurred.
     *
     * @return const char* The function name.
     */
    const char *Error::getFunctionName() const noexcept {
        return impl_->function;
    }

    /**
     * @brief Returns the arguments of the function where the exception occurred.
     *
     * @return const char*  The arguments.
     */
    const char *Error::getArgs() const noexcept {
        return impl_->args;
    }

    /***************Below functions are deprecated, leave here for compatibility***************/

    /**
     * @brief (Deprecated) Returns the name of the function where the exception occurred.
     * @brief Use the getFunctionName() function instead.
     *
     * @return const char* The function name.
     */
    OB_DEPRECATED const char *Error::getName() const noexcept {
        return impl_->function;
    }

    /**
     * @brief (Deprecated) Returns the error code of the exception.
     * @brief Use the what() function instead.
     *
     * @return const char* The error message.
     */
    OB_DEPRECATED const char *Error::getMessage() const noexcept {
        return impl_->message;
    }
};
}  // namespace ob
