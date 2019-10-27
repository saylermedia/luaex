# luaex
The Lua programming language with CMake based build and with extreme functional

* Added try...catch([e])...end, for example:
```lua
try
  error('Exception!')
catch (e)
  print(e)
end
 ```
 
* Added syntax constructs to support client-server architecture, for example:
```lua
server function floor(x)
  return math.floor(x)
end

print(floor(34.54343))
 ```
 server function call interception:
 ```C
 static int sscall(lua_State *L) {
  lua_pushfstring(L, "Server: %s #%d", lua_tostring(L, lua_upvalueindex(1)), lua_gettop(L));
  return 1;
}
int main() {
  lua_State *L = luaL_newstate();
  lua_setside(L, LUAEX_SCLIENT, sscall, NULL);
  ...
}
 ```
