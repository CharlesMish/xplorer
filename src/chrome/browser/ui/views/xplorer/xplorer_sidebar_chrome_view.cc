// Copyright 2026 The Xplorer Authors.
// Use of this source code is governed by a BSD-style license.

#include "chrome/browser/ui/views/xplorer/xplorer_sidebar_chrome_view.h"

#include "chrome/browser/ui/views/xplorer/xplorer_sidebar_section_label.h"

#include <memory>
#include <string>

#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "cc/paint/paint_flags.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/autocomplete/autocomplete_classifier_factory.h"
#include "chrome/browser/grok_companion/grok_companion_util.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/navigator/browser_navigator.h"
#include "chrome/browser/ui/navigator/browser_navigator_params.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/omnibox/browser/autocomplete_classifier.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/url_formatter/elide_url.h"
#include "components/url_formatter/url_formatter.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

namespace xplorer {

namespace {

constexpr gfx::Insets kHeaderMargins = gfx::Insets::TLBR(8, 8, 2, 8);
constexpr gfx::Insets kSectionLabelMargins = gfx::Insets::TLBR(14, 8, 2, 8);
constexpr int kSpaceSwatchSize = 16;
constexpr int kPinSize = 28;
constexpr SkColor kDefaultSwatch = SkColorSetRGB(0x7D, 0x87, 0x94);

SkColor ParseThemeColor(const std::string& hex) {
  if (hex.size() != 7 || hex[0] != '#')
    return kDefaultSwatch;
  uint32_t rgb = 0;
  if (!base::HexStringToUInt(hex.substr(1), &rgb))
    return kDefaultSwatch;
  return SkColorSetRGB((rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF);
}

bool NavigationAllowed(const GURL& url) {
  return url.is_valid() &&
         (url.SchemeIsHTTPOrHTTPS() || url.SchemeIs(content::kChromeUIScheme) ||
          url.SchemeIs(content::kChromeUIUntrustedScheme) ||
          url.SchemeIs("about") || url.SchemeIsFile());
}

std::u16string DisplayUrl(const GURL& url) {
  if (!url.is_valid() || url.IsAboutBlank() ||
      grok_companion::IsGrokHomeURL(url) ||
      url.SchemeIs(content::kChromeUIScheme)) {
    return {};
  }
  // Arc shows the domain in the sidebar. The full URL lives in the popup.
  return url_formatter::FormatUrlForDisplayOmitSchemePathAndTrivialSubdomains(
      url);
}

std::u16string FullUrl(const GURL& url) {
  if (!url.is_valid())
    return {};
  return url_formatter::FormatUrl(
      url, url_formatter::kFormatUrlOmitNothing, base::UnescapeRule::NONE,
      nullptr, nullptr, nullptr);
}

class XplorerSpaceSwatch : public views::View {
  METADATA_HEADER(XplorerSpaceSwatch, views::View)

 public:
  XplorerSpaceSwatch() {
    SetPreferredSize(gfx::Size(kSpaceSwatchSize, kSpaceSwatchSize));
  }

  void SetSwatchColor(SkColor color) {
    if (color_ == color)
      return;
    color_ = color;
    SchedulePaint();
  }

  void OnPaint(gfx::Canvas* canvas) override {
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setStyle(cc::PaintFlags::kFill_Style);
    flags.setColor(color_);
    canvas->DrawRoundRect(gfx::RectF(GetLocalBounds()), 5.f, flags);
  }

 private:
  SkColor color_ = kDefaultSwatch;
};

BEGIN_METADATA(XplorerSpaceSwatch)
END_METADATA

class XplorerUrlField : public views::Textfield {
  METADATA_HEADER(XplorerUrlField, views::Textfield)

 public:
  explicit XplorerUrlField(XplorerSidebarChromeView* owner) : owner_(owner) {}

  void OnFocus() override {
    views::Textfield::OnFocus();
    SelectAll(false);
  }

  void OnBlur() override {
    views::Textfield::OnBlur();
    if (owner_)
      owner_->OnUrlFieldBlur();
  }

 private:
  const raw_ptr<XplorerSidebarChromeView> owner_;
};

BEGIN_METADATA(XplorerUrlField)
END_METADATA

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

  pins_ = AddChildView(std::make_unique<views::View>());
  auto* pins_layout =
      pins_->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal, gfx::Insets::VH(8, 8),
          6));
  pins_layout->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kStart);
  pins_->SetVisible(false);

  auto* header = AddChildView(std::make_unique<views::View>());
  auto* header_layout =
      header->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal, gfx::Insets::VH(4, 6),
          8));
  header_layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  header->SetProperty(views::kMarginsKey, kHeaderMargins);
  space_swatch_ = header->AddChildView(std::make_unique<XplorerSpaceSwatch>());
  auto* title = header->AddChildView(std::make_unique<views::Label>(u"Xplor"));
  title->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  title->SetFontList(title->font_list().DeriveWithSizeDelta(1).DeriveWithWeight(
      gfx::Font::Weight::SEMIBOLD));

  // Back, forward, reload, then the address field. This replaces the toolbar
  // that used to sit across the top of the page.
  auto* nav = AddChildView(std::make_unique<views::View>());
  auto* nav_layout = nav->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, gfx::Insets::TLBR(2, 4, 6, 8),
      2));
  nav_layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  back_button_ = nav->AddChildView(views::ImageButton::CreateIconButton(
      base::BindRepeating(&XplorerSidebarChromeView::RunCommand,
                          base::Unretained(this), IDC_BACK),
      vector_icons::kArrowBackIcon, u"Back",
      views::ImageButton::MaterialIconStyle::kSmall));
  forward_button_ = nav->AddChildView(views::ImageButton::CreateIconButton(
      base::BindRepeating(&XplorerSidebarChromeView::RunCommand,
                          base::Unretained(this), IDC_FORWARD),
      vector_icons::kArrowForwardIcon, u"Forward",
      views::ImageButton::MaterialIconStyle::kSmall));
  reload_button_ = nav->AddChildView(views::ImageButton::CreateIconButton(
      base::BindRepeating(&XplorerSidebarChromeView::RunCommand,
                          base::Unretained(this), IDC_RELOAD),
      vector_icons::kRefreshIcon, u"Reload",
      views::ImageButton::MaterialIconStyle::kSmall));

  auto* capsule = nav->AddChildView(std::make_unique<views::View>());
  capsule->SetBackground(
      views::CreateRoundedRectBackground(ui::kColorSysHeader, 8.f));
  auto* capsule_layout =
      capsule->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal, gfx::Insets::VH(0, 10)));
  capsule_layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  nav_layout->SetFlexForView(capsule, 1);

  auto* field = capsule->AddChildView(std::make_unique<XplorerUrlField>(this));
  url_field_ = field;
  field->SetController(this);
  field->SetReadOnly(true);
  field->SetPlaceholderText(u"Search or Enter URL");
  field->SetBorder(views::CreateEmptyBorder(gfx::Insets()));
  field->SetBackgroundColor(SK_ColorTRANSPARENT);
  field->SetTextColorId(ui::kColorSysOnSurface);
  field->SetAccessibleName(u"Search or Enter URL");
  capsule_layout->SetFlexForView(field, 1);

  auto* tabs_label =
      AddChildView(std::make_unique<XplorerSidebarSectionLabel>(u"Today"));
  tabs_label->SetProperty(views::kMarginsKey, kSectionLabelMargins);

  if (browser_ && browser_->GetTabStripModel()) {
    browser_->GetTabStripModel()->AddObserver(this);
    observing_tabs_ = true;
  }
  onboarding_subscription_ = grok_companion::AddOnboardingChangedCallback(
      base::BindRepeating(&XplorerSidebarChromeView::ReloadChrome,
                          base::Unretained(this)));
  // Traffic lights sit in the top of the sidebar. Keep the address row below
  // them, then pinned apps, then the space name.
  auto* spacer = AddChildView(std::make_unique<views::View>());
  spacer->SetPreferredSize(gfx::Size(1, 28));
  ReorderChildView(spacer, 0);
  if (nav)
    ReorderChildView(nav, 1);
  if (pins_)
    ReorderChildView(pins_, 2);

  ReloadChrome();
  UpdateUrlField();
  UpdateNavButtons();
}

