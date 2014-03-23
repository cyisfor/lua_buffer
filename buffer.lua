local makeRaw = require('_buffer');

sliceMeta = {
    __eq = function(a,b)
        return a[3] == b[3] and a[1]:equals(a[2],a[3],b[1],b[2],b[3])
    end,
    __tostring = function(this)
        return '<slice '..this[1]..' ('..this[2]..','..this[3]')>'
    end,
    __len = function()
        return amount
    end
}

function make(init)
    if init == nil then init = 0x1000 end
    raw = makeRaw(init)
    function makeSlice(this,offset,amount)
        if offset then
            offset = math.min(offset,raw.size-1)
        else
            offset = 0
        end
        if amount then
            amount = math.min(amount,raw.size-offset)
        else
            amount = amount or (raw.size - offset)
        end
        slice = {raw,offset,amount}
        -- XXX: we could assert whether offset/amount fit inside the slice, not just the buffer.
        slice.slice = makeSlice 
        slice.actually_slice = function()
            dest = make(amount)
            raw.actually_slice(dest,0,raw,offset,amount)
            return dest
        end
        slice.zero = function()
            raw:zero(offset,amount)
        end
        slice.clone = function()
            dest = make(amount)
            dest.actually_slice(dest,0,raw,offset,amount)
            return dest
        end
        slice.tostring = function()
            return raw:tostring(offset,amount)
        end

        setmetatable(slice,sliceMeta)
        return slice
    end
    raw.slice = makeSlice
    meta = getmetatable(raw)
    meta.__eq = function(a,b)
        return a:equals(0,a.size,b,0,b.size)
    end
    return raw
end

return make
