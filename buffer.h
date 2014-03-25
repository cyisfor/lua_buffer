#include <stdint.h>
#include <stdlib.h>

#include "lua.h"
#include "lauxlib.h"

typedef struct {
    uint8_t* data;
    ssize_t size;
    uint8_t isConst; // this is strictly advisory... 
    // anything modifying data should error out if it is true
    // otherwise may segfault!    
} buffer;

typedef struct {
    uint8_t* data;
    ssize_t size;
    uint8_t isConst;
} derpslice; // derp...
// this is what lua slices become coming to the C side...
// just as stack variables... XXX: why is this so weird

buffer* buffer_new(lua_State* L, lua_Integer amount);

void buffer_setO(buffer* self, size_t offset, unsigned const char* s, size_t len);
inline void buffer_set(buffer* self, unsigned const char* s, size_t len) {
    buffer_setO(self, 0, s, len);
}

void buffer_zero(buffer* self, lua_Integer offset, lua_Integer amount);
buffer* buffer_wrap(lua_State* L, uint8_t* s, size_t len);
buffer* buffer_get(lua_State* L, int pos);
void buffer_getsliced(lua_State* L, int pos, derpslice* result);

void buffer_pushslice(lua_State* L, lua_Integer offset, lua_Integer amount);

inline buffer* buffer_wrap_const(lua_State* L, const uint8_t* s, size_t len) {
    buffer* self = buffer_new(L, 0);
    self->data = (uint8_t*) s;
    self->size = len;
    self->isConst = 1;
    return self;
}

inline int buffer_assure_const(lua_State* L, buffer* self) {
    if(self->isConst == 0) {
        return luaL_error(L, "Attempt to modify constant data in a buffer!");
    }
    return 0;
}
