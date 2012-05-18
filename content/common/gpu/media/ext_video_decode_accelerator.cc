// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/media/ext_video_decode_accelerator.h"
#include "base/debug/trace_event.h"

void *handle = dlopen("libvideodecode.so", RTLD_NOW);

typedef struct ext_decoder * (*initialize_decoder_t)(
		EGLDisplay egl_display, EGLContext egl_context,
		int codec, int profile,
		struct ext_decoder_client *client);
initialize_decoder_t initialize_decoder =
		reinterpret_cast<initialize_decoder_t>(dlsym(handle, "initialize_decoder"));

struct ext_decoder_client_impl {
	struct ext_decoder_client base;
	ExtVideoDecodeAccelerator *self;
};

#define RETURN_ON_FAILURE(result, log, error, ret_val)             \
  do {                                                             \
    if (!(result)) {                                               \
      DLOG(ERROR) << log;                                          \
      StopOnError(error);                                          \
      return ret_val;                                              \
    }                                                              \
  } while (0)

ExtVideoDecodeAccelerator::ExtVideoDecodeAccelerator(
		media::VideoDecodeAccelerator::Client* client,
		EGLDisplay egl_display,
		EGLContext egl_context) :
	egl_display_(egl_display),
	egl_context_(egl_context),
	client_(client),
	message_loop_(MessageLoop::current()),
	decoder_thread_("ExternalDecoderThread")
{
}

ExtVideoDecodeAccelerator::~ExtVideoDecodeAccelerator()
{
	DCHECK_EQ(message_loop_, MessageLoop::current());
}

void ExtVideoDecodeAccelerator::StopOnError(
		media::VideoDecodeAccelerator::Error error)
{
	DCHECK_EQ(message_loop_, MessageLoop::current());

	if (client_)
		client_->NotifyError(error);
	client_ = NULL;

	// XXX cleanup engine/codec instance..
}

/*
 * VDA API:
 */

bool ExtVideoDecodeAccelerator::Initialize(media::VideoCodecProfile profile)
{
	static const struct ext_decoder_client_funcs funcs = {
			&ExtVideoDecodeAccelerator::NotifyInitializationDone,
			&ExtVideoDecodeAccelerator::ProvidePictureBuffers,
			&ExtVideoDecodeAccelerator::DismissPictureBuffer,
			&ExtVideoDecodeAccelerator::PictureReady,
			&ExtVideoDecodeAccelerator::NotifyEndOfBitstreamBuffer,
			&ExtVideoDecodeAccelerator::NotifyFlushDone,
			&ExtVideoDecodeAccelerator::NotifyResetDone,
			&ExtVideoDecodeAccelerator::NotifyError,
	};
	struct ext_decoder_client_impl *client_impl =
			(struct ext_decoder_client_impl *)calloc(1, sizeof(*client_impl));

	DCHECK_EQ(message_loop_, MessageLoop::current());

	client_impl->base.funcs = &funcs;
	client_impl->self = this;

	decoder_client_ = (struct ext_decoder_client *)client_impl;

	decoder_ = initialize_decoder(egl_display_, egl_context_,
			media::kCodecH264, profile, decoder_client_);

	return true;
}

void ExtVideoDecodeAccelerator::DecodeTask(
		const media::BitstreamBuffer& bitstream_buffer)
{
	scoped_ptr<base::SharedMemory> shm(
			new base::SharedMemory(bitstream_buffer.handle(), true));
	RETURN_ON_FAILURE(shm->Map(bitstream_buffer.size()),
			"Failed to SharedMemory::Map()", UNREADABLE_INPUT,);
	// XXX after this fxn returns, shm->memory is no longer valid.. this
	// is ok for a synchronous decoder, but for the benefit of a asynch
	// decoder we should probably keep the SharedMemory or document that
	// the passed input buffer is only valid until this call returns..
	// ie. if the external decoder copies the bitstream into the decoders
	// input buffer, and then kicks an asynchronous decode, that is fine.
	decoder_->funcs->decode(decoder_, (const char *)shm->memory(),
			bitstream_buffer.size(), bitstream_buffer.id());
}

void ExtVideoDecodeAccelerator::Decode(
		const media::BitstreamBuffer& bitstream_buffer)
{
	DCHECK_EQ(message_loop_, MessageLoop::current());

	TRACE_EVENT1("Video Decoder", "ExtVideoDecodeAccelerator::Decode", "Buffer id",
			bitstream_buffer.id());

	decoder_thread_.message_loop()->PostTask(FROM_HERE,
			base::Bind(&ExtVideoDecodeAccelerator::DecodeTask, this, bitstream_buffer));
}

