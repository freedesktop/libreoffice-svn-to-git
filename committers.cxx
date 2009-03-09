#include "committers.hxx"

#include <string>

using namespace std;

Committers::Committers()
{
}

void Committers::load( const char *fname )
{
}

const Committer& Committers::getAuthor( const char* name )
{
    CommittersMap::iterator it( committers.find( name ) );

    if ( it == committers.end() )
    {
        fprintf( stderr, "Author '%s' is missing, adding as '%s@openoffice.org'.", name, name );

        return ( committers[ name ] = Committer( string( name ), string( name ) + "@openoffice.org" ) );
    }

    return it->second;
}
