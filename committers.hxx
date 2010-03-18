/*
 * Read committers from a file, assign commits to them.
 *
 * Author: Jan Holesovsky <kendy@suse.cz>
 * License: MIT <http://www.opensource.org/licenses/mit-license.php>
 */

#ifndef _COMMITTERS_HXX_
#define _COMMITTERS_HXX_

#include <map>
#include <string>

struct Committer
{
    std::string name;
    std::string email;

    Committer() : name(), email() {}

    Committer( const std::string& name_, const std::string& email_ ) : name( name_ ), email( email_ ) {};
};

namespace Committers
{
    void load( const char *fname );

    const Committer& getAuthor( const char* name );
    const Committer& getAuthor( const std::string& name );
}

#endif // _COMMITTERS_HXX_
