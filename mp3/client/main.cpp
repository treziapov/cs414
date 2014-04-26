#include <stdlib.h>
#include <stdio.h>
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
GtkWidget *videoWindow;
GtkWidget *videoMode_option, *videoResolution_option;
GtkWidget *bandwidth_entry, *videoRate_entry;
GtkWidget *current_bandwidth;
GtkWidget *current_rate;
GtkWidget *syncWidget, *ping, *failures;

Mode mode;
bool started = 0;
bool active = true;
Settings settingsData;
GstData gstData;
SinkData sinkData;

/* 
	This function is called when the GUI toolkit creates the physical mainWindow that will hold the video.
	At this point we can retrieve its handler (which has a different meaning depending on the windowing system)
	and pass it to GStreamer through the XOverlay interface.
*/
void setVideoWindow_event(GtkWidget *widget) 
{
	GdkWindow *window = gtk_widget_get_window(widget);
	guintptr window_handle;
	
	if (!gdk_window_ensure_native(window))
	{
		g_error("Couldn't create native mainWindow needed for GstXOverlay!");
	}	

	// Retrieve mainWindow handler from 
	#if defined (GDK_WINDOWING_WIN32)
		window_handle = (guintptr)GDK_WINDOW_HWND (window);
	#elif defined (GDK_WINDOWING_QUARTZ)
	  	window_handle = gdk_quartz_window_get_nsview (window);
	#elif defined (GDK_WINDOWING_X11)
	  	window_handle = GDK_WINDOW_XID (window);
	#endif

	// Pass it to a GstElement, which implements XOverlay and forwards to the video sink
	//gst_x_overlay_set_window_handle(GST_X_OVERLAY(gstData.videoSink), window_handle);
	//gst_x_overlay_set_xwindow_id(GST_X_OVERLAY(gstData.videoSink), window_handle);
}


//Saves the current bandwidth to resource.txt (for the clientside)
void saveBandwidth(int bandwidth){
	FILE * myFile;
	myFile = fopen("resource.txt", "w+");
	fprintf (myFile, "%d\0", bandwidth);
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
	if(active){
		active=false;
		gtk_entry_set_text (GTK_ENTRY(videoRate_entry), "10");
		gtk_entry_set_editable(GTK_ENTRY(videoRate_entry), FALSE);

		settingsData.rate = 10;
		settingsData.mode = 2;
		gstData.mode = (Mode)2;
	}
	else{
		active=true;
		gtk_entry_set_text (GTK_ENTRY(videoRate_entry), "15");
		gtk_entry_set_editable(GTK_ENTRY(videoRate_entry), TRUE);

		settingsData.rate = 15;
		settingsData.mode = 1;
		gstData.mode = (Mode)1;
	}

	if(started != 0){
		int retval = switchMode(&settingsData);

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
			GstClient::stopAndFreeResources(&gstData);
			GstClient::initPipeline(&gstData, settingsData.videoPort, settingsData.audioPort, &sinkData);
			GstClient::buildPipeline(&gstData);
			GstClient::playPipeline(&gstData);
			setVideoWindow_event(videoWindow);
		}
	}
}

void updateResolution(GtkWidget * widget, gpointer data){
	if(started == 0){
		if(settingsData.resolution == R240){
			settingsData.resolution = R480;
		}else{
			settingsData.resolution = R240;
		}
	}else{
		if(settingsData.resolution == R240){
			gtk_combo_box_set_active(GTK_COMBO_BOX(videoResolution_option), 0);
		}else{
			gtk_combo_box_set_active(GTK_COMBO_BOX(videoResolution_option), 1);
		}
	}
}

