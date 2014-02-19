#include <gst/gst.h>
#include <cstring>
#include <stdio.h>
#include <gst/interfaces/xoverlay.h>
#include <gtk-2.0\gtk\gtk.h>
#include <gdk/gdk.h>
#include <gdk/gdkwin32.h>

#include "audio.h"

//Handles the demuxer's new pad signal
static void addPadSignalHandler(GstElement * element, GstPad * newPad, AudioData * data){
	if(element == data->demuxer){

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
		}

		gst_object_unref(decoderPad);
	}
}

char * getFileType(char * file){
	int length = strlen(file);

	if(length < 4){
		g_printerr("Invalid file type\n");
		return NULL;
	}else if(strcmp(&file[length - 4], ".ogg") == 0){
		return "vorbis";
	}else if(strcmp(&file[length - 4], ".raw") == 0){
		return "raw";
	}else if(strcmp(&file[length - 5], ".ulaw") == 0){
		return "mulaw";
	}else if(strcmp(&file[length - 5], ".alaw") == 0){
		return "alaw";
	}else{
		printf("%s\n", &file[length - 5]);
		g_printerr("Invalid file type\n");
		return NULL;
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

		if (gst_element_link(data->source, data->filter) != (gboolean)TRUE || gst_element_link_filtered(data->filter, data->sink, caps) != (gboolean)TRUE) {
			g_printerr("Elements could not be linked");
			gst_object_unref(data->pipeline);
		}
		
		gst_caps_unref(caps);
	}else if(format == "mulaw"){
		data->filter = gst_element_factory_make("audioconvert", "convert");
		data->encoder = gst_element_factory_make("mulawenc", "encoder");
		data->sink = gst_element_factory_make("filesink", "sink");

		data->pipeline = gst_pipeline_new("audio_pipeline");

		if(!data->pipeline || !data->source || !data->filter || !data->encoder || !data->sink){
			g_printerr("Elements could not be created");
			return;
		}

		g_object_set(G_OBJECT(data->sink), "location", filepath, NULL);

		gst_bin_add_many(GST_BIN(data->pipeline), data->source, data->filter, data->encoder, data->sink, NULL);

		caps = gst_caps_new_simple("audio/x-raw-int", "rate", G_TYPE_INT, rate, "channels", G_TYPE_INT, channel, NULL);

		if(gst_element_link(data->source, data->filter) != (gboolean)TRUE || gst_element_link_filtered(data->filter, data->encoder, caps) != (gboolean)TRUE || gst_element_link(data->encoder, data->sink) != (gboolean)TRUE){
			g_printerr("Elements could not be linked");
			gst_object_unref(data->pipeline);
		}

		gst_caps_unref(caps);
	}else if(format == "alaw"){
		data->filter = gst_element_factory_make("audioconvert", "convert");
		data->encoder = gst_element_factory_make("alawenc", "encoder");
		data->sink = gst_element_factory_make("filesink", "sink");

		data->pipeline = gst_pipeline_new("audio_pipeline");

		if(!data->pipeline || !data->source || !data->filter || !data->encoder || !data->sink){
			g_printerr("Elements could not be created");
			return;
		}

		g_object_set(G_OBJECT(data->sink), "location", filepath, NULL);

		gst_bin_add_many(GST_BIN(data->pipeline), data->source, data->filter, data->encoder, data->sink, NULL);

		caps = gst_caps_new_simple("audio/x-raw-int", "rate", G_TYPE_INT, rate, "channels", G_TYPE_INT, channel, NULL);

		if(gst_element_link(data->source, data->filter) != (gboolean)TRUE || gst_element_link_filtered(data->filter, data->encoder, caps) != (gboolean)TRUE || gst_element_link(data->encoder, data->sink) != (gboolean)TRUE){
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
	}else if(format == "mulaw"){
		data->decoder = gst_element_factory_make("mulawdec", "decoder");
		data->sink = gst_element_factory_make("directsoundsink", "sink");

		data->pipeline = gst_pipeline_new("audio_pipeline");

		if(!data->pipeline || !data->source || !data->decoder || !data->sink){
			g_printerr("Elements could not be created");
			return;
		}

		gst_bin_add_many(GST_BIN(data->pipeline), data->source, data->decoder, data->sink, NULL);

		caps = gst_caps_new_simple("audio/x-mulaw", "rate", G_TYPE_INT, rate, "channels", G_TYPE_INT, channel, NULL);

		if(gst_element_link_filtered(data->source, data->decoder, caps) != (gboolean)TRUE || gst_element_link(data->decoder, data->sink) != (gboolean)TRUE){
			g_printerr("Elements could not be linked");
			gst_object_unref(data->pipeline);
		}

		gst_caps_unref(caps);
	}else if(format == "alaw"){
		data->decoder = gst_element_factory_make("alawdec", "decoder");
		data->sink = gst_element_factory_make("directsoundsink", "sink");

		data->pipeline = gst_pipeline_new("audio_pipeline");

		if(!data->pipeline || !data->source || !data->decoder || !data->sink){
			g_printerr("Elements could not be created");
			return;
		}

		gst_bin_add_many(GST_BIN(data->pipeline), data->source, data->decoder, data->sink, NULL);

		caps = gst_caps_new_simple("audio/x-alaw", "rate", G_TYPE_INT, rate, "channels", G_TYPE_INT, channel, NULL);

		if(gst_element_link_filtered(data->source, data->decoder, caps) != (gboolean)TRUE || gst_element_link(data->decoder, data->sink) != (gboolean)TRUE){
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
void createAudioPipeline(char * source, int rate, int channel, char * format, char * sink, AudioData * data){
	GstBus * bus;
	GstMessage * msg;
	GstStateChangeReturn ret;

	if(source == "dshowaudiosrc"){
		recordAudioPipeline(source, rate, channel, format, sink, data);
	}else{
		playAudioPipeline(format, source, rate, channel, data);
	}

	if(data->pipeline == NULL){
		g_printerr("Failed to create pipeline");
		return;
	}

	ret = gst_element_set_state (data->pipeline, GST_STATE_PLAYING);
	if (ret == GST_STATE_CHANGE_FAILURE) {
		g_printerr("Unable to set the pipeline to the playing state.\n");
		gst_object_unref(data->pipeline);
		return;
	}

	if(source == "dshowaudiosrc"){
		g_print("recording...\n");
	}else{
		g_print("playing...\n");
	}

	/*
	bus = gst_element_get_bus(data->pipeline);
	msg = gst_bus_timed_pop_filtered(bus, GST_CLOCK_TIME_NONE, (GstMessageType)(GST_MESSAGE_ERROR | GST_MESSAGE_EOS));
  
	//bool terminate = false;
	//while(!terminate){
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
					//terminate = true;
					break;
				case GST_MESSAGE_EOS:
					g_print("End-Of-Stream reached.\n");
					//terminate = true;
					break;
				case GST_MESSAGE_STATE_CHANGED:
					//terminate = true;
					break;
				default:
					g_printerr("Unexpected message received.\n");
					break;
			}
			gst_message_unref(msg);
		}
	//}
  
	gst_object_unref(bus);
	gst_element_set_state(data->pipeline, GST_STATE_NULL);
	gst_object_unref(data->pipeline);*/

}

void stopAudioPipeline(AudioData * data){
	gst_element_set_state(data->pipeline, GST_STATE_READY);
	gst_element_set_state(data->pipeline, GST_STATE_NULL);
	gst_object_unref(data->pipeline);
}

void audio_start_recording(GtkWidget* source, gpointer* data){
	//request file name
	GtkWidget *dialog;
	dialog = gtk_file_chooser_dialog_new("Save File",
     				      NULL,
     				      GTK_FILE_CHOOSER_ACTION_SAVE,
     				      GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
     				      GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
     				      NULL);
	gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER (dialog), TRUE);
	gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (dialog), "C:/");
    gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (dialog), "Untitled");
	
	gtk_dialog_run(GTK_DIALOG (dialog));
	char* filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
	gtk_widget_destroy (dialog);
	printf("Enter the desired filename (include extensions): ");


	//char filename[256];
	//fgets(filename, 256, stdin);
	//filename[strlen(filename) - 1] = '\0';
	
	char * filetype = getFileType(filename);
	if(filetype == NULL){
		return;
	}

	printf("Enter the desired bitrate (default is 44100): ");
	char bitrateBuffer[256];
	fgets(bitrateBuffer, 256, stdin);

	int rate = atoi(bitrateBuffer);
	if(rate < 8000 || rate > 44100){
		printf("Invalid Bitrate. Defaulting to 44100.\n");
		rate = 44100;
	}
	
	//Create the audio pipeline
	createAudioPipeline("dshowaudiosrc", rate, 1, filetype, filename, (AudioData *)data);
}

void audio_stop_recording(GtkWidget * source, gpointer * data){
	//stop the audio pipeline
	stopAudioPipeline((AudioData *)data);
	printf("Stopped recording audio\n");
}

void audio_start_playback(GtkWidget * source, gpointer * data){
	//request file name
	GtkWidget *filechooserdialog = gtk_file_chooser_dialog_new("FileChooserDialog", NULL, GTK_FILE_CHOOSER_ACTION_OPEN, GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, GTK_STOCK_OPEN, GTK_RESPONSE_OK, NULL);
	gtk_dialog_run(GTK_DIALOG(filechooserdialog));
    char* filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(filechooserdialog));
	gtk_widget_destroy(filechooserdialog);
	printf("Enter the desired filename (include extensions): ");
	//char filename[256];
	//fgets(filename, 256, stdin);
	//filename[strlen(filename) - 1] = '\0';
	printf("%s\n", filename);
	char * filetype = getFileType(filename);
	if(filetype == NULL){
		return;
	}

	//Create the audio pipeline
	createAudioPipeline(filename, 44100, 1, filetype, "directsoundsink", (AudioData *)data);
}

void audio_stop_playback(GtkWidget * source, gpointer * data){
	//Stop the audio pipeline
	stopAudioPipeline((AudioData *)data);
	printf("Stopped playing audio\n");
}
