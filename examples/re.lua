-- Lua Extreme example: Perl-Compatible Regular Expressions library
-- See Agreement in LICENSE
-- Copyright (C) 2019, Alexey Smirnov <saylermedia@gmail.com>
--
-- how to use:
  
local str = '200 Enter (127,0,0,1,24,31)'
print(str)

local x = re.compile('(\\d+),(\\d+),(\\d+),(\\d+),(\\d+),(\\d+)')
print(x:exec(str))

print(re.match(str, '(\\d+),(\\d+),(\\d+),(\\d+),(\\d+),(\\d+)'))

for a, b, c in re.gmatch(str, '(\\d+),(\\d+)') do
  print(a, b, c)
end

print(re.gsub(str, '(\\d+),(\\d+)', '\\2,\\1'))

print('success')
