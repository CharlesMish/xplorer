// Copyright 2026 The Xplorer Authors.
// Use of this source code is governed by a BSD-style license.

#include "chrome/browser/ui/views/xplorer/xplorer_sidebar_section_label.h"

#include "chrome/browser/ui/color/chrome_color_id.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/font.h"
#include "ui/views/controls/label.h"

namespace xplorer {

namespace {
constexpr int kSectionHeight = 18;
}  // namespace

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
  SetEnabledColor(colors->GetColor(kColorTabForegroundInactiveFrameInactive));
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