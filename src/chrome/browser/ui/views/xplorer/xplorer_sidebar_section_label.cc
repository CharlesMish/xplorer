// Copyright 2026 The Xplorer Authors.
// Use of this source code is governed by a BSD-style license.

#include "chrome/browser/ui/views/xplorer/xplorer_sidebar_section_label.h"

#include "build/build_config.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/font.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/controls/label.h"

namespace xplorer {

namespace {
constexpr int kSectionHeight = 18;
}  // namespace

int g_sample_x = 0;
int g_sample_y = 0;
int g_sample_w = 0;
int g_sample_h = 0;

void NoteSidebarSampleBounds(const gfx::Rect& screen_bounds) {
  g_sample_x = screen_bounds.x();
  g_sample_y = screen_bounds.y();
  g_sample_w = screen_bounds.width();
  g_sample_h = screen_bounds.height();
}

bool SidebarBackdropIsDark() {
  // Follow the system appearance. Sampling the pixels behind the window
  // needed Screen Recording permission and ran on every tab repaint.
  return ui::NativeTheme::GetInstanceForNativeUi()->preferred_color_scheme() ==
         ui::NativeTheme::PreferredColorScheme::kDark;
}

SkColor SidebarInkColor() {
  // Near-black on the light frost. White only when the backdrop is actually dark.
  return SidebarBackdropIsDark() ? SK_ColorWHITE
                                 : SkColorSetRGB(0x14, 0x14, 0x16);
}

XplorerSidebarSectionLabel::XplorerSidebarSectionLabel(
    const std::u16string& text)
    : views::Label(text) {
  SetHorizontalAlignment(gfx::ALIGN_LEFT);
  // Arc section labels are small, medium, and quiet. Not a second title.
  SetFontList(font_list()
                  .DeriveWithSizeDelta(-3)
                  .DeriveWithWeight(gfx::Font::Weight::MEDIUM));
}

void XplorerSidebarSectionLabel::OnThemeChanged() {
  views::Label::OnThemeChanged();
  const ui::ColorProvider* colors = GetColorProvider();
  if (!colors) {
    return;
  }
  SetEnabledColor(SidebarInkColor());
  SetBackgroundColor(SK_ColorTRANSPARENT);
}

XplorerSidebarSectionLabel::~XplorerSidebarSectionLabel() = default;

gfx::Size XplorerSidebarSectionLabel::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  gfx::Size size = Label::CalculatePreferredSize(available_size);
  size.set_height(kSectionHeight);
  return size;
}

BEGIN_METADATA(XplorerSidebarSectionLabel)
END_METADATA

}  // namespace xplorer