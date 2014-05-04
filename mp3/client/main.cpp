#include <stdlib.h>
#include <stdio.h>f
#include <string.h>
#include <gtk-2.0/gtk/gtk.h>
#include <gst/gst.h>
#include <gst/interfaces/xoverlay.h>
#include <gdk/gdk.h>
#if defined (GDK_WINDOWING_X11)
#include <gdk/gdkx.h>
#elif defined (GDK_WINDOWING_WIN32)
#include <gdk/gdkwin32.h>
#elif defined (GDK_WINDOWING_QUARTZ)
#include <gdk/gdkquartz.h>
#endif

#include "connecter.h"
#include "gst_client.h"

GtkWidget *mainWindow;
GtkWidget *mainWindow2;
GtkWidget *videoWindowServer1;
GtkWidget *videoMode_option_server1, *videoResolution_option_server1;
GtkWidget *bandwidth_entry_server1, *videoRate_entry_server1;
GtkWidget *current_bandwidth_server1;
GtkWidget *current_rate_server1;
//GtkWidget *syncWidget, *ping, *failures;
GtkWidget *sync_skew_server1, *ping_server1, *failures_server1;

GtkWidget *videoWindowServer2;
GtkWidget *videoMode_option_server2, *videoResolution_option_server2;
GtkWidget *bandwidth_entry_server2, *videoRate_entry_server2;
GtkWidget *current_bandwidth_server2;
GtkWidget *current_rate_server2;
GtkWidget *sync_skew_server2, *ping_server2, *failures_server2;


//0 = STOPPED/NOT PLAYING
//1 = PAUSED
//2 = PLAYING NOT MUTED
//3 = PLAYING MUTED
int video1_state = 0; 
int video2_state = 0;


Mode mode;
bool started_server1 = 0;
bool active_server1 = true;
Settings settingsData1;
GstData gstData1;
SinkData sinkData1;

Mode mode_server2;
bool started_server2 = 0;
bool active_server2 = true;
Settings settingsData2;
GstData gstData2;
SinkData sinkData2;

/* 
	This function is called when the GUI toolkit creates the physical mainWindow that will hold the video.
	At this point we can retrieve its handler (which has a different meaning depending on the windowing system)
	and pass it to GStreamer through the XOverlay interface.
*/
void setVideoWindow_event(GtkWidget *widget, gpointer data) 
{

	
	GdkWindow *window = gtk_widget_get_window(widget);
	guintptr window_handle;
	
	if (!gdk_window_ensure_native(window))
	{
		g_error("Couldn't create native mainWindow needed for GstXOverlay!");
	}	

	// Retrieve mainWindow handler from GDK
	#if defined (GDK_WINDOWING_WIN32)
		window_handle = (guintptr)GDK_WINDOW_HWND (window);
	#elif defined (GDK_WINDOWING_QUARTZ)
	  	window_handle = gdk_quartz_window_get_nsview (window);
	#elif defined (GDK_WINDOWING_X11)
	  	window_handle = GDK_WINDOW_XID (window);
	#endif

	int server = GPOINTER_TO_INT(data);
	if(server == 1){
		// Pass it to a GstElement, which implements XOverlay and forwards to the video sink
		//gst_x_overlay_set_window_handle(GST_X_OVERLAY(gstData.videoSink), window_handle);
		gst_x_overlay_set_xwindow_id(GST_X_OVERLAY(gstData1.videoSink), window_handle);
	}

	
	if(server == 2){
		// Pass it to a GstElement, which implements XOverlay and forwards to the video sink
		//gst_x_overlay_set_window_handle(GST_X_OVERLAY(gstData.videoSink), window_handle);
		gst_x_overlay_set_xwindow_id(GST_X_OVERLAY(gstData2.videoSink), window_handle);
	}
}


//Saves the current bandwidth to resource.txt (for the clientside)
void saveBandwidth(int bandwidth){
	FILE * myFile;
	myFile = fopen("resource.txt", "w+");
	fprintf (myFile, "%d", bandwidth);
	fclose(myFile);

	printf ("saveBandwidth(): Saved bandwidth at: resource.txt.\n");
}

//Gets the saved bandwidth from resource.txt
int getBandwidth(){
	FILE * myFile = fopen("resource.txt", "r");
	if (!myFile) {
		printf("\t Couldn't open the resource file.\n");
		saveBandwidth(1000000000);
		fclose(myFile);
		myFile = fopen("resource.txt", "r");
	}

	fseek(myFile, 0, SEEK_END);
	long fileSize = ftell(myFile);
	fseek(myFile, 0, SEEK_SET);

	char * buffer = new char[fileSize + 1];
	fread(buffer, fileSize, 1, myFile);
	buffer[fileSize] = '\0';
	fclose(myFile);
	printf ("\t Current bandwidth: %s.\n", buffer); 
	return atoi(buffer);
}

