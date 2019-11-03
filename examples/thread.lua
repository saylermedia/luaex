-- Lua Extreme example: Native threads library
-- See Agreement in LICENSE
-- Copyright (C) 2019, Alexey Smirnov <saylermedia@gmail.com>
--
-- how to use:
  -- thread(f, ...) - call function f(...) in new thread, return thread object
  -- thread.join(t) - join thread, return status (true or false) and results (if status true) or error (if status false)
  -- thread.cancel(t) - cancel execution
  -- thread.running(t) - thread is running
  -- thread.interrupt() - set interrupted flag to true
  -- thread.interrupted([t]) - flag is interrupted
  -- thread.id([t]) - get current thread id
  -- thread.time() - get current time in milliseconds
  -- thread.sleep(time) - pause in milliseconds
  -- thread.pool() - create thread pool, return pool object
    -- :add(f, ...) - call function f(...) in new thread and store to pool, return thread object
    -- :wait() - wait until all threads are completed
    -- :interrupt() - set interrupted flag to true for all threads
    -- :cancel() - cancel execution for all threads

  -- join, running, interrupt, interrupted, id is thread object methods.
  -- in called function to all arguments or local variables is serialized copies.

  
local time = thread.time()
local d = 100

print('call function in new thread:')

local function abc(a, b, c)
  print('a', 'b', 'c', 'd', 'time')
  print(a, b, c, d, time)
  print()
  print('#', 'id', 'interrupted')
  print('0', thread.id(), thread.interrupted())
  thread.sleep(200)
  print('1', thread.id(), thread.interrupted())
  return c, b, a
end

local t = thread(abc, 1, 2, 3)
thread.sleep(100)
t:interrupt()
t:join()

print()
print('results:')
print('status', 'c', 'b', 'a')
print(t:join())

print()
print('calculating factorial:')

local function fac(x)
  if x == 0 then
    return 1
  end
  return x * fac(x - 1)
end


local pool = thread.pool()
for i = 1, 10 do
  print('new thread fac(' .. i .. ')') 
  pool:add(fac, i)
end

print('wait until all threads are completed') 
pool:wait()

print('result:')
print('value', 'id', 'status', 'result')
for i = 1, #pool do
  print(i, pool[i]:id(), pool[i]:join())
end

print()
print('elapsed ' .. (thread.time() - time) .. ' milliseconds')

local function loop()
  while true do
    thread.sleep(10000)
  end
end

local t = thread(loop)
thread.sleep(500)
t:cancel()
print('cancel:', t:join())
