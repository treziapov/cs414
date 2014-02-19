#include <iostream>
#include <string>
#include <sstream>
#include <gtk-2.0\gtk\gtk.h>

using namespace std;

string integerToString(int i);
int stringToInteger(string s);
int charPointerToInteger(const char* c);

string gtkGetUserSaveFile();
string gtkGetUserFile();