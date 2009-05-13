#include "filter.hxx"

#include <regex.h>

#include <cstring>
#include <iostream>

using namespace std;

struct Tabs {
    int spaces;
    regex_t regex;

    Tabs() : spaces( 0 ) {}
    ~Tabs() { regfree( &regex ); }
};

static Tabs tabs;

Filter::Filter( const string& fname_ )
    : tabs_to_spaces( true ),
      type( NO_FILTER )
{
    data.reserve( 16384 );

    if ( tabs.spaces > 0 && regexec( &tabs.regex, fname_.c_str(), 0, NULL, 0 ) == 0 )
        type = FILTER_TABS;
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
    {
        if ( *it == '\t' && tabs_to_spaces )
        {
            for ( int i = 0; i < tabs.spaces; ++i )
                *dest++ = ' ';
            continue;
        }
        else if ( *it == '\n' )
            tabs_to_spaces = true;
        else if ( *it != ' ' )
            tabs_to_spaces = false;

        *dest++ = *it;
    }

    data.append( tmp, dest - tmp );
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
        fprintf( stderr, "ERROR: Cannot create regex '%s' (for tabs_to_spaces_files).\n", files_regex_.c_str() );
        tabs.spaces = 0;
    }
}
