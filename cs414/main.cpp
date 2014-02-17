#include <gst/gst.h>
#include <gtk-2.0\gtk\gtk.h>
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

enum PlayerMode { Camera, File };

typedef struct _VideoData 
{
	GstElement *pipeline, *muxer, *demuxer, *tee, *fileQueue, *playerQueue, *fileSource, *fileSink;
	GstElement *videoSource, *videoSink, *videoFilter, *videoConvert, *videoEncoder, *videoDecoder, *playbin;
	GstCaps *videoCaps;
	GMainLoop *loop;

	int height, width, recordingRate;
	double playbackRate;

	bool playing;

	string encoderPlugin, decoderPlugin;
	string muxerPlugin, demuxerPlugin;
	string fileSourcePath, fileSinkPath;
} VideoData;

/*
	Other pipelines:
		"ksvideosrc ! video/x-raw-yuv,width=640,height=480 ! ffmpegcolorspace ! tee name=my_videosink ! jpegenc ! avimux ! filesink location=video.avi my_videosink. ! queue ! autovideosink"
		"ksvideosrc ! video/x-raw-yuv,width=640,height=480 ! ffmpegcolorspace ! autovideosink"
		"filesrc location="C:\\Users\\Timur\\Documents\\Visual Studio 2010\\Projects\\cs414\\cs414\\test.avi" ! decodebin2 ! ffmpegcolorspace ! autovideosink"
*/

/*
	TODO: explain
*/
static void send_seek_event (VideoData *data) {
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
    seek_event = gst_event_new_seek (data->playbackRate, GST_FORMAT_TIME, GstSeekFlags(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_ACCURATE),
        GST_SEEK_TYPE_SET, position, GST_SEEK_TYPE_NONE, 0);
  } else {
    seek_event = gst_event_new_seek (data->playbackRate, GST_FORMAT_TIME, GstSeekFlags(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_ACCURATE),
        GST_SEEK_TYPE_SET, 0, GST_SEEK_TYPE_SET, position);
  }

  // Send the event
  gst_element_send_event (data->videoSink, seek_event);
   
  g_print ("Current playbackRate: %g\n", data->playbackRate);
}
   
/* 
	Process keyboard input 
*/
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
    send_seek_event(data);
    break;
  case 'd':
    data->playbackRate *= -1.0;
    send_seek_event(data);
    break;
  case 'n':   
    gst_element_send_event(data->videoSink, gst_event_new_step(GST_FORMAT_BUFFERS, 1, data->recordingRate, TRUE, FALSE));
    g_print ("Stepping one frame\n");
    break;
  case 'q':
    g_main_loop_quit (data->loop);
    break;
  default:
    break;
  }
   
  g_free (str);
  return TRUE;
}

