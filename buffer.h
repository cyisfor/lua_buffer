#include <stdint.h>
#include <stdlib.h>

#include "lua.h"
#include "lauxlib.h"

typedef struct {
    void *(*alloc) (void *ud, void *ptr, size_t osize, size_t nsize);
    void* ud;

    uint8_t* data;
    ssize_t length;
    ssize_t offset;
    uint8_t isSlice;

    uint8_t isConst; // this is strictly advisory... 
    // anything modifying data should error out if it is true
    // otherwise may segfault!
    
} buffer;

inline uint8_t* BUFFER_DATA(buffer* b) {
    return b->data + b->offset;
}

buffer* buffer_get(lua_State* L, int pos);
buffer* buffer_new(lua_State* L);

// preallocate buffer space, can just access and set ->data after that.
void buffer_alloc(buffer* self, ssize_t amt);
void buffer_reset(buffer* self);
void buffer_set(buffer* self, unsigned const char* s,size_t len);
void buffer_wrap(buffer* self, uint8_t* s,size_t len);

inline void buffer_wrap_const(buffer* self, const uint8_t* s, size_t len) {
    buffer_wrap(self,(uint8_t*)s, len);
    self->isConst = 1;
}
void buffer_empty(buffer* self);
buffer* buffer_slice(lua_State* L, buffer* self, int lower, int upper);

inline int buffer_assure_const(lua_State* L, buffer* self) {
    if(self->isConst == 0) {
        return luaL_error(L, "Attempt to modify constant data in a buffer!");
    }
    return 0;
}
