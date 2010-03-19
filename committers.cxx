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

typedef std::map< string, Committer > CommittersMap;

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
        string login = line.substr( 0, delim1 );
        committers[login] = Committer( line.substr( delim1 + 1, delim2 - delim1 - 1 ),
                                       line.substr( delim2 + 1 ) );
    }
}

const Committer& Committers::getAuthor( const char* name )
{
    return getAuthor( string( name ) );
}

const Committer& Committers::getAuthor( const string& name )
{
    CommittersMap::iterator it( committers.find( name ) );

    if ( it != committers.end() )
        return it->second;

    // name + email
    size_t addr = name.rfind( " <" );
    if ( addr != string::npos )
    {
        size_t at = name.find( "@", addr );
        if ( at != string::npos )
        {
            size_t end = name.find( ">", at );
            if ( end != string::npos )
            {
                return ( committers[name] = Committer( name.substr( 0, addr ) , name.substr( addr + 2, end - addr - 2 ) ) );
            }
        }
    }

    // email only
    size_t at = name.find( "@" );
    if ( at != string::npos )
    {
        return ( committers[name] = Committer( name.substr( 0, at ) , name ) );
    }

    Error::report( string( "Author '" ) + name + "' is missing, adding as '" + name + default_address + "'" );
    return ( committers[name] = Committer( name, name + default_address ) );
}