void playVideo(GtkWidget *widget,  gpointer data){
	if(started == 0){
		printf ("playVideo: starting new connection.\n");

		int retval = startStream(&settingsData);
		GstClient::initPipeline(&gstData, settingsData.videoPort, settingsData.audioPort, &sinkData);
		GstClient::buildPipeline(&gstData);
		GstClient::setPipelineToRun(&gstData);
		setVideoWindow_event(videoWindow);

		if(retval == 0){
			started=1;
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
		GstClient::setPipelineToRun(&gstData);
		resumeStream();
	}
}

void pauseVideo(GtkWidget *widget,  gpointer data){
	printf("clicked PAUSE stream\n");
	pauseStream();
	GstClient::pausePipeline(&gstData);
}

void fastForwardVideo(GtkWidget *widget,  gpointer data){
	printf("clicked FAST FORWARD video\n");
	fastforwardStream();
	GstClient::playPipeline(&gstData);
}

void rewindVideo(GtkWidget *widget,  gpointer data){
	printf("clicked REWIND video\n");
	rewindStream();
	GstClient::playPipeline(&gstData);
}

void stopVideo(GtkWidget *widget, gpointer data){
	printf("clicked STOP stream\n");
	started=0;
	stopStream();
	GstClient::stopAndFreeResources(&gstData);
}

void updateBandwidth(GtkWidget *widget, gpointer data){
	saveBandwidth(atoi(GTK_ENTRY(bandwidth_entry)->text));
	settingsData.bandwidth = atoi(GTK_ENTRY(bandwidth_entry)->text);

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
		gtk_label_set_text(GTK_LABEL(current_bandwidth), string);
	//}
}

void updateVideo(GtkWidget *widget, gpointer data){
	int rate = atoi(GTK_ENTRY(videoRate_entry)->text);
	if(settingsData.mode == ACTIVE){
		if(rate >= 15 && rate <= 25){
			settingsData.rate = rate;

			int retval = changeResources(&settingsData);
			if(retval == CONNECTION_ERROR){
				//report connection error
				gtk_dialog_run(GTK_DIALOG(gtk_message_dialog_new          (GTK_WINDOW(mainWindow),
                                             GTK_DIALOG_DESTROY_WITH_PARENT ,
                                             GTK_MESSAGE_ERROR,
                                             GTK_BUTTONS_NONE,
                                             "Connection Error"
                                             )));
				started = 0;
			}else if(retval == RESOURCES_ERROR){
				//report resources error
				gtk_dialog_run(GTK_DIALOG(gtk_message_dialog_new          (GTK_WINDOW(mainWindow),
                                             GTK_DIALOG_DESTROY_WITH_PARENT ,
                                             GTK_MESSAGE_ERROR,
                                             GTK_BUTTONS_NONE,
                                             "There was a resouce error."
                                             )));
				started = 0;
			}
			else{
				//success. Can change rate on label.
				char * numstr = new char[22]; // enough to hold all numbers up to 64-bits
				sprintf(numstr, "%d", rate);
				char string[19] = "Current Rate ";
				strcat(string, numstr);
				gtk_label_set_text(GTK_LABEL(current_rate), string);
			}
		}	
	}
	else{
		//don't need to check or set rate. Rate is set to 10 at switch
		int retval = changeResources(&settingsData);
		if(retval == CONNECTION_ERROR){
			//r
eport connection error
			gtk_dialog_run(GTK_DIALOG(gtk_message_dialog_new          (GTK_WINDOW(mainWindow),
                                             GTK_DIALOG_DESTROY_WITH_PARENT ,
                                             GTK_MESSAGE_ERROR,
                                             GTK_BUTTONS_NONE,
                                             "Connection error."
                                             )));
			started = 0;
		}else if(retval == RESOURCES_ERROR){
			//report resources error
			gtk_dialog_run(GTK_DIALOG(gtk_message_dialog_new          (GTK_WINDOW(mainWindow),
                                             GTK_DIALOG_DESTROY_WITH_PARENT ,
                                             GTK_MESSAGE_ERROR,
                                             GTK_BUTTONS_NONE,
                                             "There was a resouce error."
                                             )));
			started = 0;
		}
		else{
			//Success. Can change the rate on label.
			gtk_label_set_text(GTK_LABEL(current_rate), "Current Rate: 10");
		}
	}
}

gboolean refreshText(void * ptr){
	char buffer[512];
	if(active){
		GstClockTime skew = abs(sinkData.videoTSD - sinkData.audioTSD);
		sprintf(buffer, "Current skew: %dms", skew);
		gtk_label_set_text(GTK_LABEL(syncWidget), buffer);
	}
	sprintf(buffer, "Current end to end delay: %dms", sinkData.ping);
	gtk_label_set_text(GTK_LABEL(ping), buffer);
	float ratio = (sinkData.failures / (float)(sinkData.failures + sinkData.successes)) * 100;
	sprintf(buffer, "Total packets lost: %f%%", ratio);
	gtk_label_set_text(GTK_LABEL(failures), buffer);

	return (gboolean)true;
}

/* 
	Set up initial UI 
*/
/*
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
	current_bandwidth_server1 = gtk_label_new("temp");
	current_rate_server1 = gtk_label_new("Current Rate: 15");
	sync_skew_server1 = gtk_label_new("Current synchronization skew: 0ms");
	ping_server1 = gtk_label_new("Current end to end delay: 0ms");
	failures_server1 = gtk_label_new("Packets lost: 0");

	gtk_init(&argc, &argv);
   
	mainWindow = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	g_signal_connect(G_OBJECT(mainWindow), "delete-event", G_CALLBACK (gtkEnd_event), videoData);

	videoWindowServer1 = gtk_drawing_area_new();
	gtk_widget_set_double_buffered(videoWindowServer1, FALSE);
	g_signal_connect(videoWindow, "realize", G_CALLBACK (setVideoWindow_event), NULL);

	// Set up options
	videoMode_option_server1 = gtk_combo_box_new_text();
	gtk_combo_box_prepend_text(GTK_COMBO_BOX(videoMode_option_server1), "PASSIVE");
	gtk_combo_box_prepend_text(GTK_COMBO_BOX(videoMode_option_server1), "ACTIVE");
	gtk_combo_box_set_active(GTK_COMBO_BOX(videoMode_option_server1), 0);
	g_signal_connect(G_OBJECT(videoMode_option), "changed", G_CALLBACK(updateOptions), NULL);

	videoResolution_option_server1 = gtk_combo_box_new_text();
	gtk_combo_box_prepend_text(GTK_COMBO_BOX(videoResolution_option_server1), "480p");
	gtk_combo_box_prepend_text(GTK_COMBO_BOX(videoResolution_option_server1), "240p");
	gtk_combo_box_set_active(GTK_COMBO_BOX(videoResolution_option_server1), 0);
	g_signal_connect(G_OBJECT(videoResolution_option), "changed", G_CALLBACK(updateResolution), NULL);

	//char buffer[16];
	videoRate_entry_server1 = gtk_entry_new();
	bandwidth_entry_server1 = gtk_entry_new();
	gtk_entry_set_text (GTK_ENTRY(videoRate_entry), itoa(settingsData.rate, buffer, 10));
	gtk_entry_set_text (GTK_ENTRY(bandwidth_entry), itoa(settingsData.bandwidth, buffer, 10));

	playVideoFile_button_server1 = gtk_button_new_from_stock(GTK_STOCK_MEDIA_PLAY);
	g_signal_connect(G_OBJECT(playVideoFile_button), "clicked", G_CALLBACK(playVideo), NULL);
	pauseVideoFile_button_server1 = gtk_button_new_from_stock (GTK_STOCK_MEDIA_PAUSE);
	g_signal_connect(G_OBJECT(pauseVideoFile_button), "clicked", G_CALLBACK(pauseVideo), NULL);
	forwardVideoFile_button_server1 = gtk_button_new_from_stock (GTK_STOCK_MEDIA_FORWARD);
	g_signal_connect(G_OBJECT(forwardVideoFile_button), "clicked", G_CALLBACK(fastForwardVideo), NULL);
	rewindVideoFile_button_server1 = gtk_button_new_from_stock (GTK_STOCK_MEDIA_REWIND);
	g_signal_connect(G_OBJECT(rewindVideoFile_button), "clicked", G_CALLBACK(rewindVideo), NULL);
	stopVideoFile_button_server1 = gtk_button_new_from_stock (GTK_STOCK_MEDIA_STOP);
	g_signal_connect(G_OBJECT(stopVideoFile_button), "clicked", G_CALLBACK(stopVideo), NULL);

	
	updateBandwidth_button_server1 = gtk_button_new_with_label("Update Bandwidth ");
	g_signal_connect(G_OBJECT(updateBandwidth_button), "clicked", G_CALLBACK(updateBandwidth), NULL);
	updateVideo_button_server1 = gtk_button_new_with_label("Update Video 1 Stream");

	g_signal_connect(G_OBJECT(updateVideo_button), "clicked", G_CALLBACK(updateVideo), NULL);


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
	current_bandwidth = gtk_label_new(stringHead);
	
	current_rate_server2 = gtk_label_new("Current Rate: 15");
	sync_skew_server2 = gtk_label_new("Current synchronization skew: 0ms");
	ping_server2 = gtk_label_new("Current end to end delay: 0ms");
	failures_server2 = gtk_label_new("Packets lost: 0");

	gtk_init(&argc, &argv);
   
	mainWindow = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	g_signal_connect(G_OBJECT(mainWindow), "delete-event", G_CALLBACK (gtkEnd_event), videoData);

	videoWindowServer2 = gtk_drawing_area_new();
	gtk_widget_set_double_buffered(videoWindowServer2, FALSE);
	g_signal_connect(videoWindow, "realize", G_CALLBACK (setVideoWindow_event), NULL);

	// Set up options
	videoMode_option_server2 = gtk_combo_box_new_text();
	gtk_combo_box_prepend_text(GTK_COMBO_BOX(videoMode_option_server2), "PASSIVE");
	gtk_combo_box_prepend_text(GTK_COMBO_BOX(videoMode_option_server2), "ACTIVE");
	gtk_combo_box_set_active(GTK_COMBO_BOX(videoMode_option_server2), 0);
	g_signal_connect(G_OBJECT(videoMode_option), "changed", G_CALLBACK(updateOptions), NULL);

	videoResolution_option_server2 = gtk_combo_box_new_text();
	gtk_combo_box_prepend_text(GTK_COMBO_BOX(videoResolution_option_server2), "480p");
	gtk_combo_box_prepend_text(GTK_COMBO_BOX(videoResolution_option_server2), "240p");
	gtk_combo_box_set_active(GTK_COMBO_BOX(videoResolution_option_server2), 0);
	g_signal_connect(G_OBJECT(videoResolution_option), "changed", G_CALLBACK(updateResolution), NULL);

	//char buffer[16];
	videoRate_entry_server2 = gtk_entry_new();
	
	gtk_entry_set_text (GTK_ENTRY(videoRate_entry), itoa(settingsData.rate, buffer, 10));
	gtk_entry_set_text (GTK_ENTRY(bandwidth_entry), itoa(settingsData.bandwidth, buffer, 10));

	playVideoFile_button_server2 = gtk_button_new_from_stock(GTK_STOCK_MEDIA_PLAY);
	g_signal_connect(G_OBJECT(playVideoFile_button), "clicked", G_CALLBACK(playVideo), NULL);
	pauseVideoFile_button_server2 = gtk_button_new_from_stock (GTK_STOCK_MEDIA_PAUSE);
	g_signal_connect(G_OBJECT(pauseVideoFile_button), "clicked", G_CALLBACK(pauseVideo), NULL);
	forwardVideoFile_button_server2 = gtk_button_new_from_stock (GTK_STOCK_MEDIA_FORWARD);
	g_signal_connect(G_OBJECT(forwardVideoFile_button), "clicked", G_CALLBACK(fastForwardVideo), NULL);
	rewindVideoFile_button_server2 = gtk_button_new_from_stock (GTK_STOCK_MEDIA_REWIND);
	g_signal_connect(G_OBJECT(rewindVideoFile_button), "clicked", G_CALLBACK(rewindVideo), NULL);
	stopVideoFile_button_server2 = gtk_button_new_from_stock (GTK_STOCK_MEDIA_STOP);
	g_signal_connect(G_OBJECT(stopVideoFile_button), "clicked", G_CALLBACK(stopVideo), NULL);

	
	
	g_signal_connect(G_OBJECT(updateBandwidth_button), "clicked", G_CALLBACK(updateBandwidth), NULL);
	updateVideo_button_server2 = gtk_button_new_with_label("Update Video 2 Stream");

	g_signal_connect(G_OBJECT(updateVideo_button), "clicked", G_CALLBACK(updateVideo), NULL);

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
	

	gtk_window_set_default_size (GTK_WINDOW (mainWindow), 1500, 500);
    
	gtk_widget_show_all (mainWindow);
	//GTK_DIALOG_DESTROY_WITH_PARENT
	gtk_widget_realize(videoWindowServer1);

	g_timeout_add(500, refreshText, NULL);
}
*/

void gtkSetup(int argc, char *argv[])// VideoData *videoData, AudioData *audioData)
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
	sprintf(buffer, "%d", settingsData.rate);
	gtk_entry_set_text (GTK_ENTRY(videoRate_entry), buffer);
	sprintf(buffer, "%d", settingsData.bandwidth);
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
}

/*
	Main Method
*/
int main(int argc, char* argv[])
{
	settingsData.bandwidth = getBandwidth();
	settingsData.remainingBandwidth = settingsData.bandwidth;
	settingsData.ip = argv[1];	// TODO: parameterize
	settingsData.mode = ACTIVE;
	settingsData.rate = 15;
	settingsData.resolution = R240;
	gstData.mode = ACTIVE;
	gstData.serverIp = argv[1];
	gstData.clientIp = argv[2];

	printf ("Server on '%s', client on '%s'.\n", argv[1], argv[2]);

	sinkData.videoTSD = 0;
	sinkData.audioTSD = 0;
	sinkData.ping = 0;
	sinkData.failures = 0;
	sinkData.successes = 1;

    gtk_init(&argc, &argv);
    gtkSetup(argc, argv);

	//gstData.mode = Active;
	//GstClient::initPipeline(&gstData, 5000, 5001, &sinkData);
	//GstClient::buildPipeline(&gstData);
	//GstClient::playPipeline(&gstData);

    gtk_main();

	GstClient::stopAndFreeResources(&gstData);
    return 0;
}
