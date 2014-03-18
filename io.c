#include "buffer.h"

#include <string.h>
#include <errno.h>
#include <assert.h>

#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h> // *_FILENO


/* XXX: this shouldn't be in the buffer module.
 * it's just an example of a module using buffers,
 * to facilitate file I/O
 */

typedef struct {
    int fd;
    int closed;
} FileInfo;


static FileInfo* io_get(lua_State* L, int pos) {
    FileInfo* self = NULL;
    if(lua_istable(L,pos)) {
        lua_getfield(L,pos,"d");
        if(lua_isuserdata(L,-1)) {
            self = (FileInfo*)lua_touserdata(L,-1);
        }
    }
    return self;
}

static void closeup(FileInfo* place) {
    if(!place) return;
    if(place->closed == 1) return;
    close(place->fd);
    place->closed = 1;
    place->fd = -1;
}

static int io_close(lua_State* L) {
    closeup(io_get(L, 1));
    return 0;
}

static int io_write(lua_State* L) {
    FileInfo* place = io_get(L, 1);
    int self = place->fd;
    buffer* buf = buffer_get(L, 2);

    size_t offset = 0;
    for(;;) {
        size_t amt = write(self,buf->data+offset,buf->length - offset);
        offset += amt;
        if(offset == buf->length) break;
    }
}

#if 0
this is dumb
static int io_readfull(lua_State* L) {
    FileInfo* place = (FileInfo*) lua_touserdata(L, 1);
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
    FileInfo* place = io_get(L, 1);
    int fd = place->fd;
    buffer* buf = buffer_get(L, 2);
    // returns either a new buffer or the existing one
    if(buf==NULL) {
        buf = buffer_new(L);
        buffer_alloc(buf,0x1000);
    } else {
        lua_pushvalue(L, 2);
    }

    buffer_reset(buf);

    size_t amt = read(fd,buf->data,buf->length);
    if(amt < 0)
        cleanup(place);
    lua_pushnumber(L,amt);
    return 1;
}

static int io_copy(lua_State* L) {
    FileInfo* reader = io_get(L, 1);
    if(reader==NULL)
        luaL_error(L,"The reader argument 1 has to be an io object");
    FileInfo* writer = io_get(L, 2);
    if(writer==NULL)
        luaL_error(L,"The writer argument 2 has to be an io object");

    int rd = reader->fd;
    int wd = writer->fd;

    char buf[0x1000];

    for(;;) {
        size_t amt = read(rd,buf,0x1000);
        if(amt <= 0) break;
        // XXX: technically write could write less than amt...
        size_t offset = 0;
        for(;;) {
            size_t derpamt = write(wd, buf + offset, amt - offset);

            if(derpamt < 0)
                luaL_error(L,"write error");
            else if(derpamt == 0)
                luaL_error(L, "couldn't write some that we read. Do it in libuv instead :p");

            offset += derpamt;
            if(offset == amt) break;
        }
    }
    return 0;
}

static FileInfo* newplace(lua_State* L, int fd) {
    lua_createtable(L,0,3);

    lua_pushliteral(L,"d");
    FileInfo* place = lua_newuserdata(L, sizeof(FileInfo));
    place->fd = fd;
    place->closed = 0;
    lua_rawset(L,-3);
    lua_pushliteral(L,"read");
    lua_pushcfunction(L, io_read);
    lua_rawset(L,-3);
    lua_pushliteral(L,"write");
    lua_pushcfunction(L, io_write);
    lua_rawset(L,-3);
    
    // assuming the metatable is hanging around above our new object
    lua_pushvalue(L, -2);
    lua_setmetatable(L,-2);
    return place;
}
// sigh file descriptor... need libuv...

enum { READ, WRITE, APPEND };

static int io_open(lua_State* L) {
    lua_pushliteral(L,"mode");
    lua_gettable(L,-2);
    int hasflags = lua_isnil(L,-1);
    int flags = O_RDWR | O_CREAT; // sensible default
    if(hasflags)
        flags = lua_tointeger(L,-1);
    lua_pop(L,1);
    lua_pushliteral(L,"plus");
    lua_gettable(L,-2);
    int doplus = lua_toboolean(L,-1);
    lua_pop(L,1);

    if(doplus) {
        switch(flags) {
            case READ:
                flags = O_RDWR;
                break;
            case WRITE:
                flags = O_RDWR | O_TRUNC | O_CREAT;
                break;
            case APPEND:
                flags = O_RDONLY | O_APPEND | O_CREAT;
                break;
            case O_RDWR | O_CREAT:
                break;
            default:
                luaL_error(L,"flags must be read, write or append");
        };
    } else {
        switch(flags) {
            case READ:
                flags = O_RDONLY;
                break;
            case WRITE:
                flags = O_WRONLY | O_TRUNC | O_CREAT;
                break;
            case APPEND:
                flags = O_APPEND | O_CREAT;
                break;
            case O_RDWR | O_CREAT:
                break;
            default:
                luaL_error(L,"flags must be read, write or append");
        };
    }

    lua_pushliteral(L,"descriptor");
    lua_gettable(L,-2);
    int self = -1;
    if(0 == lua_isnil(L,-1)) {
        self = lua_tointeger(L,-1);
        if(hasflags || doplus) {
            fcntl(self,F_SETFL,fcntl(self,F_GETFL) | flags);
        }
    } else {
        lua_pushliteral(L,"path");
        lua_gettable(L,-2);
        if(0 == lua_isnil(L,-1)) {
            const char* path = lua_tostring(L, -1);
            self = open(path,flags);
        } else {
            luaL_error(L,"You must specify either a path or a descriptor.");
        }
    }
    if(self == -1)
        luaL_error(L,"Could not open file: %s",strerror(errno));

    newplace(L, self);
    return 1;
}

static int luaio_open(lua_State* L) {
    lua_createtable(L,0,10);

    lua_pushliteral(L,"metatable");
    lua_createtable(L,0,4);

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
    newplace(L,STDIN_FILENO);
    lua_rawset(L, -4);
    lua_pushliteral(L,"stdout");
    newplace(L,STDOUT_FILENO);
    lua_rawset(L, -4);
    lua_pushliteral(L,"stderr");
    newplace(L,STDERR_FILENO);
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
