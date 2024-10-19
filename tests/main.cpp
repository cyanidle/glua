#include "glua/glua.hpp"

static int Func(int a, int b) {
    return a + b;
}

static int FuncOverload(lua_State* L, int a) {
    int b;
    if (lua_isinteger(L, 2)) {
        b = lua_tointeger(L, 2);
    } else {
        b = 123;
    }
    return a + b;
}

struct Person {
    std::string name;
    int age;

    std::string Hello() {
        return "Hello " + name;
    }
};
DESCRIBE(Person, &_::name, &_::age, &_::Hello)

static std::string GreetAnother(Person* a, Person* b) {
    return "Hello from: " + a->name + " to: " + b->name;
}

int main(int argc, char *argv[])
{
    auto L = luaL_newstate();
    luaL_openlibs(L);

    // basic
    lua_register(L, "Func", glua::Wrap<Func>);

    // use current state directly
    lua_register(L, "FuncOverload", glua::Wrap<FuncOverload>);

    // Add methods
    glua::PushMethodsTable<Person>(L);
    lua_pushcfunction(L, glua::Wrap<GreetAnother>);
    lua_setfield(L, -2, "GreetAnother");
    // Generic push
    glua::Push(L, 123);
    lua_setglobal(L, "num");
    glua::Push(L, Person{"polina", 23});
    lua_setglobal(L, "polina");
    glua::Push(L, Person{"alexej", 22});
    lua_setglobal(L, "alexej");

    auto script = R"(
        print(polina:GreetAnother(alexej))
    )";

    luaL_loadstring(L, script);
    auto ok = lua_pcall(L, 0, 0, 0);

    int ret;
    if (ok == LUA_OK) {
        fprintf(stderr, "glua: Test ok.\n");
        ret = 0;
    } else {
        fprintf(stderr, "glua: Test fail.\n");
        ret = 1;
    }

    lua_close(L);
    return ret;
}
