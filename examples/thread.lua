local function abc(a, b, c)
	print(a, b, c)
	return c, b, a
end

local t = thread(abc, 1, 2, 3)
print(t:join())

local function fib(x)
	local r = 0
	for i = 1, x do
		r = r + i
	end
	return r
end

local ar = {}
for i = 1, 10 do
	table.insert(ar, thread(fib, i * 1000))
end

local cont = true
while cont do
	cont = false
	for _, v in ipairs(ar) do
		if v:running() then
			cont = true
			break
		end
	end
end

for _, v in ipairs(ar) do
	print(v, v:running(), v:join())
end
