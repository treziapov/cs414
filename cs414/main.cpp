#include <gst/gst.h>
#include <gst/interfaces/xoverlay.h>
#include <gtk-2.0\gtk\gtk.h>
#include <gdk/gdk.h>
#include <gdk/gdkwin32.h>
#include <iostream>
#include <string>

#define MJPEG "jpegenc"
#define MPEG4 "ffenc_mpeg4"
#define AVI_MUXER "avimux"
#define MKV_MUXER "matroskamux"
#define AVI_DEMUXER "avidemux"
#define MKV_DEMUXER "matroskademux"
#define IDENTITY "identity"

using namespace std;

enum PlayerMode { Initial, Camera, File };

typedef struct _VideoData 
{
	GstElement *pipeline, *muxer, *demuxer, *tee, *fileQueue, *playerQueue, *fileSource, *fileSink;
	GstElement *videoSource, *videoSink, *videoFilter, *videoConvert, *videoEncoder, *videoDecoder, *playbin;
	GstCaps *videoCaps;

	int height, width, recordingRate;
	double playbackRate;

	bool playing;

	string encoderPlugin, decoderPlugin;
	string muxerPlugin, demuxerPlugin;
	string fileSourcePath, fileSinkPath;
} VideoData;

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
	gst_x_overlay_set_window_handle(GST_X_OVERLAY(data->playbin), window_handle);
}

/*
	Send a seek event for navigating the video content
	i.e. fast-forwarding, rewinding
*/
static void sendSeekEvent(VideoData *data) {
	gint64 position;
	GstFormat format = GST_FORMAT_TIME;
	GstEvent *seek_event;
   
	// Obtain the current position, needed for the seek event
	if (!gst_element_query_position (data->pipeline, &format, &position)) {
		g_printerr ("Unable to retrieve current position.\n");
		return;
	}
  
	// Create the seek event
	if (data->playbackRate > 0) {
		seek_event = gst_event_new_seek(data->playbackRate, GST_FORMAT_TIME, GstSeekFlags(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_ACCURATE),
			GST_SEEK_TYPE_SET, position, GST_SEEK_TYPE_NONE, 0);
	} else {
		seek_event = gst_event_new_seek(data->playbackRate, GST_FORMAT_TIME, GstSeekFlags(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_ACCURATE),
			GST_SEEK_TYPE_SET, 0, GST_SEEK_TYPE_SET, position);
	}

	// Send the event
	gst_element_send_event(data->videoSink, seek_event);
	g_print("Current playbackRate: %g\n", data->playbackRate);
}
   
/* Process keyboard input */
static gboolean handle_keyboard (GIOChannel *source, GIOCondition cond, VideoData *data) {
  gchar *str = NULL;
   
  if (g_io_channel_read_line(source, &str, NULL ,NULL, NULL) != G_IO_STATUS_NORMAL) {
    return TRUE;
  }
   
  switch (g_ascii_tolower (str[0])) {
  case 'p':
    data->playing = !data->playing;
    gst_element_set_state(data->pipeline, data->playing ? GST_STATE_PLAYING : GST_STATE_PAUSED);
    g_print("Setting state to %s\n", data->playing ? "PLAYING" : "PAUSE");
    break;
  case 's':
    if (g_ascii_isupper (str[0])) {
      data->playbackRate *= 2.0;
    } else {
      data->playbackRate /= 2.0;
    }
    sendSeekEvent(data);
    break;
  case 'd':
    data->playbackRate *= -1.0;
    sendSeekEvent(data);
    break;
  case 'n':   
    gst_element_send_event(data->videoSink, gst_event_new_step(GST_FORMAT_BUFFERS, 1, data->recordingRate, TRUE, FALSE));
    g_print ("Stepping one frame\n");
    break;
  default:
    break;
  }
   
  g_free (str);
  return TRUE;
}

