#include <stdlib.h>
#include <stdio.h>
#include <direct.h>

#include "listener.h"

//Saves the current bandwidth to resource.txt (for the clientside)
void saveBandwidth(int bandwidth){
	char* resourceFile = 
		GstServer::getFilePathInHomeDirectory(GstServer::videoDirectory, "resource.txt");

	FILE * myFile;
	myFile = fopen(resourceFile, "w+");
	fprintf (myFile, "%d\0", bandwidth);
	fclose(myFile);

	printf ("saveBandwidth(): Saved bandwidth at: %s.\n", resourceFile);
}

//Gets the saved bandwidth from resource.txt
int getBandwidth(){
	char path[FILENAME_MAX];
	if (!_getcwd(path, sizeof(path))) {
		return errno;
	}
	printf ("getBandwidth(): Current directory: %s.\n", path);

	char* resourceFile = 
		GstServer::getFilePathInHomeDirectory(GstServer::videoDirectory, "resource.txt");
	printf ("\t Looking for resource file at: %s.\n", resourceFile);

	FILE * myFile = fopen(resourceFile, "r");
	if (!myFile) {
		printf("\t Couldn't open the resource file.\n");
		saveBandwidth(1000000000);
		fclose(myFile);
		myFile = fopen(resourceFile, "r");
	}

	fseek(myFile, 0, SEEK_END);
	long fileSize = ftell(myFile);
	fseek(myFile, 0, SEEK_SET);

	char * buffer = new char[fileSize + 1];
	fread(buffer, fileSize, 1, myFile);
	buffer[fileSize] = '\0';
	fclose(myFile);

	return atoi(buffer);
}

int main(int argc, char *argv[]){
	int bandwidth = getBandwidth();
	printf("server bandwidth: %d.\n", bandwidth);

	if (bandwidth == -1) {
		return -1;
	}

	init_listener(bandwidth);
	//GstData gstData;
	//gstData.clientIp = "localhost";
	//gstData.videoPort = 5000;
	//gstData.audioPort = 5001;
	//gstData.videoFrameRate = 10;
	//gstData.mode = Passive;
	//gstData.resolution = (Resolution)R240;
	//GstServer::initPipeline(&gstData);
	//GstServer::buildPipeline(&gstData);
	//GstServer::configurePipeline(&gstData);
	//GstServer::playPipeline(&gstData);
	//GstServer::waitForEosOrError(&gstData);
	return 0;
}