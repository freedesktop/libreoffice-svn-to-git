#include "committers.hxx"
#include "repository.hxx"

#include <iostream>
#include <sstream>
#include <vector>

using namespace std;

Repository::Repository( const char* regex_ )
    : mark( 1 )
{
    int status = regcomp( &regex_rule, regex_, REG_EXTENDED | REG_NOSUB );
    if ( status != 0 )
        fprintf( stderr, "ERROR: Cannot create regex '%s'.\n", regex_ );
}

Repository::~Repository()
{
    regfree( &regex_rule );
}

bool Repository::matches( const char* fname_ ) const
{
    return ( regexec( &regex_rule, fname_, 0, NULL, 0 ) == 0 );
}

void Repository::deleteFile( const char* fname_ )
{
    file_changes.append( "D " );
    file_changes.append( fname_ );
    file_changes.append( "\n" );
}

ostream& Repository::modifyFile( const char* fname_, const char* mode_ )
{
    ostringstream sstr;

    sstr << "M " << mode_ << " :" << mark << " " << fname_ << "\n";
    
    file_changes.append( sstr.str() );

    // FIXME the right stream for output of the file here
    ostream& out = cout;

    out << "blob" << endl
        << "mark :" << mark << endl;

    ++mark;

    return out;
}

void Repository::commit( const Committer& committer_, time_t time_, const char* log_, size_t log_len_ )
{
    if ( file_changes.length() != 0 )
    {
        cout << "commit refs/heads/master\n"
             << "committer " << committer_.name << " <" << committer_.email << "> " << time_ << " -0000\n"
             << "data " << log_len_ << "\n"
             << log_ << "\n"
             << file_changes
             << endl;
    }

    file_changes.clear();
    mark = 1;
}

typedef vector< Repository* > Repos;

static Repos repos;

bool Repositories::load( const char* fname_ )
{
    repos.push_back( new Repository( ".*" ) );

    return true;
}

Repository& Repositories::get( const char* fname_ )
{
    Repository* repo = repos.front();
    for ( Repos::const_iterator it = repos.begin(); it != repos.end(); ++it )
    {
        repo = (*it);
        if ( repo->matches( fname_ ) )
            break;
    }

    // the last one is the fallback
    return *repo;
}

void Repositories::commit( const Committer& committer_, time_t time_, const char* log_, size_t log_len_ )
{
    for ( Repos::iterator it = repos.begin(); it != repos.end(); ++it )
        (*it)->commit( committer_, time_, log_, log_len_ );
}
