#include "filter.hxx"

#include <iostream>

using namespace std;

Filter::Filter( const char* fname_, unsigned int mark_ )
    : mark( mark_ ),
      tabs_to_spaces( true ),
      type( NO_FILTER )
{
    data.reserve( 16384 );

    char* suffix = strrchr( fname_, '.' );
    if ( suffix != NULL )
    {
        ++suffix;
#define IS_SUFFIX( suf ) strcmp( suf, suffix ) == 0
        if ( IS_SUFFIX( "c"   ) ||
             IS_SUFFIX( "cpp" ) ||
             IS_SUFFIX( "cxx" ) ||
             IS_SUFFIX( "h"   ) ||
             IS_SUFFIX( "hxx" ) ||
             IS_SUFFIX( "idl" ) ||
             IS_SUFFIX( "mk"  ) ||
             IS_SUFFIX( "pmk" ) ||
             IS_SUFFIX( "pl"  ) ||
             IS_SUFFIX( "pm"  ) ||
             IS_SUFFIX( "sh"  ) ||
             IS_SUFFIX( "src" ) ||
             IS_SUFFIX( "xcu" ) ||
             IS_SUFFIX( "xml" ) )
        {
            type = FILTER_TABS;
        }
#undef IS_SUFFIX
    }
}

void Filter::addData( const char* data_, size_t len_ )
{
    switch ( type )
    {
        case NO_FILTER:   addDataNoFilter( data_, len_ ); break;
        case FILTER_TABS: addDataFilterTabs( data_, len_ ); break;
    }
}

void Filter::addDataNoFilter( const char* data_, size_t len_ )
{
    data.append( data_, len_ );
}

void Filter::addDataFilterTabs( const char* data_, size_t len_ )
{
    char tmp[4*len_];
    char *dest = tmp;

    // convert the leading tabs to 4 spaces
    for ( const char* it = data_; it < data_ + len_; ++it )
    {
        if ( *it == '\t' && tabs_to_spaces )
        {
            *dest++ = ' ';
            *dest++ = ' ';
            *dest++ = ' ';
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
    out_ << "blob" << endl
         << "mark :" << mark << endl
         << "data " << data.size() << endl
         << data << endl;
}
