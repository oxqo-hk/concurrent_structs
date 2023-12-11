#ifndef CST_COMMON_H
#define CST_COMMON_H
#include <errno.h>

typedef unsigned long long  u64;
typedef unsigned int        u32;
typedef unsigned short      u16;
typedef unsigned char       u8;

#define CST_ALWAYS_INLINE static inline __attribute__((always_inline))

typedef void (*defer_func)(void *ptr);
typedef struct{
    void* ptr;
    defer_func func;
}defer_t;

#define defer_stack(size, name) \
    defer_t defer_stack_##name[size] = {NULL,}; \
    u32 defer_stack_##name_p = 0

#define defer_add(target, func_p, name) ({ \
    int __defer_res=0; \
    if(!(target) || !(func_p)) __defer_res=ENOMEM; \
    else \
    { \
        defer_stack_##name[defer_stack_##name_p].ptr = (void *)(target); \
        defer_stack_##name[defer_stack_##name_p++].func = (defer_func)(func_p); \
    } \
    __defer_res; })

#define defer_release(name) \
    while(defer_stack_##name_p--){ \
        defer_func __defer_func = defer_stack_##name[defer_stack_##name_p].func; \
        void *__defer_ptr = defer_stack_##name[defer_stack_##name_p].ptr; \
        (*__defer_func)(__defer_ptr); \
    } 

#define defer_stack_dynamic(size, name, return_val) \
    defer_t *defer_stack_##name = calloc(size, sizeof(defer_t)); \
    u32 defer_stack_##name_p = 0; \
    if(defer_add(defer_stack_##name, &free, name)) \
    { \
        printf("failed to create dynamic defer stack.\n"); \
        return return_val; \
    }

#endif