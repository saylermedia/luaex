-- Lua Extreme example: Use math as metatable for numbers
-- See Agreement in LICENSE
-- Copyright (C) 2019, Alexey Smirnov <saylermedia@gmail.com>
--
-- how to use:
	-- <number variable>:<method>(...)
	-- (<number>):<method>(...)
	-- tonumber(...):<method>(...)
	
	-- for example:
		-- local n = 44.5
		-- print(n:floor())
		-- print((44.5):floor())


local n = 44.5
print('<number variable>:<method>(...)', n:floor())
print('(<number>):<method>(...)', (44.5):floor())
print('tonumber(...):<method>(...)', tonumber("44.5"):floor())

print()
print('value', 'floor', 'ceil')
for i = 1, 10 do
	local v = math.random() * 5
	print(v, v:floor(), v:ceil())
end
