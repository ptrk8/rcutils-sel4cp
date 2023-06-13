// Copyright 2015 Open Source Robotics Foundation, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <stdlib.h>
#include <stdio.h>
#ifdef RCUTILS_SEL4CP
#include <string.h>
#endif

#include "rcutils/allocator.h"

#include "rcutils/error_handling.h"
#include "rcutils/macros.h"

// When this define evaluates to true (default), then messages will printed to
// stderr when an error is encoutered while setting the error state.
// For example, when memory cannot be allocated or a previous error state is
// being overwritten.
#ifndef RCUTILS_REPORT_ERROR_HANDLING_ERRORS
#define RCUTILS_REPORT_ERROR_HANDLING_ERRORS 1
#endif

static void *
__default_allocate(size_t size, void *state) {
    RCUTILS_CAN_RETURN_WITH_ERROR_OF(NULL);

    RCUTILS_UNUSED(state);
#ifdef RCUTILS_SEL4CP
    size_t *result = (size_t *) malloc(sizeof(size_t) + size);
    if (result == NULL) {
        return NULL;
    }
    *result = size;
    ++result;
    return result;
#else
    return malloc(size);
#endif
}

static void
__default_deallocate(void *pointer, void *state) {
    RCUTILS_UNUSED(state);
#ifdef RCUTILS_SEL4CP
    if (pointer) {
        size_t *in = pointer;
        --in;
        free(in);
    }
#else
    free(pointer);
#endif
}

#ifdef RCUTILS_SEL4CP
static size_t get_size(void *p) {
    size_t *in = (size_t *) p;
    --in;
    return *in;
}
#endif

static void *
__default_reallocate(void *pointer, size_t size, void *state) {
    RCUTILS_CAN_RETURN_WITH_ERROR_OF(NULL);

    RCUTILS_UNUSED(state);
#ifdef RCUTILS_SEL4CP
    /* If pointer is NULL, realloc() is the same as malloc(). */
    if (pointer == NULL) {
        return __default_allocate(size, state);
    }
    void *const orig_ptr = pointer;
    size_t const new_size = size;
    size_t const orig_size = get_size(orig_ptr);
    if (new_size == orig_size) {
        return pointer;
    } else {
        void *new_ptr = __default_allocate(new_size, state);
        if (new_ptr == NULL) {
            return NULL;
        }
        if (new_size > orig_size) {
            memcpy(new_ptr, orig_ptr, orig_size);
        } else if (new_size < orig_size) {
            memcpy(new_ptr, orig_ptr, new_size);
        }
        __default_deallocate(orig_ptr, state);
        return new_ptr;
    }
#else
    return realloc(pointer, size);
#endif
}

static void *
__default_zero_allocate(size_t number_of_elements, size_t size_of_element, void *state) {
    RCUTILS_CAN_RETURN_WITH_ERROR_OF(NULL);

    RCUTILS_UNUSED(state);
#ifdef RCUTILS_SEL4CP
    void *ptr = __default_allocate(number_of_elements * size_of_element, state);
    if (ptr == NULL) {
        return NULL;
    }
    memset(ptr, 0, number_of_elements * size_of_element);
    return ptr;
#else
    return calloc(number_of_elements, size_of_element);
#endif
}

rcutils_allocator_t
rcutils_get_zero_initialized_allocator(void) {
    static rcutils_allocator_t zero_allocator = {
            .allocate = NULL,
            .deallocate = NULL,
            .reallocate = NULL,
            .zero_allocate = NULL,
            .state = NULL,
    };
    return zero_allocator;
}

static rcutils_allocator_t default_allocator = {
        .allocate = __default_allocate,
        .deallocate = __default_deallocate,
        .reallocate = __default_reallocate,
        .zero_allocate = __default_zero_allocate,
        .state = NULL,
};

bool
rcutils_set_default_allocator(rcutils_allocator_t *allocator) {
    if (rcutils_allocator_is_valid(allocator)) {
        default_allocator.allocate = allocator->allocate;
        default_allocator.deallocate = allocator->deallocate;
        default_allocator.reallocate = allocator->reallocate;
        default_allocator.zero_allocate = allocator->zero_allocate;
        default_allocator.state = NULL;
        return true;
    }
    return false;
}

rcutils_allocator_t
rcutils_get_default_allocator() {
    return default_allocator;
}

bool
rcutils_allocator_is_valid(const rcutils_allocator_t *allocator) {
    if (
            NULL == allocator ||
            NULL == allocator->allocate ||
            NULL == allocator->deallocate ||
            NULL == allocator->zero_allocate ||
            NULL == allocator->reallocate) {
        return false;
    }
    return true;
}

void *
rcutils_reallocf(void *pointer, size_t size, rcutils_allocator_t *allocator) {
    if (!rcutils_allocator_is_valid(allocator)) {
        // cannot deallocate pointer, so print message to stderr and return NULL
#if RCUTILS_REPORT_ERROR_HANDLING_ERRORS
        RCUTILS_SAFE_FWRITE_TO_STDERR(
                "[rcutils|allocator.c:" RCUTILS_STRINGIFY(__LINE__) "] rcutils_reallocf(): "
                                                                    "invalid allocator or allocator function pointers, memory leaked\n");
#endif
        return NULL;
    }
    void *new_pointer = allocator->reallocate(pointer, size, allocator->state);
    if (NULL == new_pointer) {
        allocator->deallocate(pointer, allocator->state);
    }
    return new_pointer;
}
