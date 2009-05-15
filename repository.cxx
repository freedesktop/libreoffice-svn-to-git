#include "committers.hxx"
#include "error.hxx"
#include "filter.hxx"
#include "repository.hxx"

#include <cstdlib>
#include <cstring>
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
typedef vector< Tag* > Tags;

static Repos repos;
static Branches branches;
static RevisionIgnore revision_ignore;
static TagIgnore tag_ignore;
static BranchIds branch_ids; // needed in addition to 'branches' because here we create the ids on demand
static Tags tags;

struct CommitMessages
{
    bool convert;
    regex_t regex;

    CommitMessages() : convert( false )
    {
        // Match a the 'header' of a ChangeLog entry, or a part starting with
        // asterisk
        regcomp( &regex, "^([0-9]{4}-[0-9][0-9]?-[0-9][0-9]? .*<.*@.*>|^[ \t]*\\*)", REG_EXTENDED | REG_NOSUB );
    }
    
    ~CommitMessages()
    {
        regfree( &regex );
    }

    bool match( const string& message_ )
    {
        string first_line;
        size_t eol = message_.find( '\n' );

        if ( eol == string::npos )
            first_line = message_;
        else
            first_line = message_.substr( 0, eol );

        return ( regexec( &regex, first_line.c_str(), 0, NULL, 0 ) == 0 );
    }
};

static CommitMessages commit_messages;

static BranchId branchId( const string& branch_ )
{
    BranchId id = 1;
    BranchIds::const_iterator it = branch_ids.begin();

    for ( ; ( it != branch_ids.end() ) && ( *it != branch_ ); ++it, ++id );

    if ( it == branch_ids.end() )
        branch_ids.push_back( branch_ );

    return id;
}

static string eatWhitespace( const string& text_, bool uppercase_first_letter = false, bool eat_eol = false )
{
    char result[text_.length()];
    char* it = result;

    bool eat = true;
    bool first_eol = true;
    bool first_letter = true;
    for ( string::const_iterator i = text_.begin(); i != text_.end(); ++i )
    {
        if ( *i == '\n' )
        {
            eat = true;
            if ( !first_eol && !eat_eol )
                *it++ = *i;
            else if ( !first_eol && eat_eol )
                *it++ = ' ';
        }
        else if ( !eat || ( *i != ' ' && *i != '\t' ) )
        {
            eat = false;
            first_eol = false;
            if ( uppercase_first_letter && first_letter )
            {
                first_letter = false;
                if ( *i >= 'a' && *i <= 'z' )
                    *it++ = *i - ( 'a' - 'A' );
                else
                    *it++ = *i;
            }
            else    
                *it++ = *i;
        }
    }

    return string( result, it - result );
}

static string commitMessage( const string& log_ )
{
    if ( !commit_messages.convert )
        return log_;

    // It's not a ChangeLog-like entry, do just some cosmetic changes.
    if ( !commit_messages.match( log_ ) )
        return eatWhitespace( log_, true );

    // It's a ChangeLog-like entry, try to find a sentence and use it as the
    // first line description of the commit, eg.
    // from
    // |2008-02-12  The Name  <the@address.com>
    // |
    // |        * some/file.txt: Changed this to that.
    // |        * some/other/file.txt: The same here.
    // to
    // |Changed this to that.
    // |
    // |* some/file.txt: Changed this to that.
    // |* some/other/file.txt: The same here.
    size_t start_line = 0;
    if ( log_[0] >= '0' && log_[0] <= '9' )
    {
        // Let's skip the first line if it contains just the date & committer.
        start_line = log_.find( '\n' );
        if ( start_line == string::npos )
            return eatWhitespace( log_ );
    }

    ++start_line;
    size_t text = log_.find_first_of( "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789", start_line );
    size_t asterisk = log_.find( '*', start_line );

    if ( text == string::npos )
        return eatWhitespace( log_ );

    if ( asterisk == string::npos )
        return eatWhitespace( log_.substr( start_line ) );

    if ( asterisk < text )
    {
        text = asterisk + 1;
        do {
            text = log_.find( ":", text );
            if ( text == string::npos )
                return eatWhitespace( log_ );

            ++text;
        } while ( text < log_.length() && log_[text] == '\n' );
        
        if ( text >= log_.length() )
            return eatWhitespace( log_ );
    }

    size_t fullstop = log_.find_first_of( "*.", text );
    if ( fullstop != string::npos && log_[fullstop] == '*' )
        fullstop = log_.find_last_not_of( "* \t", fullstop );

    string description;
    if ( fullstop == string::npos )
        description = eatWhitespace( log_.substr( text ), true, true );
    else
        description = eatWhitespace( log_.substr( text, fullstop - text + 1 ), true, true );

    if ( description.length() > 0 )
        return description + "\n\n" + eatWhitespace( log_.substr( start_line ) );

    return eatWhitespace( log_.substr( start_line ) );
}

