#include <stdlib.h>
#include <stdio.h>
//#include <iostream>
#include <direct.h>
#include "listener.h"

//Gets the saved bandwidth from resource.txt
int getBandwidth(){
	char path[FILENAME_MAX];
	if (!_getcwd(path, sizeof(path))) {
		return errno;
	}
	printf ("Current directory: %s\n", path);

	FILE * myFile = fopen("resource.txt", "r");
	if (!myFile) {
		printf("Couldn't open the file\n");
		return -1;
	}

	fseek(myFile, 0, SEEK_END);
	long fileSize = ftell(myFile);
	fseek(myFile, 0, SEEK_SET);

	char * buffer = new char[fileSize + 1];
	fread(buffer, fileSize, 1, myFile);
	buffer[fileSize] = '\0';
	printf(buffer);
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
	printf("server bandwidth: %d\n", bandwidth);
	if (bandwidth == -1) {
		bandwidth = 1000000000;
	}
	init_listener(bandwidth);
	return 0;
}