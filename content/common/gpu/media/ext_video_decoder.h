/*
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
 */

#ifndef EXT_VIDEO_DECODER_H_
#define EXT_VIDEO_DECODER_H_

#include <stdint.h>
#include <EGL/egl.h>

/**
 * The interface from chrome to decoder.. called in it's own thread to
 * allow for synchronous decoders.  But response callbacks from the
 * decoder can come on same or different thread to allow for asynchronous
 * decoders.
 */
struct ext_decoder {
	const struct ext_decoder_funcs *funcs;
};
struct ext_decoder_funcs {

	/**
	 * Bind a texture created by chrome to some actual backing storage.
	 * Called once for each buffer requested by request_picture_buffers()
	 *
	 * @picture_buffer_id: the buffer id, passed back into reuse_picture_buffer
	 * @texture:   the GL texture id
	 * @width:     the picture width (including padding)
	 * @height:    the picture height (including padding)
	 */
	void (*bind_texture)(struct ext_decoder *decoder,
			int picture_buffer_id, uint32 texture, int width, int height);

	/**
	 * Tell the decoder that the picture identified by @id is available
	 * for re-use.
	 */
	void (*reuse_picture_buffer)(struct ext_decoder *decoder,
			int picture_buffer_id);

	/**
	 * Tell the decoder to decode the specified buffer, which may not
	 * contain an integer number of pictures, but at least contains an
	 * integer number of NALU's
	 */
	void (*decode)(struct ext_decoder *decoder, const char *buf, int sz,
			int bitstream_buffer_id);

	/**
	 * Flush and return.  Call @picture_ready() for buffers returned by the
	 * codec.  Called at end of stream.
	 */
	void (*flush)(struct ext_decoder *decoder);

	/**
	 * Flush and discard.  Do not call @picture_ready() for buffers returned
	 * by the codec, but instead hold on to them.  Called for seek.
	 */
	void (*reset)(struct ext_decoder *decoder);

	/**
	 * Destroy the codec and cleanup all resources.
	 */
	void (*destroy)(struct ext_decoder *decoder);
};

/* The client interface implemented by chrome, and called back by the
 * decoder.  These can be called either synchronously from the decoder
 * thread or asynchronously.
 */
struct ext_decoder_client {
	const struct ext_decoder_client_funcs *funcs;
};
struct ext_decoder_client_funcs {
	/* XXX hmm, do we need this?  Or maybe we just call client_->NotifyInitializeDone()
	 * automatically after the decoder is constructed..  not sure where this needs
	 * to be WRT buffer allocation, etc..
	 */
	void (*notify_initialization_done)(struct ext_decoder_client *client);

	/**
	 * Request picture buffers to be handed to the decoder.  This will
	 * assign textures and trigger @bind_texture() to be called for
	 * each of the requested buffers.
	 */
	void (*provide_picture_buffers)(struct ext_decoder_client *client,
			int num, int width, int height);

	/**
	 * Tell chrome that the specified buffer can be free'd because the
	 * decoder is no longer using it.
	 */
	void (*dismiss_picture_buffer)(struct ext_decoder_client *client,
			int picture_buffer_id);

	/**
	 * Tell chrome that the specified picture buffer is read to display.
	 */
	void (*picture_ready)(struct ext_decoder_client *client,
			int picture_buffer_id, int bitstream_buffer_id,
			int crop_x, int crop_y, int crop_w, int crop_h);

	void (*notify_end_of_bitstream_buffer)(struct ext_decoder_client *client,
			int bitstream_buffer_id);
	void (*notify_flush_done)(struct ext_decoder_client *client);
	void (*notify_reset_done)(struct ext_decoder_client *client);
	void (*notify_error)(struct ext_decoder_client *client, int error);
};

/**
 * Attempt to construct a decoder for the specified codec and profile.
 *
 * Note:  For now the codec is always h264, but including it as a parameter
 * to avoid changing the API later.
 */
extern struct ext_decoder * (*initialize_decoder)(
		EGLDisplay egl_display, EGLContext egl_context,
		int codec, int profile,
		struct ext_decoder_client *client);

#endif /* EXT_VIDEO_DECODER_H_ */
