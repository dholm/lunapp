/**
 * Licensed under the MIT license:
 *  http://www.opensource.org/licenses/mit-license.php
 */

#include <iostream>
#include <cassert>

#include "Luna.hpp"

class Test {
public:
    Test(int x) : m_x(x) { }
    int getValue() const { return m_x; }
private:
    int m_x;
};

class LuaTest {
public:
    LUNA_TYPES(LuaTest);

    LuaTest(lua_State* l) :
	m_test(luaL_checknumber(l, 1))
    {
    }

    LuaTest(int x) :
	m_test(x)
    {
    }

    int getValue(lua_State* l) {
	lua_pushnumber(l, static_cast<double>(m_test.getValue()));
	return 1;
    }

    int getInstance(lua_State* l) {
	Luna<LuaTest>::push(l, new LuaTest(2), true);
	return 1;
    }
private:
    Test m_test;
};

LUNA_CLASS(LuaTest, Test, Test) = {
    LUNA_METHOD(LuaTest, getValue),
    LUNA_METHOD(LuaTest, getInstance),
    {0}
};

int main(int argc, char** argv)
{
    lua_State* l = lua_open();
    luaL_openlibs(l);

    Luna<LuaTest>::commit(l);

    luaL_dofile(l, argv[1]);

    lua_close(l);
    return 0;
}
