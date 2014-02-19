#include <gst/gst.h>
#include <gtk-2.0\gtk\gtk.h>
#include <gdk/gdk.h>
#include <iostream>
#include <string>
#include "video.h"
#include "audio.h"
#include "utility.h"

using namespace std;

// Options
GtkWidget *videoEncoding_option;
GtkWidget *videoWidth_entry, *videoHeight_entry, *videoRate_entry;
GtkWidget *audioEncoding_option;
GtkWidget *sampingRate_entry, *sampleSize_entry;

/* 
	This function is called everytime the video mainWindow needs to be redrawn (due to damage/exposure,
	rescaling, etc). GStreamer takes care of this in the PAUSED and PLAYING states, otherwise,
	we simply draw a black rectangle to avoid garbage showing up. 
*/
static gboolean exposeVideoWindow_event(GtkWidget *widget, GdkEventExpose *event, VideoData *data) {
	if (data->state < GST_STATE_PAUSED) 
	{
		GtkAllocation allocation;
		GdkWindow *mainWindow = gtk_widget_get_window (widget);
		cairo_t *cr;
     
		/* Cairo is a 2D graphics library which we use here to clean the video mainWindow.
			* It is used by GStreamer for other reasons, so it will always be available to us. */
		gtk_widget_get_allocation(widget, &allocation);
		cr = gdk_cairo_create(mainWindow);
		cairo_set_source_rgb(cr, 0, 0, 0);
		cairo_rectangle(cr, 0, 0, allocation.width, allocation.height);
		cairo_fill(cr);
		cairo_destroy(cr);
	}

	return FALSE;
}

/* 
	This function is called periodically to refresh the GUI 
*/
static gboolean refreshUI(VideoData *data) {
  GstFormat fmt = GST_FORMAT_TIME;
  gint64 current = -1;
   
  /* We do not want to update anything unless we are in the PAUSED or PLAYING states */
  if (data->state < GST_STATE_PAUSED)
    return TRUE;
   
  /* If we didn't know it yet, query the stream duration */
  if (!GST_CLOCK_TIME_IS_VALID (data->duration)) {
    if (!gst_element_query_duration (data->videoSource, &fmt, &data->duration)) {
      g_printerr ("Could not query current duration.\n");
    } else {
      /* Set the range of the slider to the clip duration, in SECONDS */
      gtk_range_set_range (GTK_RANGE (data->slider), 0, (gdouble)data->duration / GST_SECOND);
    }
  }
   
  if (gst_element_query_position (data->videoSource, &fmt, &current)) {
    /* Block the "value-changed" signal, so the slider_cb function is not called
     * (which would trigger a seek the user has not requested) */
    g_signal_handler_block (data->slider, data->sliderUpdateSignalId);
    /* Set the position of the slider to the current pipeline positoin, in SECONDS */
    gtk_range_set_value (GTK_RANGE (data->slider), (gdouble)current / GST_SECOND);
    /* Re-enable the signal */
    g_signal_handler_unblock (data->slider, data->sliderUpdateSignalId);
  }
  return TRUE;
}

/* This function is called when an error message is posted on the bus */
static void error_callback(GstBus *bus, GstMessage *msg, VideoData *data) {
  GError *err;
  gchar *debug_info;
   
  /* Print error details on the screen */
  gst_message_parse_error (msg, &err, &debug_info);
  g_printerr ("Error received from element %s: %s\n", GST_OBJECT_NAME (msg->src), err->message);
  g_printerr ("Debugging information: %s\n", debug_info ? debug_info : "none");
  g_clear_error (&err);
  g_free (debug_info);
   
  /* Set the pipeline to READY (which stops playback) */
  gst_element_set_state (data->pipeline, GST_STATE_READY);
  data->state = GST_STATE_READY;
}

/* 
	This function is called when an End-Of-Stream message is posted on the bus.	
	We just set the pipeline to READY (which stops playback) 
*/
static void EOS_callback(GstBus *bus, GstMessage *msg, VideoData *data) {
  g_print ("End-Of-Stream reached.\n");
  gst_element_set_state (data->pipeline, GST_STATE_READY);
  data->state = GST_STATE_READY;
}

/* This function is called when the pipeline changes states. We use it to
 * keep track of the current state. */
static void stateChanged_callback(GstBus *bus, GstMessage *msg, VideoData *data) {
  GstState old_state, new_state, pending_state;
  gst_message_parse_state_changed (msg, &old_state, &new_state, &pending_state);
  if (GST_MESSAGE_SRC (msg) == GST_OBJECT (data->pipeline)) {
    data->state = new_state;
    g_print ("State set to %s\n", gst_element_state_get_name (new_state));
    if (old_state == GST_STATE_READY && new_state == GST_STATE_PAUSED) {
      /* For extra responsiveness, we refresh the GUI as soon as we reach the PAUSED state */
      refreshUI(data);
    }
  }
}

