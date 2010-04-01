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
#include <ostream>
#include <vector>

#include "committers.hxx"
#include "error.hxx"
#include "filter.hxx"
#include "repository.hxx"

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#include <boost/python/extract.hpp>
#include <boost/python/import.hpp>
#include <boost/python/list.hpp>
#include <boost/python/object.hpp>
#include <boost/python/type_id.hpp>

#if 0
#undef SVN_ERR
#define SVN_ERR(expr) SVN_INT_ERR(expr)
#define apr_sane_push(arr, contents) *(char **)apr_array_push(arr) = contents
#endif

using namespace std;
using namespace boost;

static string trunk_base = "/trunk";
static string trunk = trunk_base + "/";
static string branches = "/branches/";
static string tags = "/tags/";

static bool split_into_branch_filename( const char* path_, string& branch_, string& fname_ );

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

#if 0
static int delete_hierarchy( svn_fs_root_t *fs_root, char *path, apr_pool_t *pool )
{
    // we have to crawl the hierarchy and delete the files one by one because
    // the regexp deciding to what repository does the file belong can be just
    // anything
    svn_boolean_t is_dir;
    SVN_ERR( svn_fs_is_dir( &is_dir, fs_root, path, pool ) );

    if ( is_dir )
    {
        apr_hash_t *entries;
        SVN_ERR( svn_fs_dir_entries( &entries, fs_root, path, pool ) );

        for ( apr_hash_index_t *i = apr_hash_first( pool, entries ); i; i = apr_hash_next( i ) )
        {
            const void *key;
            void       *val;
            apr_hash_this( i, &key, NULL, &val );

            delete_hierarchy( fs_root, (char *)( string( path ) + '/' + (char *)key ).c_str(), pool );
        }
    }
    else
    {
        string this_branch, fname;

        // we don't have to care about the branch name, it cannot change
        if ( split_into_branch_filename( path, this_branch, fname ) )
            Repositories::deleteFile( fname );
    }
}
#endif

#if 0
static int delete_hierarchy_rev( svn_fs_t *fs, svn_revnum_t rev, char *path, apr_pool_t *pool )
{
    svn_fs_root_t *fs_root;

    // rev - 1: the last rev where the deleted thing still existed
    SVN_ERR( svn_fs_revision_root( &fs_root, fs, rev - 1, pool ) );

    return delete_hierarchy( fs_root, path, pool );
}
#endif

#if 0
static int dump_hierarchy( svn_fs_root_t *fs_root, char *path, int skip,
        const string &prefix, apr_pool_t *pool )
{
    svn_boolean_t is_dir;
    SVN_ERR( svn_fs_is_dir( &is_dir, fs_root, path, pool ) );

    if ( is_dir )
    {
        apr_hash_t *entries;
        SVN_ERR( svn_fs_dir_entries( &entries, fs_root, path, pool ) );

        for ( apr_hash_index_t *i = apr_hash_first( pool, entries ); i; i = apr_hash_next( i ) )
        {
            const void *key;
            void       *val;
            apr_hash_this( i, &key, NULL, &val );

            dump_hierarchy( fs_root, (char *)( string( path ) + '/' + (char *)key ).c_str(), skip, prefix, pool );
        }
    }
    else
        dump_blob( fs_root, path, prefix + string( path + skip ), pool );

    return 0;
}
#endif

#if 0
static int copy_hierarchy( svn_fs_t *fs, svn_revnum_t rev, char *path_from, const string &path_to, apr_pool_t *pool )
{
    svn_fs_root_t *fs_root;
    SVN_ERR( svn_fs_revision_root( &fs_root, fs, rev, pool ) );

    return dump_hierarchy( fs_root, path_from, strlen( path_from ), path_to, pool );
}
#endif

static bool is_trunk( const char* path_ )
{
    const size_t len = trunk.length();
    return trunk.compare( 0, len, path_, 0, len ) == 0;
}

static bool is_branch( const char* path_ )
{
    const size_t len = branches.length();
    return branches.compare( 0, len, path_, 0, len ) == 0;
}

static bool is_tag( const char* path_ )
{
    const size_t len = tags.length();
    return tags.compare( 0, len, path_, 0, len ) == 0;
}

static bool split_into_branch_filename( const char* path_, string& branch_, string& fname_ )
{
    if ( is_trunk( path_ ) )
    {
        branch_ = "master";
        fname_  = path_ + trunk.length();
    }
    else if ( trunk_base == path_ )
    {
        branch_ = "master";
        fname_  = string();
    }
    else
    {
        string tmp;
        string prefix;
        if ( is_branch( path_ ) )
            tmp = path_ + branches.length();
        else if ( is_tag( path_ ) )
        {
            tmp = path_ + tags.length();
            prefix = TAG_TEMP_BRANCH;
        }
        else
            return false;

        size_t slash = tmp.find( '/' );
        if ( slash == 0 )
            return false;
        else if ( slash == string::npos )
        {
            branch_ = prefix + tmp;
            fname_  = string();
        }
        else
        {
            branch_ = prefix + tmp.substr( 0, slash );
            fname_  = tmp.substr( slash + 1 );
        }
    }

    return true;
}

int export_changeset( python::object context )
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

    if ( !Repositories::hasParents( merges ) )
    {
        fprintf( stderr, "ignored, no parents.\n" );
        return 0;
    }

    // author
    string author = python::extract< string >( context.attr( "user" )() );

    // date
    python::object date = context.attr( "date" )();
    Time epoch( static_cast< double >( python::extract< double >( date[0] ) ), python::extract< int >( date[1] ) );

    // commit message
    string message = python::extract< string >( context.attr( "description" )() );

    // create tag? (a tag branch, to be exact)
    python::object tags = context.attr( "tags" )();
    for ( int i = 0; i < python::len( tags ); ++i )
    {
        python::object tag = tags[i];
        string tag_name = python::extract< string >( tag );

        if ( tag_name != "tip" )
        {
            // don't create the tag itself, only the tag branch
            Repositories::createBranchOrTag( true,
                    rev, "master",
                    Committers::getAuthor( author ),
                    TAG_TEMP_BRANCH + tag_name, rev,
                    epoch,
                    message );
        }
    }

    // files
    bool no_changes = true;
    python::object files = context.attr( "files" )();
    for ( int i = 0; i < python::len( files ); ++i )
    {
        python::object file = files[i];
        string path = python::extract< string >( file );

        if ( no_changes )
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
                Repositories::updateMercurialTags( python::extract< string >( filectx.attr( "data" )() ),
                        Committers::getAuthor( author ), epoch, message );
        }
        else
            Repositories::deleteFile( path );

        no_changes = false;
    }

    if ( no_changes )
        fprintf( stderr, "no files changed, merge commit? " );

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

    if ( !Repositories::load( repos_config, max_rev, min_rev, trunk_base, trunk, branches, tags ) )
    {
        Error::report( "Must have at least one valid repository definition." );
        return 1;
    }

    for ( int rev = min_rev; rev < max_rev; rev++ )
    {
        //python::object node = repo.attr( "lookup" )( rev );
        //python::object changeset = changelog.attr( "read" )( node );
        python::object context = repo[rev];

        export_changeset( context );
    }

    return 0;
}

int main(int argc, char *argv[])
{
    if (argc != 4) {
        Error::report( string( "usage: " ) + argv[0] + " REPOS_PATH committers.txt reposlayout.txt\n" );
        return Error::returnValue();
    }

    // Initialize Python
    Py_Initialize();

    Committers::load( argv[2] );

    crawl_revisions( argv[1], argv[3] );

    Py_Finalize();

    Repositories::close();

    return Error::returnValue();
}
