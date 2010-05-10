/**
 * Modified Luna from http://kuchen.googlecode.com/svn/trunk/Pong/Luna.h
 *
 * Sample binding to the Foo class with the Foo::call method.
 *
 * class Foo {
 * public:
 *   // Declares the types required by Luna (must be public)
 *   LUNA_TYPES(Foo);
 *
 *   Foo(lua_State* state) {
 *     ...
 *   }
 *
 *   int call(lua_State* state) {
 *     ...
 *     return 0;
 *   }
 * };
 *
 * // Binds the C++ class Foo using the Lua name Foo in Lua via the LuaFoo
 * // binding and exposing the call method.
 * LUNA_CLASS(LuaFoo, Foo, Foo) = {
 *     LUNA_METHOD(LuaFoo, call),
 *     {0}
 * };
 */

#ifndef LUNA_HPP
#define LUNA_HPP

#include <cstring>
#include <typeinfo>

#include <lua.hpp>

#define LUNA_TYPES(T) \
    static const std::type_info* s_lunaTypeInfoPtr; \
    static const char s_lunaClassName[]; \
    static Luna<T>::MethodMap s_lunaMethods[]

#define LUNA_CLASS(T, MAPS_T, CLASS_NAME) \
    const std::type_info* T::s_lunaTypeInfoPtr(&typeid(MAPS_T)); \
    const char T::s_lunaClassName[] = #CLASS_NAME; \
    Luna<T>::MethodMap T::s_lunaMethods[]

#define LUNA_METHOD(T, NAME) {#NAME, &T::NAME}

/**
 * A helper for registering and working with C++ objects and types in Lua.
 * \tparam T C++ class type
 */
template <typename T>
class Luna {
    typedef struct { T* pT; } Userdata;

public:
    typedef int Index; /**< Lua table index type */

    typedef int (T::*Method)(lua_State* l); /**< Luna method type */

    typedef struct {
        const char* name;
        Method method;
    } MethodMap; /**< Mapping of method name to method */

    /**
     * Commit the class into Lua's global registry.
     * \param l Lua state
     */
    static void commit(lua_State* l) {
        lua_newtable(l);
        int methods = lua_gettop(l);

        luaL_newmetatable(l, T::s_lunaClassName);
        int metatable = lua_gettop(l);

        lua_pushvalue(l, methods);
	set(l, LUA_GLOBALSINDEX, T::s_lunaClassName);

        lua_pushvalue(l, methods);
	set(l, metatable, "__metatable");

        lua_pushvalue(l, methods);
	set(l, metatable, "__index");

        lua_pushcfunction(l, tostringT);
	set(l, metatable, "__tostring");

        lua_pushcfunction(l, gcT);
	set(l, metatable, "__gc");

        lua_newtable(l);
        lua_pushcfunction(l, newT);
	lua_pushvalue(l, -1);
	set(l, methods, "new");
	set(l, -3, "__call");
        lua_setmetatable(l, methods);

        for (MethodMap* m = T::s_lunaMethods; m->name; ++m) {
            lua_pushstring(l, m->name);
            lua_pushlightuserdata(l, reinterpret_cast<void*>(m));
            lua_pushcclosure(l, thunk, 1);
            lua_settable(l, methods);
        }

        TypeInfo::commit(l, methods);

        lua_pop(l, 2);
    }

    /**
     * Push an object instance onto the stack.
     * \param l Lua state
     * \param instance Pointer to object instance
     * \param gc If true let Lua garbage collect the instance
     * \returns Index of instance metatable
     */
    static Index push(lua_State* l, T* instance, bool gc = false) {
        if (!instance) {
            lua_pushnil(l);
            return 0;
        }

        luaL_getmetatable(l, T::s_lunaClassName);
        if (lua_isnil(l, -1)) {
            luaL_error(l, "[Luna::%s] Class %s has not been commited!", __func__, T::s_lunaClassName);
            return 0;
        }
        Index metatable = lua_gettop(l);

        subtable(l, metatable, "userdata", "v");
        Userdata* userdata = allocUserdata(l, metatable, instance, sizeof(Userdata));
        if (userdata) {
            userdata->pT = instance;
            lua_pushvalue(l, metatable);
            lua_setmetatable(l, -2);
            if (!gc) {
                lua_checkstack(l, 3);
                subtable(l, metatable, "unmanaged", "k");
                lua_pushvalue(l, -2);
                lua_pushboolean(l, 1);
                lua_settable(l, -3);
                lua_pop(l, 1);
            }
        }
        lua_replace(l, metatable);
        lua_settop(l, metatable);
        return metatable;
    }

    /**
     * Return the object of type T from the argument narg.
     * \param l Lua state
     * \param narg Argument index
     * \returns Pointer to object or NULL if it isn't of type T
     */
    static T* check(lua_State* l, int narg) {
        Userdata* ud = reinterpret_cast<Userdata*>(luaL_checkudata(l, narg, T::s_lunaClassName));
        if (!ud) {
            luaL_typerror(l, narg, T::s_lunaClassName);
        }
        return ud->pT;
    }

private:
    Luna();

