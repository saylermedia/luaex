-- Lua Extreme example: socket library
-- See Agreement in LICENSE
-- Copyright (C) 2019, Alexey Smirnov <saylermedia@gmail.com>
--
-- how to use:

local ifr = socket.ifr()
for k, v in pairs(ifr) do
  print(k)
  for k, v1 in pairs(v) do
     print(k, '=', v1)
  end
end
  
local s = socket.tcp()
--print(s:connect('127.0.0.1', 80))
--print(s:send('GET / HTTP/1.1\r\nHOST: 127.0.0.1\r\n\r\n'))
--print(s:recv())
--s:close()
print('closed')