/* 
	This function is called when the main window is closed 
*/
static void gtkEnd_event(GtkWidget *widget, GdkEvent *event, VideoData *data) {
  gstreamerCleanup(data);
  gtk_main_quit();
}

/* 
	This function is called when the slider changes its position. We perform a seek to the
	new position here. 
*/
static void sliderChange_event(GtkRange *range, VideoData *data) {
	gdouble value = gtk_range_get_value(GTK_RANGE(data->slider));
	gst_element_seek_simple(data->videoSink, GST_FORMAT_TIME, (GstSeekFlags)(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT),
		(gint64)(value * GST_SECOND));
}

/* 
	Set up initial UI 
*/
void gtkSetup(int argc, char *argv[], VideoData *videoData, AudioData *audioData)
{
	GtkWidget *mainWindow;		// Contains all other windows
	GtkWidget *videoWindow;		// Contains the video
	GtkWidget *mainBox;			// Vbox, holds HBox and videoControls
	GtkWidget *mainHBox;		// Hbox, holds video window and option box
	GtkWidget *videoControls;	// Hbox, holds the buttons and the slider for video
	GtkWidget *audioControls;	// Hbox, holds the buttons for audio
	GtkWidget *options;			// Vbox, holds the settings options

	// Video related buttons
	GtkWidget *playVideoFile_button, *pauseVideoFile_button, *forwardVideoFile_button, *rewindVideoFile_button;
	GtkWidget *startCameraVideoCapture_button, *stopCameraVideoCapture_button;

	// Audio related buttons
	GtkWidget *audio_start_rec, *audio_stop_rec, *audio_start_play, *audio_stop_play;

	// Informational
	GtkWidget *video_label = gtk_label_new("Video:");
	GtkWidget *audio_label = gtk_label_new("Audio:");

	gtk_init(&argc, &argv);
   
	mainWindow = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	g_signal_connect(G_OBJECT(mainWindow), "delete-event", G_CALLBACK (gtkEnd_event), videoData);

	videoWindow = gtk_drawing_area_new();
	gtk_widget_set_double_buffered(videoWindow, FALSE);
	g_signal_connect(videoWindow, "realize", G_CALLBACK (setVideoWindow_event), videoData);
	videoData->window = videoWindow;
	
	// Set up options
	videoEncoding_option = gtk_combo_box_new_text();
	gtk_combo_box_prepend_text(GTK_COMBO_BOX(videoEncoding_option), MJPEG_ENCODER);
	gtk_combo_box_prepend_text(GTK_COMBO_BOX(videoEncoding_option), MPEG4_ENCODER);
	gtk_combo_box_prepend_text(GTK_COMBO_BOX(videoEncoding_option), NO_ENCODER);
	gtk_combo_box_set_active(GTK_COMBO_BOX(videoEncoding_option), 0);

	videoWidth_entry = gtk_entry_new();
	gtk_entry_set_text (GTK_ENTRY(videoWidth_entry), integerToString(videoData->width).c_str());
	//g_signal_connect_after(GTK_ENTRY(videoWidth_entry), "insert-text", G_CALLBACK(updateWidth_callback), videoData);
	videoHeight_entry = gtk_entry_new();
	gtk_entry_set_text (GTK_ENTRY(videoHeight_entry), integerToString(videoData->height).c_str());
	videoRate_entry = gtk_entry_new();
	gtk_entry_set_text (GTK_ENTRY(videoRate_entry), integerToString(videoData->recordingRate).c_str());

	audioData->audioRate_entry = gtk_entry_new();
	gtk_entry_set_text (GTK_ENTRY(audioData->audioRate_entry), integerToString(audioData->bitrate).c_str());

	// Video control callbacks
	startCameraVideoCapture_button = gtk_button_new_from_stock(GTK_STOCK_MEDIA_RECORD);
	g_signal_connect(G_OBJECT (startCameraVideoCapture_button), "clicked", G_CALLBACK(gstreamerStartCameraVideoCapture), videoData);
	stopCameraVideoCapture_button = gtk_button_new_from_stock(GTK_STOCK_MEDIA_STOP);
	g_signal_connect(G_OBJECT (stopCameraVideoCapture_button), "clicked", G_CALLBACK(gstreamerStopCameraVideoCapture), videoData);

	playVideoFile_button = gtk_button_new_from_stock(GTK_STOCK_MEDIA_PLAY);
	g_signal_connect(G_OBJECT(playVideoFile_button), "clicked", G_CALLBACK(gstreamerPlayVideoFile), videoData);
	pauseVideoFile_button = gtk_button_new_from_stock (GTK_STOCK_MEDIA_PAUSE);
	g_signal_connect(G_OBJECT(pauseVideoFile_button), "clicked", G_CALLBACK(gstreamerPauseVideoFile), videoData);
	forwardVideoFile_button = gtk_button_new_from_stock (GTK_STOCK_MEDIA_FORWARD);
	g_signal_connect(G_OBJECT(forwardVideoFile_button), "clicked", G_CALLBACK(gstreamerForwardVideoFile), videoData);
	rewindVideoFile_button = gtk_button_new_from_stock (GTK_STOCK_MEDIA_REWIND);
	g_signal_connect(G_OBJECT(rewindVideoFile_button), "clicked", G_CALLBACK(gstreamerRewindVideoFile), videoData);

	videoData->slider = gtk_hscale_new_with_range(0, 100, 1);
	gtk_scale_set_draw_value (GTK_SCALE (videoData->slider), 0);
	videoData->sliderUpdateSignalId = g_signal_connect(G_OBJECT(videoData->slider), "value-changed", G_CALLBACK(sliderChange_event), videoData);

	// Audio control callbacks
	audio_start_rec = gtk_button_new_from_stock(GTK_STOCK_MEDIA_RECORD);
	g_signal_connect(G_OBJECT (audio_start_rec), "clicked", G_CALLBACK (audio_start_recording), audioData);
	audio_stop_rec = gtk_button_new_from_stock(GTK_STOCK_MEDIA_STOP);
	g_signal_connect(G_OBJECT (audio_stop_rec), "clicked", G_CALLBACK (audio_stop_recording), audioData);
	audio_start_play = gtk_button_new_from_stock(GTK_STOCK_MEDIA_PLAY);
	g_signal_connect(G_OBJECT (audio_start_play), "clicked", G_CALLBACK (audio_start_playback), audioData);
	audio_stop_play = gtk_button_new_from_stock (GTK_STOCK_MEDIA_PAUSE);
	g_signal_connect(G_OBJECT (audio_stop_play), "clicked", G_CALLBACK (audio_stop_playback), audioData);

	// Layout
	videoControls = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (videoControls), gtk_label_new("Play Video File:"), FALSE, FALSE, 20);
	gtk_box_pack_start (GTK_BOX (videoControls), playVideoFile_button, FALSE, FALSE, 2);
	gtk_box_pack_start (GTK_BOX (videoControls), rewindVideoFile_button, FALSE, FALSE, 2);
	gtk_box_pack_start (GTK_BOX (videoControls), pauseVideoFile_button, FALSE, FALSE, 2);
	gtk_box_pack_start (GTK_BOX (videoControls), forwardVideoFile_button, FALSE, FALSE, 2);
	gtk_box_pack_start (GTK_BOX (videoControls), gtk_label_new("Camera Video:"), FALSE, FALSE, 20);
	gtk_box_pack_start (GTK_BOX (videoControls), startCameraVideoCapture_button, FALSE, FALSE, 2);
	gtk_box_pack_start (GTK_BOX (videoControls), stopCameraVideoCapture_button, FALSE, FALSE, 2);
	//gtk_box_pack_start (GTK_BOX (videoControls), videoData->slider, FALSE, FALSE, 100);

	//audioControls = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (videoControls), gtk_label_new("Audio Playback:"), FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (videoControls), audio_start_play, FALSE, FALSE, 2);
	gtk_box_pack_start (GTK_BOX (videoControls), audio_stop_play, FALSE, FALSE, 2);
	gtk_box_pack_start (GTK_BOX (videoControls), gtk_label_new("Record Audio:"), FALSE, FALSE, 20);
	gtk_box_pack_start (GTK_BOX (videoControls), audio_start_rec, FALSE, FALSE, 2);
	gtk_box_pack_start (GTK_BOX (videoControls), audio_stop_rec, FALSE, FALSE, 2);
   
	options = gtk_vbox_new(FALSE, 0);
	gtk_box_pack_start (GTK_BOX (options), gtk_label_new("Width:"), FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (options), videoWidth_entry, FALSE, FALSE, 1);
	gtk_box_pack_start (GTK_BOX (options), gtk_label_new("Height:"), FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (options), videoHeight_entry, FALSE, FALSE, 1);
	gtk_box_pack_start (GTK_BOX (options), gtk_label_new("Recording Rate:"), FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (options), videoRate_entry, FALSE, FALSE, 1);
	gtk_box_pack_start (GTK_BOX (options), gtk_label_new("Compression:"), FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (options), videoEncoding_option, FALSE, FALSE, 1);
	gtk_box_pack_start (GTK_BOX (options), gtk_label_new("Audio Bitrate:"), FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (options), audioData->audioRate_entry, FALSE, FALSE, 1);

	mainHBox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start (GTK_BOX (mainHBox), videoWindow, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (mainHBox), options, FALSE, FALSE, 10);
   
	mainBox = gtk_vbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (mainBox), mainHBox, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (mainBox), videoControls, FALSE, FALSE, 2);
	//gtk_box_pack_start (GTK_BOX (mainBox), audioControls, FALSE, FALSE, 1);
	gtk_container_add (GTK_CONTAINER (mainWindow), mainBox);
	gtk_window_set_default_size (GTK_WINDOW (mainWindow), 1280, 960);
   
	gtk_widget_show_all (mainWindow);
}