//CALLBACK FUNCTIONS
void updateOptions(GtkWidget *widget, gpointer data){

	int server = GPOINTER_TO_INT(data);
	if(server == 1){
		if(active_server1){
			active_server1=false;
			gtk_entry_set_text (GTK_ENTRY(videoRate_entry_server1), "10");
			gtk_entry_set_editable(GTK_ENTRY(videoRate_entry_server1), FALSE);

			settingsData1.rate = 10;
			settingsData1.mode = 2;
			gstData1.mode = (Mode)2;
		}
		else{
			active_server1=true;
			gtk_entry_set_text (GTK_ENTRY(videoRate_entry_server1), "15");
			gtk_entry_set_editable(GTK_ENTRY(videoRate_entry_server1), TRUE);

			settingsData1.rate = 15;
			settingsData1.mode = 1;
			gstData1.mode = (Mode)1;
		}

		if(started_server1 != 0){
			int retval = switchMode(&settingsData1);

			if(retval == CONNECTION_ERROR){
				//report connection error or server resource error
				gtk_dialog_run(GTK_DIALOG(gtk_message_dialog_new          (GTK_WINDOW(mainWindow),
						                             GTK_DIALOG_DESTROY_WITH_PARENT ,
						                             GTK_MESSAGE_ERROR,
						                             GTK_BUTTONS_NONE,
						                             "connection error or server resource error"
						                             )));
			}else if(retval == RESOURCES_ERROR){
				//report client side error
				gtk_dialog_run(GTK_DIALOG(gtk_message_dialog_new          (GTK_WINDOW(mainWindow),
						                             GTK_DIALOG_DESTROY_WITH_PARENT ,
						                             GTK_MESSAGE_ERROR,
						                             GTK_BUTTONS_NONE,
						                             "client side error"
						                             )));
			} 
			else {
				GstClient::stopAndFreeResources(&gstData1);
				GstClient::initPipeline(&gstData1, settingsData1.videoPort, settingsData1.audioPort, &sinkData1);
				GstClient::buildPipeline(&gstData1);
				GstClient::playPipeline(&gstData1);
				setVideoWindow_event(videoWindowServer1, GINT_TO_POINTER(1));
			}
		}
	}

	if(server == 2){
		if(active_server2){
			active_server2=false;
			gtk_entry_set_text (GTK_ENTRY(videoRate_entry_server2), "10");
			gtk_entry_set_editable(GTK_ENTRY(videoRate_entry_server2), FALSE);

			settingsData2.rate = 10;
			settingsData2.mode = 2;
			gstData2.mode = (Mode)2;
		}
		else{
			active_server2=true;
			gtk_entry_set_text (GTK_ENTRY(videoRate_entry_server2), "15");
			gtk_entry_set_editable(GTK_ENTRY(videoRate_entry_server2), TRUE);

			settingsData2.rate = 15;
			settingsData2.mode = 1;
			gstData2.mode = (Mode)1;
		}

		if(started_server2 != 0){
			int retval = switchMode(&settingsData2);

			if(retval == CONNECTION_ERROR){
				//report connection error or server resource error
				gtk_dialog_run(GTK_DIALOG(gtk_message_dialog_new          (GTK_WINDOW(mainWindow),
						                             GTK_DIALOG_DESTROY_WITH_PARENT ,
						                             GTK_MESSAGE_ERROR,
						                             GTK_BUTTONS_NONE,
						                             "connection error or server resource error"
						                             )));
			}else if(retval == RESOURCES_ERROR){
				//report client side error
				gtk_dialog_run(GTK_DIALOG(gtk_message_dialog_new          (GTK_WINDOW(mainWindow),
						                             GTK_DIALOG_DESTROY_WITH_PARENT ,
						                             GTK_MESSAGE_ERROR,
						                             GTK_BUTTONS_NONE,
						                             "client side error"
						                             )));
			} 
			else {
				GstClient::stopAndFreeResources(&gstData2);
				GstClient::initPipeline(&gstData2, settingsData2.videoPort, settingsData2.audioPort, &sinkData2);
				GstClient::buildPipeline(&gstData2);
				GstClient::playPipeline(&gstData2);
				setVideoWindow_event(videoWindowServer2, GINT_TO_POINTER(2));
			}
		}
	}
}

void updateResolution(GtkWidget * widget, gpointer data){

	int server = GPOINTER_TO_INT(data);
	if(server == 1){
		if(started_server1 == 0){
			if(settingsData1.resolution == R240){
				settingsData1.resolution = R480;
			}else{
				settingsData1.resolution = R240;
			}
		}else{
			if(settingsData1.resolution == R240){
				gtk_combo_box_set_active(GTK_COMBO_BOX(videoResolution_option_server1), 0);
			}else{
				gtk_combo_box_set_active(GTK_COMBO_BOX(videoResolution_option_server1), 1);
			}
		}
	}

	if(server == 2){
		if(started_server2 == 0){
			if(settingsData2.resolution == R240){
				settingsData2.resolution = R480;
			}else{
				settingsData2.resolution = R240;
			}
		}else{
			if(settingsData2.resolution == R240){
				gtk_combo_box_set_active(GTK_COMBO_BOX(videoResolution_option_server2), 0);
			}else{
				gtk_combo_box_set_active(GTK_COMBO_BOX(videoResolution_option_server2), 1);
			}
		}
	}
}

void playVideo(GtkWidget *widget,  gpointer data){

	
	int server = GPOINTER_TO_INT(data);
	
	if(server == 1){
		if(started_server1 == 0){
			printf ("playVideo: starting new connection for server 1.\n");

			int retval = startStream(&settingsData1);
			GstClient::initPipeline(&gstData1, settingsData1.videoPort, settingsData1.audioPort, &sinkData1);
			GstClient::buildPipeline(&gstData1);
			video1_state=2;
			if(video2_state == 2){ //video2 was not muted
				GstClient::muteAudio(&gstData2);
				video2_state = 3;
			}
			GstClient::setPipelineToRun(&gstData1);
			setVideoWindow_event(videoWindowServer1, GINT_TO_POINTER(1));

			if(retval == 0){
				started_server1=1;
			}else if(retval == CONNECTION_ERROR){
				//report connection error or server resource error
				gtk_dialog_run(GTK_DIALOG(gtk_message_dialog_new (GTK_WINDOW(mainWindow),
					GTK_DIALOG_DESTROY_WITH_PARENT ,
					GTK_MESSAGE_ERROR,
					GTK_BUTTONS_NONE,
					"Connection error or server resource error")));
			}else{
				//report client side error
				gtk_dialog_run(GTK_DIALOG(gtk_message_dialog_new (GTK_WINDOW(mainWindow),
		            GTK_DIALOG_DESTROY_WITH_PARENT ,
		            GTK_MESSAGE_ERROR,
		            GTK_BUTTONS_NONE,
		            "There was a client-side error.")));
			}
			
		}
		else{
			//send resume
			GstClient::setPipelineToRun(&gstData1);
			video1_state = 2;
			if(video2_state == 2){ //video2 was not muted
				GstClient::muteAudio(&gstData2);
				video2_state = 3;
			}
			resumeStream(&settingsData1);
		}
	}

	if(server == 2){
		if(started_server2 == 0){
			printf ("playVideo: starting new connection for server 2.\n");

			int retval = startStream(&settingsData2);
			GstClient::initPipeline(&gstData2, settingsData2.videoPort, settingsData2.audioPort, &sinkData2);
			GstClient::buildPipeline(&gstData2);

			video2_state = 2;
			if(video1_state == 2){ //video1 was not muted
				GstClient::muteAudio(&gstData1);
				video2_state = 3;
			}
			GstClient::setPipelineToRun(&gstData2);
			setVideoWindow_event(videoWindowServer2, GINT_TO_POINTER(2));

			if(retval == 0){
				started_server2=1;
			}else if(retval == CONNECTION_ERROR){
				//report connection error or server resource error
				gtk_dialog_run(GTK_DIALOG(gtk_message_dialog_new (GTK_WINDOW(mainWindow),
					GTK_DIALOG_DESTROY_WITH_PARENT ,
					GTK_MESSAGE_ERROR,
					GTK_BUTTONS_NONE,
					"Connection error or server resource error")));
			}else{
				//report client side error
				gtk_dialog_run(GTK_DIALOG(gtk_message_dialog_new (GTK_WINDOW(mainWindow),
		            GTK_DIALOG_DESTROY_WITH_PARENT ,
		            GTK_MESSAGE_ERROR,
		            GTK_BUTTONS_NONE,
		            "There was a client-side error.")));
			}

			
		}
		else{
			//send resume

			video2_state = 2;
			if(video1_state == 2){ //video1 was not muted
				GstClient::muteAudio(&gstData1);
				video2_state = 3;
			}
			GstClient::setPipelineToRun(&gstData2);
			resumeStream(&settingsData2);
		}
	}
}