Tag::Tag( const Committer& committer_, const std::string& name_, time_t time_, const std::string& log_ )
    : name( name_ ), tag_branch( name_ ), committer( committer_ ), time( time_ ), log( commitMessage( log_ ) )
{
    const size_t tag_branches_len = strlen( TAG_TEMP_BRANCH );
    if ( name.compare( 0, tag_branches_len, TAG_TEMP_BRANCH ) == 0 )
        name = name.substr( tag_branches_len );
    else
        Error::report( "Cannot guess the branch name for '" + name_ + "'" );
}

Repository::Repository( const std::string& reponame_, const string& regex_, unsigned int max_revs_ )
    : mark( 1 ),
      out( ( reponame_ + ".dump" ).c_str() ),
      commits( new BranchId[max_revs_ + 10] ),
      max_revs( max_revs_ )
{
    int status = regcomp( &regex_rule, regex_.c_str(), REG_EXTENDED | REG_NOSUB );
    if ( status != 0 )
        Error::report( "Cannot create regex '" + regex_ + "'" );

    memset( commits, 0, ( max_revs_ + 10 ) * sizeof( BranchId ) );
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

void Repository::commit( const Committer& committer_, const std::string& name_, unsigned int commit_id_, time_t time_, const std::string& log_, bool force_ )
{
    if ( force_ || !file_changes.empty() || !path_copies.empty() )
    {
        out << "commit refs/heads/" << name_ << "\n";

        if ( commit_id_ )
            out << "mark :" << 100000 + commit_id_ << "\n";

        string log( commitMessage( log_ ) );

        out << "committer " << committer_.name << " <" << committer_.email << "> " << time_ << " -0000\n"
            << "data " << log.length() << "\n"
            << log << "\n"
            << path_copies
            << file_changes
            << endl;

        commits[commit_id_] = branchId( name_ );
    }

    file_changes.clear();
    path_copies.clear();
    mark = 1;
}

void Repository::createBranch( unsigned int from_, const std::string& from_branch_,
        const Committer& committer_, const std::string& name_, unsigned int commit_id_, time_t time_, const std::string& log_ )
{
    unsigned int from = findCommit( from_, from_branch_ );
    if ( from == 0 )
        return;

    out << "reset refs/heads/" << name_ << "\nfrom :" << 100000 + from << "\n" << endl;

    commit( committer_, name_, commit_id_, time_, log_, true );
}

void Repository::createTag( const Tag& tag_ )
{
    unsigned int from = findCommit( max_revs, tag_.tag_branch );
    if ( from == 0 )
        return;
    
    out << "tag " << tag_.name
        << "\nfrom :" << 100000 + from
        << "\ntagger " << tag_.committer.name << " <" << tag_.committer.email << "> " << tag_.time << " -0000\n"
        << "data " << tag_.log.length() << "\n"
        << tag_.log
        << endl;
}

unsigned int Repository::findCommit( unsigned int from_, const std::string& from_branch_ )
{
    BranchId branch_id = branchId( from_branch_ );
    unsigned int commit_no = from_;
    
    while ( commit_no > 0 && commits[commit_no] != branch_id )
        --commit_no;

    return commit_no;
}

bool Repositories::load( const char* fname_, unsigned int max_revs_, std::string& trunk_base_, std::string& trunk_, std::string& branches_, std::string& tags_ )
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
                if ( ( equals == string::npos && line.substr( arg ) == "tabs_to_spaces" ) ||
                     ( equals != string::npos && line.substr( arg, equals - arg ) == "tabs_to_spaces" ) )
                {
                    if ( equals == string::npos )
                        Filter::setTabsToSpaces( 8, string( ".*" ) );
                    else
                    {
                        size_t comma = line.find( ',', equals + 1 );
                        if ( comma == string::npos )
                            Filter::setTabsToSpaces( atoi( line.substr( equals + 1 ).c_str() ), string( ".*" ) );
                        else
                            Filter::setTabsToSpaces( atoi( line.substr( equals + 1, comma - equals - 1 ).c_str() ),
                                    line.substr( comma + 1 ) );
                    }
                }
                else if ( line.substr( arg, equals - arg ) == "convert_commit_messages" )
                {
                    commit_messages.convert = true;
                }
                else if ( equals != string::npos && line.substr( arg, equals - arg ) == "trunk" )
                {
                    string tmp = line.substr( equals + 1 );
                    if ( tmp.empty() )
                        trunk_ = trunk_base_ = "/";
                    else
                    {
                        trunk_base_ = "/" + tmp;
                        trunk_ = trunk_base_ + "/";
                    }
                }
                else if ( equals != string::npos && line.substr( arg, equals - arg ) == "branches" )
                {
                    string tmp = line.substr( equals + 1 );
                    if ( tmp.empty() )
                        branches_ = "/";
                    else
                        branches_ = "/" + tmp + "/";
                }
                else if ( equals != string::npos && line.substr( arg, equals - arg ) == "tags" )
                {
                    string tmp = line.substr( equals + 1 );
                    if ( tmp.empty() )
                        tags_ = "/";
                    else
                        tags_ = "/" + tmp + "/";
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
                    tag_ignore.insert( TAG_TEMP_BRANCH + line.substr( colon + 1 ) );
            }

            continue;
        }

        // find the separators
        size_t delim = line.find( '=' );
        if ( delim == string::npos )
        {
            Error::report( "Wrong repository description '" + line + "'" );
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
    // write tags for all the 'tag tracking' branches
    for ( Repos::iterator it = repos.begin(); it != repos.end(); ++it )
        for ( Tags::const_iterator tag = tags.begin(); tag != tags.end(); ++tag )
            (*it)->createTag( *(*tag) );

    while ( !repos.empty() )
    {
        delete repos.back();
        repos.pop_back();
    }
    
    while ( !tags.empty() )
    {
        delete tags.back();
        tags.pop_back();
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

void Repositories::commit( const Committer& committer_, const std::string& name_, unsigned int commit_id_, time_t time_, const std::string& log_ )
{
    if ( branches.find( name_ ) == branches.end() )
        Error::report( "Committing to a branch that hasn't been initialized using Repositories::createBranchOrTag()!" );

    for ( Repos::iterator it = repos.begin(); it != repos.end(); ++it )
        (*it)->commit( committer_, name_, commit_id_, time_, log_ );
}

void Repositories::createBranchOrTag( bool is_branch_, unsigned int from_, const std::string& from_branch_,
        const Committer& committer_, const std::string& name_, unsigned int commit_id_, time_t time_, const std::string& log_ )
{
    for ( Repos::iterator it = repos.begin(); it != repos.end(); ++it )
        (*it)->createBranch( from_, from_branch_, committer_, name_, commit_id_, time_, log_ );

    branches.insert( name_ );

    if ( !is_branch_ )
        tags.push_back( new Tag( committer_, name_, time_, log_ ) );
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
