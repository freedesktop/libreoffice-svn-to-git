#ifndef _FILTER_HXX_
#define _FILTER_HXX_

#include <string>
#include <ostream>

class Filter
{
    std::string data;

    /// Should we convert?
    ///
    /// We have to remember it between addData() calls.
    bool tabs_to_spaces;

    enum FilterType { NO_FILTER, FILTER_TABS };

    FilterType type;

public:
    Filter( const std::string& fname_ );

    void addData( const char* data_, size_t len_ );

    void write( std::ostream& out_ );

    static void setTabsToSpaces( int how_many_spaces_, const std::string& files_regex_ );
};

#endif // _FILTER_HXX_
