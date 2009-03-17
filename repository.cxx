#include "committers.hxx"
#include "filter.hxx"
#include "repository.hxx"

#include <iostream>
#include <set>
#include <sstream>
#include <vector>

using namespace std;

typedef vector< Repository* > Repos;
typedef set< string > Branches;
typedef set< unsigned int > RevisionIgnore;
typedef set< string > TagIgnore;
typedef vector< string > BranchIds;

static Repos repos;
static Branches branches;
static RevisionIgnore revision_ignore;
static TagIgnore tag_ignore;
static BranchIds branch_ids;

static unsigned char branchId( const string& branch_ )
{
    unsigned char id = 1;
    BranchIds::const_iterator it = branch_ids.begin();

    for ( ; ( it != branch_ids.end() ) && ( *it != branch_ ); ++it, ++id );

    if ( it == branch_ids.end() )
        branch_ids.push_back( branch_ );

    return id;
}

Repository::Repository( const std::string& reponame_, const string& regex_, unsigned int max_revs_ )
    : mark( 1 ), out( ( reponame_ + ".dump" ).c_str() ), commits( new unsigned char[max_revs_ + 10] )
{
    int status = regcomp( &regex_rule, regex_.c_str(), REG_EXTENDED | REG_NOSUB );
    if ( status != 0 )
        fprintf( stderr, "ERROR: Cannot create regex '%s'.\n", regex_.c_str() );

    memset( commits, 0, ( max_revs_ + 10 ) * sizeof( unsigned char ) );
}

Repository::~Repository()
{
    regfree( &regex_rule );
    delete[] commits;
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

        commits[commit_id_] = branchId( branch_ );
    }

    file_changes.clear();
    mark = 1;
}

void Repository::createBranch( const std::string& branch_, unsigned int from_, const std::string& from_branch_ )
{
    unsigned int from = findCommit( from_, from_branch_ );
    if ( from == 0 )
        return;

    out << "reset refs/heads/" << branch_ << "\nfrom :" << from << "\n" << endl;
}

void Repository::createTag( const Committer& committer_, const std::string& name_, unsigned int from_, const std::string& from_branch_, time_t time_, const char* log_, size_t log_len_ )
{
    unsigned int from = findCommit( from_, from_branch_ );
    if ( from == 0 )
        return;

    out << "tag " << name_
        << "\nfrom :" << from
        << "\ntagger " << committer_.name << " <" << committer_.email << "> " << time_ << " -0000\n"
        << "data " << log_len_ << "\n"
        << log_
        << endl;
}

unsigned int Repository::findCommit( unsigned int from_, const std::string& from_branch_ )
{
    unsigned char branch_id = branchId( from_branch_ );
    unsigned int commit_no = from_;
    
    while ( commit_no > 0 && commits[commit_no] != branch_id )
        --commit_no;

    return commit_no;
}

bool Repositories::load( const char* fname_, unsigned int max_revs_ )
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

        // settings
        if ( line[0] == ':' )
        {
            size_t space = line.find( ' ' );
            if ( space == string::npos )
                continue;

            string command( line.substr( 1, space - 1 ) );
            size_t arg = space + 1;
            
            if ( command == "set" )
            {
                size_t equals = line.find( '=', arg );
                if ( equals == string::npos )
                    continue;

                if ( line.substr( arg, equals - arg ) == "tabs_to_spaces" )
                {
                    size_t comma = line.find( ',', equals + 1 );
                    if ( comma = string::npos )
                        Filter::setTabsToSpaces( atoi( line.substr( equals + 1 ).c_str() ), string( ".*" ) );
                    else
                        Filter::setTabsToSpaces( atoi( line.substr( equals + 1, comma - equals - 1 ).c_str() ),
                                line.substr( comma + 1 ) );
                }
            }
            else if ( command == "revision" )
            {
                size_t colon = line.find( ':', arg );
                if ( colon == string::npos )
                    continue;

                if ( line.substr( arg, colon - arg ) == "ignore" )
                {
                    unsigned int which = atoi( line.substr( colon + 1 ).c_str() );
                    if ( which > 0 )
                        revision_ignore.insert( which );
                }
            }
            else if ( command == "tag" )
            {
                size_t colon = line.find( ':', arg );
                if ( colon == string::npos )
                    continue;

                if ( line.substr( arg, colon - arg ) == "ignore" )
                    tag_ignore.insert( line.substr( colon + 1 ) );
            }

            continue;
        }

        // find the separators
        size_t delim = line.find( '=' );
        if ( delim == string::npos )
        {
            fprintf( stderr, "ERROR: Wrong repository description '%s'\n", line.c_str() );
            continue;
        }

        repos.push_back( new Repository( line.substr( 0, delim ), line.substr( delim + 1 ), max_revs_ ) );

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

void Repositories::createBranch( const std::string& branch_, unsigned int from_, const std::string& from_branch_ )
{
    for ( Repos::iterator it = repos.begin(); it != repos.end(); ++it )
        (*it)->createBranch( branch_, from_, from_branch_ );

    branches.insert( branch_ );
}

void Repositories::createTag( const Committer& committer_, const std::string& name_, unsigned int from_, const std::string& from_branch_, time_t time_, const char* log_, size_t log_len_ )
{
    for ( Repos::iterator it = repos.begin(); it != repos.end(); ++it )
        (*it)->createTag( committer_, name_, from_, from_branch_, time_, log_, log_len_ );
}

bool Repositories::ignoreRevision( unsigned int commit_id_ )
{
    RevisionIgnore::const_iterator it = revision_ignore.find( commit_id_ );

    return ( it != revision_ignore.end() );
}

bool Repositories::ignoreTag( const string& name_ )
{
    TagIgnore::const_iterator it = tag_ignore.find( name_ );

    return ( it != tag_ignore.end() );
}
