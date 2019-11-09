-- Lua Extreme example: Use big numbers
-- See Agreement in LICENSE
-- Copyright (C) 2019, Alexey Smirnov <saylermedia@gmail.com>
--
-- how to use:
  -- number(...)
  -- methods:
    -- abs()
    -- floor()
    -- ceil()
    -- trunc()
    -- round(x)
  
-- for example:
  -- local n = number("44.54353463465346346346876894768947689347634975837498578394")
	-- print(n)
  -- print(n:floor())
	-- print(n:ceil())

local function fac(x)
  if x == 0 then
    return 1
  end
  return x * fac(x - 1)
end

local function facx(x)
  local n = number(x)
  return fac(x), fac(n)
end

local p = thread.pool()
for i = 1, 50 do
  p:add(facx, i)
end
p:wait()

print('value', 'status', 'integer', 'big integer')
for i = 1, 50 do
  print(i, p[i]:join())
end

local a = number(33.58)
print('a = ', a)
print('a == 33', a == 33)
print('a == 33.8', a == 33.58)
print('a > 33.8', a > 33.58)
print('a >= 33.8', a >= 33.58)

print('round = ', a:round(1))

print('1 << 5', number(1) << 5)
print('prev >> 5', (number(1) << 5) >> 5)
