/*
 * Based on svn-fast-export.cxx & hg-fast-export.py
 *
 * Walk through each revision of a local Subversion repository and export it
 * in a stream that git-fast-import can consume.
 *
 * Author: Chris Lee <clee@kde.org>
 *         Jan Holesovsky <kendy@suse.cz>
 * License: MIT <http://www.opensource.org/licenses/mit-license.php>
 */

#define _XOPEN_SOURCE
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#include <iomanip>
#include <map>
#include <ostream>
#include <vector>

#include "committers.hxx"
#include "error.hxx"
#include "filter.hxx"
#include "repository.hxx"

#include <boost/python/dict.hpp>
#include <boost/python/extract.hpp>
#include <boost/python/import.hpp>
#include <boost/python/list.hpp>
#include <boost/python/object.hpp>
#include <boost/python/type_id.hpp>

using namespace std;
using namespace boost;

static int dump_blob( const python::object& filectx, const string &target_name )
{
    string flags = python::extract< string >( filectx.attr( "flags" )() );

    const char* mode = "644";
    if ( flags == "x" )
        mode = "755";
    else if ( flags != "" )
        Error::report( "Got an unknown flag '" + flags + "'; we cannot handle eg. symlinks now." );

    // prepare the stream
    ostream& out = Repositories::modifyFile( target_name, mode );

    // dump the content of the file
    Filter filter( target_name );
    filter.addData( python::extract< string >( filectx.attr( "data" )() ) );
    filter.write( out );

    return 0;
}

static int to_hex( char c )
{
    if ( '0' <= c && c <= '9' )
        return c - '0';
    else if ( 'a' <= c && c <= 'f' )
        return 10 + ( c - 'a' );
    else if ( 'A' <= c && c <= 'F' )
        return 10 + ( c - 'A' );

    return 0;
}

static string mercurial_node( const string& nodestr )
{
    string node;
    for ( int i = 0; i + 1 < nodestr.length(); i += 2 )
        node += static_cast< char >( ( to_hex( nodestr[i] ) << 4 ) + to_hex( nodestr[i+1] ) );

    return node;
}

inline void dump_file( const python::object& file, const string& path,
        const python::object& context, const python::object& repo,
        const string& author, const Time& epoch,
        const string& message, bool dbg_out )
{
    if ( dbg_out )
        fprintf( stderr, "path: %s... ", path.c_str() );

    // dump the file
    python::object filectx;
    try
    {
        filectx = context.attr( "filectx" )( file );
    } catch ( python::error_already_set& )
    {
        PyErr_Clear();
    }

    if ( filectx )
    {
        if ( path != ".hgtags" )
            dump_blob( filectx, path );
        else
        {
            string hgtags = python::extract< string >( filectx.attr( "data" )() );
            istringstream istr( hgtags );

            while ( !istr.eof() )
            {
                string id, name;
                istr >> id >> name;

                if ( id.empty() || name.empty() )
                    continue;

                python::object node( mercurial_node( id ) );
                python::object ctx = repo[node];
                int tag_rev = python::extract< int >( ctx.attr( "rev" )() );

                Repositories::updateMercurialTag( name, tag_rev,
                        Committers::getAuthor( author ), epoch, message );
            }
        }
    }
    else
        Repositories::deleteFile( path );
}

struct ChangedFile {
    bool touched;
    python::object file;
    python::object hash;

    ChangedFile( const python::object& file_, const python::object& hash_ )
        : touched( false ), file( file_ ), hash( hash_ ) {}
};

typedef map< string, ChangedFile > ChangedFiles;

