local oldtostring = tostring
function argh(o)
    if type(o) == 'userdata' then
        local meta = getmetatable(o)
        if meta then
            local ts = meta.__tostring
            if ts then
                return ts(o)
            end
        end
    end
    return oldtostring(o)
end
tostring = argh

local buffer = require('buffer');
local a = buffer("this is a");
print(type(a))
local b = buffer(" test.");
print(a)
local c = a..b
print('concat',tostring(c))
print(c == buffer("this is a test."))