    class TypeInfo {
    public:
        static void commit(lua_State* l, int methods) {
            lua_getglobal(l, typeid(T).name());
            if (lua_istable(l, -1)) {
                lua_settable(l, methods);
                return;
            }

            lua_newtable(l);
            int type = lua_gettop(l);

            assert(T::s_lunaTypeInfoPtr);
            lua_pushstring(l, T::s_lunaTypeInfoPtr->name());
	    set(l, type, "name");

            lua_pushvalue(l, type);
	    set(l, methods, "type");

            lua_pushvalue(l, methods);
	    set(l, LUA_GLOBALSINDEX, T::s_lunaTypeInfoPtr->name());
        }
    };

    /**
     * Allocate a userdata entry in the specified table at the specified key of
     * the predefined size.
     * \param l Lua state
     * \param table Table to put userdata in
     * \param key Position in table to put userdata in
     * \param size Size of userdata entry to allocate
     * \returns Pointer to allocated userdata or NULL on failure
     */
    static Userdata* allocUserdata(lua_State* l, Index table, void* key, std::size_t size) {
        Userdata* userdata = NULL;
        lua_pushlightuserdata(l, key);
        lua_gettable(l, table);
        if (lua_isnil(l, -1)) {
            lua_pop(l, 1);
            lua_checkstack(l, 3);
            userdata = reinterpret_cast<Userdata*>(lua_newuserdata(l, size));
            lua_pushlightuserdata(l, key);
            lua_pushvalue(l, -2);

            lua_settable(l, -4);
        }
        return userdata;
    }

    /**
     * Thunk for dispatching to class methods
     * \param l Lua state
     * \returns Number of items on stack after method call
     */
    static int thunk(lua_State* l) {
        T* obj = check(l, 1);
        lua_remove(l, 1);
        MethodMap* m = reinterpret_cast<MethodMap*>(lua_touserdata(l, lua_upvalueindex(1)));
        return (obj->*(m->method))(l);
    }

    /**
     * Class instantiator.
     * \param l Lua state
     * \returns One if instantiation was successful and the object is on the stack
     */
    static int newT(lua_State* l) {
        lua_remove(l, 1);
        T* obj = new T(l);
        Userdata* ud = reinterpret_cast<Userdata*>(lua_newuserdata(l, sizeof(Userdata)));
        ud->pT = obj;
        luaL_getmetatable(l, T::s_lunaClassName);
        lua_setmetatable(l, -2);
        return 1;
    }

    /**
     * Garbage collector for class instances.
     * \param l Lua state
     * \returns Zero since no items will be pushed onto the stack
     */
    static int gcT(lua_State* l) {
        Userdata* ud = reinterpret_cast<Userdata*>(lua_touserdata(l, 1));
        T* obj = ud->pT;
        delete obj;
        return 0;
    }

    /**
     * Pushes the name of the class and pointer onto the stack as a string.
     * \param l Lua state
     * \returns One which is the number of strings pushed onto the stack
     */
    static int tostringT(lua_State* l) {
        char buffer[32] = {0, };
        Userdata* ud = reinterpret_cast<Userdata*>(lua_touserdata(l, 1));
        T* obj = ud->pT;
        snprintf(buffer, sizeof(buffer), "%p", obj);
        lua_pushfstring(l, "%s (%s)", T::s_lunaClassName, buffer);
        return 1;
    }

    /**
     * Put the item on the top of the stack into the specified table at the
     * specified key.
     * \param l Lua state
     * \param table Stack index of table
     * \param key Key in table
     */
    static void set(lua_State* l, Index table, const char* key) {
	lua_pushstring(l, key);
	lua_insert(l, -2);
	lua_settable(l, table);
    }

    /**
     * Create a new weak table which controls garbage collection for the key,
     * value or both.
     * \param l Lua state
     * \param mode Garbage collection mode, "k" = weak key, "v" = weak value, "kv" = weak key and value
     */
    static void weaktable(lua_State* l, const char* mode) {
        lua_newtable(l);
        lua_pushvalue(l, -1);
        lua_setmetatable(l, -2);
        lua_pushliteral(l, "__mode");
        lua_pushstring(l, mode);
        lua_settable(l, -3);
    }

    /**
     * Create a new subtable to the specified metatable with a specific name and table mode.
     * \param l Lua state
     * \param metatable Index of metatable
     * \param name Name of subtable
     * \param mode Garbage collection mode, "k" = weak key, "v" = weak value, "kv" = weak key and value
     */
    static void subtable(lua_State* l, Index metatable, const char* name, const char* mode) {
        lua_pushstring(l, name);
        lua_gettable(l, metatable);
        if (lua_isnil(l, -1)) {
            lua_pop(l, 1);
            lua_checkstack(l, 3);
            weaktable(l, mode);
            lua_pushstring(l, name);
            lua_pushvalue(l, -2);
            lua_settable(l, metatable);
        }
    }
};

#endif /* LUNA_HPP */

