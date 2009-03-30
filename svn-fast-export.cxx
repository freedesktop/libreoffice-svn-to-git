/*
 * svn-fast-export.c
 * ----------
 *  Walk through each revision of a local Subversion repository and export it
 *  in a stream that git-fast-import can consume.
 *
 * Author: Chris Lee <clee@kde.org>
 * License: MIT <http://www.opensource.org/licenses/mit-license.php>
 */

#define _XOPEN_SOURCE
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#include <ostream>

#include "committers.hxx"
#include "filter.hxx"
#include "repository.hxx"

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#include <apr_lib.h>
#include <apr_getopt.h>
#include <apr_general.h>

#include <svn_fs.h>
#include <svn_repos.h>
#include <svn_pools.h>
#include <svn_types.h>

#undef SVN_ERR
#define SVN_ERR(expr) SVN_INT_ERR(expr)
#define apr_sane_push(arr, contents) *(char **)apr_array_push(arr) = contents

using namespace std;

static string trunk_base = "/trunk";
static string trunk = trunk_base + "/";
static string branches = "/branches/";
static string tags = "/tags/";

static time_t get_epoch( const char *svn_date )
{
    struct tm tm = {0};
    char date[(strlen(svn_date) * sizeof(char *))];
    strncpy(date, svn_date, strlen(svn_date) - 8);
    strptime(date, "%Y-%m-%dT%H:%M:%S", &tm);
    return mktime(&tm);
}

static int dump_blob(svn_fs_root_t *root, char *full_path, apr_pool_t *pool, ostream& out)
{
    svn_stream_t   *stream;

    SVN_ERR( svn_fs_file_contents( &stream, root, full_path, pool ) );

    const size_t buffer_size = 8192;
    char buffer[buffer_size];

    Filter filter( full_path );
    apr_size_t len;
    do {
        len = buffer_size;
        SVN_ERR( svn_stream_read( stream, buffer, &len ) );
        filter.addData( buffer, len );
    } while ( len > 0 );

    filter.write( out );

    return 0;
}

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

static string branch_name( const char* path_ )
{
    if ( is_trunk( path_ ) || trunk_base == path_ )
        return string( "master" );
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
            return string();

        size_t slash = tmp.find( '/' );
        if ( slash == string::npos )
            return prefix + tmp;
        else
            return prefix + tmp.substr( 0, slash );
    }
}

static string file_name( const char* path_ )
{
    if ( is_trunk( path_ ) )
        return string( path_ + trunk.length() );
    else if ( trunk_base == path_ )
        return string();
    else
    {
        string tmp;
        if ( is_branch( path_ ) )
            tmp = path_ + branches.length();
        else if ( is_tag( path_ ) )
            tmp = path_ + tags.length();
        else
            return string();

        size_t slash = tmp.find( '/' );
        if ( slash == string::npos )
            return string();
        else
            return tmp.substr( slash + 1 );
    }
}

