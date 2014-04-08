#include <gtk-2.0\gtk\gtk.h>

#include "connecter.h"

GtkWidget *videMode_option;
GtkWidget *videoResolution_entry, *videoRate_entry;
bool started = 0;
bool active = TRUE;
//CALLBACK FUNCTIONS
void updateOptions(GtkWidget *widget, gpointer data){
	if(active){
		active=false;
		gtk_entry_set_text (GTK_ENTRY(videoRate_entry), "10");
		gtk_entry_set_editable(GTK_ENTRY(videoRate_entry), FALSE);
	}
	else{
		active=true;
		gtk_entry_set_editable(GTK_ENTRY(videoRate_entry), FALSE);
	}
}
void playVideo(GtkWidget *widget,  gpointer data){
	//TODO parse resource.txt for info
	
	if(started == 0){
		//send start
		//startStream(char * ip, int mode, int resolution, int rate, int bandwidth);
		started=1;
	}
	else{
		//send resume
		resumeStream();
	}
}

void pauseVideo(GtkWidget *widget,  gpointer data){
	pauseStream();
}

void fastForwardVideo(GtkWidget *widget,  gpointer data){
	fastforwardStream();
}

void rewindVideo(GtkWidget *widget,  gpointer data){
	rewindStream();
}

void stopVideo(GtkWidget *widget, gpointer data){
	started=0;
	stopStream();
}
void updateBandwidth(GtkWidget *widget, gpointer data){
	//TODO: Implement Update Bandwidth (i.e. resource.txt)

}
void updateVideo(GtkWidget *widget, gpointer data){
	//TODO: implement updateVideo
	
		
	

}
/* 
	Set up initial UI 
*/
void gtkSetup(int argc, char *argv[])// VideoData *videoData, AudioData *audioData)
{
	GtkWidget *mainWindow;		// Contains all other windows
	GtkWidget *videoWindow;		// Contains the video
	GtkWidget *mainBox;			// Vbox, holds HBox and videoControls
	GtkWidget *mainHBox;		// Hbox, holds video window and option box
	GtkWidget *videoControls;	// Hbox, holds the buttons and the slider for video
	
	GtkWidget *options;			// Vbox, holds the settings options

	// Video related buttons
	GtkWidget *playVideoFile_button, *pauseVideoFile_button, *forwardVideoFile_button, *rewindVideoFile_button, *stopVideoFile_button;
	GtkWidget *updateBandwidth_button, *updateVideo_button;
	

	

	// Informational
	GtkWidget *video_label = gtk_label_new("Video:");
	

	gtk_init(&argc, &argv);
   
	mainWindow = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	//g_signal_connect(G_OBJECT(mainWindow), "delete-event", G_CALLBACK (gtkEnd_event), videoData);

	videoWindow = gtk_drawing_area_new();
	gtk_widget_set_double_buffered(videoWindow, FALSE);
	//g_signal_connect(videoWindow, "realize", G_CALLBACK (setVideoWindow_event), videoData);
	//videoData->window = videoWindow;
	
	// Set up options
	videMode_option = gtk_combo_box_new_text();
	gtk_combo_box_prepend_text(GTK_COMBO_BOX(videMode_option), "PASSIVE");
	gtk_combo_box_prepend_text(GTK_COMBO_BOX(videMode_option), "ACTIVE");
	gtk_combo_box_set_active(GTK_COMBO_BOX(videMode_option), 0);
	g_signal_connect(G_OBJECT(videMode_option), "changed", G_CALLBACK(updateOptions), NULL);

	videoRate_entry = gtk_entry_new();
	videoResolution_entry = gtk_entry_new();
	gtk_entry_set_text (GTK_ENTRY(videoRate_entry), "10");

	
	

	

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
	gtk_box_pack_start (GTK_BOX (options), gtk_label_new("Bandwith:"), FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (options), videoResolution_entry, FALSE, FALSE, 1);
	gtk_box_pack_start (GTK_BOX (options), updateBandwidth_button, FALSE, FALSE, 2);

	gtk_box_pack_start (GTK_BOX (options), gtk_label_new("Mode:"), FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (options), videMode_option, FALSE, FALSE, 1);
	gtk_box_pack_start (GTK_BOX (options), gtk_label_new("Rate:"), FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (options), videoRate_entry, FALSE, FALSE, 1);
	gtk_box_pack_start (GTK_BOX (options), updateVideo_button, FALSE, FALSE, 2);




	mainHBox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start (GTK_BOX (mainHBox), videoWindow, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (mainHBox), options, FALSE, FALSE, 10);
   
	mainBox = gtk_vbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (mainBox), mainHBox, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (mainBox), videoControls, FALSE, FALSE, 2);
	gtk_container_add (GTK_CONTAINER (mainWindow), mainBox);
	gtk_window_set_default_size (GTK_WINDOW (mainWindow), 1280, 960);
   
	gtk_widget_show_all (mainWindow);
}





int main(int argc, char* argv[])
{
	connect("127.0.0.1", ACTIVE, R240, 15);

	//while(true);
    gtk_init(&argc, &argv);

    gtkSetup(argc, argv);   

    gtk_main();   
    return 0;
}