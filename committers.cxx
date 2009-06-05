/*
 * Read committers from a file, assign commits to them.
 *
 * Author: Jan Holesovsky <kendy@suse.cz>
 * License: MIT <http://www.opensource.org/licenses/mit-license.php>
 */

#include "committers.hxx"
#include "error.hxx"

#include <cstring>
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

static string default_address( "@localhost" );

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

        // default committer address
        if ( line[0] == '@' )
        {
            default_address = line;
            continue;
        }

        // find the separators
        size_t delim1 = line.find( "|" );
        size_t delim2 = ( delim1 == string::npos )? string::npos: line.find( "|", delim1 + 1 );

        if ( delim2 == string::npos )
        {
            Error::report( "Wrong committer '" + line + "'" );
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
        Error::report( string( "Author '" ) + name + "' is missing, adding as '" + name + default_address + "'" );

        return ( committers[ strdup( name ) ] = Committer( string( name ), string( name ) + default_address ) );
    }

    return it->second;
}
