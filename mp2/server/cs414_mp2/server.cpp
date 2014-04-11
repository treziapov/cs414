#include <stdlib.h>
#include <stdio.h>

#include "listener.h"

//Gets the saved bandwidth from resource.txt
int getBandwidth(){
	FILE * myFile;
	myFile = fopen("server_resource.txt", "r");

	fseek(myFile, 0, SEEK_END);
	long fileSize = ftell(myFile);
	fseek(myFile, 0, SEEK_SET);

	char * buffer = new char[fileSize + 1];
	fread(buffer, fileSize, 1, myFile);
	buffer[fileSize] = '\0';

	fclose(myFile);

	return atoi(buffer);
}

//Saves the current bandwidth to resource.txt (for the clientside)
void saveBandwidth(int bandwidth){
	FILE * myFile;
	myFile = fopen("resource.txt", "w+");

	fclose(myFile);
}

int main(int argc, char *argv[]){
	int bandwidth = getBandwidth();

	init_listener(bandwidth);

	return 0;
}