// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_FRAME_PAINTER_H_
#define ASH_WM_FRAME_PAINTER_H_
#pragma once

#include <set>

#include "ash/ash_export.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"  // OVERRIDE
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "ui/base/animation/animation_delegate.h"
#include "ui/gfx/rect.h"
#include "ui/aura/window_observer.h"

class SkBitmap;
namespace aura {
class Window;
}
namespace gfx {
class Canvas;
class Font;
class Point;
class Size;
}
namespace ui {
class SlideAnimation;
}
namespace views {
class ImageButton;
class NonClientFrameView;
class View;
class Widget;
}

namespace ash {

// Helper class for painting window frames.  Exists to share code between
// various implementations of views::NonClientFrameView.  Canonical source of
// layout constants for Ash window frames.
class ASH_EXPORT FramePainter : public aura::WindowObserver,
                                public ui::AnimationDelegate {
 public:
  // Opacity values for the window header in various states, from 0 to 255.
  static int kActiveWindowOpacity;
  static int kInactiveWindowOpacity;
  static int kSoloWindowOpacity;

  enum HeaderMode {
    ACTIVE,
    INACTIVE
  };

  // What happens when the |size_button_| is pressed.
  enum SizeButtonBehavior {
    SIZE_BUTTON_MINIMIZES,
    SIZE_BUTTON_MAXIMIZES
  };

  FramePainter();
  virtual ~FramePainter();

  // |frame| and buttons are used for layout and are not owned.
  void Init(views::Widget* frame,
            views::View* window_icon,
            views::ImageButton* size_button,
            views::ImageButton* close_button,
            SizeButtonBehavior behavior);

  // Helpers for views::NonClientFrameView implementations.
  gfx::Rect GetBoundsForClientView(int top_height,
                                   const gfx::Rect& window_bounds) const;
  gfx::Rect GetWindowBoundsForClientBounds(
      int top_height,
      const gfx::Rect& client_bounds) const;
  int NonClientHitTest(views::NonClientFrameView* view,
                       const gfx::Point& point);
  gfx::Size GetMinimumSize(views::NonClientFrameView* view);

  // Paints the frame header.
  void PaintHeader(views::NonClientFrameView* view,
                   gfx::Canvas* canvas,
                   HeaderMode header_mode,
                   int theme_frame_id,
                   const SkBitmap* theme_frame_overlay);

  // Paints the header/content separator line.  Exists as a separate function
  // because some windows with complex headers (e.g. browsers with tab strips)
  // need to draw their own line.
  void PaintHeaderContentSeparator(views::NonClientFrameView* view,
                                   gfx::Canvas* canvas);

  // Returns size of the header/content separator line in pixels.
  int HeaderContentSeparatorSize() const;

  // Paint the title bar, primarily the title string.
  void PaintTitleBar(views::NonClientFrameView* view,
                     gfx::Canvas* canvas,
                     const gfx::Font& title_font);

  // Performs layout for the header based on whether we want the shorter
  // |maximized_layout| appearance.
  void LayoutHeader(views::NonClientFrameView* view, bool maximized_layout);

  // aura::WindowObserver overrides:
  virtual void OnWindowPropertyChanged(aura::Window* window,
                                       const void* key,
                                       intptr_t old) OVERRIDE;
  virtual void OnWindowVisibilityChanged(aura::Window* window,
                                         bool visible) OVERRIDE;
  virtual void OnWindowDestroying(aura::Window* window) OVERRIDE;

  // Overridden from ui::AnimationDelegate
  virtual void AnimationProgressed(const ui::Animation* animation) OVERRIDE;

 private:
  FRIEND_TEST_ALL_PREFIXES(FramePainterTest, Basics);
  FRIEND_TEST_ALL_PREFIXES(FramePainterTest, UseSoloWindowHeader);
  FRIEND_TEST_ALL_PREFIXES(FramePainterTest, GetHeaderOpacity);

  // Sets the images for a button base on IDs from the |frame_| theme provider.
  void SetButtonImages(views::ImageButton* button,
                       int normal_bitmap_id,
                       int hot_bitmap_id,
                       int pushed_bitmap_id);

  // Returns the offset between window left edge and title string.
  int GetTitleOffsetX() const;

  // Returns the opacity value used to paint the header.
  int GetHeaderOpacity(HeaderMode header_mode,
                       int theme_frame_id,
                       const SkBitmap* theme_frame_overlay);

  // Returns true if there is exactly one visible, normal-type window using
  // a header painted by this class, in which case we should paint a transparent
  // window header.
  static bool UseSoloWindowHeader();

  // Schedules a paint for the window header of the solo window.  Invoke this
  // when another window is hidden or destroyed to force the transparency of
  // the now-solo window to update.
  static void SchedulePaintForSoloWindow();

  static std::set<FramePainter*>* instances_;

  // Not owned
  views::Widget* frame_;
  views::View* window_icon_;  // May be NULL.
  views::ImageButton* size_button_;
  views::ImageButton* close_button_;
  aura::Window* window_;

  // Window frame header/caption parts.
  const SkBitmap* button_separator_;
  const SkBitmap* top_left_corner_;
  const SkBitmap* top_edge_;
  const SkBitmap* top_right_corner_;
  const SkBitmap* header_left_edge_;
  const SkBitmap* header_right_edge_;

  // Bitmap id and opacity last used for painting header.
  int previous_theme_frame_id_;
  int previous_opacity_;

  // Bitmap id and opacity we are crossfading from.
  int crossfade_theme_frame_id_;
  int crossfade_opacity_;

  gfx::Rect header_frame_bounds_;
  scoped_ptr<ui::SlideAnimation> crossfade_animation_;

  SizeButtonBehavior size_button_behavior_;

  DISALLOW_COPY_AND_ASSIGN(FramePainter);
};

}  // namespace ash

#endif  // ASH_WM_FRAME_PAINTER_H_
