#include "committers.hxx"
#include "repository.hxx"

#include <iostream>
#include <set>
#include <sstream>
#include <vector>

using namespace std;

Repository::Repository( const std::string& reponame_, const string& regex_ )
    : mark( 1 ), out( ( reponame_ + ".dump" ).c_str() )
{
    int status = regcomp( &regex_rule, regex_.c_str(), REG_EXTENDED | REG_NOSUB );
    if ( status != 0 )
        fprintf( stderr, "ERROR: Cannot create regex '%s'.\n", regex_.c_str() );
}

Repository::~Repository()
{
    regfree( &regex_rule );
}

bool Repository::matches( const std::string& fname_ ) const
{
    return ( regexec( &regex_rule, fname_.c_str(), 0, NULL, 0 ) == 0 );
}

void Repository::deleteFile( const std::string& fname_ )
{
    file_changes.append( "D " );
    file_changes.append( fname_ );
    file_changes.append( "\n" );
}

ostream& Repository::modifyFile( const std::string& fname_, const char* mode_ )
{
    ostringstream sstr;

    sstr << "M " << mode_ << " :" << mark << " " << fname_ << "\n";
    
    file_changes.append( sstr.str() );

    // write the file header
    out << "blob" << endl
        << "mark :" << mark << endl;

    ++mark;

    return out;
}

void Repository::commit( const Committer& committer_, const std::string& branch_, unsigned int commit_id_, time_t time_, const char* log_, size_t log_len_ )
{
    if ( file_changes.length() != 0 )
    {
        out << "commit refs/heads/" << branch_ << "\n";

        if ( commit_id_ )
            out << "mark :" << commit_id_ << "\n";

        out << "committer " << committer_.name << " <" << committer_.email << "> " << time_ << " -0000\n"
            << "data " << log_len_ << "\n"
            << log_ << "\n"
            << file_changes
            << endl;
    }

    file_changes.clear();
    mark = 1;
}

void Repository::createBranch( const std::string& branch_, unsigned int from_ )
{
    out << "reset refs/heads/" << branch_ << "\nfrom :" << from_ << "\n" << endl;
}

void Repository::createTag( const Committer& committer_, const std::string& name_, unsigned int from_, time_t time_, const char* log_, size_t log_len_ )
{
    out << "tag " << name_
        << "\nfrom :" << from_
        << "\ntagger " << committer_.name << " <" << committer_.email << "> " << time_ << " -0000\n"
        << "data " << log_len_ << "\n"
        << log_ << "\n"
        << endl;
}

typedef vector< Repository* > Repos;
typedef set< string > Branches;

static Repos repos;

static Branches branches;

bool Repositories::load( const char* fname_ )
{
    ifstream input( fname_, ifstream::in );
    string line;
    bool result = false;

    while ( !input.eof() )
    {
        getline( input, line );

        // comments
        if ( line.length() == 0 || line[0] == '#' )
            continue;

        // find the separators
        size_t delim = line.find( "=" );
        if ( delim == string::npos )
        {
            fprintf( stderr, "ERROR: Wrong repository description '%s'\n", line.c_str() );
            continue;
        }

        repos.push_back( new Repository( line.substr( 0, delim ), line.substr( delim + 1 ) ) );

        result = true;
    }

    branches.insert( "master" );

    return result;
}

void Repositories::close()
{
    while ( !repos.empty() )
    {
        delete repos.back();
        repos.pop_back();
    }
}

Repository& Repositories::get( const std::string& fname_ )
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

void Repositories::commit( const Committer& committer_, const std::string& branch_, unsigned int commit_id_, time_t time_, const char* log_, size_t log_len_ )
{
    if ( branches.find( branch_ ) == branches.end() )
        cerr << "ERROR: Committing to a branch that hasn't been initialized using Repositories::createBranch()!" << endl;

    for ( Repos::iterator it = repos.begin(); it != repos.end(); ++it )
        (*it)->commit( committer_, branch_, commit_id_, time_, log_, log_len_ );
}

void Repositories::createBranch( const std::string& branch_, unsigned int from_ )
{
    for ( Repos::iterator it = repos.begin(); it != repos.end(); ++it )
        (*it)->createBranch( branch_, from_ );

    branches.insert( branch_ );
}

void Repositories::createTag( const Committer& committer_, const std::string& name_, unsigned int from_, time_t time_, const char* log_, size_t log_len_ )
{
    for ( Repos::iterator it = repos.begin(); it != repos.end(); ++it )
        (*it)->createTag( committer_, name_, from_, time_, log_, log_len_ );
}
