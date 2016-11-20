/**
 * main.cpp
 */

#define CATCH_CONFIG_RUNNER

#include "grosfs.hpp"
#include "fuse_calls.hpp"
#include <cstdio>
#include <cstring>
#include "../include/catch.hpp"

int main( int argc, char * argv[] ) {
    initfuseops();
    int result;

    const char* env_p = std::getenv("RUN_ENV");
    if (env_p && strcmp(env_p, "test") == 0) {
        result = Catch::Session() . run( argc, argv );
    } else {
        umask(0);
        result = fuse_main(argc, argv, &grosfs_oper, NULL);
    }

    return result;
}