int main(int argc, char *argv[]) 
{
	GstBus *bus;
	GstMessage *msg;
	GIOChannel *ioStdin;
	
	PlayerMode mode = File;
	VideoData videoData;

	// Initialize our data structure
	memset(&videoData, 0, sizeof(videoData));
   
	// Print usage map
	g_print("USAGE: Choose one of the following options, then press enter:\n"
		" 'P' to toggle between PAUSE and PLAY\n"
		" 'S' to increase playback speed, 's' to decrease playback speed\n"
		" 'D' to toggle playback direction\n"
		" 'N' to move to next frame (in the current direction, better in PAUSE)\n"
		" 'Q' to quit\n");

	// Set default video player settings 
	videoData.height = 480;
	videoData.width = 640;
	videoData.recordingRate = 20;
	videoData.playbackRate = 1.0;
	videoData.playing = TRUE;
	videoData.encoderPlugin = IDENTITY;
	videoData.decoderPlugin = MJPEG;
	videoData.muxerPlugin = AVI_MUXER;
	videoData.demuxerPlugin = AVI_MUXER;
	videoData.fileSourcePath = "file:///C:/Users/Timur/Documents/Visual Studio 2010/Projects/cs414/cs414/test.avi";
	videoData.fileSinkPath = "video" + string(videoData.muxerPlugin == AVI_MUXER ? ".avi" : ".mp4");

	// Initialize GStreamer 
	gst_init(&argc, &argv);
  
	// Build Elements 
	videoData.pipeline = gst_pipeline_new("mp1-pipeline");
	videoData.tee = gst_element_factory_make("tee", "tee");
	videoData.fileQueue = gst_element_factory_make("queue", "fileQueue");
	videoData.playerQueue = gst_element_factory_make("queue", "playerQueue");
	videoData.fileSource = gst_element_factory_make("filesrc", "fileSource");
	videoData.fileSink = gst_element_factory_make("filesink", "filesink");
	videoData.playbin = gst_element_factory_make("playbin2", "playbin");

	videoData.muxer = gst_element_factory_make(videoData.muxerPlugin.c_str(), "muxer");
	videoData.demuxer = gst_element_factory_make(videoData.demuxerPlugin.c_str(), "muxer");
	videoData.videoSource = gst_element_factory_make("dshowvideosrc", "videoSource");
	videoData.videoSink = gst_element_factory_make("dshowvideosink", "videoSink");
	videoData.videoConvert = gst_element_factory_make("autovideoconvert", "videoConvert");
	videoData.videoFilter = gst_element_factory_make("capsfilter", "videoFilter");
	videoData.videoEncoder = gst_element_factory_make(videoData.encoderPlugin.c_str(), "videoEncoder");
	videoData.videoDecoder = gst_element_factory_make(videoData.decoderPlugin.c_str(), "videoDecoder");

	if (videoData.muxer == NULL) g_error("Could not create muxer element");
	if (videoData.videoDecoder == NULL) g_error("Could not create video decoder element");
	if (videoData.demuxer == NULL) g_error("Could not create demuxer element");
	if (videoData.videoSource == NULL) g_error("Could not create video source element");
	if (videoData.videoSink == NULL) g_error("Could not create video sink element");
	if (videoData.videoConvert == NULL) g_error("Could not create video convert element");
	if (videoData.videoFilter == NULL) g_error("Could not create video filter element");
	if (videoData.videoEncoder == NULL) g_error("Could not create video encoder element");

	// Set values 
	g_object_set(videoData.fileSink, "location", videoData.fileSinkPath.c_str(), NULL);
	g_object_set(videoData.fileSource, "location", videoData.fileSourcePath.c_str(), NULL);
	g_object_set(videoData.playbin, "uri", videoData.fileSourcePath.c_str(), "video-sink", videoData.videoSink, NULL);

	videoData.videoCaps = gst_caps_new_simple("video/x-raw-yuv",
		"format", GST_TYPE_FOURCC, GST_MAKE_FOURCC('Y', 'U', 'Y', '2'),
		"height", G_TYPE_INT, videoData.height,
		"width", G_TYPE_INT, videoData.width,
		"pixel-aspect-ratio", GST_TYPE_FRACTION, 1, 1,
		"framerecordingRate", GST_TYPE_FRACTION, videoData.recordingRate, 1, 
		NULL);
	g_object_set(G_OBJECT(videoData.videoFilter), "caps", videoData.videoCaps, NULL);
	gst_caps_unref (videoData.videoCaps);
	
	switch (mode)
	{
		case Camera:
			// Build the pipeline
			gst_bin_add_many(GST_BIN(videoData.pipeline), 
				videoData.videoSource, videoData.videoConvert, videoData.videoFilter, videoData.tee, videoData.fileQueue, 
				videoData.videoEncoder, videoData.muxer, videoData.fileSink, videoData.playerQueue, videoData.videoSink, NULL);

			// Link the video recording component: videoSrc -> convert -> filter -> tee
			if (!gst_element_link_many(videoData.videoSource, videoData.videoConvert, videoData.videoFilter, videoData.tee, NULL))
			{
				gst_object_unref(videoData.pipeline);
				g_error("Failed linking: videoSrc -> convert -> filter -> tee \n");
			}
			// Link the media sink pipeline: tee -> queue -> encoder -> muxer -> fileSink
			if (!gst_element_link_many(videoData.tee, videoData.fileQueue, videoData.videoEncoder, videoData.muxer, videoData.fileSink, NULL)) 
			{
				gst_object_unref(videoData.pipeline);
				g_print("Failed linking: tee -> queue -> encoder -> muxer -> fileSink \n");
			}
			// Link the media player pipeline: tee -> queue -> mediaSink
			if (!gst_element_link_many(videoData.tee, videoData.playerQueue, videoData.videoSink, NULL)) 
			{
				gst_object_unref(videoData.pipeline);
				g_print("Failed linking: tee -> queue -> mediaSink \n");
			}
			break;

		case File:
			gst_bin_add(GST_BIN(videoData.pipeline), videoData.playbin);
			// Add a keyboard watch so we get notified of keystrokes
			#ifdef _WIN32
				ioStdin = g_io_channel_win32_new_fd(fileno(stdin));
			#else
				ioStdin = g_io_channel_unix_new(fileno(stdin));
			#endif
				g_io_add_watch(ioStdin, G_IO_IN, (GIOFunc)handle_keyboard, &videoData);
	}

	// Run
	gst_element_set_state(videoData.pipeline, GST_STATE_PLAYING);

	// Create a GLib Main Loop and set it to run
	videoData.loop = g_main_loop_new(NULL, FALSE);
	g_main_loop_run(videoData.loop);

	//// Wait until error or EOS
	//bus = gst_element_get_bus(videoData.pipeline);
	//msg = gst_bus_timed_pop_filtered(bus, GST_CLOCK_TIME_NONE, (GstMessageType)(GST_MESSAGE_ERROR | GST_MESSAGE_EOS));
	//
	//// Parse message 
	//if (msg != NULL) {
	//	GError *err;
	//	gchar *debug_info;
    
	//	switch (GST_MESSAGE_TYPE (msg)) {
	//		case GST_MESSAGE_ERROR:
	//			gst_message_parse_error (msg, &err, &debug_info);
	//			g_printerr ("Error received from element %s: %s\n", GST_OBJECT_NAME (msg->src), err->message);
	//			g_printerr ("Debugging information: %s\n", debug_info ? debug_info : "none");
	//			g_clear_error (&err);
	//			g_free (debug_info);
	//			break;
	//		case GST_MESSAGE_EOS:
	//			g_print ("End-Of-Stream reached.\n");
	//			break;
	//		default:
	//			g_printerr ("Unexpected message received.\n");
	//			break;
	//		}
	//	gst_message_unref (msg);
	//}
    
	// Free the resources 
	g_main_loop_unref (videoData.loop);
	gst_object_unref(bus);
	gst_element_set_state(videoData.pipeline, GST_STATE_NULL);
	gst_object_unref(videoData.pipeline);
	g_io_channel_unref(ioStdin);
	return 0;
}

