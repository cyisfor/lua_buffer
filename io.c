#include "buffer.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

typedef struct {
    FILE* fp;
    int closed;
} filederp;

static void douserdataderp(lua_State* L, FILE* self) {
    filederp* place = lua_newuserdata(L, sizeof(filederp));
    place->fp = self;
    place->closed = 0;
    // assuming the metatable is hanging around above our userdata
    lua_pushvalue(L, -2);
    lua_setmetatable(L,-2);
}

static void closeup(filederp* place) {
    if(!place) return;
    if(place->closed == 1) return;
    fclose(place->fp);
    place->closed = 1;
    place->fp = NULL;
}

static int io_close(lua_State* L) {
    closeup((filederp*) lua_touserdata(L, 1));
    return 0;
}

static int io_write(lua_State* L) {
    filederp* place = (filederp*) lua_touserdata(L, 1);
    FILE* self = place->fp;
    buffer* buf = (buffer*) lua_touserdata(L, 2);

    size_t offset = 0;
    for(;;) {
        size_t amt = fwrite(buf->data+offset,1,buf->length - offset,self);
        offset += amt;
        if(offset == buf->length) break;
    }
}

#if 0
this is dumb
static int io_readfull(lua_State* L) {
    filederp* place = (filederp*) lua_touserdata(L, 1);
    FILE* self = place->fp;
    buffer* buf = (buffer*) lua_touserdata(L, 2);

    size_t offset = 0;
    for(;;) {
        size_t amt = fread(buf->data+offset,1,buf->length - offset,self);
        if(amt < 0) {
            closeup(place);
            return 0;
        }
        offset += amt;
        if(offset == buf->length) break;
    }
    return 0;
}
#endif

static int io_read(lua_State* L) {
    filederp* place = (filederp*) lua_touserdata(L, 1);
    FILE* self = place->fp;
    buffer* buf = (buffer*) lua_touserdata(L, 2);

    // XXX: this is bad coupling
    buf->length += buf->offset;
    buf->offset = 0;
    size_t amt = fread(buf->data,1,buf->length,self);
    if(amt < 0)
        cleanup(place);
    lua_pushnumber(L,amt);
    return 1;
}

static int io_copy(lua_State* L) {
    filederp* reader = (filederp*) lua_touserdata(L, 1);
    filederp* writer = (filederp*) lua_touserdata(L, 2);

    char buf[0x1000];

    for(;;) {
        size_t amt = fread(buf,1,0x1000,reader->fp);
        if(amt <= 0) break;
        // XXX: technically fwrite could write less than amt...
        assert(amt == fwrite(buf,1,amt,writer->fp));
    }
    return 0;
}

enum { READ, WRITE, APPEND };

static int io_open(lua_State* L) {
    lua_pushliteral(L,"mode");
    lua_gettable(L,-2);
    int derpmode = lua_tointeger(L,-1);
    lua_pop(L,1);
    lua_pushliteral(L,"plus");
    lua_gettable(L,-2);
    int doplus = lua_toboolean(L,-1);
    lua_pop(L,1);

    char mode[4] = "";
    switch(derpmode) {
        case READ:
            mode[0] = 'r';
            break;
        case WRITE:
            mode[0] = 'w';
            break;
        case APPEND:
            mode[0] = 'a';
            break;
    };
    if(doplus) {
        mode[1] = '+';
        mode[2] = 'b';
        mode[3] = '\0';
    } else {
        mode[1] = 'b';
        mode[2] = '\0';
    }

    lua_pushliteral(L,"descriptor");
    lua_gettable(L,-2);
    FILE* self = NULL;
    if(0 == lua_isnil(L,-1)) {
        int fd = lua_tointeger(L,-1);
        self = fdopen(fd,mode);
    } else {
        lua_pushliteral(L,"path");
        lua_gettable(L,-2);
        if(0 == lua_isnil(L,-1)) {
            const char* path = lua_tostring(L, -1);
            self = fopen(path,mode);
        } else {
            luaL_error(L,"You must specify either a path or a descriptor.");
        }
    }
    if(self == NULL)
        luaL_error(L,"Could not open file: %s",strerror(errno));

    douserdataderp(L, self);
    return 1;
}

static int luaio_open(lua_State* L) {
    lua_createtable(L,0,10);

    lua_pushliteral(L,"metatable");
    lua_createtable(L,0,4);

    lua_pushliteral(L,"read");
    lua_pushcfunction(L, io_read);
    lua_rawset(L,-3);
    lua_pushliteral(L,"write");
    lua_pushcfunction(L, io_write);
    lua_rawset(L,-3);
    lua_pushliteral(L,"__gc");
    lua_pushcfunction(L, io_close);
    lua_rawset(L,-3);
    lua_pushliteral(L,"close");
    lua_pushcfunction(L, io_close);
    lua_rawset(L,-3);

    // set io.metatable leaving metatable on top
    lua_pushvalue(L, -1);
    lua_rawset(L, -4); 

    // now with metatable on top, rawset must have -4 to get at top io table.

    lua_pushliteral(L,"open");
    lua_pushcfunction(L, io_open);
    lua_rawset(L,-4);
    lua_pushliteral(L,"stdin");
    FILE* self = freopen(NULL,"rb",stdin);
    douserdataderp(L,self);
    lua_rawset(L, -4);
    lua_pushliteral(L,"stdout");
    self = freopen(NULL,"ab",stdout);
    douserdataderp(L,self);
    lua_rawset(L, -4);
    lua_pushliteral(L,"stderr");
    self = freopen(NULL,"ab",stderr);
    douserdataderp(L,self);
    lua_rawset(L, -4);
    lua_pop(L,1); // no need for metatable anymore.

    lua_pushliteral(L,"copy");
    lua_pushcfunction(L, io_copy);
    lua_rawset(L, -3);

    lua_pushliteral(L,"READ");
    lua_pushnumber(L,READ);
    lua_rawset(L, -3);
    lua_pushliteral(L,"WRITE");
    lua_pushnumber(L,WRITE);
    lua_rawset(L, -3);
    lua_pushliteral(L,"APPEND");
    lua_pushnumber(L,APPEND);
    lua_rawset(L, -3);
}
