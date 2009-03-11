#ifndef _REPOSITORY_HXX_
#define _REPOSITORY_HXX_

#include <string>
#include <fstream>

#include <regex.h>

class Committer;

class Repository
{
    /// Remember what files we changed and how (deletes/modifications).
    std::string file_changes;

    /// Counter for the files.
    unsigned int mark;

    /// Regex for matching the fnames.
    regex_t regex_rule;

    /// FIXME for the testing reasons let's store to files.
    std::ofstream out;

public:
    /// The regex_ is here to decide if the file belongs to this repository.
    Repository( const std::string& reponame_, const std::string& regex_ );

    ~Repository();

    /// Does the file belong to this repository (based on the regex we got?)
    bool matches( const char* fname_ ) const;

    /// The file should be marked for deletion.
    void deleteFile( const char* fname_ );

    /// The file should be marked for addition/modification.
    std::ostream& modifyFile( const char* fname_, const char* mode_ );

    /// Commit all the changes we did.
    void commit( const Committer& committer_, time_t time_, const char* log_, size_t log_len_ );
};

namespace Repositories
{
    /// Load the repositories layout from the config file.
    bool load( const char* fname_ );

    /// Get the right repository according to the filename.
    Repository& get( const char* fname_ );

    /// Commit to the all repositories that have some changes.
    void commit( const Committer& committer_, time_t time_, const char* log_, size_t log_len_ );
}

#endif // _REPOSITORY_HXX_
