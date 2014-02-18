package = "buffer"
version = "0.1-1"
source = {
    url = "..."
}
description = {
    summary = "Efficient binary buffer operations.",
    detailed = [[
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
]],
}

dependencies = {
    "lua ~> 5.1"
}

build = {
    type = "builtin",
    modules = {
        buffer = "buffer.c",
        iobufferderp = "io.c"
    }
}
