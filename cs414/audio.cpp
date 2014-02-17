#include <gst/gst.h>

typedef struct AudioData{
	GstElement * pipeline;
	GstElement * source;
	GstElement * filter;
	GstElement * muxer;
	GstElement * demuxer;
	GstElement * encoder;
	GstElement * decoder;
	GstElement * sink;
} AudioData;

//Handles the demuxer's new pad signal
static void addPadSignalHandler(GstElement * element, GstPad * newPad, AudioData * data){
	if(element == data->demuxer){
		g_printerr("signal recieved\n");

		GstPad * decoderPad = gst_element_get_static_pad(data->decoder, "sink");
		GstPadLinkReturn ret;
		
		if(gst_pad_is_linked(decoderPad)){
			g_print("Pad is already linked\n");
			gst_object_unref(decoderPad);
			return;
		}

		ret = gst_pad_link(newPad, decoderPad);
		if(GST_PAD_LINK_FAILED(ret)){
			g_print("Could not link pads\n");
		}else{
			g_print("Linked demuxer to decoder\n");
		}

		gst_object_unref(decoderPad);
	}
}

//Records and stores audio in the desired format and location
//Supports raw audio and vorbis ogg audio
void recordAudioPipeline(char * source, int rate, int channel, char * format, char * filepath, AudioData * data){
	GstCaps * caps;

	data->source = gst_element_factory_make(source, "source");
	if(format == "raw"){
		data->filter = gst_element_factory_make("audioconvert", "convert");
		data->sink = gst_element_factory_make("filesink", "sink");

		data->pipeline = gst_pipeline_new("audio_pipeline");

		if (!data->pipeline || !data->source || !data->filter || !data->sink) {
			g_printerr("Elements could not be created");
			return;
		}

		g_object_set(G_OBJECT(data->sink), "location", filepath, NULL);

		gst_bin_add_many(GST_BIN(data->pipeline), data->source, data->filter, data->sink, NULL);

		caps = gst_caps_new_simple("audio/x-raw-int", "rate", G_TYPE_INT, rate, "channels", G_TYPE_INT, channel, "endianness", G_TYPE_INT, 1234, "signed", G_TYPE_BOOLEAN, (gboolean)TRUE, NULL);

		if (gst_element_link (data->source, data->filter) != (gboolean)TRUE || gst_element_link_filtered(data->filter, data->sink, caps) != (gboolean)TRUE) {
			g_printerr("Elements could not be linked");
			gst_object_unref(data->pipeline);
		}

		gst_caps_unref(caps);
	}else{
		data->filter = gst_element_factory_make("audioconvert", "convert");
		data->encoder = gst_element_factory_make("vorbisenc", "encoder");
		data->muxer = gst_element_factory_make("oggmux", "muxer");
		data->sink = gst_element_factory_make("filesink", "sink");

		data->pipeline = gst_pipeline_new("audio_pipeline");

		if(!data->pipeline || !data->source || !data->filter || !data->encoder || !data->muxer, !data->sink){
			g_printerr("Elements could not be created");
		}

		g_object_set(G_OBJECT(data->sink), "location", filepath, NULL);

		gst_bin_add_many(GST_BIN(data->pipeline), data->source, data->filter, data->encoder, data->muxer, data->sink, NULL);

		caps = gst_caps_new_simple("audio/x-raw-float", "rate", G_TYPE_INT, rate, "channels", G_TYPE_INT, channel, "endianness", G_TYPE_INT, 1234, NULL);

		if(gst_element_link(data->source, data->filter) != (gboolean)TRUE || gst_element_link_filtered(data->filter, data->encoder, caps) != (gboolean)TRUE, gst_element_link(data->encoder, data->muxer) != (gboolean)TRUE, gst_element_link(data->muxer, data->sink) != (gboolean)TRUE){
			g_printerr("Elements could not be linked");
			gst_object_unref(data->pipeline);
		}

		gst_caps_unref(caps);
	}
}