/* 
	Application entry point 
*/
int main(int argc, char *argv[])
{
	VideoData videoData;
	AudioData audioData;
	audioData.bitrate = 44100;
	
	initializeVideoData(&videoData);

	gst_init(&argc, &argv);
	gstreamerBuildElementsOnce(&videoData);
	gstreamerBuildElements(&videoData);

	gtkSetup(argc, argv, &videoData, &audioData);

	// Instruct the bus to emit signals for each received message, and connect to the interesting signals
	GstBus *bus = gst_element_get_bus(videoData.pipeline);
	gst_bus_add_signal_watch(bus);
	g_signal_connect (G_OBJECT (bus), "message::error", (GCallback)error_callback, &videoData);
	g_signal_connect (G_OBJECT (bus), "message::eos", (GCallback)EOS_callback, &videoData);
	g_signal_connect (G_OBJECT (bus), "message::state-changed", (GCallback)stateChanged_callback, &videoData);
	gst_object_unref (bus);

	// Register a function that GLib will call every second
	//g_timeout_add_seconds (1, (GSourceFunc)refreshUI, &videoData);

	gtk_main();

	gstreamerCleanup(&videoData);
}

/* 
	Callbacks for UI buttons
*/
void gstreamerPlayVideoFile(GtkWidget *widget, VideoData *videoData)
{
	if (videoData->state == GST_STATE_PAUSED && videoData->playerMode == File)
	{
		gstreamerPlay(videoData);
	}
	else
	{
		videoData->fileSourcePath = gtkGetUserFile();
		gstreamerBuildPipeline(videoData, File);
		gstreamerPlay(videoData);
		videoData->playerMode = File;
	}
}

