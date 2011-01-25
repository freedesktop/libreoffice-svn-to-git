/*
 * Filter tabs -> spaces.
 *
 * Author: Jan Holesovsky <kendy@suse.cz>
 * License: MIT <http://www.opensource.org/licenses/mit-license.php>
 */

#include "error.hxx"
#include "filter.hxx"

#include <regex.h>

#include <cstring>
#include <iostream>

using namespace std;

struct Tabs {
    int spaces;
    bool has_exclusion;
    regex_t regex;
    regex_t exclusion_regex;

    Tabs() : spaces( 0 ), has_exclusion(false) {}
    ~Tabs() { regfree( &regex ); }
};

static Tabs tabs;

Filter::Filter( const string& fname_ )
    : tabs_to_spaces( true ),
      type( NO_FILTER )
{
    data.reserve( 16384 );

    if ( tabs.spaces > 0 && regexec( &tabs.regex, fname_.c_str(), 0, NULL, 0 ) == 0 && !(tabs.has_exclusion && (regexec( &tabs.exclusion_regex, fname_.c_str(), 0, NULL, 0) == 0)))
    {
        type = FILTER_TABS;
//        fprintf( stderr, "Filter : %s\n", fname_.c_str() );
    }
    else
    {
//        fprintf( stderr, "Do Not filter : %s\n", fname_.c_str() );
    }
}

inline void addDataLoop( char*& dest, char what, bool& tabs_to_spaces, int no_spaces )
{
    if ( what == '\t' && tabs_to_spaces )
    {
        for ( int i = 0; i < no_spaces; ++i )
            *dest++ = ' ';
        return;
    }
    else if ( what == '\n' )
        tabs_to_spaces = true;
    else if ( what != ' ' )
        tabs_to_spaces = false;

    *dest++ = what;
}

void Filter::addData( const char* data_, size_t len_ )
{
    if ( type == NO_FILTER || tabs.spaces <= 0 )
    {
        data.append( data_, len_ );
        return;
    }

    // type == FILTER_TABS
    char tmp[4*len_];
    char *dest = tmp;

    // convert the leading tabs to N spaces (according to tabs.spaces)
    for ( const char* it = data_; it < data_ + len_; ++it )
        addDataLoop( dest, *it, tabs_to_spaces, tabs.spaces );

    data.append( tmp, dest - tmp );
}

void Filter::addData( const string& data_ )
{
    if ( type == NO_FILTER || tabs.spaces <= 0 )
    {
        data.append( data_ );
        return;
    }

    // type == FILTER_TABS
    char *tmp = new char[4*data_.size()];
    char *dest = tmp;

    // convert the leading tabs to N spaces (according to tabs.spaces)
    for ( const char* it = data_.data(), *end = data_.data() + data_.size(); it < end; ++it )
        addDataLoop( dest, *it, tabs_to_spaces, tabs.spaces );

    data.append( tmp, dest - tmp );

    delete[] tmp;
}

void Filter::write( std::ostream& out_ )
{
    out_ << "data " << data.size() << endl
         << data << endl;
}

void Filter::setTabsToSpaces( int how_many_spaces_, const std::string& files_regex_ )
{
    tabs.spaces = how_many_spaces_;

    int status = regcomp( &tabs.regex, files_regex_.c_str(), REG_EXTENDED | REG_NOSUB );
    if ( status != 0 )
    {
        Error::report( "Cannot create regex '" + files_regex_ + "' (for tabs_to_spaces_files)." );
        tabs.spaces = 0;
    }
}

void Filter::setExclusions( const std::string& exclusion_regex_ )
{
    int status = regcomp( &tabs.exclusion_regex, exclusion_regex_.c_str(), REG_EXTENDED | REG_NOSUB );
    if ( status != 0 )
    {
        Error::report( "Cannot create regex '" + exclusion_regex_ + "' (for exclude_tabs)." );
    }
    else
    {
       fprintf( stderr, "Setup exclusion filter : %s\n", exclusion_regex_.c_str() );
       tabs. has_exclusion = true;
    }

}