//Plays audio from a given file and format
//Supports raw audio and vorbis ogg format
void playAudioPipeline(char * format, char * filepath, int rate, int channel, AudioData * data){
	GstCaps * caps;

	data->source = gst_element_factory_make("filesrc", "source");
	g_object_set(G_OBJECT(data->source), "location", filepath, NULL);

	if(format == "raw"){
		data->sink = gst_element_factory_make("directsoundsink", "sink");

		data->pipeline = gst_pipeline_new("audio_pipeline");

		if(!data->pipeline || !data->source || !data->sink){
			g_printerr("Elements could not be created");
			return;
		}

		gst_bin_add_many(GST_BIN(data->pipeline), data->source, data->sink, NULL);

		caps = gst_caps_new_simple("audio/x-raw-int", "rate", G_TYPE_INT, rate, "channels", G_TYPE_INT, channel, "endianness", G_TYPE_INT, 4321, "signed", G_TYPE_BOOLEAN, (gboolean)TRUE, NULL);

		if(gst_element_link_filtered(data->source, data->sink, caps) != (gboolean)TRUE){
			g_printerr("Elements could not be linked");
			gst_object_unref(data->pipeline);
		}

		gst_caps_unref(caps);
	}else{
		data->demuxer = gst_element_factory_make("oggdemux", "demuxer");
		data->decoder = gst_element_factory_make("vorbisdec", "decoder");
		data->filter = gst_element_factory_make("audioconvert", "convert");
		data->sink = gst_element_factory_make("directsoundsink", "sink");

		data->pipeline = gst_pipeline_new("audio_pipeline");

		if(!data->pipeline || !data->source || !data->demuxer || !data->decoder || !data->filter || !data->sink){
			g_printerr("Elements could not be created");
			return;
		}

		g_signal_connect(data->demuxer, "pad-added", G_CALLBACK(addPadSignalHandler), data);

		gst_bin_add_many(GST_BIN(data->pipeline), data->source, data->demuxer, data->decoder, data->filter, data->sink, NULL);
		if(gst_element_link(data->source, data->demuxer) != (gboolean)TRUE ||  gst_element_link(data->decoder, data->filter) != (gboolean)TRUE || gst_element_link(data->filter, data->sink) != (gboolean)TRUE){
			g_printerr("Elements could not be linked");
			gst_object_unref(data->pipeline);
		}
	}
}

//Function to call for audio pipeline creation
//Calls helper functions based on the inputs and outputs
void createAudioPipeline(char * source, int rate, int channel, char * format, char * sink){
	AudioData data;
	GstBus * bus;
	GstMessage * msg;
	GstStateChangeReturn ret;

	if(source == "dshowaudiosrc"){
		recordAudioPipeline(source, rate, channel, format, sink, &data);
	}else{
		playAudioPipeline(format, source, rate, channel, &data);
	}

	if(data.pipeline == NULL){
		g_printerr("Failed to create pipeline");
		return;
	}

	ret = gst_element_set_state (data.pipeline, GST_STATE_PLAYING);
	if (ret == GST_STATE_CHANGE_FAILURE) {
		g_printerr("Unable to set the pipeline to the playing state.\n");
		gst_object_unref(data.pipeline);
		return;
	}

	if(source == "dshowaudiosrc"){
		g_print("recording...\n");
	}else{
		g_print("playing...\n");
	}
  
	bus = gst_element_get_bus(data.pipeline);
	msg = gst_bus_timed_pop_filtered(bus, GST_CLOCK_TIME_NONE, (GstMessageType)(GST_MESSAGE_ERROR | GST_MESSAGE_EOS));
  
	if (msg != NULL) {
		GError * err;
		gchar * debug_info;
    
		switch (GST_MESSAGE_TYPE (msg)) {
			case GST_MESSAGE_ERROR:
				gst_message_parse_error(msg, &err, &debug_info);
				g_printerr("Error received from element %s: %s\n", GST_OBJECT_NAME (msg->src), err->message);
				g_printerr("Debugging information: %s\n", debug_info ? debug_info : "none");
				g_clear_error(&err);
				g_free(debug_info);
				break;
			case GST_MESSAGE_EOS:
				g_print("End-Of-Stream reached.\n");
				break;
			default:
				g_printerr("Unexpected message received.\n");
				break;
		}
		gst_message_unref(msg);
	}
  
	gst_object_unref(bus);
	gst_element_set_state(data.pipeline, GST_STATE_NULL);
	gst_object_unref(data.pipeline);

}

int main(int argc, char *argv[]) {
	gst_init(&argc, &argv);

	//call to audio pipeline creator function
	//Parameters:
	//1. source (string) - dshowaudiosrc for recording, file name and location for playback
	//2. bitrate (int) - Desired bitrate to record at, if user does not specify default to 44100
	//3. channels (int) - Desired number of channels, if user does not specify default to 1
	//4. format (string) - Desired format to either play or record (might change to NULL for playback as I can write my own function to read the file type)
	//5. sink (string) - NULL for playback, desired file location and name for recording
	createAudioPipeline("M:/music.ogg", 44100, 2, "vorbis", NULL);

	while(1);
	return 0;
}
