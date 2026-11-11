#include "micant/ucrt/bounded_arena.hpp"
#include "micant/ucrt/onexit_table.hpp"

#include <cassert>

namespace {

int g_exitOrder{};
int g_initializerOrder{};

int exitOne() {
    g_exitOrder = g_exitOrder * 10 + 1;
    return 0;
}

int exitTwo() {
    g_exitOrder = g_exitOrder * 10 + 2;
    return 0;
}

int exitThree() {
    g_exitOrder = g_exitOrder * 10 + 3;
    return 0;
}

void initializeOne() {
    g_initializerOrder = g_initializerOrder * 10 + 1;
}

void initializeTwo() {
    g_initializerOrder = g_initializerOrder * 10 + 2;
}

int initializeWithFailure() {
    g_initializerOrder = g_initializerOrder * 10 + 3;
    return 7;
}

int initializeAfterFailure() {
    g_initializerOrder = g_initializerOrder * 10 + 4;
    return 0;
}

}

int main() {
    using namespace micant::ucrt;

    BoundedArena<4096> arena;
    OnExitTable table{};
    assert(initializeOnExitTable(&table) == 0);
    assert(initializeOnExitTable(nullptr) == -1);
    assert(registerOnExitFunction(&table, exitOne, arena) == 0);
    assert(registerOnExitFunction(&table, exitTwo, arena) == 0);
    assert(registerOnExitFunction(&table, exitThree, arena) == 0);
    assert(registerOnExitFunction(&table, nullptr, arena) == -1);
    assert(executeOnExitTable(&table, arena) == 0);
    assert(g_exitOrder == 321);
    assert(table._first == nullptr && table._last == nullptr &&
           table._end == nullptr);
    assert(executeOnExitTable(&table, arena) == 0);

    void (*initializers[])(void){initializeOne, nullptr, initializeTwo};
    initializeFunctionTable(initializers, initializers + 3);
    assert(g_initializerOrder == 12);

    int32_t (*fallibleInitializers[])(void){
        initializeWithFailure, initializeAfterFailure};
    assert(initializeFunctionTableWithStatus(
               fallibleInitializers, fallibleInitializers + 2) == 7);
    assert(g_initializerOrder == 123);
}
