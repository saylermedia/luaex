-- Lua Extreme example: Use big numbers (Decimal floating point arithmetic library)
-- See Agreement in LICENSE
-- Copyright (C) 2019, Alexey Smirnov <saylermedia@gmail.com>
--
-- how to use:
  -- decimal(...)
  -- methods:
    -- abs()
    -- floor()
    -- ceil()
    -- trunc()
    -- round(x)
  
-- for example:
  -- local n = decimal("44.54353463465346346346876894768947689347634975837498578394")
	-- print(n)
  -- print(n:floor())
	-- print(n:ceil())
  
local ctx = decimal.context()
ctx['prec'] = 100
decimal.context(ctx)

local function fac(x)
  if x == 0 then
    return 1
  end
  return x * fac(x - 1)
end

local function facx(x)
  decimal.context(ctx)
  local n = decimal(x)
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

local a = decimal(33.58)
print('a = ', a)
print('a == 33', a == 33)
print('a == 33.8', a == 33.58)
print('a > 33.8', a > 33.58)
print('a >= 33.8', a >= 33.58)

print('a:round(1) = ', a:round(1))

print('1 << 5', decimal(1) << 5)
print('prev >> 5', (decimal(1) << 5) >> 5)

print('a:exp() =', a:exp())
print('a:sqrt() =', a:sqrt())
print('a:ln() =', a:ln())
print('a:log10() =', a:log10())
print('a:invroot() =', a:invroot())

print('decimal.fma(3, 2, 5) =', decimal.fma(3, 2, 5))

--print(fac(decimal(50)):round(2):abs())

function print_ctx(x)
  local s = '{'
  for k, v in pairs(x) do
    if s ~= '{' then
      s = s .. ','
    end
    s = s .. k .. '=' .. v
  end
  s = s .. '}'
  print(s)
end

local ctx = decimal.context()
print_ctx(ctx)
print(ctx['prec'])
ctx['prec'] = 200
print_ctx(ctx)


