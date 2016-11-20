/**
 * main.cpp
 */

#define CATCH_CONFIG_RUNNER

#include "grosfs.hpp"
#include "fuse_calls.hpp"
#include "../include/catch.hpp"

int main( int argc, char * argv[] ) {
    // global setup...
    initfuseops();

    // int result = Catch::Session() . run( argc, argv );
    // // global clean-up...
    //
    // return result;
    umask(0);
    return fuse_main(argc, argv, &grosfs_oper, NULL);
}
