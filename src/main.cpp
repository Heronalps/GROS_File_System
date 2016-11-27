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
    struct fuse_operations ops = initfuseops();
    int  result;
    char cwd[ 1024 ];

    pdebug << "Hello Debug" << std::endl;

    const char * env_p = std::getenv( "RUN_ENV" );
    if( env_p && strcmp( env_p, "test" ) == 0 )
        result = Catch::Session().run( argc, argv );
    else {
        umask( 0 );
        if( getcwd( cwd, sizeof( cwd ) ) != NULL )
            std::cout << "Current working dir: " << cwd << std::endl;
        else
            perror( "getcwd() error" );

        result = fuse_main( argc, argv, &ops, NULL );
    }

    pdebug << "Exiting with code " << result << std::endl;

    return result;
}