int gtk_main( int argc, char *argv[])
{
  GtkWidget *window;
  GtkWidget *vbox;

  GtkWidget *menubar;
  GtkWidget *recordermenu;
  GtkWidget *playermenu;
  GtkWidget *record;
  GtkWidget *play;
  GtkWidget *start_rec;
  GtkWidget *stop_rec;
  GtkWidget *start_play;
  GtkWidget *stop_play;
  GtkWidget *pause;
  
  GtkWidget *rewind;
  GtkWidget *forward;
  gtk_init(&argc, &argv);

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

  g_signal_connect_swapped(G_OBJECT(window), "destroy",
        G_CALLBACK(gtk_main_quit), NULL);

  /*
	TODO: Connect start_play button with a file viewer and integrate video playback of file in a function play
  */
  //g_signal_connect(G_OBJECT(start_play), "start_play",
        //G_CALLBACK(play), NULL);

  /*
	TODO: integrate the termination of video viewing in a function stop
  */
  //g_signal_connect(G_OBJECT(stop_play), "stop_play",
        //G_CALLBACK(stop), NULL);

  /*
	TODO: integrate the rewinding of video viewing in a function rewind
  */
  //g_signal_connect(G_OBJECT(rewind), "rewind",
        //G_CALLBACK(reqind), NULL);

  /*
	TODO: integrate the fast-forward of video viewing in a function forward
  */
  //g_signal_connect(G_OBJECT(forward), "forward",
        //G_CALLBACK(forward), NULL);

  /*
	TODO: integrate the recording of video in a function record and spawn a different thread to see frames during record
  */
  //g_signal_connect(G_OBJECT(record), "record",
        //G_CALLBACK(record), NULL);

  /*
	TODO: integrate the stopping of video/audio recording and then compress video/audio to a file in a function rec_stop 
  */
  //g_signal_connect(G_OBJECT(rec_stop), "rec_stop",
        //G_CALLBACK(stop_rec), NULL);

  gtk_widget_show_all(window);

  gtk_main();

  return 0;
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