void changed_during_merge( python::object& files,
        const python::dict& context_man, const python::dict& parent_man )
{
    // copy the changed files map from parent to our map
    ChangedFiles parent;
    python::object iter = parent_man.iteritems();
    for ( int i = 0, count = python::len( parent_man ); i < count; ++i )
    {
        python::object item = iter.attr( "next" )();
        python::object file = item[0];
        python::object hash = item[1];

        ChangedFile chf( file, hash );
        parent.insert( pair< string, ChangedFile >( python::extract< string >( file ), chf ) );
    }

    python::list files_list;

    // find out what files have changed during the merge
    iter = context_man.iteritems();
    for ( int i = 0, count = python::len( context_man ); i < count; ++i )
    {
        python::object item = iter.attr( "next" )();
        python::object file = item[0];
        python::object hash = item[1];

        ChangedFiles::iterator it = parent.find( python::extract< string >( file ) );
        if ( it == parent.end() )
            files_list.append( file );
        else
        {
            it->second.touched = true;
            if ( it->second.hash != hash )
                files_list.append( file );
        }
    }

    // find the files deleted (present only in the parent)
    for ( ChangedFiles::const_iterator it = parent.begin(); it != parent.end(); ++it )
    {
        if ( !it->second.touched )
            files_list.append( it->second.file );
    }

    files = files_list;
}

static int export_changeset( const python::object& repo, const python::object& context )
{
    int rev = python::extract< int >( context.attr( "rev" )() );

    ostringstream stm;
    python::object pnode( context.attr( "node" )() );
    for ( int i = 0; i < python::len( pnode ); ++i )
    {
        unsigned char val = python::extract< char >( pnode[i] );
        stm << hex << setfill( '0' ) << setw( 2 ) << static_cast< int >( val );
    }
    string node( stm.str() );

    fprintf( stderr, "Exporting revision %d (%s)... ", rev, node.c_str() );

    // merges
    vector< int > merges;
    python::object parents = context.attr( "parents" )();
    for ( int i = 0; i < python::len( parents ); ++i )
    {
        python::object parent = parents[i];
        int parent_rev = python::extract< int >( parent.attr( "rev" )() );

        merges.push_back( parent_rev );
    }

    if ( merges.size() == 0 || !Repositories::hasParent( merges[0] ) )
    {
        Error::report( "ignored, no parent." );
        return 0;
    }

    // author
    string author = python::extract< string >( context.attr( "user" )() );

    // date
    python::object date = context.attr( "date" )();
    Time epoch( static_cast< double >( python::extract< double >( date[0] ) ), python::extract< int >( date[1] ) );

    // commit message
    string message = python::extract< string >( context.attr( "description" )() );

    // files
    python::object files;
    if ( python::len( parents ) == 1 )
    {
        files = context.attr( "files" )();
    }
    else if ( python::len( parents ) > 1 )
    {
        changed_during_merge( files,
                python::extract< python::dict >( context.attr( "manifest" )() ),
                python::extract< python::dict >( parents[0].attr( "manifest" )() ) );

    }

    // output
    bool first = true;
    for ( int i = 0; i < python::len( files ); ++i )
    {
        string fname = python::extract< string >( files[i] );
        dump_file( files[i], fname, context, repo, author, epoch, message, first );
        first = false;
    }

    Repositories::commit( Committers::getAuthor( author ),
            "master", rev,
            epoch,
            message,
            merges );

    fprintf( stderr, "done!\n" );

    return 0;
}

int crawl_revisions( const char *repos_path, const char* repos_config )
{
    python::object module_ui = python::import( "mercurial.ui" );
    python::object module_hg = python::import( "mercurial.hg" );

    python::object ui = module_ui.attr( "ui" )();
    ui.attr( "setconfig" )( "ui", "interactive", "off" );

    python::object repository = module_hg.attr( "repository" );
    python::object repo = repository( ui, repos_path );

    python::object changelog = repo.attr( "changelog" );

    int min_rev = 0;
    int max_rev = python::len( changelog );

    string dummy1, dummy2, dummy3, dummy4;
    if ( !Repositories::load( repos_config, max_rev, min_rev, dummy1, dummy2, dummy3, dummy4 ) )
    {
        Error::report( "Must have at least one valid repository definition." );
        return 1;
    }

    // dump all the data
    for ( int rev = min_rev; rev < max_rev; rev++ )
        export_changeset( repo, repo[rev] );

    return 0;
}

int main(int argc, char *argv[])
{
    if (argc != 4) {
        Error::report( string( "usage: " ) + argv[0] + " REPOS_PATH committers.txt reposlayout.txt\n" );
        return Error::returnValue();
    }

    // initialize Python
    Py_Initialize();

    Committers::load( argv[2] );

    // do the work
    crawl_revisions( argv[1], argv[3] );

    Py_Finalize();

    Repositories::close();

    return Error::returnValue();
}
