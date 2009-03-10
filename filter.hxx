#ifndef _FILTER_HXX_
#define _FILTER_HXX_

#include <string>
#include <ostream>

class Filter
{
    unsigned int mark;

    std::string data;

    /// Should we convert?
    ///
    /// We have to remember it between addData() calls.
    bool tabs_to_spaces;

    enum FilterType { NO_FILTER, FILTER_TABS };

    FilterType type;

public:
    Filter( const char* fname_, unsigned int mark_ );

    void addData( const char* data_, size_t len_ );

    void write( std::ostream& out_ );

private:
    void addDataNoFilter( const char* data_, size_t len_ );

    void addDataFilterTabs( const char* data_, size_t len_ );
};

#endif // _FILTER_HXX_