void pauseVideo(GtkWidget *widget,  gpointer data){
	int server = GPOINTER_TO_INT(data);
	if(server == 1){

		video1_state = 1;
		if(video2_state == 3){ //video2 was muted
			GstClient::unmuteAudio(&gstData2);
			video2_state = 2;
		}
		printf("clicked PAUSE stream 1\n");
		pauseStream(&settingsData1);
		GstClient::pausePipeline(&gstData1);
	}

	
	if(server == 2){

		video2_state = 1;
		if(video1_state == 3){ //video2 was muted
			GstClient::unmuteAudio(&gstData1);
			video2_state = 2;
		}
		printf("clicked PAUSE stream 2\n");
		pauseStream(&settingsData2);
		GstClient::pausePipeline(&gstData2);
	}
}

void fastForwardVideo(GtkWidget *widget,  gpointer data){

	int server = GPOINTER_TO_INT(data);
	if(server == 1){
		
		printf("clicked FAST FORWARD video 1\n");
		fastforwardStream(&settingsData1);
		GstClient::playPipeline(&gstData1);

	}
	if(server == 2){
		printf("clicked FAST FORWARD video 2\n");
		fastforwardStream(&settingsData2);
		GstClient::playPipeline(&gstData2);
	}
}

void rewindVideo(GtkWidget *widget,  gpointer data){

	int server = GPOINTER_TO_INT(data);
	if(server == 1){
		video1_state = 0;
		printf("clicked REWIND video 1\n");
		rewindStream(&settingsData1);
		GstClient::playPipeline(&gstData1);

	}
	if(server == 2){
		printf("clicked REWIND video 2\n");
		rewindStream(&settingsData2);
		GstClient::playPipeline(&gstData2);
	}
	
}

void stopVideo(GtkWidget *widget, gpointer data){

	int server = GPOINTER_TO_INT(data);
	if(server == 1){
		video1_state = 0;
		if(video2_state == 3){ //video2 was muted
			GstClient::unmuteAudio(&gstData2);
			video2_state = 2
		}
		printf("clicked STOP stream 1\n");
		started_server1=0;
		stopStream(&settingsData1);
		GstClient::stopAndFreeResources(&gstData1);

	}
	if(server == 2){
		video2_state = 0;
		if(video1_state = 3){
			GstClient::unmuteAudio(&gstData1);
			video1_state = 2;
		}
		printf("clicked STOP stream 2\n");
		started_server2=0;
		stopStream(&settingsData2);
		GstClient::stopAndFreeResources(&gstData2);
	}
	
}

void updateBandwidth(GtkWidget *widget, gpointer data){
	
	saveBandwidth(atoi(GTK_ENTRY(bandwidth_entry_server1)->text));
	settingsData1.bandwidth = atoi(GTK_ENTRY(bandwidth_entry_server1)->text);
	settingsData2.bandwidth = atoi(GTK_ENTRY(bandwidth_entry_server1)->text);
	//int retval = changeResources(&settingsData);
	//if(retval == CONNECTION_ERROR){
	//	//report connection error
	//	gtk_dialog_run(GTK_DIALOG(gtk_message_dialog_new          (GTK_WINDOW(mainWindow),
 //                                            GTK_DIALOG_DESTROY_WITH_PARENT ,
 //                                            GTK_MESSAGE_ERROR,
 //                                            GTK_BUTTONS_NONE,
 //                                            "Connection Error"
 //                                            )));
	//	started = 0;
	//}
	//else{
		char * numstr = new char[22]; // enough to hold all numbers up to 64-bits
		sprintf(numstr, "%d", getBandwidth());
		char string[128] = "Current Bandwidth ";
		strcat(string, numstr);
		gtk_label_set_text(GTK_LABEL(current_bandwidth_server1), string);
	//}
}

