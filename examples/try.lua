-- Lua Extreme example: try...catch...end
-- See Agreement in LICENSE
-- Copyright (C) 2019, Alexey Smirnov <saylermedia@gmail.com>
--
-- how to use:
  -- try
    -- ...block
  -- catch ([e])
    -- ...when exception
  -- end
  
  -- it's equal:
  -- xpcall(
    -- function ()
    -- ...block
    -- end,
    -- function (e)
    -- ...when exception
    -- end)

try
  print('Hello try!')
  error('text exception')
catch (e)
  print('message:', e)
end

local function proc()
  local r
  try
    error('raise')
  catch (e)
    r = true
  end
  return r
end

print('proc', proc())
