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

#define TRUNK "/trunk/"

using namespace std;

static time_t get_epoch( const char *svn_date )
{
    struct tm tm = {0};
    char date[(strlen(svn_date) * sizeof(char *))];
    strncpy(date, svn_date, strlen(svn_date) - 8);
    strptime(date, "%Y-%m-%dT%H:%M:%S", &tm);
    return mktime(&tm);
}

int dump_blob(svn_fs_root_t *root, char *full_path, apr_pool_t *pool, ostream& out)
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

    fprintf(stderr, "Exporting revision %ld... ", rev);

    SVN_ERR(svn_fs_revision_root(&fs_root, fs, rev, pool));
    SVN_ERR(svn_fs_paths_changed(&changes, fs_root, pool));
    SVN_ERR(svn_fs_revision_proplist(&props, fs, rev, pool));

    revpool = svn_pool_create(pool);

    bool any_changes = false;
    for (i = apr_hash_first(pool, changes); i; i = apr_hash_next(i)) {
        svn_pool_clear(revpool);
        apr_hash_this(i, &key, NULL, &val);
        path = (char *)key;
        change = (svn_fs_path_change_t *)val;

        SVN_ERR(svn_fs_is_dir(&is_dir, fs_root, path, revpool));

        if (is_dir || strncmp(TRUNK, path, strlen(TRUNK))) {
            continue;
        }

        Repository& repo = Repositories::get( path + strlen(TRUNK) );

        if (change->change_kind == svn_fs_path_change_delete) {
            repo.deleteFile( path + strlen(TRUNK) );
        } else {
            svn_string_t *propvalue;
            SVN_ERR(svn_fs_node_prop(&propvalue, fs_root, (char *)path, "svn:executable", pool));
            const char* mode = "644";
            if (propvalue)
                mode = "755";
            
            SVN_ERR(svn_fs_node_prop(&propvalue, fs_root, (char *)path, "svn:special", pool));
            if (propvalue)
                fprintf(stderr, "ERROR: Got a symlink; we cannot handle symlinks now.\n");

            ostream& out = repo.modifyFile( path + strlen(TRUNK), mode );

            dump_blob(fs_root, (char *)path, revpool, out);
        }

        any_changes = true;
    }

    if ( !any_changes )
    {
        fprintf( stderr, "skipping.\n" );
        svn_pool_destroy( revpool );
        return 0;
    }

    author = static_cast<svn_string_t*>( apr_hash_get(props, "svn:author", APR_HASH_KEY_STRING) );
    if (svn_string_isempty(author))
        author = svn_string_create("nobody", pool);
    svndate = static_cast<svn_string_t*>( apr_hash_get(props, "svn:date", APR_HASH_KEY_STRING) );
    svnlog = static_cast<svn_string_t*>( apr_hash_get(props, "svn:log", APR_HASH_KEY_STRING) );

    Repositories::commit( Committers::getAuthor( author->data ),
            get_epoch( static_cast<const char *>( svndate->data ) ),
            svnlog->data, svnlog->len );

    svn_pool_destroy(revpool);

    fprintf(stderr, "done!\n");

    return 0;
}

int crawl_revisions(char *repos_path)
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

    if ( !Repositories::load( argv[3] ) )
    {
        fprintf( stderr, "ERROR: Must have at least one valid repository definition.\n" );
        return 1;
    }

    crawl_revisions( argv[1] );

    apr_terminate();

    return 0;
}
