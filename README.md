Lua strings normally are copy on use, and NOT by reference. That makes passing binary data between lua and C a relative nightmare. Who knows what lua does internally, but the C API does not expose that. So this library will sort of skirt around that by making a new type that acts like strings,
but copies as rarely as possible, and can be passed to lua and back to C without copying twice.

This is for purely binary undecoded data, octet strings so to speak. All decoding and formatting is deferred as far as possible, leaving this stuff purely about shuffling around data agnostically.

something like this...

    local buffer = require("buffer")
    local io = require("iobufferderp")
    local mod = require("somebufferusingcmodule")
    local f = io:open({path="somedest",mode=io.WRITE})
    local buf = buffer:new(1024)
    while true do
        mod:fill(buf)
        f:write(buf)
    end
    f:close()
