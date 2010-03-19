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

#include <ostream>

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
    fprintf( stderr, "Exporting revision %d... ", rev ); //TODO output node too?

    if ( Repositories::ignoreRevision( rev ) )
    {
        fprintf( stderr, "ignored.\n" );
        return 0;
    }

    string author = python::extract< string >( context.attr( "user" )() );

    python::object date = context.attr( "date" )();
    Time epoch( static_cast< time_t >( python::extract< double >( date[0] ) ), python::extract< int >( date[1] ) );

    string message = python::extract< string >( context.attr( "description" )() );

//    printf( "%s %ld %s\n", author.c_str(), epoch, message.c_str() );

    string branch( "master" );
    bool no_changes = true;
    bool debug_once = true;
    bool tagged_or_branched = false;
    python::object files = context.attr( "files" )();
    for ( int i = 0; i < python::len( files ); ++i )
    {
        python::object file = files[i];
        string path = python::extract< string >( file );

        if ( debug_once )
        {
            fprintf( stderr, "path: %s... ", path.c_str() );
            debug_once = false;
        }

        python::object filectx;
        try
        {
            filectx = context.attr( "filectx" )( file );
        } catch ( python::error_already_set& )
        {
            PyErr_Clear();
        }

#if 0
        svn_pool_clear(revpool);
        apr_hash_this(i, &key, NULL, &val);
        path = (char *)key;
        change = (svn_fs_path_change_t *)val;

        // don't care about anything in the toplevel
        if ( path[0] != '/' || strchr( path + 1, '/' ) == NULL )
            continue;

        string this_branch, fname;

        // skip if we cannot find the branch
        if ( !split_into_branch_filename( path, this_branch, fname ) )
            continue;

        // ignore the tags we do not want
        if ( is_tag( path ) && Repositories::ignoreTag( this_branch ) )
            continue;

        SVN_ERR(svn_fs_is_dir(&is_dir, fs_root, path, revpool));

        // detect creation of branch/tag
        if ( is_dir && change->change_kind == svn_fs_path_change_add )
        {
            // create a new branch/tag?
            if ( is_branch( path ) || is_tag( path ) )
            {
                bool branching = is_branch( path );

                if ( fname.empty() )
                {
                    // is it a new branch/tag
                    svn_revnum_t rev_from;
                    const char* path_from;
                    SVN_ERR( svn_fs_copied_from( &rev_from, &path_from, fs_root, path, revpool ) );

                    string from_branch, from_fname;
                    if ( path_from != NULL &&
                         split_into_branch_filename( path_from, from_branch, from_fname ) &&
                         from_fname.empty() )
                    {
                        Repositories::createBranchOrTag( branching,
                                rev_from, from_branch,
                                Committers::getAuthor( author ),
                                this_branch, rev,
                                epoch,
                                message );

                        tagged_or_branched = true;
                    }
                    continue;
                }
            }
        }

        // sanity check
        if ( branch.empty() )
            branch = this_branch;
        else if ( branch != this_branch )
        {
            // we found a commit that belongs to more branches at once!
            // let's commit what we have so far so that we can commit the
            // rest to the other branch later
            Repositories::commit( Committers::getAuthor( author ),
                    branch, rev,
                    epoch,
                    message );
            branch = this_branch;
        }

        // add/remove/move the files
        if ( change->change_kind == svn_fs_path_change_delete )
            delete_hierarchy_rev( fs, rev, (char *)path, revpool );
        else if ( is_dir )
        {
            svn_revnum_t rev_from;
            const char* path_from;
            SVN_ERR( svn_fs_copied_from( &rev_from, &path_from, fs_root, path, revpool ) );

            if ( path_from == NULL )
                continue;

            copy_hierarchy( fs, rev_from, (char *)path_from, fname, revpool );
        }
        else
#endif
        if ( filectx )
            dump_blob( filectx, path );
        else
            Repositories::deleteFile( path );

        no_changes = false;
    }

    if ( no_changes || branch.empty() )
    {
        fprintf( stderr, "%s.\n", tagged_or_branched? "created": "skipping" );
        return 0;
    }

    Repositories::commit( Committers::getAuthor( author ),
            branch, rev,
            epoch,
            message );

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

    if ( !Repositories::load( repos_config, max_rev, trunk_base, trunk, branches, tags ) )
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