/* Set up initial UI */
void gtkSetup(int argc, char *argv[], VideoData *videoData)
{
	GtkWidget *vbox;
	GtkWidget *window, *video_window;
	GtkWidget *menubar, *recordermenu, *playermenu;
	GtkWidget *record, *play, *start_rec, *stop_rec, *start_play, *stop_play, *pause, *rewind, *forward;

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

	record = gtk_menu_item_new_with_label("Recorder");
	play = gtk_menu_item_new_with_label("Player");
	start_rec = gtk_menu_item_new_with_label("Start Recording");
	start_play = gtk_menu_item_new_with_label("Start Playing A Video");
	stop_rec = gtk_menu_item_new_with_label("Stop Recording");
	stop_play = gtk_menu_item_new_with_label("Stop Playing Video");
	pause = gtk_menu_item_new_with_label("Pause/Resume Playing");
	rewind = gtk_menu_item_new_with_label("Rewind Video");
	forward = gtk_menu_item_new_with_label("Fast Forward Video");

	gtk_menu_item_set_submenu(GTK_MENU_ITEM(record), recordermenu);
	gtk_menu_shell_append(GTK_MENU_SHELL(recordermenu), start_rec);
	gtk_menu_shell_append(GTK_MENU_SHELL(recordermenu), stop_rec);
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

	g_signal_connect_swapped(G_OBJECT(window), "destroy",
		G_CALLBACK(gtk_main_quit), NULL);

	// TODO: Connect start_play button with a file viewer and integrate video playback of file in a function play
	//g_signal_connect(G_OBJECT(start_play), "start_play", G_CALLBACK(play), NULL);
	// TODO: integrate the termination of video viewing in a function stop
	//g_signal_connect(G_OBJECT(stop_play), "stop_play", G_CALLBACK(stop), NULL);

	gtk_widget_show_all(window);
}

/* Initialize GStreamer components */
void gstreamerSetup(int argc, char *argv[], VideoData *videoData) 
{
	gst_init(&argc, &argv);
  
	// Build Elements 
	videoData->pipeline = NULL;
	videoData->tee = gst_element_factory_make("tee", "tee");
	videoData->fileQueue = gst_element_factory_make("queue", "fileQueue");
	videoData->playerQueue = gst_element_factory_make("queue", "playerQueue");
	videoData->fileSource = gst_element_factory_make("filesrc", "fileSource");
	videoData->fileSink = gst_element_factory_make("filesink", "filesink");
	videoData->playbin = gst_element_factory_make("playbin2", "playbin");

	videoData->muxer = gst_element_factory_make(videoData->muxerPlugin.c_str(), "muxer");
	videoData->demuxer = gst_element_factory_make(videoData->demuxerPlugin.c_str(), "muxer");
	videoData->videoSource = gst_element_factory_make("dshowvideosrc", "videoSource");
	videoData->videoSink = gst_element_factory_make("dshowvideosink", "videoSink");
	videoData->videoConvert = gst_element_factory_make("autovideoconvert", "videoConvert");
	videoData->videoFilter = gst_element_factory_make("capsfilter", "videoFilter");
	videoData->videoEncoder = gst_element_factory_make(videoData->encoderPlugin.c_str(), "videoEncoder");
	videoData->videoDecoder = gst_element_factory_make(videoData->decoderPlugin.c_str(), "videoDecoder");

	if (videoData->muxer == NULL) g_error("Could not create muxer element");
	if (videoData->videoDecoder == NULL) g_error("Could not create video decoder element");
	if (videoData->demuxer == NULL) g_error("Could not create demuxer element");
	if (videoData->videoSource == NULL) g_error("Could not create video source element");
	if (videoData->videoSink == NULL) g_error("Could not create video sink element");
	if (videoData->videoConvert == NULL) g_error("Could not create video convert element");
	if (videoData->videoFilter == NULL) g_error("Could not create video filter element");
	if (videoData->videoEncoder == NULL) g_error("Could not create video encoder element");

	// Set values 
	g_object_set(videoData->fileSink, "location", videoData->fileSinkPath.c_str(), NULL);
	g_object_set(videoData->fileSource, "location", videoData->fileSourcePath.c_str(), NULL);
	g_object_set(videoData->playbin, "uri", videoData->fileSourcePath.c_str(), "video-sink", videoData->videoSink, NULL);

	videoData->videoCaps = gst_caps_new_simple("video/x-raw-yuv",
		"format", GST_TYPE_FOURCC, GST_MAKE_FOURCC('Y', 'U', 'Y', '2'),
		"height", G_TYPE_INT, videoData->height,
		"width", G_TYPE_INT, videoData->width,
		"pixel-aspect-ratio", GST_TYPE_FRACTION, 1, 1,
		"framerecordingRate", GST_TYPE_FRACTION, videoData->recordingRate, 1, 
		NULL);
	g_object_set(G_OBJECT(videoData->videoFilter), "caps", videoData->videoCaps, NULL);
	gst_caps_unref (videoData->videoCaps);
}