void ExtVideoDecodeAccelerator::BindTextureTask(int picture_buffer_id,
		uint32_t texture, int width, int height)
{
	decoder_->funcs->bind_texture(decoder_, picture_buffer_id, texture,
			width, height);
}

void ExtVideoDecodeAccelerator::AssignPictureBuffers(
		const std::vector<media::PictureBuffer>& buffers)
{
	for (size_t i = 0; i < buffers.size(); ++i) {
		decoder_thread_.message_loop()->PostTask(FROM_HERE,
				base::Bind(&ExtVideoDecodeAccelerator::BindTextureTask, this,
						buffers[i].id(), buffers[i].texture_id(),
						buffers[i].size().width(), buffers[i].size().height()));
	}
}

void ExtVideoDecodeAccelerator::ReusePictureBufferTask(
		int32 picture_buffer_id)
{
	decoder_->funcs->reuse_picture_buffer(decoder_, picture_buffer_id);
}

void ExtVideoDecodeAccelerator::ReusePictureBuffer(
		int32 picture_buffer_id)
{
	TRACE_EVENT1("Video Decoder", "OVDA::ReusePictureBuffer",
			"Picture id", picture_buffer_id);
	decoder_thread_.message_loop()->PostTask(FROM_HERE,
			base::Bind(&ExtVideoDecodeAccelerator::ReusePictureBufferTask,
					this, picture_buffer_id));
}

void ExtVideoDecodeAccelerator::FlushTask()
{
	decoder_->funcs->flush(decoder_);
}

void ExtVideoDecodeAccelerator::Flush()
{
	DCHECK_EQ(message_loop_, MessageLoop::current());
	DVLOG(1) << "Got flush request";
	decoder_thread_.message_loop()->PostTask(FROM_HERE,
			base::Bind(&ExtVideoDecodeAccelerator::FlushTask, this));
}

void ExtVideoDecodeAccelerator::ResetTask()
{
	decoder_->funcs->reset(decoder_);
}

void ExtVideoDecodeAccelerator::Reset()
{
	DCHECK_EQ(message_loop_, MessageLoop::current());
	DVLOG(1) << "Got reset request";
	decoder_thread_.message_loop()->PostTask(FROM_HERE,
			base::Bind(&ExtVideoDecodeAccelerator::ResetTask, this));
}

void ExtVideoDecodeAccelerator::Destroy()
{
	// XXX PostTask?
	// This one probably shouldn't be async.. or at least should block
	// until it returns, because the ExtVideoDecodeAccelerator will
	// be deleted when this method returns..
	decoder_->funcs->destroy(decoder_);
}

// decoder client callbacks:

void ExtVideoDecodeAccelerator::NotifyInitializationDoneTask()
{
	client_->NotifyInitializeDone();
}

void ExtVideoDecodeAccelerator::NotifyInitializationDone2()
{
	message_loop_->PostTask(FROM_HERE,
			base::Bind(&ExtVideoDecodeAccelerator::NotifyInitializationDoneTask,
					this));
}

void ExtVideoDecodeAccelerator::NotifyInitializationDone(
		struct ext_decoder_client *client)
{
	struct ext_decoder_client_impl *client_impl =
			(struct ext_decoder_client_impl *)client;
	client_impl->self->NotifyInitializationDoneTask();
}

void ExtVideoDecodeAccelerator::ProvidePictureBuffersTask(
		int num, int width, int height)
{
	client_->ProvidePictureBuffers(num, gfx::Size(width, height));
}

void ExtVideoDecodeAccelerator::ProvidePictureBuffers2(
		int num, int width, int height)
{
	message_loop_->PostTask(FROM_HERE,
			base::Bind(&ExtVideoDecodeAccelerator::ProvidePictureBuffersTask,
					this, num, width, height));
}

void ExtVideoDecodeAccelerator::ProvidePictureBuffers(
		struct ext_decoder_client *client,
		int num, int width, int height)
{
	struct ext_decoder_client_impl *client_impl =
			(struct ext_decoder_client_impl *)client;
	client_impl->self->ProvidePictureBuffers2(num, width, height);
}

void ExtVideoDecodeAccelerator::DismissPictureBufferTask(
		int picture_buffer_id)
{
	client_->DismissPictureBuffer(picture_buffer_id);
}

void ExtVideoDecodeAccelerator::DismissPictureBuffer2(
		int picture_buffer_id)
{
	message_loop_->PostTask(FROM_HERE,
			base::Bind(&ExtVideoDecodeAccelerator::DismissPictureBufferTask,
					this, picture_buffer_id));
}

