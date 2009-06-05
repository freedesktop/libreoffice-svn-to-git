/*
 * Trivial error handling.
 *
 * Author: Jan Holesovsky <kendy@suse.cz>
 * License: MIT <http://www.opensource.org/licenses/mit-license.php>
 */

#include <string>

namespace Error {
    void report( const std::string& message_ );

    int returnValue();
}