/* Run the current gstreamer pipeline */
void gstreamerPlay(VideoData *videoData)
{
	if (!videoData->pipeline) g_error("Invalid pipeline");
	gst_element_set_state(videoData->pipeline, GST_STATE_PLAYING);
}
/* Pause the current gstreamer pipeline */
void gstreamerPause(VideoData *videoData)
{
	if (!videoData->pipeline) g_error("Invalid pipeline");
	gst_element_set_state(videoData->pipeline, GST_STATE_PAUSED);
}

/* Free GStreamer resources */
void gstreamerCleanup(VideoData *videoData)
{
	gst_element_set_state(videoData->pipeline, GST_STATE_NULL);
	gst_object_unref(videoData->pipeline);
}

/* Build the appropriate pipeline from data based on parameter mode */
void gstreamerBuildPipeline(VideoData *videoData, PlayerMode mode)
{
	// Clear previous pipeline if one has already been set up
	if (videoData->pipeline || videoData->pipeline != NULL)
	{
		gst_object_unref(videoData->pipeline);
	}

	videoData->pipeline = gst_pipeline_new("mp1-pipeline");
		
	switch(mode)
	{
		case Camera:
			// Build the camera pipeline
			gst_bin_add_many(GST_BIN(videoData->pipeline), 
				videoData->videoSource, videoData->videoConvert, videoData->videoFilter, videoData->tee, videoData->fileQueue, 
				videoData->videoEncoder, videoData->muxer, videoData->fileSink, videoData->playerQueue, videoData->videoSink, NULL);

			// Link the video recording component: videoSrc -> convert -> filter -> tee
			if (!gst_element_link_many(videoData->videoSource, videoData->videoConvert, videoData->videoFilter, videoData->tee, NULL))
			{
				gst_object_unref(videoData->pipeline);
				g_error("Failed linking: videoSrc -> convert -> filter -> tee \n");
			}
			// Link the media sink pipeline: tee -> queue -> encoder -> muxer -> fileSink
			if (!gst_element_link_many(videoData->tee, videoData->fileQueue, videoData->videoEncoder, videoData->muxer, videoData->fileSink, NULL)) 
			{
				gst_object_unref(videoData->pipeline);
				g_print("Failed linking: tee -> queue -> encoder -> muxer -> fileSink \n");
			}
			// Link the media player pipeline: tee -> queue -> mediaSink
			if (!gst_element_link_many(videoData->tee, videoData->playerQueue, videoData->videoSink, NULL)) 
			{
				gst_object_unref(videoData->pipeline);
				g_print("Failed linking: tee -> queue -> mediaSink \n");
			}
			break;

		case File:
			// Build the file playback pipeline
			gst_bin_add(GST_BIN(videoData->pipeline), videoData->playbin);
			break;
	}
}

/* Sets default video data parameters*/
void initializeVideoData(VideoData * videoData)
{
	videoData->width = 640;
	videoData->height = 480;
	videoData->recordingRate = 20;
	videoData->playbackRate = 1.0;
	videoData->playing = TRUE;
	videoData->encoderPlugin = IDENTITY;
	videoData->decoderPlugin = MJPEG;
	videoData->muxerPlugin = AVI_MUXER;
	videoData->demuxerPlugin = AVI_MUXER;
	videoData->fileSinkPath = "video" + string(videoData->muxerPlugin == AVI_MUXER ? ".avi" : ".mp4");
	// NOTE: Absolute user-specific path
	videoData->fileSourcePath = "file:///C:/Users/Timur/Documents/Visual Studio 2010/Projects/cs414/cs414/test.avi";
}

int main(int argc, char *argv[])
{
	PlayerMode mode = File;

	VideoData videoData;
	initializeVideoData(&videoData);

	gstreamerSetup(argc, argv, &videoData);
	gstreamerBuildPipeline(&videoData, mode);
	gtkSetup(argc, argv, &videoData);

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

