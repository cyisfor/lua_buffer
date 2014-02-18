#include <stdint.h>
#include <stdlib.h>

#include "lua.h"

typedef struct {
    void *(*alloc) (void *ud, void *ptr, size_t osize, size_t nsize);
    void* ud;

    uint8_t* backend;

    uint8_t* data;
    ssize_t length;
    ssize_t offset;
} buffer;
