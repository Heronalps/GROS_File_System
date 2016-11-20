/**
 * main.cpp
 */

#define CATCH_CONFIG_RUNNER

#include "grosfs.hpp"
#include "../include/catch.hpp"


int main( int argc, char * const argv[] ) {
    // global setup...

    disk = gros_open_disk();

    int result = Catch::Session() . run( argc, argv );
    //Disk *disk = gros_open_disk();
    // global clean-up...

    return result;
}
