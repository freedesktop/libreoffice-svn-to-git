#ifndef _REPOSITORY_HXX_
#define _REPOSITORY_HXX_

#include <string>
#include <ostream>

class Committer;

class Repository
{
    std::string file_changes;

public:
    Repository();

    void deleteFile( const char* fname_ );

    std::ostream& modifyFile( const char* fname_, const char* mode_, unsigned int mark_ );

    const std::string& getChanges() const { return file_changes; }

    void reset();

public:
    static Repository& get( const char* fname_ );

    static void commit( const Committer& committer_, time_t time_, const char* log_, size_t log_len_ );
};

#endif // _REPOSITORY_HXX_