XplorerSidebarChromeView::~XplorerSidebarChromeView() {
  if (observing_tabs_ && browser_ && browser_->GetTabStripModel())
    browser_->GetTabStripModel()->RemoveObserver(this);
}

void XplorerSidebarChromeView::FocusUrlField() {
  ShowUrlPopup();
}

void XplorerSidebarChromeView::OnUrlFieldBlur() {
  UpdateUrlField();
}

bool XplorerSidebarChromeView::HandleKeyEvent(
    views::Textfield* sender,
    const ui::KeyEvent& key_event) {
  if (sender != url_field_ || key_event.type() != ui::EventType::kKeyPressed)
    return false;
  if (key_event.key_code() == ui::VKEY_RETURN) {
    if (sender == popup_field_ && popup_field_)
      NavigateFromText(std::u16string(popup_field_->GetText()));
    else
      NavigateFromField();
    CloseUrlPopup();
    return true;
  }
  if (key_event.key_code() == ui::VKEY_ESCAPE) {
    UpdateUrlField();
    if (auto* focus = url_field_->GetFocusManager())
      focus->ClearFocus();
    return true;
  }
  return false;
}

void XplorerSidebarChromeView::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  if (selection.active_tab_changed())
    UpdateUrlField();
  UpdateNavButtons();
}

