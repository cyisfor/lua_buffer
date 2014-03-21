#include "buffer.h"

#include "lua.h"
#include "lauxlib.h"
#include "string.h"

buffer* buffer_new(lua_State* L);

void buffer_alloc(buffer* self, ssize_t amt) {
    // this may truncate data already in the buffer
    self->data = self->alloc(self->ud,self->data,self->length+self->offset,amt+self->offset);
    self->isConst = 0;
    self->length = amt;
}

void buffer_reset(buffer* self) {
    // prepare for writing, discard any offsets
    self->length += self->offset;
    self->offset = 0;
}

void buffer_set(buffer* self, unsigned const char* s,size_t len) {
    self->data = self->alloc(self->ud,self->data,self->length + self->offset,len);
    self->isConst = 0;
    memcpy(self->data,s,len);
    self->length = len;
    self->offset = 0;
}

void buffer_wrap(buffer* self, uint8_t* s,size_t len) {
    // this is about as shallow as you can get!
    self->isSlice = 1;
    self->data = s;
    self->length = len;
    self->offset = 0;
}

void buffer_empty(buffer* self) {
    if(self->isSlice == 1) {
        self->isSlice = 0;
    } else {
        self->alloc(self->ud,self->data,self->length + self->offset,0);
    }
    self->data = NULL;
    self->isConst = 1;
    self->length = self->offset = 0;
}

static int l_buffer_new(lua_State* L) {
    int top = lua_gettop(L);
    buffer* self = buffer_new(L);

    if(top > 1)
        luaL_error(L,"Wrong number of arguments, only one (string, or integer)");
    if(top == 1) {
        if(lua_isnumber(L,2)) {
            buffer_alloc(self, lua_tointeger(L,1));
        } else {
            size_t len = 0;
            unsigned const char* s = lua_tolstring(L,1,&len);
            buffer_set(self,s,len);
        }
    } 

    return 1;
}

buffer* buffer_get(lua_State* L, int pos) {
    if(!lua_istable(L,pos)) return NULL;
    lua_getfield(L,pos,"self");
    return (buffer*) lua_touserdata(L,-1);
}

buffer* buffer_slice(lua_State* L, buffer* self, int lower, int upper) {
    buffer* slice = buffer_new(L);
    slice->isSlice = 1;
    slice->data = self->data;
    slice->offset = lower;
    slice->length = upper - lower;
    return slice;
}

static int l_buffer_concat(lua_State* L) {
    buffer* old = buffer_get(L,1);
    buffer* other = buffer_get(L,2);
    buffer* self = buffer_new(L);
    buffer_alloc(self,old->length + other->length);
    memcpy(self->data, old->data + old->offset, old->length);
    memcpy(self->data+old->length, other->data + other->offset, other->length);
    return 1;
}

static int l_buffer_length(lua_State* L) {
    buffer* self = buffer_get(L, 1);
    lua_pushinteger(L,self->length);
    return 1;
}

static int l_buffer_equal(lua_State* L) {
    int top = lua_gettop(L);
    buffer* self = buffer_get(L,1);
    buffer* other = buffer_get(L,2);
    if(other) {
        lua_pushboolean(L,
                (self == other || 
                 (self->data == NULL && other->data == NULL) ||
                 (self->length == other->length && 
                    0 == memcmp(self->data+self->offset,
                        other->data+other->offset,
                        self->length))) ?
                1 :
                0);
    } else {
        size_t len = 0;
        const char* s = lua_tolstring(L,2,&len);
        lua_pushboolean(L,
                ((s == NULL && self->data == NULL) ||
                 (self->length == len &&
                     0 == memcmp(self->data+self->offset,s,len))) ?
                1 :
                0);
    }
        
    return 1;
}

static int l_buffer_display(lua_State* L) {
    buffer* self = buffer_get(L,1);
    lua_pushfstring(L,"<buffer %p:%d:%d>",self,self->offset,self->length);
    return 1;
}

static int l_buffer_tostring(lua_State* L) {
    buffer* self = buffer_get(L,1);
    lua_pushlstring(L,self->data+self->offset,self->length);
    return 1;
}

static int l_buffer_slice(lua_State* L) {
    // this doesn't copy the data, so changing the slice changes the original buffer's contents!
    // slice(lower bound, upper bound)
    
    if(lua_isnil(L,2) && lua_isnil(L,3)) {
        lua_pushvalue(L,1);
        return 1;
    }

    buffer* self = buffer_get(L, 1);
    lua_Integer lower, upper;
    if (lua_isnil(L,2)) {
        lower = self->offset;
    } else {
        lower = lua_tointeger(L, 2) + self->offset;
    }
    if(lua_isnil(L,3)) {
        upper = self->length + self->offset;
    } else {
        upper = lua_tointeger(L, 3);
    }
    buffer_slice(L,self,lower,upper);

    return 1;
}