void gstreamerPauseVideoFile(GtkWidget *widget, VideoData *videoData)
{
	gstreamerPause(videoData);
}

void gstreamerStartCameraVideoCapture(GtkWidget *widget, VideoData *videoData)
{
	videoData->width = charPointerToInteger(GTK_ENTRY(videoWidth_entry)->text);
	videoData->height = charPointerToInteger(GTK_ENTRY(videoHeight_entry)->text);
	videoData->recordingRate = charPointerToInteger(GTK_ENTRY(videoRate_entry)->text);
	char *activeComboItem = gtk_combo_box_get_active_text(GTK_COMBO_BOX(videoEncoding_option));
	videoData->encoderPlugin = string(activeComboItem);
	videoData->fileSinkPath = gtkGetUserSaveFile();

	gstreamerBuildPipeline(videoData, Camera);
	gstreamerPlay(videoData);

}

void gstreamerStopCameraVideoCapture(GtkWidget *widget, VideoData *videoData)
{
	gstreamerStop(videoData);
	videoData->playerMode = Initial;
}

void gstreamerForwardVideoFile(GtkWidget *widget, VideoData *videoData)
{
	if (videoData->playerMode == File)
	{
		if (videoData->playbackRate < 0)
		{
			videoData->playbackRate *= -1.0;
		}
		else
		{
			videoData->playbackRate *=2.0;
		}
		sendSeekEvent(videoData);
		gstreamerPlay(videoData);
	}
}

void gstreamerRewindVideoFile(GtkWidget *widget, VideoData *videoData)
{
	if (videoData->playerMode == File)
	{
		if (videoData->playbackRate > 0)
		{
			videoData->playbackRate *= -1.0;
		}
		else
		{
			videoData->playbackRate *=2.0;
		}
		sendSeekEvent(videoData);
		gstreamerPlay(videoData);
	}
}
