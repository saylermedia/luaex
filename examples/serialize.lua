-- Lua Extreme example: Serialize / Deserialize library
-- See Agreement in LICENSE
-- Copyright (C) 2019, Alexey Smirnov <saylermedia@gmail.com>
--
-- how to use:
  -- serialize(...) - serialize arguments, return one argument (string)
  -- deserialize(s) - deserialize string, return arguments
  
  -- in 'C':
  -- void lua_serialize (lua_State *L, int idx, int lastidx) - serialize values from idx to lastidx and push string
  -- int lua_deserialize (lua_State *L, int idx) - deserialize string to values (push) and return count

  -- In metatable of userdata, declare functions:
  -- __serialize - convert userdata object to serialized value (must be return one result)
  -- __deserialize - create userdata from __serialize result


local function ser(v)
  print(type(v), v, serialize(v), deserialize(serialize(v)))
end

print('type', 'value', 'serialized', 'deserialized')
ser(nil)
ser(true)
ser(false)
ser(100500)
ser(17384.8751)
ser('Hello World!')

print()
print('Serialize function:')
local function fib(x)
  if x == 0 then
    return 1
  end
  return x * fib(x - 1)
end

print('original = ' .. fib(10))
print('serializeable = ' .. deserialize(serialize(fib))(10))

print()
print('Serialize object with metatable:')

local mt = {
  add = function(self, v)
    self.v = self.v + v
  end,
  sub = function(self, v)
    self.v = self.v - v
  end,
}
mt['__index'] = mt
local vec = setmetatable({v = 10}, mt)
local svec = deserialize(serialize(vec))

vec:add(8)
vec:sub(3)
svec:add(8)
svec:sub(3)

print('object', 'value')
print(vec, vec.v)
print(svec, svec.v)

local tt = {}
for i = 1, 1000 do
  table.insert(tt, i)
end

local s = serialize(tt)
print(s, #s)

print('success')



