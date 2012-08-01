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
#include <cstdio>
#include <iostream>
#include <vector>

using namespace std;

struct Tabs {
    int spaces;
    FilterType type;
    FilePermission perm;
    regex_t regex;

    Tabs( int spaces_, FilterType type_, FilePermission perm_ ) : spaces( spaces_ ), type( type_ ), perm( perm_ ) {}
    ~Tabs() { regfree( &regex ); }

    bool matches( const string& fname_ ) { return regexec( &regex, fname_.c_str(), 0, NULL, 0 ) == 0; }
};

static std::vector< Tabs* > tabs_vector;

Filter::Filter( const string& fname_ )
    : spaces( 0 ),
      column( 0 ),
      spaces_to_write( 0 ),
      nonspace_appeared( false ),
      type( NO_FILTER )
{
    data.reserve( 16384 );

    for ( std::vector< Tabs* >::const_iterator it = tabs_vector.begin(); it != tabs_vector.end(); ++it )
    {
        if ( (*it)->matches( fname_.c_str() ) )
        {
            spaces = (*it)->spaces;
            type = (*it)->type;
            perm = (*it)->perm;
            break; // 1st wins
        }
    }
}

/// The old way of tabs -> spaces: Just the leading whitespace, tab stop is always the same, regardless of the position
inline void addDataLoopOld( char*& dest, char what, int& column, int& spaces_to_write, bool& nonspace_appeared, int no_spaces )
{
    if ( what == '\t' && !nonspace_appeared )
    {
        column += no_spaces;
        spaces_to_write += no_spaces;
    }
    else if ( what == ' ' )
    {
        ++column;
        ++spaces_to_write;
    }
    else if ( what == '\n' )
    {
        // write out any spaces that we need
        for ( int i = 0; i < spaces_to_write; ++i )
            *dest++ = ' ';

        *dest++ = what;
        column = 0;
        spaces_to_write = 0;
        nonspace_appeared = false;
    }
    else
    {
        nonspace_appeared = true;

        // write out any spaces that we need
        for ( int i = 0; i < spaces_to_write; ++i )
            *dest++ = ' ';

        *dest++ = what;
        ++column;
        spaces_to_write = 0;
    }
}

/// Combine the 'old' way of tabs -> spaces with all (as if the old is applied first, and then the new one on top of that)
inline void addDataLoopCombined( char*& dest, char what, int& column, int& spaces_to_write, bool& nonspace_appeared, int no_spaces )
{
    if ( what == '\t' )
    {
        if ( nonspace_appeared )
        {
            // new behavior
            const int tab_size = no_spaces - ( column % no_spaces );
            column += tab_size;
            spaces_to_write += tab_size;
        }
        else
        {
            // old one
            column += no_spaces;
            spaces_to_write += no_spaces;
        }
    }
    else if ( what == ' ' )
    {
        ++column;
        ++spaces_to_write;
    }
    else if ( what == '\n' )
    {
        *dest++ = what;
        column = 0;
        spaces_to_write = 0;
        nonspace_appeared = false;
    }
    else
    {
        nonspace_appeared = true;

        // write out any spaces that we need
        for ( int i = 0; i < spaces_to_write; ++i )
            *dest++ = ' ';

        *dest++ = what;
        ++column;
        spaces_to_write = 0;
    }
}

/// Combine the 'old' way of tabs -> spaces with all + convert to DOS line ends
inline void addDataLoopCombinedDos( char*& dest, char what, int& column, int& spaces_to_write, bool& nonspace_appeared, int no_spaces )
{
    if ( what == '\t' )
    {
        if ( nonspace_appeared )
        {
            // new behavior
            const int tab_size = no_spaces - ( column % no_spaces );
            column += tab_size;
            spaces_to_write += tab_size;
        }
        else
        {
            // old one
            column += no_spaces;
            spaces_to_write += no_spaces;
        }
    }
    else if ( what == ' ' )
    {
        ++column;
        ++spaces_to_write;
    }
    else if ( what == '\n' )
    {
        // write out any spaces that we need
        for ( int i = 0; i < spaces_to_write; ++i )
            *dest++ = ' ';

        *dest++ = '\r';
        *dest++ = what;
        column = 0;
        spaces_to_write = 0;
        nonspace_appeared = false;
    }
    else
    {
        nonspace_appeared = true;

        // write out any spaces that we need
        for ( int i = 0; i < spaces_to_write; ++i )
            *dest++ = ' ';

        *dest++ = what;
        ++column;
        spaces_to_write = 0;
    }
}

