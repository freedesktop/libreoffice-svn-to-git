#include "filter.hxx"

#include <iostream>

using namespace std;

Filter::Filter( const char* fname_, unsigned int mark_ )
    : mark( mark_ ),
      tabs_to_spaces( true ),
      type( NO_FILTER )
{
    string fname( fname_ );

    size_t last_dot = fname.find_last_of( '.' );
    if ( last_dot != string::npos )
    {
        string suffix( fname.substr( last_dot + 1 ) );
        if ( suffix == "c"   ||
             suffix == "cpp" ||
             suffix == "cxx" ||
             suffix == "h"   ||
             suffix == "hxx" ||
             suffix == "idl" ||
             suffix == "mk"  ||
             suffix == "pmk" ||
             suffix == "pl"  ||
             suffix == "pm"  ||
             suffix == "sh"  ||
             suffix == "src" ||
             suffix == "xcu" ||
             suffix == "xml" )
        {
            type = FILTER_TABS;
        }
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
    string tmp;

    for ( const char* it = data_; it < data_ + len_; ++it )
    {
        switch ( *it )
        {
            case '\t':
                if ( tabs_to_spaces )
                    tmp += "    ";
                else
                    tmp += *it;
                break;

            case '\n':
                tabs_to_spaces = true;
                // no break
            case ' ':
                tmp += *it;
                break;

            default:
                tabs_to_spaces = false;
                tmp += *it;
        }
    }

    data.append( tmp );
}

void Filter::write( std::ostream& out_ )
{
    out_ << "blob" << endl
         << "mark :" << mark << endl
         << "data " << data.size() << endl
         << data << endl;
}
