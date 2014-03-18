#include <stdint.h>
#include <stdlib.h>

#include "lua.h"

typedef struct {
    void *(*alloc) (void *ud, void *ptr, size_t osize, size_t nsize);
    void* ud;

    uint8_t* data;
    ssize_t length;
    ssize_t offset;
} buffer;


buffer* buffer_get(lua_State* L, int pos);
buffer* buffer_new(lua_State* L);

// preallocate buffer space, can just access and set ->data after that.
void buffer_alloc(buffer* self, ssize_t amt);
void buffer_reset(buffer* self);
void buffer_set(buffer* self, unsigned const char* s,size_t len);
void buffer_empty(buffer* self);
