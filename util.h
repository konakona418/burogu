#pragma once

typedef unsigned char Bool;
#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

#define DYNARRAY_PUSHBACK(arr, value)                                          \
    do {                                                                       \
        if (arr##_count >= arr##_capacity) {                                   \
            arr##_capacity = (arr##_capacity == 0) ? 4 : arr##_capacity * 2;   \
            arr = (typeof(arr)) realloc(arr, arr##_capacity * sizeof(*(arr))); \
        }                                                                      \
        arr[arr##_count++] = value;                                            \
    } while (0)

#define DYNARRAY_INIT(arr, initCapacity)                             \
    arr = NULL;                                                      \
    int arr##_count = 0;                                             \
    int arr##_capacity = 0;                                          \
    do {                                                             \
        arr##_capacity = initCapacity;                               \
        arr = (typeof(arr)) malloc(arr##_capacity * sizeof(*(arr))); \
    } while (0)

#define DYNARRAY_SIZE(arr) (arr##_count)
#define DYNARRAY_FREE(arr)  \
    do {                    \
        free(arr);          \
        arr = NULL;         \
        arr##_count = 0;    \
        arr##_capacity = 0; \
    } while (0)

#define STACK_INIT(arr, initCapacity)                                \
    arr = NULL;                                                      \
    int arr##_count = 0;                                             \
    int arr##_capacity = 0;                                          \
    do {                                                             \
        arr##_capacity = initCapacity;                               \
        arr = (typeof(arr)) malloc(arr##_capacity * sizeof(*(arr))); \
    } while (0)

#define STACK_PUSH(arr, value)                                                 \
    do {                                                                       \
        if (arr##_count >= arr##_capacity) {                                   \
            arr##_capacity = (arr##_capacity == 0) ? 4 : arr##_capacity * 2;   \
            arr = (typeof(arr)) realloc(arr, arr##_capacity * sizeof(*(arr))); \
        }                                                                      \
        arr[arr##_count++] = value;                                            \
    } while (0)

#define STACK_POP(arr, outValue)            \
    do {                                    \
        if (arr##_count > 0) {              \
            *outValue = arr[--arr##_count]; \
        }                                   \
    } while (0)

#define STACK_FREE(arr)     \
    do {                    \
        free(arr);          \
        arr = NULL;         \
        arr##_count = 0;    \
        arr##_capacity = 0; \
    } while (0)

#define STACK_TOP(arr) \
    ((arr##_count > 0) ? &arr[arr##_count - 1] : NULL)