void updateVideo(GtkWidget *widget, gpointer data){


	//update server 1
	int server = GPOINTER_TO_INT(data);
	if(server == 1){
		int rate = atoi(GTK_ENTRY(videoRate_entry_server1)->text);
		if(settingsData1.mode == ACTIVE){
			if(rate >= 15 && rate <= 25){
				settingsData1.rate = rate;

				int retval = changeResources(&settingsData1);
				if(retval == CONNECTION_ERROR){
					//report connection error
					gtk_dialog_run(GTK_DIALOG(gtk_message_dialog_new          (GTK_WINDOW(mainWindow),
			                                     GTK_DIALOG_DESTROY_WITH_PARENT ,
			                                     GTK_MESSAGE_ERROR,
			                                     GTK_BUTTONS_NONE,
			                                     "Connection Error"
			                                     )));
					started_server1 = 0;
				}else if(retval == RESOURCES_ERROR){
					//report resources error
					gtk_dialog_run(GTK_DIALOG(gtk_message_dialog_new          (GTK_WINDOW(mainWindow),
			                                     GTK_DIALOG_DESTROY_WITH_PARENT ,
			                                     GTK_MESSAGE_ERROR,
			                                     GTK_BUTTONS_NONE,
			                                     "There was a resouce error."
			                                     )));
					started_server1 = 0;
				}
				else{
					//success. Can change rate on label.
					char * numstr = new char[22]; // enough to hold all numbers up to 64-bits
					sprintf(numstr, "%d", rate);
					char string[19] = "Current Rate ";
					strcat(string, numstr);
					gtk_label_set_text(GTK_LABEL(current_rate_server1), string);
				}
			}	
		}
		else{
			//don't need to check or set rate. Rate is set to 10 at switch
			int retval = changeResources(&settingsData1);
			if(retval == CONNECTION_ERROR){
				//report connection error
				gtk_dialog_run(GTK_DIALOG(gtk_message_dialog_new          (GTK_WINDOW(mainWindow),
			                                     GTK_DIALOG_DESTROY_WITH_PARENT ,
			                                     GTK_MESSAGE_ERROR,
			                                     GTK_BUTTONS_NONE,
			                                     "Connection error."
			                                     )));
				started_server1 = 0;
			}else if(retval == RESOURCES_ERROR){
				//report resources error
				gtk_dialog_run(GTK_DIALOG(gtk_message_dialog_new          (GTK_WINDOW(mainWindow),
			                                     GTK_DIALOG_DESTROY_WITH_PARENT ,
			                                     GTK_MESSAGE_ERROR,
			                                     GTK_BUTTONS_NONE,
			                                     "There was a resouce error."
			                                     )));
				started_server1 = 0;
			}
			else{
				//Success. Can change the rate on label.
				gtk_label_set_text(GTK_LABEL(current_rate_server1), "Current Rate: 10");
			}
		}
	}
	//update server2
	if(server == 2){
		int rate = atoi(GTK_ENTRY(videoRate_entry_server2)->text);
		if(settingsData2.mode == ACTIVE){
			if(rate >= 15 && rate <= 25){
				settingsData2.rate = rate;

				int retval = changeResources(&settingsData2);
				if(retval == CONNECTION_ERROR){
					//report connection error
					gtk_dialog_run(GTK_DIALOG(gtk_message_dialog_new          (GTK_WINDOW(mainWindow),
			                                     GTK_DIALOG_DESTROY_WITH_PARENT ,
			                                     GTK_MESSAGE_ERROR,
			                                     GTK_BUTTONS_NONE,
			                                     "Connection Error"
			                                     )));
					started_server2 = 0;
				}else if(retval == RESOURCES_ERROR){
					//report resources error
					gtk_dialog_run(GTK_DIALOG(gtk_message_dialog_new          (GTK_WINDOW(mainWindow),
			                                     GTK_DIALOG_DESTROY_WITH_PARENT ,
			                                     GTK_MESSAGE_ERROR,
			                                     GTK_BUTTONS_NONE,
			                                     "There was a resouce error."
			                                     )));
					started_server2 = 0;
				}
				else{
					//success. Can change rate on label.
					char * numstr = new char[22]; // enough to hold all numbers up to 64-bits
					sprintf(numstr, "%d", rate);
					char string[19] = "Current Rate ";
					strcat(string, numstr);
					gtk_label_set_text(GTK_LABEL(current_rate_server2), string);
				}
			}	
		}
		else{
			//don't need to check or set rate. Rate is set to 10 at switch
			int retval = changeResources(&settingsData2);
			if(retval == CONNECTION_ERROR){
				//report connection error
				gtk_dialog_run(GTK_DIALOG(gtk_message_dialog_new          (GTK_WINDOW(mainWindow),
			                                     GTK_DIALOG_DESTROY_WITH_PARENT ,
			                                     GTK_MESSAGE_ERROR,
			                                     GTK_BUTTONS_NONE,
			                                     "Connection error."
			                                     )));
				started_server2 = 0;
			}else if(retval == RESOURCES_ERROR){
				//report resources error
				gtk_dialog_run(GTK_DIALOG(gtk_message_dialog_new          (GTK_WINDOW(mainWindow),
			                                     GTK_DIALOG_DESTROY_WITH_PARENT ,
			                                     GTK_MESSAGE_ERROR,
			                                     GTK_BUTTONS_NONE,
			                                     "There was a resouce error."
			                                     )));
				started_server2 = 0;
			}
			else{
				//Success. Can change the rate on label.
				gtk_label_set_text(GTK_LABEL(current_rate_server2), "Current Rate: 10");
			}
		}
	}
}

gboolean refreshText_server1(void * ptr){
	char buffer[512];
	if(active_server1){
		GstClockTime skew = abs(sinkData1.videoTSD - sinkData1.audioTSD);
		sprintf(buffer, "Current skew: %dms", (int)skew);
		gtk_label_set_text(GTK_LABEL(sync_skew_server1), buffer);
	}
	sprintf(buffer, "Current end to end delay: %dms", (int)sinkData1.ping);
	gtk_label_set_text(GTK_LABEL(ping_server1), buffer);
	float ratio = (sinkData1.failures / (float)(sinkData1.failures + sinkData1.successes)) * 100;
	sprintf(buffer, "Total packets lost: %f%%", ratio);
	gtk_label_set_text(GTK_LABEL(failures_server2), buffer);

	return (gboolean)true;
}

gboolean refreshText_server2(void * ptr){
	char buffer[512];
	if(active_server2){
		GstClockTime skew = abs(sinkData2.videoTSD - sinkData2.audioTSD);
		sprintf(buffer, "Current skew: %dms", (int)skew);
		gtk_label_set_text(GTK_LABEL(sync_skew_server2), buffer);
	}
	sprintf(buffer, "Current end to end delay: %dms", (int)sinkData2.ping);
	gtk_label_set_text(GTK_LABEL(ping_server2), buffer);
	float ratio = (sinkData2.failures / (float)(sinkData2.failures + sinkData2.successes)) * 100;
	sprintf(buffer, "Total packets lost: %f%%", ratio);
	gtk_label_set_text(GTK_LABEL(failures_server2), buffer);

	return (gboolean)true;
}

/* 
	Set up initial UI 
*/

