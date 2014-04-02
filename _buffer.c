#include "buffer.h"

#include "lua.h"
#include "lauxlib.h"
#include "string.h"

#include <assert.h>

#define MIN(a,b) ((a) < (b) ? (a) : (b))

void buffer_set0(buffer* self, size_t offset, unsigned const char* s, size_t len) {
    assert(self->isConst != 1);
    assert(len+offset <= self->size);
    len = MIN(self->size-offset,len);
    memcpy(self->data+offset,s,len);
}

void buffer_zero(buffer* self, lua_Integer offset, lua_Integer amount) {
    offset = MIN(self->size-1,offset);
    amount = MIN(self->size-offset,amount);
    memset(self->data+offset,0,amount);
}

buffer* buffer_wrap(lua_State* L, uint8_t* s, size_t len) {
    // this is about as shallow as you can get!
    buffer* self = buffer_new(L,0);
    self->isConst = 0;
    self->data = s;
    self->size = len;
}

buffer* buffer_get(lua_State* L, int pos) {
    // buffer_get should leave the stack as it was when first called.
    if(lua_istable(L,pos)) {
        lua_getfield(L,pos,"self");
        if(lua_isnil(L,-1)) return NULL;
    } else if(!lua_isuserdata(L,pos)) {
        return NULL;
        if(lua_type(L,pos)==LUA_TSTRING) {
            luaL_error(L, "Huh? why a string? %s",lua_tostring(L,pos));
        }
        luaL_error(L,"Weird thing passed to buffer get %s",lua_typename(L,lua_type(L,pos)));
    }
    if(0==lua_getmetatable(L,-1)) return NULL;
    luaL_getmetatable(L,"buffer");
    // XXX: is rawequal better?
    if(1!=lua_equal(L,-1,-2)) {
        lua_pop(L,2);
        return NULL;
    }
    lua_pop(L,2);
    return (buffer*) lua_touserdata(L,-1);
}

void buffer_getsliced(lua_State* L, int pos, derpslice* result) {
    // should leave the buffer, or slice, on top of the stack
    // XXX: what this returns isn't a REAL buffer... how to differentiate?
    buffer* self = NULL;
    if(lua_objlen(L,pos)==3) {
        lua_rawgeti(L,pos,1);
        self = buffer_get(L,-1);
        result->isConst = self->isConst;
        lua_pop(L,1);
        lua_rawgeti(L,pos,2);
        result->data = self->data + lua_checkinteger(L,-1);
        lua_pop(L,1);
        lua_rawgeti(L,pos,3);
        result->size = lua_checkinteger(L,-1);
        lua_pop(L,1);
    } else {
        self = buffer_get(L,-1);
        result->isConst = self->isConst;
        result->data = self->data;
        result->size = self->size;
    }
}


/* idea: defer the slice operation.
 * buffer "slices" are {buffer,offset,amount}
 * when printing, make a dest buffer of amount, then copy, then print
 * when i/o, take offset and amount arguments. or take a lua array like above
 * buf:slice(a,b) just returns {buffer,offset,amount}
 */

void buffer_actually_slice(buffer* dest, lua_Integer dest_offset, buffer* src, lua_Integer src_offset, lua_Integer amount) {
    // We really ever use this?
    assert(0);
}

void buffer_pushslice(lua_State* L, lua_Integer offset, lua_Integer amount) {
    // takes <buffer> or -> and replaces with {<buffer>,offset,amount}
    if(!lua_objlen(L,-1)==3) {
        lua_createtable(L,3,0);
        lua_insert(L, -2);
        lua_rawseti(L, -2, 1);
    }
    lua_pushinteger(L, offset);
    lua_rawseti(L, -2, 2);
    lua_pushinteger(L, amount);
    lua_rawseti(L, -2, 3);
}