static int l_buffer_zero(lua_State* L) {
    buffer* self = buffer_get(L,1);
    memset(self->data+self->offset, 0, self->length);
    return 0;
}

static int l_buffer_set(lua_State* L) {
    buffer* self = buffer_get(L,1);
    buffer_empty(self);
    buffer* other = buffer_get(L, 2);
    if(other) {
        // this is about as shallow as assignment can get.
        self->data = other->data;
        self->length = other->length;
        self->offset = other->offset;
        self->isSlice = 1;
        self->isConst = other->isConst;
    } else if(lua_isstring(L, 2)) {
        size_t len = 0;
        unsigned const char* s = lua_tolstring(L, 2, &len);
        buffer_set(self,s,len);
    } else {
        luaL_error(L, "Can only set from a string or another buffer");
    }
    return 0;
}

static int l_buffer_clear(lua_State* L) {
    buffer* self = buffer_get(L,1);
    buffer_empty(self);
    return 0;
}

static int l_buffer_clone(lua_State* L) {
    buffer* self = buffer_get(L,1);
    buffer* newer = buffer_new(L);
    buffer_set(newer,self->data+self->offset,self->length);
    return 1;
}

static int l_buffer_consolidate(lua_State* L) {
    // aka chop out a piece then throw the rest away
    // any slices to this buffer, including the original, may become kaput
    // buf = buf:slice(5,10):consolidate()
    // slightly more efficient than
    // buf = buf:slice(5,10):clone()
    buffer* self = buffer_get(L,1);
    lua_pushvalue(L,1);
    if(self->isSlice) {
        memmove(self->data, self->data + self->offset, self->length);
        buffer_alloc(self,self->length);
        self->isSlice = 0;
    }
    return 1;
}

buffer* buffer_new(lua_State* L) {
    // note this pushes the buffer onto the stack, so returning 1 after calling this
    // will return the buffer it produces.
    buffer* self = (buffer*) lua_newuserdata(L, sizeof(buffer));
    self->alloc = lua_getallocf(L,&self->ud);
    self->data = NULL;
    self->length = self->offset = 0;
    self->isSlice = 0;
    self->isConst = 1;

    // add methods etc to buffer userdata
    lua_createtable(L,0, 6);
    lua_pushliteral(L,"self");
    lua_pushvalue(L,-3);
    if(!lua_isuserdata(L,-1))
        luaL_error(L,"Where'd the buffer go??");
    lua_rawset(L,-3);

    lua_pushliteral(L,"slice");
    lua_pushcfunction(L,l_buffer_slice);
    lua_rawset(L,-3);
    lua_pushliteral(L,"zero");
    lua_pushcfunction(L,l_buffer_zero);
    lua_rawset(L,-3);
    lua_pushliteral(L,"set");
    lua_pushcfunction(L,l_buffer_set);
    lua_rawset(L,-3);
    lua_pushliteral(L,"clear");
    lua_pushcfunction(L,l_buffer_clear);
    lua_rawset(L,-3);
    lua_pushliteral(L,"clone");
    lua_pushcfunction(L,l_buffer_clone);
    lua_rawset(L,-3);
    lua_pushliteral(L,"consolidate");
    lua_pushcfunction(L,l_buffer_consolidate);
    lua_rawset(L,-3);
    lua_pushliteral(L,"string");
    lua_pushcfunction(L,l_buffer_tostring);
    lua_rawset(L,-3);
    
    luaL_getmetatable(L,"buffer");
    lua_setmetatable(L,-2);
    return self;
}

    
int luaopen_buffer(lua_State* L) {    
    if (luaL_newmetatable(L,"buffer") != 0) {
        lua_pushliteral(L,"__concat");
        lua_pushcfunction(L,l_buffer_concat);
        lua_settable(L,-3);
        lua_pushliteral(L,"__len");
        lua_pushcfunction(L,l_buffer_length);
        lua_settable(L,-3);
        lua_pushliteral(L,"__eq");
        lua_pushcfunction(L,l_buffer_equal);
        lua_settable(L,-3);
        lua_pushliteral(L,"__tostring");
        lua_pushcfunction(L,l_buffer_display);
        lua_settable(L,-3);
        lua_pushliteral(L,"__gc");
        lua_pushcfunction(L,l_buffer_clear);
        lua_settable(L,-3);
        lua_pop(L,1);
    }

    lua_pushcfunction(L,l_buffer_new);
    return 1;
}

