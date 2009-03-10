#include "committers.hxx"

#include <fstream>
#include <string>

using namespace std;

struct ltstr
{
    bool operator()( const char* s1, const char* s2 ) const
    {
        return strcmp( s1, s2 ) < 0;
    }
};

typedef std::map< const char*, Committer, ltstr > CommittersMap;

static CommittersMap committers;

void Committers::load( const char *fname )
{
    ifstream input( fname, ifstream::in );
    string line;

    while ( !input.eof() )
    {
        getline( input, line );

        // comments
        if ( line.length() == 0 || line[0] == '#' )
            continue;

        // find the separators
        size_t delim1 = line.find( "|" );
        if ( delim1 == string::npos )
        {
            fprintf( stderr, "ERROR: Wrong committer '%s'\n", line.c_str() );
            continue;
        }
        
        size_t delim2 = line.find( "|", delim1 + 1 );
        if ( delim2 == string::npos )
        {
            fprintf( stderr, "ERROR: Wrong committer '%s'\n", line.c_str() );
            continue;
        }

        // store the data
        const char* login = strdup( line.substr( 0, delim1 ).c_str() );
        committers[login] = Committer( line.substr( delim1 + 1, delim2 - delim1 - 1 ),
                                       line.substr( delim2 + 1 ) );
    }
}

const Committer& Committers::getAuthor( const char* name )
{
    CommittersMap::iterator it( committers.find( name ) );

    if ( it == committers.end() )
    {
        fprintf( stderr, "ERROR: Author '%s' is missing, adding as '%s@openoffice.org'.\n", name, name );

        return ( committers[ strdup( name ) ] = Committer( string( name ), string( name ) + "@openoffice.org" ) );
    }

    return it->second;
}
