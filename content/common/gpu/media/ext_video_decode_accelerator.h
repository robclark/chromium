// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/*
 * ext_video_decode_accelerator.h
 *
 * A shim interface for an external hw video decode accelerator.  This
 * essentially gives a plugin interface for decoders, to (a) make it
 * easier to develop decoder support without having to rebuild all of
 * chrome, and (b) avoid a proliferation of different decoder APIs
 * that must be supported by chrome.
 *
 *  Created on: May 15, 2012
 *      Author: robclark
 */

#ifndef CONTENT_COMMON_GPU_MEDIA_EXT_VIDEO_DECODE_ACCELERATOR_H_
#define CONTENT_COMMON_GPU_MEDIA_EXT_VIDEO_DECODE_ACCELERATOR_H_

#include <dlfcn.h>
#include <map>
#include <queue>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop.h"
#include "base/shared_memory.h"
#include "base/threading/thread.h"
#include "media/video/video_decode_accelerator.h"
#include "media/base/bitstream_buffer.h"
#include "media/base/video_decoder_config.h"
#include "media/video/picture.h"
#include "third_party/angle/include/EGL/egl.h"
#include "third_party/angle/include/EGL/eglext.h"

extern "C" {
#include "content/common/gpu/media/ext_video_decoder.h"
};


class ExtVideoDecodeAccelerator : public media::VideoDecodeAccelerator {
public:
	// Does not take ownership of |client| which must outlive |*this|.
	ExtVideoDecodeAccelerator(media::VideoDecodeAccelerator::Client* client,
			EGLDisplay egl_display, EGLContext egl_context);

	virtual bool Initialize(media::VideoCodecProfile profile);
	virtual void Decode(const media::BitstreamBuffer& bitstream_buffer);
	virtual void AssignPictureBuffers(
			const std::vector<media::PictureBuffer>& buffers);
	virtual void ReusePictureBuffer(int32 picture_buffer_id);
	virtual void Flush();
	virtual void Reset();
	virtual void Destroy();

private:
	virtual ~ExtVideoDecodeAccelerator();

	// variants of public decoder API that are called from the context
	// of the decoder_thread_
	// NOTE: currently only these are shunted off to other task.. other
	// decoder operations shouldn't really take long enough to worry
	// about this, but maybe we should think about redirecting those to
	// the decoder_thread_ too just for the purpose of keeping things
	// single threaded from the perspective of the decoder??
	void DecodeTask(const media::BitstreamBuffer& bitstream_buffer);
	void BindTextureTask(int picture_buffer_id, uint32_t texture,
			int width, int height);
	void ReusePictureBufferTask(int32 picture_buffer_id);
	void FlushTask();
	void ResetTask();

	// Stop the component when any error is detected.
	void StopOnError(media::VideoDecodeAccelerator::Error error);

	EGLDisplay egl_display_;
	EGLContext egl_context_;

	// To expose client callbacks from VideoDecodeAccelerator.
	// NOTE: all calls to this object *MUST* be executed in message_loop_.
	Client* client_;

	MessageLoop* message_loop_;

	// decoder thread.. all the calls to the decoder are done from the
	// context of the message-loop of this thread
	base::Thread decoder_thread_;

	// the external decoder instance:
	struct ext_decoder *decoder_;

	// the client interface handed to the decoder:
	struct ext_decoder_client *decoder_client_;

	// client callbacks for ext_decoder_client_funcs:
	static void NotifyInitializationDone(struct ext_decoder_client *client);
	static void ProvidePictureBuffers(struct ext_decoder_client *client,
			int num, int width, int height);
	static void DismissPictureBuffer(struct ext_decoder_client *client,
			int picture_buffer_id);
	static void PictureReady(struct ext_decoder_client *client,
			int picture_buffer_id, int bitstream_buffer_id,
			int crop_x, int crop_y, int crop_w, int crop_h);
	static void NotifyEndOfBitstreamBuffer(struct ext_decoder_client *client,
			int bitstream_buffer_id);
	static void NotifyFlushDone(struct ext_decoder_client *client);
	static void NotifyResetDone(struct ext_decoder_client *client);
	static void NotifyError(struct ext_decoder_client *client, int error);

	// callbacks marshaled back to message_loop task
	void NotifyInitializationDone2();
	void NotifyInitializationDoneTask();
	void ProvidePictureBuffers2(int num, int width, int height);
	void ProvidePictureBuffersTask(int num, int width, int height);
	void DismissPictureBuffer2(int picture_buffer_id);
	void DismissPictureBufferTask(int picture_buffer_id);
	void PictureReady2(int picture_buffer_id, int bitstream_buffer_id,
			int crop_x, int crop_y, int crop_w, int crop_h);
	void PictureReadyTask(int picture_buffer_id, int bitstream_buffer_id,
			int crop_x, int crop_y, int crop_w, int crop_h);
	void NotifyEndOfBitstreamBuffer2(int bitstream_buffer_id);
	void NotifyEndOfBitstreamBufferTask(int bitstream_buffer_id);
	void NotifyFlushDone2();
	void NotifyFlushDoneTask();
	void NotifyResetDone2();
	void NotifyResetDoneTask();
	void NotifyError2(int error);
	void NotifyErrorTask(int error);

};


#endif /* CONTENT_COMMON_GPU_MEDIA_EXT_VIDEO_DECODE_ACCELERATOR_H_ */
