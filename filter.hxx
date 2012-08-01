/*
 * Filter tabs -> spaces.
 *
 * Author: Jan Holesovsky <kendy@suse.cz>
 * License: MIT <http://www.opensource.org/licenses/mit-license.php>
 */

#ifndef _FILTER_HXX_
#define _FILTER_HXX_

#include <string>
#include <ostream>

enum FilterType {
    NO_FILTER,           ///< No filtering at all
    FILTER_OLD,          ///< Old way of filtering - each tab is exactly <n> spaces, no filtering after 1st non-tab, non-space character
    FILTER_COMBINED,     ///< Combined way of filtering - for the tabs before 1st non-tab, non-space character, behave like _OLD, for the rest as _TABS
    FILTER_COMBINED_DOS, ///< Combined way of filtering (as described above) + convert to DOS line ends
    FILTER_COMBINED_HACK,///< Combined way of filtering + let the spaces at the end of file untouched when the file does not end with \n
    FILTER_TABS,         ///< New way of filtering - each tab is converted as if it was a real tab + strip trailing whitespace
    FILTER_DOS,          ///< Convert to the DOS line ends
    FILTER_UNX,          ///< Convert to the Unx line ends
};

enum FilePermission {
    PERMISSION_NO_CHANGE, ///< Do not change the permissions, use as stated in the original repo
    PERMISSION_EXEC,      ///< Add the executable bit
    PERMISSION_NOEXEC,    ///< Remove the executable bit
};

class Filter
{
    std::string data;

    /// This filter adds considers a tab this amount of spaces.
    int spaces;

    /// Current column in the output (resets with every \n).
    int column;

    /// In order to strip trailing spaces, we do not write them immediately.
    int spaces_to_write;

    /// Needed for the 'old' and 'combined' types
    bool nonspace_appeared;

    FilterType type;

    FilePermission perm;

public:
    Filter( const std::string& fname_ );

    void addData( const char* data_, size_t len_ );

    void addData( const std::string& data_ );

    void write( std::ostream& out_ );

    FilePermission getPermission() { return perm; }

    static void addTabsToSpaces( int how_many_spaces_, FilterType type_, const std::string& files_regex_, FilePermission perm_ = PERMISSION_NO_CHANGE );
};

#endif // _FILTER_HXX_