static int l_buffer_new(lua_State* L) {
    int top = lua_gettop(L);
    if(top > 1)
        luaL_error(L,"Wrong number of arguments, only one (string, or integer)");
    buffer* self = NULL;
    if(lua_isnumber(L,2)) {
        lua_Integer amount = lua_tointeger(L,2);
        buffer_new(L,amount);
    } else {
        size_t len = 0;
        unsigned const char* s = lua_tolstring(L,1,&len);
        buffer* self = buffer_new(L,len);
        memcpy(self->data,s,len);
    }

    return 1;
}

static int l_buffer_size(lua_State* L) {
    lua_Integer amount;
    buffer* self = buffer_get(L, 1);
    lua_pushinteger(L,self->size);
    return 1;
}

static int l_buffer_equals(lua_State* L) {
    int top = lua_gettop(L);
    lua_Integer sam = luaL_checkinteger(L,3);
    lua_Integer oam = luaL_checkinteger(L,6);
#define TEST(exp,result) if(exp) { lua_pushboolean(L,result); return 1; }
    TEST(sam!=oam,0);

    buffer* self = buffer_get(L,1);
    assert(self);
    lua_Integer sof = luaL_checkinteger(L,2);
    buffer* other = buffer_get(L,4);
    if(other) {
        lua_Integer oof = luaL_checkinteger(L,5);
        TEST(self==other,1);
        TEST(self->data == other->data,1);
        TEST(self->data == NULL,0);
        TEST(other->data == NULL, 0);
        TEST(0 == memcmp(self->data+sof,other->data+oof,MIN(sam,oam)),1);
    } else {
        size_t len = 0;
        const char* s = luaL_checklstring(L,2,&len);
        TEST(sam!=len,0);
        TEST(self->data==NULL,0);
        TEST(0 == memcmp(self->data+sof,s,len),1);
    }
#undef TEST

    lua_pushboolean(L,0);    
    return 1;
}

static int l_buffer_display(lua_State* L) {
    buffer* self = buffer_get(L,1);
    if(!self) {
        // luaL_error(L,"This doesn't seem to be a buffer...");
        lua_pushfstring(L,"<??? %s>",lua_typename(L,lua_type(L,1)));
        return 1;
    }
    lua_pushfstring(L,"<%sbuffer %p:%d>",self->isConst ? "const " : "", self, self->size);
    return 1;
}

static int l_buffer_tostring(lua_State* L) {
    int n = lua_gettop(L);
    buffer* self = buffer_get(L,1);
    if(!self) {
        luaL_error(L,"tostring This doesn't seem to be a buffer...");
    }
    lua_Integer offset,amount;
    if(n == 3) {
        offset = luaL_checkinteger(L,2);
        amount = luaL_checkinteger(L,3);
    } else {
        offset = 0;
        amount = self->size;
    }
    lua_pushlstring(L,self->data+offset,amount);
    return 1;
}

static int l_buffer_actually_slice(lua_State* L) {
    int n = lua_gettop(L);
    if(n!=5) {
        luaL_error(L,"Arguments: dest, destoffset, src, srcoffset, amount");
    }
    // this always copies offset+len from 2 to 1
    lua_Integer amount = luaL_checkinteger(L, 5);

    buffer* dest = buffer_get(L, 1);
    lua_Integer dest_offset = luaL_checkinteger(L,2);
    buffer* src = buffer_get(L, 3);
    lua_Integer src_offset = luaL_checkinteger(L, 4);

    src_offset = MIN(src_offset,src->size-1);
    dest_offset = MIN(dest_offset,dest->size-1);
    amount = MIN(amount,dest->size-dest_offset);
    amount = MIN(amount,src->size-src_offset);
    memcpy(dest->data+dest_offset,src->data+src_offset,amount);

    lua_pushvalue(L, 1);
    return 1;
} 

static int l_buffer_zero(lua_State* L) {
    buffer_zero(buffer_get(L,1),luaL_checkinteger(L,2),luaL_checkinteger(L,3));
    return 0;
}

