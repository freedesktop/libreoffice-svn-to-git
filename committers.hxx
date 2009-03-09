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

class Committers
{
    struct ltstr
    {
        bool operator()( const char* s1, const char* s2 ) const
        {
            return strcmp( s1, s2 ) < 0;
        }
    };

    typedef std::map< const char*, Committer, ltstr > CommittersMap;

    CommittersMap committers;

public:
    Committers();

    void load( const char *fname );
    
    const Committer& getAuthor( const char* name );
};

#endif // _COMMITTERS_HXX_
