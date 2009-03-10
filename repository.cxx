#include "committers.hxx"
#include "repository.hxx"

#include <iostream>
#include <sstream>

using namespace std;

Repository::Repository()
{
}

void Repository::deleteFile( const char* fname_ )
{
    file_changes.append( "D " );
    file_changes.append( fname_ );
    file_changes.append( "\n" );
}

ostream& Repository::modifyFile( const char* fname_, const char* mode_, unsigned int mark_ )
{
    ostringstream sstr;

    sstr << "M " << mode_ << " :" << mark_ << " " << fname_ << "\n";
    
    file_changes.append( sstr.str() );

    return cout; // FIXME, obviously :-)
}

void Repository::reset()
{
    file_changes.clear();
}

static Repository repo;

Repository& Repository::get( const char* fname_ )
{
    return repo; // FIXME, too :-)
}

void Repository::commit( const Committer& committer_, time_t time_, const char* log_, size_t log_len_ )
{
    if ( Repository::get( NULL ).getChanges().length() == 0 ) // FIXME, here as well :-)
        return; // skip this one

    cout << "commit refs/heads/master\n"
         << "committer " << committer_.name << " <" << committer_.email << "> " << time_ << " -0000\n"
         << "data " << log_len_ << "\n"
         << log_ << "\n"
         << Repository::get( NULL ).getChanges() // FIXME, here as well :-)
         << endl << endl;

    Repository::get( NULL ).reset(); // FIXME, here as well :-)
}