void XplorerSidebarChromeView::OnTabChangedAt(tabs::TabInterface* tab,
                                              TabChangeType change_type) {
  if (!browser_)
    return;
  if (tab == browser_->GetActiveTabInterface()) {
    UpdateUrlField();
    UpdateNavButtons();
  }
}

void XplorerSidebarChromeView::OnTabStripModelDestroyed(
    TabStripModel* tab_strip_model) {
  observing_tabs_ = false;
}

void XplorerSidebarChromeView::RunCommand(int command_id) {
  if (!browser_)
    return;
  if (command_id == IDC_RELOAD) {
    tabs::TabInterface* tab = browser_->GetActiveTabInterface();
    content::WebContents* contents = tab ? tab->GetContents() : nullptr;
    if (contents && contents->IsLoading())
      command_id = IDC_STOP;
  }
  chrome::ExecuteCommand(browser_, command_id);
  UpdateNavButtons();
}

bool XplorerSidebarChromeView::HandleMouseEvent(
    views::Textfield* sender,
    const ui::MouseEvent& mouse_event) {
  if (sender != url_field_ ||
      mouse_event.type() != ui::EventType::kMousePressed)
    return false;
  ShowUrlPopup();
  return true;
}

void XplorerSidebarChromeView::CloseUrlPopup() {
  popup_field_ = nullptr;
  if (views::Widget* popup = url_popup_) {
    url_popup_ = nullptr;
    popup->Close();
  }
}

void XplorerSidebarChromeView::ShowUrlPopup() {
  if (!url_field_ || !url_field_->GetWidget())
    return;
  CloseUrlPopup();
  auto* widget = new views::Widget();
  views::Widget::InitParams params(
      views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_POPUP);
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.shadow_type = views::Widget::InitParams::ShadowType::kDrop;
  params.parent = url_field_->GetWidget()->GetNativeView();
  const gfx::Rect anchor = url_field_->GetBoundsInScreen();
  params.bounds = gfx::Rect(anchor.x(), anchor.bottom() + 6,
                            std::max(anchor.width(), 420), 52);
  widget->Init(std::move(params));

  auto contents = std::make_unique<views::View>();
  contents->SetBackground(
      views::CreateRoundedRectBackground(ui::kColorSysSurface3, 12.f));
  auto* layout = contents->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, gfx::Insets::VH(8, 12)));
  auto* field = contents->AddChildView(std::make_unique<views::Textfield>());
  popup_field_ = field;
  field->SetController(this);
  field->SetBorder(views::CreateEmptyBorder(gfx::Insets()));
  field->SetBackgroundColor(SK_ColorTRANSPARENT);
  field->SetTextColorId(ui::kColorSysOnSurface);
  field->SetPlaceholderText(u"Search or Enter URL");
  field->SetAccessibleName(u"Search or Enter URL");
  field->SetText(FullUrl(full_url_));
  layout->SetFlexForView(field, 1);
  widget->SetContentsView(std::move(contents));
  widget->Show();
  url_popup_ = widget;
  if (popup_field_) {
    popup_field_->RequestFocus();
    popup_field_->SelectAll(false);
  }
}

