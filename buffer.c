#include "buffer.h"

#include "lua.h"
#include "string.h"

static void buffer_alloc(buffer* self, ssize_t amt) {
    self->data = self->alloc(NULL,self->data,1,amt);
    self->length = amt;
    self->offset = 0;
}

static void buffer_set(buffer* self, unsigned const char* s,size_t len) {
    self->data = self->alloc(NULL,self->data,1,len);
    memcpy(self->data,s,len);
    self->length = len;
    self->offset = 0;
}

static void buffer_empty(buffer* self) {
    self->length = self->offset = 0;
    self->data = self->alloc(NULL,self->data,1,0);
}

#include "lua.h"

int l_buffer_new(lua_State* L) {
    int top = lua_gettop(L);
    buffer* self = (buffer*) lua_newuserdata(L,sizeof(buffer));
    self->alloc = lua_getallocf(L,NULL);
    if(top > 1)
        luaL_error(L,"Wrong number of arguments, only one (string, or integer)");
    if(top == 1) {
        if(lua_isnumber(L,1)) {
            buffer_alloc(self, lua_tointeger(L,1));
        } else {
            size_t len = 0;
            unsigned const char* s = lua_tolstring(L,1,&len);
            buffer_set(self,s,len);
        }
    } else {
        buffer_empty(self);
    }

    lua_pushliteral(L,"metatable");
    lua_gettable(L,-3);
    lua_setmetatable(L,-2);
    return 1;
}

int l_buffer_concat(lua_State* L) {
    buffer* old = lua_touserdata(L,1);
    buffer* other = lua_touserdata(L,2);
    buffer* self = (buffer*) lua_newuserdata(L,sizeof(buffer));
    self->alloc = lua_getallocf(L,NULL);
    buffer_alloc(self,old->length - old->offset + other->length - other->offset);
    memcpy(self->data, old->data+old->offset, old->length);
    memcpy(self->data+old->length-1, other->data+other->offset, other->length);
    return 1;
}

int l_buffer_length(lua_State* L) {
    buffer* self = lua_touserdata(L, 1);
    lua_pushinteger(L,self->length);
    return 1;
}

int l_buffer_equal(lua_State* L) {
    buffer* self = lua_touserdata(L,1);
    buffer* other = lua_touserdata(L,2);
    lua_pushboolean(L,
            self->length == other->length && 0 == memcmp(self->data+self->offset,
                other->data+other->offset,
                self->length) ?
            1 :
            0);
}

int l_buffer_slice(lua_State* L) {
    // this doesn't copy the data, so changing the slice changes the original buffer's contents!
    buffer* self = lua_touserdata(L, 1);
    lua_Integer lower = lua_tointeger(L, 2);
    lua_Integer upper = lua_tointeger(L, 3);
    buffer* slice = lua_newuserdata(L, 1);
    slice->data = self->data;
    slice->offset = lower;
    slice->length = upper - lower;
    return 1;
}

int l_buffer_copy(lua_State* L) {    
    buffer* self = lua_touserdata(L,1);
    buffer* other = lua_touserdata(L,2);
    buffer_empty(self);
    self->data = other->data;
    self->length = other->length;
    self->offset = other->offset;
    lua_pushvalue(L, 1); // return self
    return 1;
}

int l_buffer_zero(lua_State* L) {
    buffer* self = lua_touserdata(L,1);
    memset(self->data+self->offset, 0, self->length);
    return 0;
}

int l_buffer_set(lua_State* L) {
    buffer* self = lua_touserdata(L,1);
    buffer_empty(self);
    if(lua_isuserdata(L, 2)) {
        buffer* other = lua_touserdata(L, 2);
        self->data = other->data;
        self->length = other->length;
        self->offset = other->offset;
    } else if(lua_isstring(L, 2)) {
        size_t len = 0;
        unsigned const char* s = lua_tolstring(L, 2, &len);
        buffer_set(self,s,len);
    } else {
        luaL_error(L, "Can only set from a string or another buffer");
    }
    return 0;
}

int l_buffer_clear(lua_State* L) {
    buffer* self = lua_touserdata(L,1);
    buffer_empty(self);
    return 0;
}

int luabuffer_init(lua_State* L) {    
    lua_createtable(L,0,2);

    lua_pushliteral(L,"metatable");
    lua_createtable(L,0,8);

    lua_pushliteral(L,"__concat");
    lua_pushcfunction(L,l_buffer_concat);
    lua_settable(L,-3);
    lua_pushliteral(L,"__len");
    lua_pushcfunction(L,l_buffer_length);
    lua_settable(L,-3);
    lua_pushliteral(L,"__eq");
    lua_pushcfunction(L,l_buffer_equal);
    lua_settable(L,-3);
    lua_pushliteral(L,"slice");
    lua_pushcfunction(L,l_buffer_slice);
    lua_settable(L,-3);
    lua_pushliteral(L,"assign");
    lua_pushcfunction(L,l_buffer_copy);
    lua_settable(L,-3);
    lua_pushliteral(L,"zero");
    lua_pushcfunction(L,l_buffer_zero);
    lua_settable(L,-3);
    lua_pushliteral(L,"set");
    lua_pushcfunction(L,l_buffer_set);
    lua_settable(L,-3);
    lua_pushliteral(L,"clear");
    lua_pushcfunction(L,l_buffer_clear);
    lua_settable(L,-3);

    lua_settable(L,-3);
    lua_pushliteral(L,"new");
    lua_pushcfunction(L,l_buffer_new);
    lua_settable(L,-3);
    return 1;
}

