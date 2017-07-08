#include "linkmod_camera.h"

#include <stdio.h>
#include <string.h>
#include "../barcode_decoder.h"
#include "../pigeon_ui.h"
#include "../beagle_joystick.h"
#include "../util.h"
#include <time.h>
//#include "../debounce.h"


/**
 * A linkmod porovides an abstract interface for our modem to send and receive
 * data. This is necessary because the same data can be carried over a variety
 * of mediums. Linkmods are specified, in order, in pigeon_linkmods__list.h.
 * Every linkmod can be either a transmit (TX) or receive (RX) module. It has
 * a name, a function that says whether it should be used (is_available_fn), a
 * constructor (new_fn), and a destructor (free_fn).
 */

/**
 * Data used in this module, as well as public data that is shared with the
 * rest of the application. Every linkmod should create a struct with a
 * PigeonLinkmod as its first parameter.
 */
typedef struct {
	PigeonLinkmod public;
	BeagleJoystick *beagle_joystick;
} LinkmodCamera;

#define CAMERA_ENABLED_VAR_NAME "PIGEOND_CAMERA_RX"

bool _linkmod_camera_rx_thread_start(LongThread *long_thread, void *data);
bool _linkmod_camera_rx_thread_stop(LongThread *long_thread, void *data);
LongThreadResult _linkmod_camera_rx_thread_loop(LongThread *long_thread, void *data);

static struct timespec INPUT_POLL_DELAY = {
	.tv_sec=0,
	.tv_nsec=50 * MILLISECONDS_IN_NANOSECONDS
};

/**
 * This should check for available hardware and only return true if this
 * linkmod will work as expected. PigeonLink will select the first linkmod
 * that says it is available.
 */
bool linkmod_camera_tx_is_available() {
	return getenv(CAMERA_ENABLED_VAR_NAME) != NULL;
}

/**
 * This should check for available hardware and only return true if this
 * linkmod will work as expected. PigeonLink will select the first linkmod
 * that says it is available.
 */
bool linkmod_camera_rx_is_available() {
	return getenv(CAMERA_ENABLED_VAR_NAME) != NULL;
}

/**
 * Create a LinkmodCamera struct for our own use, and return it as a pointer
 * to PigeonLinkmod. It is important that we fill in the public long_thread
 * field - the caller expects us to create one. (This isn't ideal, but it was
 * a lot less work and I should probably go to sleep at some point).
 */
PigeonLinkmod *linkmod_camera_rx_new() {
	LinkmodCamera *linkmod_camera = malloc(sizeof(LinkmodCamera));
	memset(linkmod_camera, 0, sizeof(*linkmod_camera));
	linkmod_camera->public.long_thread = long_thread_new((LongThreadOptions){
		.name="linkmod-camera-rx",
		.start_fn=_linkmod_camera_rx_thread_start,
		.stop_fn=_linkmod_camera_rx_thread_stop,
		.loop_fn=_linkmod_camera_rx_thread_loop,
		.data=linkmod_camera
	});
	return (PigeonLinkmod *)linkmod_camera;
}

/**
 * Free all the data we created in our constructor, including the LongThread
 * that we created.
 */
void linkmod_camera_rx_free(PigeonLinkmod *linkmod) {
	LinkmodCamera *linkmod_camera = (LinkmodCamera *)linkmod;
	long_thread_free(linkmod_camera->public.long_thread);
	free(linkmod_camera);
}

bool _linkmod_camera_rx_thread_start(LongThread *long_thread, void *data) {
	LinkmodCamera *linkmod_camera = (LinkmodCamera *)data;

	bool error = false;

	if (!error) {
		linkmod_camera->beagle_joystick = beagle_joystick_open();
		// Any expensive initialization. (Open files, etc.).
		//linkmod_camera->foo = "bar";
	}

	return !error;
}

bool _linkmod_camera_rx_thread_stop(LongThread *long_thread, void *data) {
	LinkmodCamera *linkmod_camera = (LinkmodCamera *)data;

	bool error = false;

	if (!error) {
		beagle_joystick_close(linkmod_camera->beagle_joystick);
		// Close files opened in _linkmod_camera_tx_thread_start.
		//linkmod_camera->foo = NULL;
	}

	return !error;
}

/**
 * Main loop callback for our LongThread. Avoid doing anything too expensive
 * here. If we need to do some initialization, add a start_fn and stop_fn
 * callback when we create our LongThread in the constructor.
 */
LongThreadResult _linkmod_camera_rx_thread_loop(LongThread *long_thread, void *data) {
	LinkmodCamera *linkmod_camera = (LinkmodCamera *)data;
	PigeonLink *pigeon_link = linkmod_camera->public.pigeon_link;
	size_t buffer_size = 0;

	unsigned char *buffer = NULL;

	BeagleJoystickVectors directions;
	directions.y = 0;

	while(buffer_size == 0) {

		while(directions.y == 0) {
			beagle_joystick_get_motion(linkmod_camera->beagle_joystick, &directions);
			nanosleep(&INPUT_POLL_DELAY, NULL);
		}

		//bar_code_read allocates memory in buffer
		buffer_size = bar_code_read(&buffer);
		if(buffer_size == 0) {
			//We got bupkiss back from the camera, let the user know they need to try again
			printf("Please take another image");
		}
	}

	PigeonFrame *pigeon_frame = pigeon_frame_new(buffer, buffer_size);
	//Pigeon frame has made a copy of buffer so it's not needed anymore
	free(buffer);

	if (pigeon_frame != NULL) {
		pigeon_link_frames_push(pigeon_link, pigeon_frame);
	}
	pigeon_ui_action(UI_ACTION_RX_SUCCESS);

	return LONG_THREAD_CONTINUE;
}
