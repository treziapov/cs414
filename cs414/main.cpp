#include <gst/gst.h>
#include <gst/interfaces/xoverlay.h>
#include <gtk-2.0\gtk\gtk.h>
#include <gdk/gdk.h>
#include <gdk/gdkwin32.h>
#include <iostream>
#include <string>
#include "video.h"
#include "audio.h"

using namespace std;

/* 
	This function is called when the GUI toolkit creates the physical window that will hold the video.
	At this point we can retrieve its handler (which has a different meaning depending on the windowing system)
	and pass it to GStreamer through the XOverlay interface.
*/
static void setVideoWindow(GtkWidget *widget, VideoData *data) 
{
	GdkWindow *window = gtk_widget_get_window(widget);
	guintptr window_handle;
	
	if (!gdk_window_ensure_native(window))
	{
		g_error("Couldn't create native window needed for GstXOverlay!");
	}	

	// Retrieve window handler from GDK
	window_handle = (guintptr)GDK_WINDOW_HWND(window);
	// Pass it to playbin2, which implements XOverlay and will forward it to the video sink
	gst_x_overlay_set_window_handle(GST_X_OVERLAY(data->videoSink), window_handle);
}


/* Set up initial UI */
void gtkSetup(int argc, char *argv[], VideoData *videoData, AudioData *audioData)
{
	GtkWidget *vbox;
	GtkWidget *window, *video_window;
	GtkWidget *menubar, *recordermenu, *playermenu, * audioplayermenu;
	GtkWidget *record, *play, *start_rec, *stop_rec, *start_play, *stop_play, *pause, *rewind, *forward, *audio_start_rec, *audio_stop_rec, *audio_start_play, *audio_stop_play, *play_audio;

	gtk_init(&argc, &argv);
   
	video_window = gtk_drawing_area_new();
	gtk_widget_set_double_buffered(video_window, FALSE);
	g_signal_connect(video_window, "realize", G_CALLBACK (setVideoWindow), videoData);

	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
	gtk_window_set_default_size(GTK_WINDOW(window), 250, 200);
	gtk_window_set_title(GTK_WINDOW(window), "menu");

	vbox = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(window), vbox);

	menubar = gtk_menu_bar_new();
	recordermenu = gtk_menu_new();
	playermenu = gtk_menu_new();
	audioplayermenu = gtk_menu_new();

	record = gtk_menu_item_new_with_label("Recorder");
	play = gtk_menu_item_new_with_label("Player");
	start_rec = gtk_menu_item_new_with_label("Start Recording");
	start_play = gtk_menu_item_new_with_label("Start Playing A Video");
	stop_rec = gtk_menu_item_new_with_label("Stop Recording");
	stop_play = gtk_menu_item_new_with_label("Stop Playing Video");
	pause = gtk_menu_item_new_with_label("Pause/Resume Playing");
	rewind = gtk_menu_item_new_with_label("Rewind Video");
	forward = gtk_menu_item_new_with_label("Fast Forward Video");
	audio_start_rec = gtk_menu_item_new_with_label("Start Recording Audio");
	g_signal_connect(G_OBJECT (audio_start_rec), "activate", G_CALLBACK (audio_start_recording), audioData);
	audio_stop_rec = gtk_menu_item_new_with_label("Stop Recording Audio");
	g_signal_connect(G_OBJECT (audio_stop_rec), "activate", G_CALLBACK (audio_stop_recording), audioData);
	audio_start_play = gtk_menu_item_new_with_label("Start Playing An Audio File");
	g_signal_connect(G_OBJECT (audio_start_play), "activate", G_CALLBACK (audio_start_playback), audioData);
	audio_stop_play = gtk_menu_item_new_with_label("Stop Playing An Audio File");
	g_signal_connect(G_OBJECT (audio_stop_play), "activate", G_CALLBACK (audio_stop_playback), audioData);
	play_audio = gtk_menu_item_new_with_label("Play Audio");

	gtk_menu_item_set_submenu(GTK_MENU_ITEM(record), recordermenu);
	gtk_menu_shell_append(GTK_MENU_SHELL(recordermenu), start_rec);
	gtk_menu_shell_append(GTK_MENU_SHELL(recordermenu), stop_rec);
	gtk_menu_shell_append(GTK_MENU_SHELL(recordermenu), audio_start_rec);
	gtk_menu_shell_append(GTK_MENU_SHELL(recordermenu), audio_stop_rec);
	gtk_menu_shell_append(GTK_MENU_SHELL(menubar), record);

	gtk_menu_item_set_submenu(GTK_MENU_ITEM(play), playermenu);
	gtk_menu_shell_append(GTK_MENU_SHELL(playermenu), start_play);
	gtk_menu_shell_append(GTK_MENU_SHELL(playermenu), stop_play);
	gtk_menu_shell_append(GTK_MENU_SHELL(playermenu), pause);
	gtk_menu_shell_append(GTK_MENU_SHELL(playermenu), rewind);	
	gtk_menu_shell_append(GTK_MENU_SHELL(playermenu), forward);
	gtk_menu_shell_append(GTK_MENU_SHELL(menubar), play);
	gtk_box_pack_start(GTK_BOX(vbox), menubar, FALSE, FALSE, 3);
	gtk_box_pack_start(GTK_BOX (vbox), video_window, TRUE, TRUE, 0);
	
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(play_audio), audioplayermenu);
	gtk_menu_shell_append(GTK_MENU_SHELL(audioplayermenu), audio_start_play);
	gtk_menu_shell_append(GTK_MENU_SHELL(audioplayermenu), audio_stop_play);
	gtk_menu_shell_append(GTK_MENU_SHELL(menubar), play_audio);

	g_signal_connect_swapped(G_OBJECT(window), "destroy",
		G_CALLBACK(gtk_main_quit), NULL);

	// TODO: Connect start_play button with a file viewer and integrate video playback of file in a function play
	//g_signal_connect(G_OBJECT(start_play), "start_play", G_CALLBACK(play), NULL);
	// TODO: integrate the termination of video viewing in a function stop
	//g_signal_connect(G_OBJECT(stop_play), "stop_play", G_CALLBACK(stop), NULL);

	gtk_widget_show_all(window);
}


/* Application entry point */
int main(int argc, char *argv[])
{
	PlayerMode mode = Camera;

	VideoData videoData;
	initializeVideoData(&videoData);
	
	AudioData audioData;

	gstreamerSetup(argc, argv, &videoData);
	gstreamerBuildPipeline(&videoData, mode);
	gtkSetup(argc, argv, &videoData, &audioData);

	gstreamerPlay(&videoData);
	gtk_main();

	gstreamerCleanup(&videoData);
}

void start(GtkWidget* source, gpointer* data){
	//ask for file location and start video playback
}

void stop(GtkWidget* source, gpointer* data){
	//stop playing video
}

void pause(GtkWidget* source, gpointer* data){
	//pause or resume playing of a video
}

void rewind(GtkWidget* source, gpointer* data){
	//rewind video playing
}

void forward(GtkWidget* source, gpointer* data){
	//fast-forward video playing
}

void record(GtkWidget* source, gpointer* data){
	//start recording. spawn another thread to show frames while recording
}

void stop_rec(GtkWidget* source, gpointer* data){
	//stop recording and compress to file
}