void XplorerSidebarChromeView::NavigateFromField() {
  NavigateFromText(FullUrl(full_url_));
}

void XplorerSidebarChromeView::NavigateFromText(const std::u16string& text) {
  if (!browser_ || !profile_)
    return;
  if (text.empty())
    return;
  AutocompleteClassifier* classifier =
      AutocompleteClassifierFactory::GetForProfile(profile_);
  if (!classifier)
    return;
  AutocompleteMatch match;
  classifier->Classify(text, false, false, metrics::OmniboxEventProto::BLANK,
                       &match, nullptr);
  if (!NavigationAllowed(match.destination_url))
    return;
  NavigateParams params(browser_, match.destination_url,
                        ui::PAGE_TRANSITION_TYPED);
  params.disposition = WindowOpenDisposition::CURRENT_TAB;
  Navigate(&params);
  if (auto* focus = url_field_->GetFocusManager())
    focus->ClearFocus();
}

void XplorerSidebarChromeView::UpdateUrlField() {
  if (!url_field_ || !browser_ || url_field_->HasFocus())
    return;
  tabs::TabInterface* tab = browser_->GetActiveTabInterface();
  content::WebContents* contents = tab ? tab->GetContents() : nullptr;
  full_url_ = contents ? contents->GetVisibleURL() : GURL();
  const std::u16string shown = DisplayUrl(full_url_);
  if (url_field_->GetText() != shown)
    url_field_->SetText(shown);
}

void XplorerSidebarChromeView::UpdateNavButtons() {
  if (!browser_)
    return;
  if (back_button_)
    back_button_->SetEnabled(chrome::IsCommandEnabled(browser_, IDC_BACK));
  if (forward_button_)
    forward_button_->SetEnabled(
        chrome::IsCommandEnabled(browser_, IDC_FORWARD));
  tabs::TabInterface* tab = browser_->GetActiveTabInterface();
  content::WebContents* contents = tab ? tab->GetContents() : nullptr;
  const bool loading = contents && contents->IsLoading();
  if (reload_button_ && showing_stop_ != loading) {
    showing_stop_ = loading;
    reload_button_->SetAccessibleName(loading ? u"Stop" : u"Reload");
  }
}

void XplorerSidebarChromeView::ReloadChrome() {
  if (auto* swatch = static_cast<XplorerSpaceSwatch*>(space_swatch_.get()))
    swatch->SetSwatchColor(ParseThemeColor(grok_companion::GetThemeColor()));
  if (!pins_)
    return;
  pins_->RemoveAllChildViews();
  int shown = 0;
  for (const base::DictValue& app : grok_companion::GetPinnedAppConfigs()) {
    const std::string* url_text = app.FindString("url");
    const std::string* label = app.FindString("label");
    if (!url_text)
      continue;
    const GURL url(*url_text);
    if (!url.is_valid() || !url.SchemeIsHTTPOrHTTPS())
      continue;
    std::u16string letter = u"•";
    if (label && !label->empty())
      letter = base::UTF8ToUTF16(label->substr(0, 1));
    auto* pin = pins_->AddChildView(std::make_unique<views::LabelButton>(
        base::BindRepeating(&XplorerSidebarChromeView::OpenPinned,
                            base::Unretained(this), url),
        letter));
    pin->SetPreferredSize(gfx::Size(kPinSize, kPinSize));
    pin->SetHorizontalAlignment(gfx::ALIGN_CENTER);
    pin->SetTooltipText(label ? base::UTF8ToUTF16(*label) : letter);
    pin->SetAccessibleName(label ? base::UTF8ToUTF16(*label) : u"Pinned app");
    pin->SetBackground(views::CreateRoundedRectBackground(
        ui::kColorSysHeader, kPinSize / 2.f));
    pin->SetEnabledTextColors(ui::kColorSysOnSurface);
    ++shown;
    if (shown >= 8)
      break;
  }
  pins_->SetVisible(shown > 0);
  InvalidateLayout();
}

void XplorerSidebarChromeView::OpenPinned(const GURL& url) {
  if (!browser_ || !url.is_valid())
    return;
  NavigateParams params = GetSingletonTabNavigateParams(browser_, url);
  Navigate(&params);
}

BEGIN_METADATA(XplorerSidebarChromeView)
END_METADATA

}  // namespace xplorer
