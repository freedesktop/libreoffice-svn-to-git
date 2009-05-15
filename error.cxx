#include "error.hxx"

#include <iostream>

using namespace std;

static int return_value = 0;

void Error::report( const string& message_ )
{
    cerr << "ERROR: " << message_ << endl;
    return_value = -1;
}

int Error::returnValue()
{
    return return_value;
}
