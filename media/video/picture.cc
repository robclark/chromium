// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/video/picture.h"

namespace media {

PictureBuffer::PictureBuffer(int32 id, gfx::Size size, uint32 texture_id)
    : id_(id),
      size_(size),
      texture_id_(texture_id) {
}

Picture::Picture(int32 picture_buffer_id, int32 bitstream_buffer_id)
    : picture_buffer_id_(picture_buffer_id),
      bitstream_buffer_id_(bitstream_buffer_id),
      /* no cropping */
      crop_x_(0),
      crop_y_(0),
      crop_w_(0),
      crop_h_(0) {
}

Picture::Picture(int32 picture_buffer_id, int32 bitstream_buffer_id,
  int crop_x, int crop_y, int crop_w, int crop_h)
    : picture_buffer_id_(picture_buffer_id),
      bitstream_buffer_id_(bitstream_buffer_id),
      crop_x_(crop_x),
      crop_y_(crop_y),
      crop_w_(crop_w),
      crop_h_(crop_h) {
}

}  // namespace media
