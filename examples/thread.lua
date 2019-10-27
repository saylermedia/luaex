local function abc(a, b, c)
	print(a, b, c)
	return c, b, a
end


local t = thread(abc, 1, 2, 3)
print(t:join())
