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

    /// Let's store to files.
    ///
    /// There can be a wrapping script that sets them up as named pipes that
    /// can feed the git fast-import(s).
    std::ofstream out;

public:
    /// The regex_ is here to decide if the file belongs to this repository.
    Repository( const std::string& reponame_, const std::string& regex_ );

    ~Repository();

    /// Does the file belong to this repository (based on the regex we got?)
    bool matches( const std::string& fname_ ) const;

    /// The file should be marked for deletion.
    void deleteFile( const std::string& fname_ );

    /// The file should be marked for addition/modification.
    std::ostream& modifyFile( const std::string& fname_, const char* mode_ );

    /// Commit all the changes we did.
    void commit( const Committer& committer_, const std::string& branch_, unsigned int commit_id_, time_t time_, const char* log_, size_t log_len_ );

    /// Create a branch.
    void createBranch( const std::string& branch_, unsigned int from_, const std::string& from_branch_ );

    /// Create a tag.
    void createTag( const Committer& committer_, const std::string& name_, unsigned int from_, const std::string& from_branch_, time_t time_, const char* log_, size_t log_len_ );
};

namespace Repositories
{
    /// Load the repositories layout from the config file.
    bool load( const char* fname_ );

    /// Close all the repositories.
    void close();

    /// Get the right repository according to the filename.
    Repository& get( const std::string& fname_ );

    /// Commit to the all repositories that have some changes.
    void commit( const Committer& committer_, const std::string& branch_, unsigned int commit_id_, time_t time_, const char* log_, size_t log_len_ );

    /// Create a branch in all the repositories.
    void createBranch( const std::string& branch_, unsigned int from_, const std::string& from_branch_ );

    /// Create a tag in all the repositories.
    void createTag( const Committer& committer_, const std::string& name_, unsigned int from_, const std::string& from_branch_, time_t time_, const char* log_, size_t log_len_ );

    /// Should the revision with this number be ignored?
    bool ignoreRevision( unsigned int commit_id_ );

    /// Should the tag with this name be ignored?
    bool ignoreTag( const std::string& name_ );
}

#endif // _REPOSITORY_HXX_
