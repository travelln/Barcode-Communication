#include "linkmod_console.h"

#include <stdio.h>
#include <string.h>

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
	const char *foo;
} LinkmodConsole;

bool _linkmod_console_tx_thread_start(LongThread *long_thread, void *data);
bool _linkmod_console_tx_thread_stop(LongThread *long_thread, void *data);
LongThreadResult _linkmod_console_tx_thread_loop(LongThread *long_thread, void *data);

/**
 * This should check for available hardware and only return true if this
 * linkmod will work as expected. PigeonLink will select the first linkmod
 * that says it is available.
 */
bool linkmod_console_tx_is_available() {
	return true;
}

/**
 * Create a LinkmodConsole struct for our own use, and return it as a pointer
 * to PigeonLinkmod. It is important that we fill in the public long_thread
 * field - the caller expects us to create one. (This isn't ideal, but it was
 * a lot less work and I should probably go to sleep at some point).
 */
PigeonLinkmod *linkmod_console_tx_new() {
	LinkmodConsole *linkmod_console = malloc(sizeof(LinkmodConsole));
	memset(linkmod_console, 0, sizeof(*linkmod_console));
	linkmod_console->public.long_thread = long_thread_new((LongThreadOptions){
		.name="linkmod-console-tx",
		.start_fn=_linkmod_console_tx_thread_start,
		.stop_fn=_linkmod_console_tx_thread_stop,
		.loop_fn=_linkmod_console_tx_thread_loop,
		.data=linkmod_console
	});
	return (PigeonLinkmod *)linkmod_console;
}

/**
 * Free all the data we created in our constructor, including the LongThread
 * that we created.
 */
void linkmod_console_tx_free(PigeonLinkmod *linkmod) {
	LinkmodConsole *linkmod_console = (LinkmodConsole *)linkmod;
	long_thread_free(linkmod_console->public.long_thread);
	free(linkmod_console);
}

bool _linkmod_console_tx_thread_start(LongThread *long_thread, void *data) {
	LinkmodConsole *linkmod_console = (LinkmodConsole *)data;

	bool error = false;

	if (!error) {
		// Any expensive initialization. (Open files, etc.).
		linkmod_console->foo = "bar";
	}

	return !error;
}

bool _linkmod_console_tx_thread_stop(LongThread *long_thread, void *data) {
	LinkmodConsole *linkmod_console = (LinkmodConsole *)data;

	bool error = false;

	if (!error) {
		// Close files opened in _linkmod_console_tx_thread_start.
		linkmod_console->foo = NULL;
	}

	return !error;
}

/**
 * Main loop callback for our LongThread. Avoid doing anything too expensive
 * here. If we need to do some initialization, add a start_fn and stop_fn
 * callback when we create our LongThread in the constructor.
 */
LongThreadResult _linkmod_console_tx_thread_loop(LongThread *long_thread, void *data) {
	LinkmodConsole *linkmod_console = (LinkmodConsole *)data;
	PigeonLink *pigeon_link = linkmod_console->public.pigeon_link;

	PigeonFrame *pigeon_frame = pigeon_link_frames_pop(pigeon_link);

	if (pigeon_frame) {
		fprintf(stderr, "linkmod-console-tx: Got next frame\n");
		pigeon_frame_print_header(pigeon_frame);
		pigeon_frame_print_data(pigeon_frame);
		pigeon_frame_free(pigeon_frame);
	}

	return LONG_THREAD_CONTINUE;
}
