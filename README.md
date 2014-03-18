Lua strings normally are copy on use with interning, and NOT by reference. That makes passing binary data between lua and C a relative nightmare. This library avoids the troubles inherent in using lua strings as buffers, by making a new type that acts like strings, but copies as rarely as possible, is mutable and <i>not</i> interned, and can be passed to lua and back to C without copying twice.

Every time you lua_pushlstring on a buffer you're adding another entry to luajit's internal string lookup table, which eventually makes everything slow down. lua strings should only be used for data that's expected to repeat itself really, like function names, or keywords.

Buffers are for purely binary undecoded data, octet strings so to speak. All decoding and formatting would be in a separate module, leaving this stuff purely about shuffling around data agnostically.

Obviously buffers can't be used in modules that already use strings as buffers... like the io module that's right I'm looking at you core. But it should be relatively simple to adjust those modules, or create new ones, perhaps based on libuv, that use buffers where buffers are called for.

something like this...

    local buffer = require("buffer")
    local io = require("iobufferderp") 
    -- some third party module that uses buffers
    local mod = require("somebufferusingcmodule")
    local f = io:open({path="somedest",mode=io.WRITE})
    local buf = buffer:new(1024)
    while true do
        mod:fill(buf)
        f:write(buf)
    end
    f:close()

C code can use buffer.h to create and push around buffers. Buffers are dependent on Lua since they use a context's allocation function so that they won't skirt around any memory management being done to lua by just using the raw malloc. Other than that they're pretty pure C. You can assign to and read from buffers without worrying about the underlying lua at all.

This library is sort of erring on the side of don't copy, and not on the side of caution, so there are some issues with "two" "different" buffers both getting mutated by the same operation. I figure that's more important to do by default, since you can always wrap your buffer slice with a buffer clone, but you can't remove the buffer copy if it's inside the buffer slice to ensure independent buffer slices. It's not a big issue with buffers either, since you generally will be doing all the slicing and examination before the buffer is mutated again. But some people might get confused when they use buffer:slice(...) and what it returns is a memory <i>view</i> not a memory <i>copy</i>.