int export_revision(svn_revnum_t rev, svn_fs_t *fs, apr_pool_t *pool)
{
    const void           *key;
    void                 *val;
    char                 *path, *file_change;
    apr_pool_t           *revpool;
    apr_hash_t           *changes, *props;
    apr_hash_index_t     *i;
    svn_string_t         *author, *committer, *svndate, *svnlog;
    svn_boolean_t        is_dir;
    svn_fs_root_t        *fs_root;
    svn_fs_path_change_t *change;

    fprintf( stderr, "Exporting revision %ld... ", rev );
    
    if ( Repositories::ignoreRevision( rev ) )
    {
        fprintf( stderr, "ignored.\n" );
        return 0;
    }

    SVN_ERR(svn_fs_revision_root(&fs_root, fs, rev, pool));
    SVN_ERR(svn_fs_paths_changed(&changes, fs_root, pool));
    SVN_ERR(svn_fs_revision_proplist(&props, fs, rev, pool));

    revpool = svn_pool_create(pool);

    author = static_cast<svn_string_t*>( apr_hash_get(props, "svn:author", APR_HASH_KEY_STRING) );
    if ( !author || svn_string_isempty( author ) )
        author = svn_string_create( "nobody", pool );
    svndate = static_cast<svn_string_t*>( apr_hash_get(props, "svn:date", APR_HASH_KEY_STRING) );
    time_t epoch = get_epoch( static_cast<const char *>( svndate->data ) );

    svnlog = static_cast<svn_string_t*>( apr_hash_get(props, "svn:log", APR_HASH_KEY_STRING) );

    string branch;
    bool no_changes = true;
    bool debug_once = true;
    for (i = apr_hash_first(pool, changes); i; i = apr_hash_next(i)) {
        svn_pool_clear(revpool);
        apr_hash_this(i, &key, NULL, &val);
        path = (char *)key;
        change = (svn_fs_path_change_t *)val;

        if ( debug_once )
        {
            fprintf( stderr, "path: %s... ", path );
            debug_once = false;
        }

        // don't care about anything in the toplevel
        if ( path[0] != '/' || strchr( path + 1, '/' ) == NULL )
            continue;

        SVN_ERR(svn_fs_is_dir(&is_dir, fs_root, path, revpool));

        // detect creation of branch/tag
        if ( is_dir && change->change_kind == svn_fs_path_change_add )
        {
            // create a new branch/tag?
            if ( is_branch( path ) || is_tag( path ) )
            {
                bool branching = is_branch( path );

                string this_branch( branch_name( path ) );
                if ( file_name( path ).empty() )
                {
                    if ( !branching && Repositories::ignoreTag( this_branch ) )
                        continue;

                    // is it a new branch/tag
                    svn_revnum_t rev_from;
                    const char* path_from;
                    SVN_ERR( svn_fs_copied_from( &rev_from, &path_from, fs_root, path, revpool ) );

                    if ( path_from != NULL && file_name( path_from ).empty() )
                    {
                        Repositories::createBranchOrTag( branching,
                                rev_from, branch_name( path_from ),
                                Committers::getAuthor( author->data ),
                                this_branch, rev,
                                epoch,
                                string( svnlog->data, svnlog->len ) );
                    }
                    continue;
                }
            }
        }

        string fname( file_name( path ) );
        string check_branch( branch_name( path ) );

        // skip whatever we do not know where does it fit
        if ( check_branch.empty() )
            continue;

        // ignore the tags we do not want
        if ( is_tag( path ) && Repositories::ignoreTag( check_branch ) )
            continue;

        // sanity check
        if ( branch.empty() )
            branch = check_branch;
        else if ( branch != check_branch )
        {
            // we found a commit that belongs to more branches at once!
            // let's commit what we have so far so that we can commit the
            // rest to the other branch later
            Repositories::commit( Committers::getAuthor( author->data ),
                    branch, rev,
                    epoch,
                    string( svnlog->data, svnlog->len ) );
            branch = check_branch;
        }

        // get the repository where do we want to commit
        Repository& repo = Repositories::get( fname );

        // add/remove/move the files
        if ( change->change_kind == svn_fs_path_change_delete )
            repo.deleteFile( fname );
        else if ( is_dir )
        {
            svn_revnum_t rev_from;
            const char* path_from;
            SVN_ERR( svn_fs_copied_from( &rev_from, &path_from, fs_root, path, revpool ) );
            if ( path_from == NULL )
                continue;

            string from_branch( branch_name( path_from ) );
            string from_fname( file_name( path_from ) );

            if ( branch != from_branch )
            {
                fprintf( stderr, "ERROR: Copy of directories across branches (%s, %s).\n",
                        branch.c_str(), from_branch.c_str() );
                continue;
            }

            repo.copyPath( from_fname, fname );
        }
        else
        {
            svn_string_t *propvalue;
            SVN_ERR(svn_fs_node_prop(&propvalue, fs_root, (char *)path, "svn:executable", pool));
            const char* mode = "644";
            if (propvalue)
                mode = "755";
            
            SVN_ERR(svn_fs_node_prop(&propvalue, fs_root, (char *)path, "svn:special", pool));
            if (propvalue)
                fprintf(stderr, "ERROR: Got a symlink; we cannot handle symlinks now.\n");

            ostream& out = repo.modifyFile( fname, mode );

            dump_blob(fs_root, (char *)path, revpool, out);
        }

        no_changes = false;
    }

    if ( no_changes || branch.empty() )
    {
        fprintf( stderr, "skipping.\n" );
        svn_pool_destroy( revpool );
        return 0;
    }

    Repositories::commit( Committers::getAuthor( author->data ),
            branch, rev,
            epoch,
            string( svnlog->data, svnlog->len ) );

    svn_pool_destroy(revpool);

    fprintf(stderr, "done!\n");

    return 0;
}

int crawl_revisions( char *repos_path, const char* repos_config )
{
    apr_pool_t   *pool, *subpool;
    svn_fs_t     *fs;
    svn_repos_t  *repos;
    svn_revnum_t youngest_rev, min_rev, max_rev, rev;

    pool = svn_pool_create(NULL);

    SVN_ERR(svn_fs_initialize(pool));
    SVN_ERR(svn_repos_open(&repos, repos_path, pool));
    if ((fs = svn_repos_fs(repos)) == NULL)
        return -1;
    SVN_ERR(svn_fs_youngest_rev(&youngest_rev, fs, pool));

    min_rev = 1;
    max_rev = youngest_rev;

    if ( !Repositories::load( repos_config, max_rev, trunk_base, trunk, branches, tags ) )
    {
        fprintf( stderr, "ERROR: Must have at least one valid repository definition.\n" );
        return 1;
    }

    subpool = svn_pool_create(pool);
    for (rev = min_rev; rev <= max_rev; rev++) {
        svn_pool_clear(subpool);
        export_revision(rev, fs, subpool);
    }

    svn_pool_destroy(pool);

    return 0;
}

int main(int argc, char *argv[])
{
    if (argc != 4) {
        fprintf(stderr, "usage: %s REPOS_PATH committers.txt reposlayout.txt\n", argv[0]);
        return -1;
    }

    if (apr_initialize() != APR_SUCCESS) {
        fprintf(stderr, "You lose at apr_initialize().\n");
        return -1;
    }

    Committers::load( argv[2] );

    crawl_revisions( argv[1], argv[3] );

    apr_terminate();

    Repositories::close();

    return 0;
}
