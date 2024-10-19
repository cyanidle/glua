# GLUA

Simple lua binding generator. 

```cpp
static int Func(int a, int b) {
    return a + b;
}

// basic
lua_register(L, "Func", glua::Wrap<Func>);
```

```cpp
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
```

```cpp
static int FuncOverload(lua_State* L, int a) {
    int b;
    if (lua_isinteger(L, 2)) {
        b = lua_tointeger(L, 2);
    } else {
        b = 123;
    }
    return a + b;
}

// use current state directly
lua_register(L, "FuncOverload", glua::Wrap<FuncOverload>);
```