void gtkSetup(int argc, char *argv[])// VideoData *videoData, AudioData *audioData)
{
	
	
	GtkWidget *mainBoxBothServers;
	mainBoxBothServers = gtk_hbox_new (FALSE, 0);
	
	GtkWidget *mainBoxServer1;			// Vbox, holds HBox and videoControls
	GtkWidget *mainHBoxServer1;		// Hbox, holds video window and option box
	GtkWidget *videoControlsServer1;	// Hbox, holds the buttons and the slider for video

	GtkWidget *optionsServer1;			// Vbox, holds the settings options

	// Video related buttons
	GtkWidget *playVideoFile_button_server1, *pauseVideoFile_button_server1, *forwardVideoFile_button_server1, *rewindVideoFile_button_server1, *stopVideoFile_button_server1;
	GtkWidget *updateBandwidth_button_server1, *updateVideo_button_server1;

	char * numstr = new char[22]; // enough to hold all numbers up to 64-bits
	char stringHead[19] = "Current Bandwidth ";
	sprintf(numstr, "%d", getBandwidth());
	strcat(stringHead, numstr);

	// Informational
	GtkWidget *video_label_server1 = gtk_label_new("Video 1:");
	//current_bandwidth = gtk_label_new(stringHead);
	current_bandwidth_server1 = gtk_label_new(stringHead);
	current_rate_server1 = gtk_label_new("Current Rate: 15");
	sync_skew_server1 = gtk_label_new("Current synchronization skew: 0ms");
	ping_server1 = gtk_label_new("Current end to end delay: 0ms");
	failures_server1 = gtk_label_new("Packets lost: 0");

	gtk_init(&argc, &argv);
   
	mainWindow = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	//g_signal_connect(G_OBJECT(mainWindow), "delete-event", G_CALLBACK (gtkEnd_event), videoData);

	videoWindowServer1 = gtk_drawing_area_new();
	gtk_widget_set_double_buffered(videoWindowServer1, FALSE);
	g_signal_connect(videoWindowServer1, "realize", G_CALLBACK (setVideoWindow_event), GINT_TO_POINTER(1));

	// Set up options
	videoMode_option_server1 = gtk_combo_box_new_text();
	gtk_combo_box_prepend_text(GTK_COMBO_BOX(videoMode_option_server1), "PASSIVE");
	gtk_combo_box_prepend_text(GTK_COMBO_BOX(videoMode_option_server1), "ACTIVE");
	gtk_combo_box_set_active(GTK_COMBO_BOX(videoMode_option_server1), 0);
	g_signal_connect(G_OBJECT(videoMode_option_server1), "changed", G_CALLBACK(updateOptions), GINT_TO_POINTER(1));

	videoResolution_option_server1 = gtk_combo_box_new_text();
	gtk_combo_box_prepend_text(GTK_COMBO_BOX(videoResolution_option_server1), "480p");
	gtk_combo_box_prepend_text(GTK_COMBO_BOX(videoResolution_option_server1), "240p");
	gtk_combo_box_set_active(GTK_COMBO_BOX(videoResolution_option_server1), 0);
	g_signal_connect(G_OBJECT(videoResolution_option_server1), "changed", G_CALLBACK(updateResolution), GINT_TO_POINTER(1));

	//char buffer[16];

	char buffer[48];
	
	videoRate_entry_server1 = gtk_entry_new();
	bandwidth_entry_server1 = gtk_entry_new();

	sprintf(buffer, "%d", settingsData1.rate);
	gtk_entry_set_text (GTK_ENTRY(videoRate_entry_server1), buffer);
	sprintf(buffer, "%d", settingsData1.bandwidth);
	gtk_entry_set_text (GTK_ENTRY(bandwidth_entry_server1), buffer);

	playVideoFile_button_server1 = gtk_button_new_from_stock(GTK_STOCK_MEDIA_PLAY);
	g_signal_connect(G_OBJECT(playVideoFile_button_server1), "clicked", G_CALLBACK(playVideo), GINT_TO_POINTER(1));
	pauseVideoFile_button_server1 = gtk_button_new_from_stock (GTK_STOCK_MEDIA_PAUSE);
	g_signal_connect(G_OBJECT(pauseVideoFile_button_server1), "clicked", G_CALLBACK(pauseVideo), GINT_TO_POINTER(1));
	forwardVideoFile_button_server1 = gtk_button_new_from_stock (GTK_STOCK_MEDIA_FORWARD);
	g_signal_connect(G_OBJECT(forwardVideoFile_button_server1), "clicked", G_CALLBACK(fastForwardVideo), GINT_TO_POINTER(1));
	rewindVideoFile_button_server1 = gtk_button_new_from_stock (GTK_STOCK_MEDIA_REWIND);
	g_signal_connect(G_OBJECT(rewindVideoFile_button_server1), "clicked", G_CALLBACK(rewindVideo), GINT_TO_POINTER(1));
	stopVideoFile_button_server1 = gtk_button_new_from_stock (GTK_STOCK_MEDIA_STOP);
	g_signal_connect(G_OBJECT(stopVideoFile_button_server1), "clicked", G_CALLBACK(stopVideo), GINT_TO_POINTER(1));

	
	updateBandwidth_button_server1 = gtk_button_new_with_label("Update Bandwidth ");
	g_signal_connect(G_OBJECT(updateBandwidth_button_server1), "clicked", G_CALLBACK(updateBandwidth), GINT_TO_POINTER(1));
	updateVideo_button_server1 = gtk_button_new_with_label("Update Video 1 Stream");

	g_signal_connect(G_OBJECT(updateVideo_button_server1), "clicked", G_CALLBACK(updateVideo), GINT_TO_POINTER(1));


	// Layout
	videoControlsServer1 = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (videoControlsServer1), gtk_label_new("Play Video 1 File:"), FALSE, FALSE, 20);
	gtk_box_pack_start (GTK_BOX (videoControlsServer1), playVideoFile_button_server1, FALSE, FALSE, 2);
	gtk_box_pack_start (GTK_BOX (videoControlsServer1), rewindVideoFile_button_server1, FALSE, FALSE, 2);
	gtk_box_pack_start (GTK_BOX (videoControlsServer1), pauseVideoFile_button_server1, FALSE, FALSE, 2);
	gtk_box_pack_start (GTK_BOX (videoControlsServer1), forwardVideoFile_button_server1, FALSE, FALSE, 2);
	gtk_box_pack_start (GTK_BOX (videoControlsServer1), stopVideoFile_button_server1, FALSE, FALSE, 2);

	//Resolution, Mode, and Rate options
	optionsServer1 = gtk_vbox_new(FALSE, 0);
	gtk_box_pack_start (GTK_BOX (optionsServer1), gtk_label_new("Bandwith (B/s):"), FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (optionsServer1), bandwidth_entry_server1, FALSE, FALSE, 1);
	gtk_box_pack_start (GTK_BOX (optionsServer1), updateBandwidth_button_server1, FALSE, FALSE, 2);

	gtk_box_pack_start (GTK_BOX (optionsServer1), gtk_label_new("Mode:"), FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (optionsServer1), videoMode_option_server1, FALSE, FALSE, 1);
	gtk_box_pack_start (GTK_BOX (optionsServer1), gtk_label_new("Resolution:"), FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (optionsServer1), videoResolution_option_server1, FALSE, FALSE, 1);
	gtk_box_pack_start (GTK_BOX (optionsServer1), gtk_label_new("Rate:"), FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (optionsServer1), videoRate_entry_server1, FALSE, FALSE, 1);
	gtk_box_pack_start (GTK_BOX (optionsServer1), updateVideo_button_server1, FALSE, FALSE, 2);
	gtk_box_pack_start (GTK_BOX (optionsServer1), current_bandwidth_server1, FALSE, FALSE, 1);
	gtk_box_pack_start (GTK_BOX (optionsServer1), current_rate_server1, FALSE, FALSE, 1);
	gtk_box_pack_start (GTK_BOX (optionsServer1), sync_skew_server1, FALSE, FALSE, 1);
	gtk_box_pack_start (GTK_BOX (optionsServer1), ping_server1, FALSE, FALSE, 1);
	gtk_box_pack_start (GTK_BOX (optionsServer1), failures_server1, FALSE, FALSE, 1);

	mainHBoxServer1 = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start (GTK_BOX (mainHBoxServer1), videoWindowServer1, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (mainHBoxServer1), optionsServer1, FALSE, FALSE, 10);
   
	mainBoxServer1 = gtk_vbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (mainBoxServer1), mainHBoxServer1, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (mainBoxServer1), videoControlsServer1, FALSE, FALSE, 2);
	gtk_box_pack_start (GTK_BOX (mainBoxBothServers), mainBoxServer1, TRUE, TRUE, 0);
	


	//START VIDEO 2 CONTAINER
	GtkWidget *mainBoxServer2;			// Vbox, holds HBox and videoControls
	GtkWidget *mainHBoxServer2;		// Hbox, holds video window and option box
	GtkWidget *videoControlsServer2;	// Hbox, holds the buttons and the slider for video

	GtkWidget *optionsServer2;			// Vbox, holds the settings options

	// Video related buttons
	GtkWidget *playVideoFile_button_server2, *pauseVideoFile_button_server2, *forwardVideoFile_button_server2, *rewindVideoFile_button_server2, *stopVideoFile_button_server2;
	GtkWidget  *updateVideo_button_server2;

	char * numstr2 = new char[22]; // enough to hold all numbers up to 64-bits
	char stringHead2[19] = "Current Bandwidth ";
	sprintf(numstr2, "%d", getBandwidth());
	strcat(stringHead2, numstr2);

	// Informational
	GtkWidget *video_label_server2 = gtk_label_new("Video 2:");
	//current_bandwidth_server2 = gtk_label_new(stringHead);
	
	current_rate_server2 = gtk_label_new("Current Rate: 15");
	sync_skew_server2 = gtk_label_new("Current synchronization skew: 0ms");
	ping_server2 = gtk_label_new("Current end to end delay: 0ms");
	failures_server2 = gtk_label_new("Packets lost: 0");

	//gtk_init(&argc, &argv);
   
	//mainWindow = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	//g_signal_connect(G_OBJECT(mainWindow), "delete-event", G_CALLBACK (gtkEnd_event), videoData);

	videoWindowServer2 = gtk_drawing_area_new();
	gtk_widget_set_double_buffered(videoWindowServer2, FALSE);
	g_signal_connect(videoWindowServer2, "realize", G_CALLBACK (setVideoWindow_event), GINT_TO_POINTER(2));

	// Set up options
	videoMode_option_server2 = gtk_combo_box_new_text();
	gtk_combo_box_prepend_text(GTK_COMBO_BOX(videoMode_option_server2), "PASSIVE");
	gtk_combo_box_prepend_text(GTK_COMBO_BOX(videoMode_option_server2), "ACTIVE");
	gtk_combo_box_set_active(GTK_COMBO_BOX(videoMode_option_server2), 0);
	g_signal_connect(G_OBJECT(videoMode_option_server2), "changed", G_CALLBACK(updateOptions), GINT_TO_POINTER(2));

	videoResolution_option_server2 = gtk_combo_box_new_text();
	gtk_combo_box_prepend_text(GTK_COMBO_BOX(videoResolution_option_server2), "480p");
	gtk_combo_box_prepend_text(GTK_COMBO_BOX(videoResolution_option_server2), "240p");
	gtk_combo_box_set_active(GTK_COMBO_BOX(videoResolution_option_server2), 0);
	g_signal_connect(G_OBJECT(videoResolution_option_server2), "changed", G_CALLBACK(updateResolution), GINT_TO_POINTER(2));

	//char buffer[16];
	videoRate_entry_server2 = gtk_entry_new();
	

	sprintf(buffer, "%d", settingsData2.rate);
	gtk_entry_set_text (GTK_ENTRY(videoRate_entry_server2), buffer);


	playVideoFile_button_server2 = gtk_button_new_from_stock(GTK_STOCK_MEDIA_PLAY);
	g_signal_connect(G_OBJECT(playVideoFile_button_server2), "clicked", G_CALLBACK(playVideo), GINT_TO_POINTER(2));
	pauseVideoFile_button_server2 = gtk_button_new_from_stock (GTK_STOCK_MEDIA_PAUSE);
	g_signal_connect(G_OBJECT(pauseVideoFile_button_server2), "clicked", G_CALLBACK(pauseVideo), GINT_TO_POINTER(2));
	forwardVideoFile_button_server2 = gtk_button_new_from_stock (GTK_STOCK_MEDIA_FORWARD);
	g_signal_connect(G_OBJECT(forwardVideoFile_button_server2), "clicked", G_CALLBACK(fastForwardVideo), GINT_TO_POINTER(2));
	rewindVideoFile_button_server2 = gtk_button_new_from_stock (GTK_STOCK_MEDIA_REWIND);
	g_signal_connect(G_OBJECT(rewindVideoFile_button_server2), "clicked", G_CALLBACK(rewindVideo), GINT_TO_POINTER(2));
	stopVideoFile_button_server2 = gtk_button_new_from_stock (GTK_STOCK_MEDIA_STOP);
	g_signal_connect(G_OBJECT(stopVideoFile_button_server2), "clicked", G_CALLBACK(stopVideo), GINT_TO_POINTER(2));

	
	
	//g_signal_connect(G_OBJECT(updateBandwidth_button), "clicked", G_CALLBACK(updateBandwidth), GINT_TO_POINTER(2));
	updateVideo_button_server2 = gtk_button_new_with_label("Update Video 2 Stream");

	g_signal_connect(G_OBJECT(updateVideo_button_server2), "clicked", G_CALLBACK(updateVideo), GINT_TO_POINTER(2));

	// Layout
	videoControlsServer2 = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (videoControlsServer2), gtk_label_new("Play Video 2 File:"), FALSE, FALSE, 20);
	gtk_box_pack_start (GTK_BOX (videoControlsServer2), playVideoFile_button_server2, FALSE, FALSE, 2);
	gtk_box_pack_start (GTK_BOX (videoControlsServer2), rewindVideoFile_button_server2, FALSE, FALSE, 2);
	gtk_box_pack_start (GTK_BOX (videoControlsServer2), pauseVideoFile_button_server2, FALSE, FALSE, 2);
	gtk_box_pack_start (GTK_BOX (videoControlsServer2), forwardVideoFile_button_server2, FALSE, FALSE, 2);
	gtk_box_pack_start (GTK_BOX (videoControlsServer2), stopVideoFile_button_server2, FALSE, FALSE, 2);

	//Resolution, Mode, and Rate options
	optionsServer2 = gtk_vbox_new(FALSE, 0);
	
	
	

	gtk_box_pack_start (GTK_BOX (optionsServer2), gtk_label_new("Mode:"), FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (optionsServer2), videoMode_option_server2, FALSE, FALSE, 1);
	gtk_box_pack_start (GTK_BOX (optionsServer2), gtk_label_new("Resolution:"), FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (optionsServer2), videoResolution_option_server2, FALSE, FALSE, 1);
	gtk_box_pack_start (GTK_BOX (optionsServer2), gtk_label_new("Rate:"), FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (optionsServer2), videoRate_entry_server2, FALSE, FALSE, 1);
	gtk_box_pack_start (GTK_BOX (optionsServer2), updateVideo_button_server2, FALSE, FALSE, 2);
	
	gtk_box_pack_start (GTK_BOX (optionsServer2), current_rate_server2, FALSE, FALSE, 1);
	gtk_box_pack_start (GTK_BOX (optionsServer2), sync_skew_server2, FALSE, FALSE, 1);
	gtk_box_pack_start (GTK_BOX (optionsServer2), ping_server2, FALSE, FALSE, 1);
	gtk_box_pack_start (GTK_BOX (optionsServer2), failures_server2, FALSE, FALSE, 1);

	mainHBoxServer2 = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start (GTK_BOX (mainHBoxServer2), videoWindowServer2, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (mainHBoxServer2), optionsServer2, FALSE, FALSE, 10);
   
	mainBoxServer2 = gtk_vbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (mainBoxServer2), mainHBoxServer2, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (mainBoxServer2), videoControlsServer2, FALSE, FALSE, 2);
	gtk_box_pack_start (GTK_BOX (mainBoxBothServers), mainBoxServer2, TRUE, TRUE, 0);
	gtk_container_add (GTK_CONTAINER (mainWindow), mainBoxBothServers);
	//gtk_container_add (GTK_CONTAINER (mainWindow), mainBoxServer1);
	//gtk_container_add (GTK_CONTAINER (mainWindow2), mainBoxServer2);

	gtk_window_set_default_size (GTK_WINDOW (mainWindow), 1500, 500);
    
	gtk_widget_show_all (mainWindow);
	
	//GTK_DIALOG_DESTROY_WITH_PARENT
	gtk_widget_realize(videoWindowServer1);
	gtk_widget_realize(videoWindowServer2);
	g_timeout_add(500, refreshText_server1, NULL);
	g_timeout_add(500, refreshText_server2, NULL);
}