/// The best tabs -> spaces: converts all, strips trailing whitespace
inline void addDataLoopTabs( char*& dest, char what, int& column, int& spaces_to_write, bool& nonspace_appeared, int no_spaces )
{
    if ( what == '\t' )
    {
        const int tab_size = no_spaces - ( column % no_spaces );
        column += tab_size;
        spaces_to_write += tab_size;
    }
    else if ( what == ' ' )
    {
        ++column;
        ++spaces_to_write;
    }
    else if ( what == '\n' )
    {
        *dest++ = what;
        column = 0;
        spaces_to_write = 0;
    }
    else
    {
        // write out any spaces that we need
        for ( int i = 0; i < spaces_to_write; ++i )
            *dest++ = ' ';

        *dest++ = what;
        ++column;
        spaces_to_write = 0;
    }
}

/// Just convert Unx line ends to DOS ones
inline void addDataLoopDos( char*& dest, char what, int& column, int& spaces_to_write, bool& nonspace_appeared, int no_spaces )
{
    if ( what == '\n' )
        *dest++ = '\r';

    *dest++ = what;
}

/// Just convert DOS line ends to Unx ones
inline void addDataLoopUnx( char*& dest, char what, int& column, int& spaces_to_write, bool& nonspace_appeared, int no_spaces )
{
    if ( what == '\r' )
        return;

    *dest++ = what;
}

void Filter::addData( const char* data_, size_t len_ )
{
    if ( type == NO_FILTER )
    {
        data.append( data_, len_ );
        return;
    }

    // create big enough buffer
    const int size = ( spaces < 2 )? 2*len_: spaces*len_;
    char *tmp = new char[size];
    char *dest = tmp;

    // convert the tabs to spaces (according to spaces)
    switch ( type )
    {
        case FILTER_OLD:
            for ( const char* it = data_; it < data_ + len_; ++it )
                addDataLoopOld( dest, *it, column, spaces_to_write, nonspace_appeared, spaces );
            break;
        case FILTER_COMBINED:
        case FILTER_COMBINED_HACK:
            for ( const char* it = data_; it < data_ + len_; ++it )
                addDataLoopCombined( dest, *it, column, spaces_to_write, nonspace_appeared, spaces );
            break;
        case FILTER_COMBINED_DOS:
            for ( const char* it = data_; it < data_ + len_; ++it )
                addDataLoopCombinedDos( dest, *it, column, spaces_to_write, nonspace_appeared, spaces );
            break;
        case FILTER_TABS:
            for ( const char* it = data_; it < data_ + len_; ++it )
                addDataLoopTabs( dest, *it, column, spaces_to_write, nonspace_appeared, spaces );
            break;
        case FILTER_DOS:
            for ( const char* it = data_; it < data_ + len_; ++it )
                addDataLoopDos( dest, *it, column, spaces_to_write, nonspace_appeared, spaces );
            break;
        case FILTER_UNX:
            for ( const char* it = data_; it < data_ + len_; ++it )
                addDataLoopUnx( dest, *it, column, spaces_to_write, nonspace_appeared, spaces );
            break;
        case NO_FILTER:
            // NO_FILTER already handled
            break;
    }

    data.append( tmp, dest - tmp );

    delete[] tmp;
}

void Filter::addData( const string& data_ )
{
    addData( data_.data(), data_.size() );
}

void Filter::write( std::ostream& out_ )
{
    if ( type == FILTER_COMBINED_HACK )
    {
        // write out any spaces that we need
        for ( int i = 0; i < spaces_to_write; ++i )
            data += ' ';
    }

    out_ << "data " << data.size() << endl
         << data << endl;
}

void Filter::addTabsToSpaces( int how_many_spaces_, FilterType type_, const std::string& files_regex_, FilePermission perm_ )
{
    Tabs* tabs = new Tabs( how_many_spaces_, type_, perm_ );

    int status = regcomp( &tabs->regex, files_regex_.c_str(), REG_EXTENDED | REG_NOSUB );
    if ( status == 0 )
        tabs_vector.push_back( tabs );
    else
    {
        Error::report( "Cannot create regex '" + files_regex_ + "' (for tabs_to_spaces_files)." );
        delete tabs;
    }
}
