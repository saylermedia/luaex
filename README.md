# luaex
The Lua programming language with CMake based build and with extreme functional

1. Added try...catch([e])...end
for example:
  try                                               xpcall(function()
    error('Exception!')              ====               error('Exception!')
  catch (e)                                         end, function (e)
    print(e)                                            print(e)
  end                                               end)