/*void gtkSetup(int argc, char *argv[])// VideoData *videoData, AudioData *audioData)
{
	GtkWidget *mainBox;			// Vbox, holds HBox and videoControls
	GtkWidget *mainHBox;		// Hbox, holds video window and option box
	GtkWidget *videoControls;	// Hbox, holds the buttons and the slider for video
	
	GtkWidget *options;			// Vbox, holds the settings options

	// Video related buttons
	GtkWidget *playVideoFile_button, *pauseVideoFile_button, *forwardVideoFile_button, *rewindVideoFile_button, *stopVideoFile_button;
	GtkWidget *updateBandwidth_button, *updateVideo_button;

	char * numstr = new char[22]; // enough to hold all numbers up to 64-bits
	char stringHead[19] = "Current Bandwidth ";
	sprintf(numstr, "%d", getBandwidth());
	strcat(stringHead, numstr);
	// Informational
	GtkWidget *video_label = gtk_label_new("Video:");
	current_bandwidth = gtk_label_new(stringHead);
	current_rate = gtk_label_new("Current Rate: 15");
	syncWidget = gtk_label_new("Current synchronization skew: 0ms");
	ping = gtk_label_new("Current end to end delay: 0ms");
	failures = gtk_label_new("Packets lost: 0");
	
	gtk_init(&argc, &argv);
   
	mainWindow = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	//g_signal_connect(G_OBJECT(mainWindow), "delete-event", G_CALLBACK (gtkEnd_event), videoData);

	videoWindow = gtk_drawing_area_new();
	gtk_widget_set_double_buffered(videoWindow, FALSE);
	g_signal_connect(videoWindow, "realize", G_CALLBACK (setVideoWindow_event), NULL);

	// Set up options
	videoMode_option = gtk_combo_box_new_text();
	gtk_combo_box_prepend_text(GTK_COMBO_BOX(videoMode_option), "PASSIVE");
	gtk_combo_box_prepend_text(GTK_COMBO_BOX(videoMode_option), "ACTIVE");
	gtk_combo_box_set_active(GTK_COMBO_BOX(videoMode_option), 0);
	g_signal_connect(G_OBJECT(videoMode_option), "changed", G_CALLBACK(updateOptions), NULL);

	videoResolution_option = gtk_combo_box_new_text();
	gtk_combo_box_prepend_text(GTK_COMBO_BOX(videoResolution_option), "480p");
	gtk_combo_box_prepend_text(GTK_COMBO_BOX(videoResolution_option), "240p");
	gtk_combo_box_set_active(GTK_COMBO_BOX(videoResolution_option), 0);
	g_signal_connect(G_OBJECT(videoResolution_option), "changed", G_CALLBACK(updateResolution), NULL);

	char buffer[48];
	videoRate_entry = gtk_entry_new();
	bandwidth_entry = gtk_entry_new();
	sprintf(buffer, "%d", settingsData1.rate);
	gtk_entry_set_text (GTK_ENTRY(videoRate_entry), buffer);
	sprintf(buffer, "%d", settingsData1.bandwidth);
	gtk_entry_set_text (GTK_ENTRY(bandwidth_entry), buffer);

	playVideoFile_button = gtk_button_new_from_stock(GTK_STOCK_MEDIA_PLAY);
	g_signal_connect(G_OBJECT(playVideoFile_button), "clicked", G_CALLBACK(playVideo), NULL);
	pauseVideoFile_button = gtk_button_new_from_stock (GTK_STOCK_MEDIA_PAUSE);
	g_signal_connect(G_OBJECT(pauseVideoFile_button), "clicked", G_CALLBACK(pauseVideo), NULL);
	forwardVideoFile_button = gtk_button_new_from_stock (GTK_STOCK_MEDIA_FORWARD);
	g_signal_connect(G_OBJECT(forwardVideoFile_button), "clicked", G_CALLBACK(fastForwardVideo), NULL);
	rewindVideoFile_button = gtk_button_new_from_stock (GTK_STOCK_MEDIA_REWIND);
	g_signal_connect(G_OBJECT(rewindVideoFile_button), "clicked", G_CALLBACK(rewindVideo), NULL);
	stopVideoFile_button = gtk_button_new_from_stock (GTK_STOCK_MEDIA_STOP);
	g_signal_connect(G_OBJECT(stopVideoFile_button), "clicked", G_CALLBACK(stopVideo), NULL);

	//TODO don't make NULL
	updateBandwidth_button = gtk_button_new_with_label("Update Bandwidth");
	g_signal_connect(G_OBJECT(updateBandwidth_button), "clicked", G_CALLBACK(updateBandwidth), NULL);
	updateVideo_button = gtk_button_new_with_label("Update Stream");
	
	g_signal_connect(G_OBJECT(updateVideo_button), "clicked", G_CALLBACK(updateVideo), NULL);

	// Layout
	videoControls = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (videoControls), gtk_label_new("Play Video File:"), FALSE, FALSE, 20);
	gtk_box_pack_start (GTK_BOX (videoControls), playVideoFile_button, FALSE, FALSE, 2);
	gtk_box_pack_start (GTK_BOX (videoControls), rewindVideoFile_button, FALSE, FALSE, 2);
	gtk_box_pack_start (GTK_BOX (videoControls), pauseVideoFile_button, FALSE, FALSE, 2);
	gtk_box_pack_start (GTK_BOX (videoControls), forwardVideoFile_button, FALSE, FALSE, 2);
	gtk_box_pack_start (GTK_BOX (videoControls), stopVideoFile_button, FALSE, FALSE, 2);

	//Resolution, Mode, and Rate options
	options = gtk_vbox_new(FALSE, 0);
	gtk_box_pack_start (GTK_BOX (options), gtk_label_new("Bandwith (B/s):"), FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (options), bandwidth_entry, FALSE, FALSE, 1);
	gtk_box_pack_start (GTK_BOX (options), updateBandwidth_button, FALSE, FALSE, 2);

	gtk_box_pack_start (GTK_BOX (options), gtk_label_new("Mode:"), FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (options), videoMode_option, FALSE, FALSE, 1);
	gtk_box_pack_start (GTK_BOX (options), gtk_label_new("Resolution:"), FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (options), videoResolution_option, FALSE, FALSE, 1);
	gtk_box_pack_start (GTK_BOX (options), gtk_label_new("Rate:"), FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (options), videoRate_entry, FALSE, FALSE, 1);
	gtk_box_pack_start (GTK_BOX (options), updateVideo_button, FALSE, FALSE, 2);
	gtk_box_pack_start (GTK_BOX (options), current_bandwidth, FALSE, FALSE, 1);
	gtk_box_pack_start (GTK_BOX (options), current_rate, FALSE, FALSE, 1);
	gtk_box_pack_start (GTK_BOX (options), syncWidget, FALSE, FALSE, 1);
	gtk_box_pack_start (GTK_BOX (options), ping, FALSE, FALSE, 1);
	gtk_box_pack_start (GTK_BOX (options), failures, FALSE, FALSE, 1);

	mainHBox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start (GTK_BOX (mainHBox), videoWindow, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (mainHBox), options, FALSE, FALSE, 10);
   
	mainBox = gtk_vbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (mainBox), mainHBox, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (mainBox), videoControls, FALSE, FALSE, 2);
	gtk_container_add (GTK_CONTAINER (mainWindow), mainBox);
	gtk_window_set_default_size (GTK_WINDOW (mainWindow), 1280, 960);
    
	gtk_widget_show_all (mainWindow);
	//GTK_DIALOG_DESTROY_WITH_PARENT
	gtk_widget_realize(videoWindow);

	g_timeout_add(500, refreshText, NULL);
}*/