static int l_buffer_clone(lua_State* L) {
    int n = lua_gettop(L);
    buffer* self = buffer_get(L,1);
    lua_Integer offset, amount;
    if(n == 3) {
        offset = luaL_checkinteger(L,2);
        amount = luaL_checkinteger(L,3);
    } else {
        offset = 0;
        amount = self->size;
    }
    buffer* newer = buffer_new(L,amount);
    buffer_set0(newer,0, self->data+offset,amount);
    return 1;
}

static int l_buffer_fill(lua_State* L) {
    int n = lua_gettop(L);
    if(n!=2 && n!=5) {
        luaL_error(L,"Wrong number of arguments.");
    }
    buffer* self = buffer_get(L,1);
    buffer* other = buffer_get(L,2);
    lua_Integer oof, sof, amount;
    if(n == 5) {
        oof = luaL_checkinteger(L,3);
        sof = luaL_checkinteger(L,4);
        amount = luaL_checkinteger(L,5);
        amount = MIN(amount,self->size-sof);
    }
    if(other) {
        if(n == 2) {
            memcpy(self->data,other->data,MIN(self->size,other->size));
        } else if(n == 5) {
            amount = MIN(amount,other->size-oof);
            memcpy(self->data+sof,other->data+oof,amount);
        } else {
            luaL_error(L,"Wrong arguments, either copy a whole buffer or provide offsets + size");
        }
        return 0;
    }
    size_t len;
    const char* s = lua_tolstring(L,2,&len);
    if(n==2) {
        memcpy(self->data,s,MIN(self->size,len));
    } else if(n == 5) {
        oof = MIN(len-1,oof);
        amount = MIN(amount,len-oof);
        memcpy(self->data+sof,s+oof,amount);
    }
    return 0;
}

static void buffer_init(lua_State* L, buffer* self) {
    // add methods etc to buffer userdata
    lua_createtable(L, 0, 6);
    lua_pushliteral(L,"self");
    lua_pushvalue(L,-3);
    if(!lua_isuserdata(L,-1))
        luaL_error(L,"Where'd the buffer go??");
    lua_rawset(L,-3);

    lua_pushliteral(L,"size");
    lua_pushinteger(L,self->size);
    lua_rawset(L,-3);

#define FUNC(a) { lua_pushliteral(L,#a); lua_pushcfunction(L, l_buffer_ ## a); lua_rawset(L,-3); }
    FUNC(new);
    FUNC(equals);
    FUNC(fill);
    FUNC(display);
    FUNC(tostring);
    FUNC(actually_slice);
    FUNC(zero);
    FUNC(clone);
#undef FUNC
    luaL_getmetatable(L,"buffer");
    lua_setmetatable(L,-2);
}

buffer* buffer_new(lua_State* L, lua_Integer amt) {
    // note this pushes the buffer onto the stack, so returning 1 after calling this
    // will return the buffer it produces.
    buffer* self = (buffer*) lua_newuserdata(L, sizeof(buffer)+amt);
    if(amt == 0) {
        self->data = NULL;
        self->size = 0;
        self->isConst = 1;
    } else {
        self->data = ((uint8_t*)self) + sizeof(buffer);
        self->size = amt;
        self->isConst = 0;
    }

    luaL_getmetatable(L,"buffer");
    lua_setmetatable(L,-2);

    buffer_init(L,self);
}

    
int luaopen__buffer(lua_State* L) {    
    if (luaL_newmetatable(L,"buffer") != 0) {
        lua_pushliteral(L,"__len");
        lua_pushcfunction(L,l_buffer_size);
        lua_settable(L,-3);
        lua_pushliteral(L,"__tostring");
        lua_pushcfunction(L,l_buffer_display);
        lua_settable(L,-3);
        lua_pop(L,1);
    }

    lua_pushcfunction(L,l_buffer_new);
    return 1;
}

