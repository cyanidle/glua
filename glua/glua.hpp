#ifndef GLUA_HPP
#define GLUA_HPP
#include "describe/describe.hpp"
#include "meta/meta.hpp"
#include <optional>
extern "C" {
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
}

namespace glua {

struct ReadOnly {}; //attribute to make DESCRIBED() member field read-only

using std::string_view;
using std::string;

template<auto f> //needed to safely convert possible exceptions into lua errors
int protect(lua_State* L) noexcept {
    try {
        return f(L);
    } catch (std::exception& e) {
        lua_pushstring(L, e.what());
    }
    lua_error(L);
    std::abort();
}

template<typename T>
inline string name_for = string{describe::Get<T>().name};

template<typename T>
T* TestUData(lua_State* L, int idx) {
    return static_cast<T*>(luaL_testudata(L, idx, name_for<T>.c_str()));
}

template<typename T>
T& CheckUData(lua_State* L, int idx) {
    if (auto u = static_cast<T*>(luaL_testudata(L, idx, name_for<T>.c_str()))) {
        return *u;
    } else {
        throw Err("arg #{} is not of type '{}'", idx, name_for<T>);
    }
}

template<typename T>
std::pair<T*, string_view> self_key(lua_State* L) {
    T* self = static_cast<T*>(lua_touserdata(L, 1));
    luaL_checktype(L, 2, LUA_TSTRING);
    size_t len;
    auto s = lua_tolstring(L, 2, &len);
    return {self, string_view{s, len}};
}

inline void range_check(double v, int idx, double l, double h) {
    if (std::round(v) != v) {
        throw Err("arg #{} is not an integer", idx);
    }
    if (v < l || h < v) {
        throw Err("arg #{} does not fit into [{}-{}]", idx, l, h);
    }
}

inline void type_check(lua_State* L, int idx, int t, int wanted) {
    if (t != wanted) {
        throw Err("arg #{} is not a '{}', but a '{}'", idx, lua_typename(L, wanted), lua_typename(L, t));
    }
}

template<typename T, std::enable_if_t<std::is_arithmetic_v<T>, int> = 1>
void Pop(lua_State* L, T& val, int idx = -1) {
    auto t = lua_type(L, idx);
    type_check(L, idx, t, LUA_TNUMBER);
    if constexpr (std::is_floating_point_v<T>) {
        val = T(lua_tonumber(L, idx));
    } else {
        auto v = lua_tonumber(L, idx);
        auto l = double((std::numeric_limits<T>::min)());
        auto h = double((std::numeric_limits<T>::max)());
        range_check(v, idx, l, h);
        val = T(v);
    }
}

template<typename T, std::enable_if_t<describe::is_described_struct_v<T>, int> = 1>
void Pop(lua_State* L, T& val, int idx = -1) {
    val = CheckUData<T>(L, idx);
}

inline void Pop(lua_State* L, string_view& val, int idx = -1) {
    auto t = lua_type(L, idx);
    type_check(L, idx, t, LUA_TSTRING);
    size_t len;
    auto s = lua_tolstring(L, idx, &len);
    val = {s, len};
}

inline void Pop(lua_State* L, bool& val, int idx = -1) {
    auto t = lua_type(L, idx);
    type_check(L, idx, t, LUA_TBOOLEAN);
    val = lua_toboolean(L, idx);
}

inline void Pop(lua_State* L, string& val, int idx = -1) {
    string_view s;
    Pop(L, s, idx);
    val = string{s};
}

template<typename T>
void Pop(lua_State* L, std::optional<T>& val, int idx = -1) {
    if (lua_isnil(L, idx)) {
        val.reset();
    } else {
        Pop(L, val.emplace(), idx);
    }
}

template<typename T, std::enable_if_t<std::is_arithmetic_v<T>, int> = 1>
void Push(lua_State* L, T val) {
    if (!lua_checkstack(L, 1)) {
        throw Err("Could not reserve stack");
    }
    if constexpr (std::is_floating_point_v<T>) {
        lua_pushnumber(L, val);
    } else if constexpr (std::is_signed_v<T>) {
        lua_pushinteger(L, val);
    } else {
        lua_pushinteger(L, val);
    }
}

inline void Push(lua_State* L, string_view val) {
    lua_pushlstring(L, val.data(), val.size());
}

template<typename T>
int index_for(lua_State* L) {
    constexpr auto desc = describe::Get<T>();
    auto sk = self_key<T>(L);
    bool hit = false;
    desc.for_each_field([&](auto f){
        if (!hit && f.name == sk.second) {
            hit = true;
            Push(L, f.get(*sk.first));
        }
    });
    if (!hit) {
        // fallback to methods table
        lua_pushvalue(L, 2);
        lua_rawget(L, lua_upvalueindex(1));
    }
    return 1;
}

template<typename T>
int newindex_for(lua_State* L) {
    constexpr auto desc = describe::Get<T>();
    auto sk = self_key<T>(L);
    bool hit = false;
    lua_pushvalue(L, 3);
    desc.for_each_field([&](auto f){
        using F = decltype(f);
        constexpr auto ro = describe::has_attr_v<ReadOnly, F>;
        if constexpr (!ro) {
            if (!hit && f.name == sk.second) {
                hit = true;
                Pop(L, f.get(*sk.first));
            }
        }
    });
    if (!hit) {
        throw Err("Cannot set field '{}' on '{}'", sk.second, desc.name);
    }
    return 0;
}

template<typename T>
int dtor_for(lua_State* L) noexcept {
    auto self = static_cast<T*>(lua_touserdata(L, 1));
    self->~T();
    return 0;
}

template<typename T>
T Get(lua_State* L, int idx) {
    if constexpr (std::is_same_v<T, lua_State*>) {
        (void)idx;
        return L;
    } else {
        T res;
        Pop(L, res, idx);
        return res;
    }
}

inline void check_ret_space(lua_State* L) {
    if (!lua_checkstack(L, 1)) {
        throw Err("could not push function result");
    }
}

template<auto f, typename rip, size_t...Is, typename...Args>
void call(lua_State* L, std::index_sequence<Is...>, meta::TypeList<Args...> args);

template<auto method>
int Wrap(lua_State* L) noexcept {
    try {
        using rip = meta::RipFunc<decltype(method)>;
        using args = typename rip::Args;
        constexpr auto argc = rip::ArgCount; // first should be lua_State*
        call<method, rip>(L, std::make_index_sequence<argc>(), args{});
        return 1;
    } catch (std::exception& e) {
        lua_pushstring(L, e.what());
    } catch (const char* e) {
        lua_pushstring(L, e);
    }
    lua_error(L);
    return 0;
}

template<typename T, typename...Args>
T ctor_for(Args...a) {
    return T{std::move(a)...};
}

template<typename T, typename...Args>
void PushCtorArgs(lua_State* L, T(*)(Args...)) {
    lua_pushcclosure(L, Wrap<ctor_for<T, Args...>>, 0);
}

template<typename Sig>
void PushCtor(lua_State* L) {
    PushCtorArgs(L, static_cast<Sig*>(nullptr));
}

template<typename T>
void PushMetaTable(lua_State* L) {
    if (luaL_newmetatable(L, name_for<T>.c_str())) {
        luaL_Reg funcs[] = {
            {"__gc", dtor_for<T>},
            {"__index", protect<index_for<T>>},
            {"__newindex", protect<newindex_for<T>>},
            {nullptr, nullptr},
        };
        lua_createtable(L, 0, 0);
        lua_pushvalue(L, -1);
        lua_setfield(L, -3, "__methods");
        describe::Get<T>().for_each([&](auto f){
            using F = decltype(f);
            if constexpr (F::is_method || F::is_function) {
                lua_pushlstring(L, f.name.data(), f.name.size());
                lua_pushcclosure(L, Wrap<F::value>, 0);
                lua_rawset(L, -3);
            }
        });
        luaL_setfuncs(L, funcs, 1);
    }
}

template<typename T>
void PushFromMetatable(lua_State* L, string_view key) {
    if (!lua_checkstack(L, 2)) {
        throw Err("could not reserve stack for MetaTable");
    }
    PushMetaTable<T>(L);
    lua_pushlstring(L, key.data(), key.size());
    lua_rawget(L, -2);
    lua_insert(L, -2);
    lua_pop(L, 1);
}

template<typename T>
void PushMethodsTable(lua_State* L) {
    PushFromMetatable<T>(L, "__methods");
}

template<typename T, std::enable_if_t<describe::is_described_struct_v<T>, int> = 1>
void Push(lua_State* L, T val) {
    if (!lua_checkstack(L, 1)) {
        throw Err("Could not reserve stack");
    }
    auto ud = lua_newuserdata(L, sizeof(T));
    new (ud) T{std::move(val)};
    PushMetaTable<T>(L);
    lua_setmetatable(L, -2);
}

inline void SetThis(lua_State* L, void* key, void* th) {
    if (!lua_checkstack(L, 2)) {
        throw Err("SetThis(): out of stack space");
    }
    lua_pushlightuserdata(L, key);
    lua_pushlightuserdata(L, th);
    lua_rawset(L, LUA_REGISTRYINDEX);
}

template<typename T>
T* GetThis(lua_State* L, void* key) {
    if (!lua_checkstack(L, 1)) {
        throw Err("GetThis(): out of stack space");
    }
    lua_rawgetp(L, LUA_REGISTRYINDEX, key);
    auto res = static_cast<T*>(lua_touserdata(L, -1));
    lua_pop(L, 1);
    return res;
}

template<typename T>
void Push(lua_State* L, std::optional<T> v) {
    v ? Push(L, std::move(*v)) : lua_pushnil(L);
}

template<auto f, typename rip, size_t...Is, typename...Args>
void call(lua_State* L, std::index_sequence<Is...>, meta::TypeList<Args...> args) {
    using Head = meta::HeadTypeOf_t<decltype(args)>;
    using Ret = typename rip::Ret;
    constexpr int fix = std::is_same_v<Head, lua_State*> ? -1 : 0;
    constexpr auto is_method = std::is_member_function_pointer_v<decltype(f)>;
    if constexpr (std::is_void_v<typename rip::Ret>) {
        if constexpr (is_method) {
            auto& self = CheckUData<typename rip::Cls>(L, 1);
            (self.*f)(Get<Args>(L, int(Is + 2) + fix)...);
        } else {
            (f)(Get<Args>(L, int(Is + 1) + fix)...);
        }
        check_ret_space(L);
        lua_pushnil(L);
    } else if constexpr (is_method) {
        auto& self = CheckUData<typename rip::Cls>(L, 1);
        Ret ret = (self.*f)(Get<Args>(L, int(Is + 2) + fix)...);
        check_ret_space(L);
        Push(L, std::move(ret));
    } else {
        Ret ret = (f)(Get<Args>(L, int(Is + 1) + fix)...);
        check_ret_space(L);
        Push(L, std::move(ret));
    }
}

}

#endif //GLUA_HPP