/*
	Main Method
*/
int main(int argc, char* argv[])
{
	settingsData1.bandwidth = getBandwidth();
	settingsData1.remainingBandwidth = settingsData1.bandwidth;
	settingsData1.ip = argv[1];	// TODO: parameterize
	settingsData1.mode = ACTIVE;
	settingsData1.rate = 15;
	settingsData1.resolution = R240;
	gstData1.mode = ACTIVE;
	gstData1.serverIp = argv[1];
	gstData1.clientIp = argv[3];

	settingsData2.bandwidth = getBandwidth();
	settingsData2.remainingBandwidth = settingsData2.bandwidth;
	settingsData2.ip = argv[2];	// TODO: parameterize
	settingsData2.mode = ACTIVE;
	settingsData2.rate = 15;
	settingsData2.resolution = R240;
	gstData2.mode = ACTIVE;
	gstData2.serverIp = argv[2];
	gstData2.clientIp = argv[3];

	printf ("Server 1 on '%s', client on '%s'.\n", argv[1], argv[3]);
	printf ("Server 2 on '%s', client on '%s'.\n", argv[2], argv[3]);

	sinkData1.videoTSD = 0;
	sinkData1.audioTSD = 0;
	sinkData1.ping = 0;
	sinkData1.failures = 0;
	sinkData1.successes = 1;

	sinkData2.videoTSD = 0;
	sinkData2.audioTSD = 0;
	sinkData2.ping = 0;
	sinkData2.failures = 0;
	sinkData2.successes = 1;

    gtk_init(&argc, &argv);
    gtkSetup(argc, argv);

	//gstData.mode = Active;
	//GstClient::initPipeline(&gstData, 5000, 5001, &sinkData);
	//GstClient::buildPipeline(&gstData);
	//GstClient::playPipeline(&gstData);

    gtk_main();

	GstClient::stopAndFreeResources(&gstData1);
	GstClient::stopAndFreeResources(&gstData2);
    return 0;
}