void ExtVideoDecodeAccelerator::DismissPictureBuffer(
		struct ext_decoder_client *client, int picture_buffer_id)
{
	struct ext_decoder_client_impl *client_impl =
			(struct ext_decoder_client_impl *)client;
	client_impl->self->DismissPictureBuffer2(picture_buffer_id);
}

void ExtVideoDecodeAccelerator::PictureReadyTask(int picture_buffer_id,
		int bitstream_buffer_id, int crop_x, int crop_y, int crop_w, int crop_h)
{
	media::Picture picture(picture_buffer_id, bitstream_buffer_id,
			crop_x, crop_y, crop_w, crop_h);
	client_->PictureReady(picture);
}

void ExtVideoDecodeAccelerator::PictureReady2(int picture_buffer_id,
		int bitstream_buffer_id, int crop_x, int crop_y, int crop_w, int crop_h)
{
	message_loop_->PostTask(FROM_HERE,
			base::Bind(&ExtVideoDecodeAccelerator::PictureReadyTask,
					this, picture_buffer_id, bitstream_buffer_id,
					crop_x, crop_y, crop_w, crop_h));
}

void ExtVideoDecodeAccelerator::PictureReady(
		struct ext_decoder_client *client,
		int picture_buffer_id, int bitstream_buffer_id,
		int crop_x, int crop_y, int crop_w, int crop_h)
{
	struct ext_decoder_client_impl *client_impl =
			(struct ext_decoder_client_impl *)client;
	client_impl->self->PictureReady2(picture_buffer_id, bitstream_buffer_id,
			crop_x, crop_y, crop_w, crop_h);
}

void ExtVideoDecodeAccelerator::NotifyEndOfBitstreamBufferTask(
		int bitstream_buffer_id)
{
	client_->NotifyEndOfBitstreamBuffer(bitstream_buffer_id);
}

void ExtVideoDecodeAccelerator::NotifyEndOfBitstreamBuffer2(
		int bitstream_buffer_id)
{
	message_loop_->PostTask(FROM_HERE,
			base::Bind(&ExtVideoDecodeAccelerator::NotifyEndOfBitstreamBufferTask,
					this, bitstream_buffer_id));
}

void ExtVideoDecodeAccelerator::NotifyEndOfBitstreamBuffer(
		struct ext_decoder_client *client, int bitstream_buffer_id)
{
	struct ext_decoder_client_impl *client_impl =
			(struct ext_decoder_client_impl *)client;
	client_impl->self->NotifyEndOfBitstreamBuffer2(bitstream_buffer_id);
}

void ExtVideoDecodeAccelerator::NotifyFlushDoneTask()
{
	client_->NotifyFlushDone();
}

void ExtVideoDecodeAccelerator::NotifyFlushDone2()
{
	message_loop_->PostTask(FROM_HERE,
			base::Bind(&ExtVideoDecodeAccelerator::NotifyFlushDoneTask,
					this));
}

void ExtVideoDecodeAccelerator::NotifyFlushDone(
		struct ext_decoder_client *client)
{
	struct ext_decoder_client_impl *client_impl =
			(struct ext_decoder_client_impl *)client;
	client_impl->self->NotifyFlushDone2();
}

void ExtVideoDecodeAccelerator::NotifyResetDoneTask()
{
	client_->NotifyResetDone();
}

void ExtVideoDecodeAccelerator::NotifyResetDone2()
{
	message_loop_->PostTask(FROM_HERE,
			base::Bind(&ExtVideoDecodeAccelerator::NotifyResetDoneTask,
					this));
}

void ExtVideoDecodeAccelerator::NotifyResetDone(
		struct ext_decoder_client *client)
{
	struct ext_decoder_client_impl *client_impl =
			(struct ext_decoder_client_impl *)client;
	client_impl->self->NotifyResetDone2();
}

void ExtVideoDecodeAccelerator::NotifyErrorTask(int error)
{
	client_->NotifyError((Error)error);
}

void ExtVideoDecodeAccelerator::NotifyError2(int error)
{
	message_loop_->PostTask(FROM_HERE,
			base::Bind(&ExtVideoDecodeAccelerator::NotifyErrorTask,
					this, error));
}

void ExtVideoDecodeAccelerator::NotifyError(
		struct ext_decoder_client *client, int error)
{
	struct ext_decoder_client_impl *client_impl =
			(struct ext_decoder_client_impl *)client;
	client_impl->self->NotifyError2(error);
}
