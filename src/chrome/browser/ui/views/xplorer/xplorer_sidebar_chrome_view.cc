// Copyright 2026 The Xplorer Authors.
// Use of this source code is governed by a BSD-style license.

#include "chrome/browser/ui/views/xplorer/xplorer_sidebar_chrome_view.h"

#include <memory>

#include "chrome/browser/ui/views/xplorer/xplorer_sidebar_section_label.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/font.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view_class_properties.h"

namespace xplorer {

namespace {
constexpr gfx::Insets kHeaderMargins = gfx::Insets::TLBR(10, 6, 2, 6);
constexpr gfx::Insets kSectionLabelMargins = gfx::Insets::TLBR(14, 8, 2, 8);
constexpr SkColor kSpaceDot = SkColorSetRGB(0x3D, 0x7E, 0xFF);

}  // namespace

XplorerSidebarChromeView::XplorerSidebarChromeView(
    BrowserWindowInterface* browser,
    Profile* profile)
    : browser_(browser), profile_(profile) {
  SetBackground(nullptr);
  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStretch);

  // Arc/Dia: the sidebar opens with the space name, not a toolbar.
  auto* header = AddChildView(std::make_unique<views::View>());
  auto* header_layout =
      header->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal,
          gfx::Insets::VH(6, 6), 8));
  header_layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  header->SetProperty(views::kMarginsKey, kHeaderMargins);
  // A views::View subclass here must carry METADATA_HEADER or AddChildView
  // fails to compile. A tinted bullet is the same Arc space swatch.
  auto* dot = header->AddChildView(std::make_unique<views::Label>(u"●"));
  dot->SetEnabledColor(kSpaceDot);
  dot->SetFontList(dot->font_list().DeriveWithSizeDelta(-2));
  auto* title = header->AddChildView(std::make_unique<views::Label>(u"Xplor"));
  title->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  title->SetFontList(title->font_list().DeriveWithSizeDelta(1).DeriveWithWeight(
      gfx::Font::Weight::SEMIBOLD));

  // Unpinned tabs live under a quiet "Today" label, the Arc word for the
  // working set. Bookmarks stay a native tab group above this.
  auto* tabs_label =
      AddChildView(std::make_unique<XplorerSidebarSectionLabel>(u"Today"));
  tabs_label->SetProperty(views::kMarginsKey, kSectionLabelMargins);
}

XplorerSidebarChromeView::~XplorerSidebarChromeView() = default;

BEGIN_METADATA(XplorerSidebarChromeView)
END_METADATA

}  // namespace xplorer
