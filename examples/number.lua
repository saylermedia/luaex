
print(tonumber("12.4555"):floor())
print((45.45454):floor())

local v = 5.434429801
for i = 1, 10 do
	v = v * i / 2.245
	print(v, v:floor(), v:ceil())
end
