// Copyright 2026 The Xplorer Authors.
// Use of this source code is governed by a BSD-style license.

#include "chrome/browser/ui/views/xplorer/xplorer_sidebar_chrome_view.h"

#include <vector>

#include "chrome/browser/ui/views/xplorer/xplorer_sidebar_row_button.h"
#include "chrome/browser/ui/views/xplorer/xplorer_settings_nav.h"
#include "chrome/browser/ui/views/xplorer/xplorer_sidebar_section_label.h"
#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/cancelable_task_tracker.h"
#include "cc/paint/paint_flags.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/autocomplete/autocomplete_classifier_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/favicon/large_icon_service_factory.h"
#include "chrome/browser/grok_companion/grok_companion_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/extensions/extensions_container.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_desktop.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/browser/ui/navigator/browser_navigator.h"
#include "chrome/browser/ui/navigator/browser_navigator_params.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/favicon/core/large_icon_service.h"
#include "components/favicon_base/favicon_types.h"
#include "components/tabs/public/tab_group.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "components/omnibox/browser/autocomplete_classifier.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/url_formatter/elide_url.h"
#include "components/url_formatter/url_formatter.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "third_party/skia/include/core/SkColor.h"
#include "chrome/browser/ui/tabs/split_tab_metrics.h"
#include "components/split_tabs/split_tab_visual_data.h"
#include "content/public/browser/web_contents.h"
#include "chrome/browser/ui/views/location_bar/location_bar_bubble_delegate_view.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/models/image_model.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/menus/simple_menu_model.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/base/mojom/menu_source_type.mojom.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/views/background.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "ui/gfx/image/image_skia.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "components/favicon/content/content_favicon_driver.h"
#include "components/favicon/core/favicon_service.h"
#include "components/keyed_service/core/service_access_type.h"
#include "base/i18n/case_conversion.h"
#include "base/strings/string_util.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/vector_icons.h"
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
constexpr int kPinSize = 32;
constexpr int kPinsPerRow = 6;
constexpr int kMaxPins = 12;
constexpr int kPinIcon = 18;
constexpr SkColor kDefaultSwatch = SkColorSetRGB(0x7D, 0x87, 0x94);

constexpr net::NetworkTrafficAnnotationTag kPinnedAppFaviconAnnotation =
    net::DefineNetworkTrafficAnnotation("xplorer_pinned_app_favicon", R"(
      semantics {
        sender: "Xplor sidebar"
        description:
          "Loads the site icon for an app the user pinned in the sidebar."
        trigger: "The sidebar shows the user's pinned apps."
        data: "The pinned app's URL and the requested icon size."
        destination: GOOGLE_OWNED_SERVICE
      }
      policy {
        cookies_allowed: NO
        setting: "This request cannot be disabled in settings."
        policy_exception_justification:
          "Not implemented. The icon is only requested for apps the user pinned."
      })");

class TextPromptBubble : public LocationBarBubbleDelegateView {
 public:
  TextPromptBubble(views::View* anchor,
                   const std::u16string& title,
                   const std::u16string& initial,
                   base::OnceCallback<void(std::u16string)> callback)
      : LocationBarBubbleDelegateView(anchor, nullptr),
        callback_(std::move(callback)) {
    SetTitle(title);
    SetButtons(static_cast<int>(ui::mojom::DialogButton::kOk) |
               static_cast<int>(ui::mojom::DialogButton::kCancel));
    SetButtonLabel(ui::mojom::DialogButton::kOk, u"Save");
    set_close_on_deactivate(true);
    SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kVertical, gfx::Insets::VH(8, 12), 0));
    field_ = AddChildView(std::make_unique<views::Textfield>());
    field_->SetText(initial);
    field_->SetPreferredSize(gfx::Size(260, 28));
    field_->SelectAll(false);
  }

  views::View* GetInitiallyFocusedView() override { return field_; }

  bool Accept() override {
    if (callback_) {
      std::move(callback_).Run(std::u16string(field_->GetText()));
    }
    return true;
  }

 private:
  raw_ptr<views::Textfield> field_ = nullptr;
  base::OnceCallback<void(std::u16string)> callback_;
};

void ShowTextPrompt(views::View* anchor,
                    const std::u16string& title,
                    const std::u16string& initial,
                    base::OnceCallback<void(std::u16string)> callback) {
  auto* bubble = new TextPromptBubble(anchor, title, initial, std::move(callback));
  views::Widget* widget =
      views::BubbleDialogDelegateView::CreateBubble(bubble);
  widget->Show();
  bubble->GetInitiallyFocusedView()->RequestFocus();
}

// A pinned app is the site's own icon, not a letter in a circle.
class PinnedAppButton : public views::ImageButton,
                        public views::ContextMenuController,
                        public ui::SimpleMenuModel::Delegate {
  METADATA_HEADER(PinnedAppButton, views::ImageButton)

 public:
  PinnedAppButton(XplorerSidebarChromeView* owner,
                  Profile* profile,
                  const GURL& url,
                  const std::u16string& name,
                  PressedCallback callback)
      : views::ImageButton(std::move(callback)),
        owner_(owner),
        url_(url),
        name_(name) {
    SetPreferredSize(gfx::Size(kPinSize, kPinSize));
    SetTooltipText(name);
    SetAccessibleName(name.empty() ? u"Pinned app" : name);
    SetImageHorizontalAlignment(ALIGN_CENTER);
    SetImageVerticalAlignment(ALIGN_MIDDLE);
    SetBackground(nullptr);
    set_context_menu_controller(this);
    // Never blank: a letter until a real icon arrives.
    SetIcon(MonogramIcon(name));
    if (!profile || !url.is_valid()) {
      return;
    }
    profile_ = profile;
    // 1. An open tab on this site already has its favicon.
    if (owner_) {
      if (content::WebContents* open = owner_->FindFavoriteTab(url)) {
        if (auto* driver =
                favicon::ContentFaviconDriver::FromWebContents(open)) {
          const gfx::Image favicon = driver->GetFavicon();
          if (!favicon.IsEmpty() && driver->FaviconIsValid()) {
            SetIcon(favicon.AsImageSkia());
          }
        }
      }
    }
    // 2. The large-icon cache for the site (can be bigger and sharper).
    favicon::LargeIconService* service =
        LargeIconServiceFactory::GetForBrowserContext(profile);
    if (!service) {
      LookUpPageFavicon();
      return;
    }
    service->GetLargeIconFromCacheFallbackToGoogleServer(
        url, favicon::LargeIconService::StandardIconSize::k16x16,
        favicon::LargeIconService::StandardIconSize::k32x32,
        favicon::LargeIconService::NoBigEnoughIconBehavior::kReturnBitmap,
        /*should_trim_page_url_path=*/true, kPinnedAppFaviconAnnotation,
        base::BindOnce(&PinnedAppButton::OnIcon, weak_factory_.GetWeakPtr()),
        &tracker_);
  }

  void ExecuteCommand(int command_id, int event_flags) override {
    (void)event_flags;
    if (!owner_) {
      return;
    }
    switch (command_id) {
      case 1: {
        ui::ScopedClipboardWriter writer(ui::ClipboardBuffer::kCopyPaste);
        writer.WriteText(base::UTF8ToUTF16(url_.spec()));
        break;
      }
      case 2: {
        const GURL mail("mailto:?body=" + url_.spec());
        owner_->OpenFavoriteInNewTab(mail);
        break;
      }
      case 3:
        ShowTextPrompt(
            this, u"Rename", name_,
            base::BindOnce(
                [](XplorerSidebarChromeView* owner, GURL url,
                   std::u16string text) {
                  if (owner && !text.empty()) {
                    owner->RenameFavorite(url, text);
                  }
                },
                owner_.get(), url_));
        break;
      case 5:
        ShowTextPrompt(
            this, u"Edit pinned page", base::UTF8ToUTF16(url_.spec()),
            base::BindOnce(
                [](XplorerSidebarChromeView* owner, GURL url,
                   std::u16string text) {
                  if (!owner) {
                    return;
                  }
                  const GURL next(base::UTF16ToUTF8(text));
                  if (next.is_valid() && next.SchemeIsHTTPOrHTTPS()) {
                    owner->SetFavoriteUrl(url, next);
                  }
                },
                owner_.get(), url_));
        break;
      case 6:
        owner_->OpenFavoriteInSplit(url_);
        break;
      case 7:
        owner_->OpenFavoriteInNewTab(url_);
        break;
      case 8:
        if (content::WebContents* contents = owner_->FindFavoriteTab(url_)) {
          contents->SetAudioMuted(!contents->IsAudioMuted());
        }
        break;
      case 9:
        owner_->RemoveFavorite(url_);
        break;
      case 23:
        owner_->AddFavoriteFromActiveTab();
        break;
      case 21:
        owner_->MoveFavoriteBy(url_, -1);
        break;
      case 22:
        owner_->MoveFavoriteBy(url_, 1);
        break;
      case 10: {
        content::WebContents* active = owner_->ActiveWebContents();
        if (!active) {
          break;
        }
        const GURL next = active->GetVisibleURL();
        if (next.is_valid() && next.SchemeIsHTTPOrHTTPS()) {
          owner_->SetFavoriteUrl(url_, next);
        }
        break;
      }
      default:
        if (command_id >= 200 &&
            command_id - 200 < static_cast<int>(move_groups_.size())) {
          owner_->MoveFavoriteToFolder(url_, move_groups_[command_id - 200]);
        }
        break;
    }
  }

  bool IsCommandIdEnabled(int command_id) const override {
    if (command_id == 8) {
      return owner_ && owner_->FindFavoriteTab(url_) != nullptr;
    }
    if (command_id == 10) {
      content::WebContents* active =
          owner_ ? owner_->ActiveWebContents() : nullptr;
      return active && active->GetVisibleURL().SchemeIsHTTPOrHTTPS();
    }
    if (command_id == 23) {
      content::WebContents* active =
          owner_ ? owner_->ActiveWebContents() : nullptr;
      int count = 0;
      if (owner_)
        owner_->FavoriteIndex(GURL(), &count);
      return active && active->GetVisibleURL().SchemeIsHTTPOrHTTPS() &&
             !grok_companion::IsFavoriteUrl(active->GetVisibleURL()) &&
             count < kMaxPins;
    }
    if (command_id == 21 || command_id == 22) {
      int count = 0;
      const int at = owner_ ? owner_->FavoriteIndex(url_, &count) : -1;
      return command_id == 21 ? at > 0 : at >= 0 && at < count - 1;
    }
    return true;
  }

 private:
  void ShowContextMenuForViewImpl(
      views::View* source,
      const gfx::Point& point,
      ui::mojom::MenuSourceType source_type) override {
    (void)source;
    (void)source_type;
    edit_menu_ = std::make_unique<ui::SimpleMenuModel>(this);
    edit_menu_->AddItem(5, u"Edit URL…");
    edit_menu_->AddItem(10, u"Replace with Current Tab");

    share_menu_ = std::make_unique<ui::SimpleMenuModel>(this);
    share_menu_->AddItem(2, u"Mail");

    move_menu_ = std::make_unique<ui::SimpleMenuModel>(this);
    move_groups_.clear();
    TabStripModel* model =
        owner_ && owner_->browser() ? owner_->browser()->GetTabStripModel()
                                    : nullptr;
    if (model && model->group_model()) {
      for (const tab_groups::TabGroupId& id :
           model->group_model()->ListTabGroups()) {
        TabGroup* group = model->group_model()->GetTabGroup(id);
        std::u16string title = u"Folder";
        if (group && group->visual_data() &&
            !group->visual_data()->title().empty()) {
          title = group->visual_data()->title();
        }
        move_menu_->AddItem(200 + static_cast<int>(move_groups_.size()), title);
        move_groups_.push_back(id);
      }
    }
    move_menu_->AddItem(200 + static_cast<int>(move_groups_.size()),
                        u"New Folder");
    move_groups_.push_back(std::nullopt);

    menu_model_ = std::make_unique<ui::SimpleMenuModel>(this);
    menu_model_->AddItem(23, u"Add This Page to Favorites");
    menu_model_->AddSeparator(ui::NORMAL_SEPARATOR);
    menu_model_->AddItem(1, u"Copy Link");
    menu_model_->AddSubMenu(0, u"Share", share_menu_.get());
    menu_model_->AddSeparator(ui::NORMAL_SEPARATOR);
    menu_model_->AddItem(3, u"Rename…");
    menu_model_->AddItem(8, u"Mute");
    menu_model_->AddSubMenu(0, u"Edit Pinned Page", edit_menu_.get());
    menu_model_->AddSeparator(ui::NORMAL_SEPARATOR);
    menu_model_->AddItem(6, u"Open in Split View");
    menu_model_->AddItem(7, u"Duplicate");
    menu_model_->AddSubMenu(0, u"Move to", move_menu_.get());
    menu_model_->AddSeparator(ui::NORMAL_SEPARATOR);
    menu_model_->AddItem(21, u"Move Left");
    menu_model_->AddItem(22, u"Move Right");
    menu_model_->AddSeparator(ui::NORMAL_SEPARATOR);
    menu_model_->AddItem(9, u"Remove Favorite");
    menu_runner_ = std::make_unique<views::MenuRunner>(
        menu_model_.get(), views::MenuRunner::CONTEXT_MENU);
    menu_runner_->RunMenuAt(GetWidget(), nullptr, gfx::Rect(point, gfx::Size()),
                            views::MenuAnchorPosition::kTopLeft,
                            ui::mojom::MenuSourceType::kMouse);
  }

  // Each pin sits on a small rounded tile. Without one, dark logos such as
  // GitHub disappeared on the dark sidebar.
  void OnThemeChanged() override {
    views::ImageButton::OnThemeChanged();
    const SkColor tile = SidebarBackdropIsDark()
                             ? SkColorSetARGB(0xEB, 0xEC, 0xEC, 0xEF)
                             : SkColorSetARGB(0xB8, 0xFF, 0xFF, 0xFF);
    SetBackground(views::CreateRoundedRectBackground(tile, 7.0f));
  }

  void OnIcon(const favicon_base::LargeIconResult& result) {
    const gfx::Image image =
        result.bitmap.is_valid()
            ? gfx::Image::CreateFrom1xPNGBytes(result.bitmap.bitmap_data)
            : gfx::Image();
    if (image.IsEmpty()) {
      // The large-icon cache is keyed by site root. A favorite added from a
      // deep page only has a favicon stored for that exact page.
      LookUpPageFavicon();
      return;
    }
    SetIcon(image.AsImageSkia());
  }

  // 3. The favicon history stored for this exact page.
  void LookUpPageFavicon() {
    favicon::FaviconService* favicons =
        profile_ ? FaviconServiceFactory::GetForProfile(
                       profile_, ServiceAccessType::EXPLICIT_ACCESS)
                 : nullptr;
    if (!favicons) {
      return;
    }
    favicons->GetFaviconImageForPageURL(
        url_,
        base::BindOnce(
            [](base::WeakPtr<PinnedAppButton> self,
               const favicon_base::FaviconImageResult& result) {
              if (self && !result.image.IsEmpty()) {
                self->SetIcon(result.image.AsImageSkia());
              }
            },
            weak_factory_.GetWeakPtr()),
        &tracker_);
  }

  void SetIcon(const gfx::ImageSkia& icon) {
    const gfx::ImageSkia resized = gfx::ImageSkiaOperations::CreateResizedImage(
        icon, skia::ImageOperations::RESIZE_BEST,
        gfx::Size(kPinIcon, kPinIcon));
    const ui::ImageModel model = ui::ImageModel::FromImageSkia(resized);
    SetImageModel(views::Button::STATE_NORMAL, model);
    SetImageModel(views::Button::STATE_HOVERED, model);
    SetImageModel(views::Button::STATE_PRESSED, model);
  }

  // First letter of the name on a neutral disc, drawn at 2x.
  static gfx::ImageSkia MonogramIcon(const std::u16string& name) {
    constexpr float kScale = 2.0f;
    const int px = static_cast<int>(kPinIcon * kScale);
    gfx::Canvas canvas(gfx::Size(px, px), 1.0f, /*is_opaque=*/false);
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setColor(SkColorSetRGB(0x6B, 0x6F, 0x78));
    canvas.DrawCircle(gfx::PointF(px / 2.0f, px / 2.0f), px / 2.0f, flags);
    std::u16string letter = u"?";
    for (char16_t c : name) {
      if (!base::IsUnicodeWhitespace(c)) {
        letter = base::i18n::ToUpper(std::u16string(1, c));
        break;
      }
    }
    const gfx::FontList font = gfx::FontList().DeriveWithSizeDelta(
        px * 0.5 - gfx::FontList().GetFontSize()).DeriveWithWeight(
        gfx::Font::Weight::SEMIBOLD);
    canvas.DrawStringRectWithFlags(letter, font, SK_ColorWHITE,
                                   gfx::Rect(0, 0, px, px),
                                   gfx::Canvas::TEXT_ALIGN_CENTER);
    return gfx::ImageSkia::CreateFromBitmap(canvas.GetBitmap(), kScale);
  }

  const raw_ptr<XplorerSidebarChromeView> owner_;
  const GURL url_;
  const std::u16string name_;
  std::vector<std::optional<tab_groups::TabGroupId>> move_groups_;
  std::unique_ptr<ui::SimpleMenuModel> menu_model_;
  std::unique_ptr<ui::SimpleMenuModel> edit_menu_;
  std::unique_ptr<ui::SimpleMenuModel> share_menu_;
  std::unique_ptr<ui::SimpleMenuModel> move_menu_;
  std::unique_ptr<views::MenuRunner> menu_runner_;
  raw_ptr<Profile> profile_ = nullptr;
  base::CancelableTaskTracker tracker_;
  base::WeakPtrFactory<PinnedAppButton> weak_factory_{this};
};

BEGIN_METADATA(PinnedAppButton)
END_METADATA

SkColor ParseThemeColor(const std::string& hex) {
  if (hex.size() != 7 || hex[0] != '#')
    return kDefaultSwatch;
  uint32_t rgb = 0;
  if (!base::HexStringToUInt(hex.substr(1), &rgb))
    return kDefaultSwatch;
  return SkColorSetRGB((rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF);
}

struct PanelColor {
  int command;
  const char16_t* name;
  SkColor color;
  ui::mojom::BrowserColorVariant variant;
};

constexpr PanelColor kPanelColors[] = {
    {40, u"Blue", SkColorSetRGB(140, 171, 228),
     ui::mojom::BrowserColorVariant::kTonalSpot},
    {41, u"Cool grey", SkColorSetRGB(140, 171, 228),
     ui::mojom::BrowserColorVariant::kNeutral},
    {42, u"Grey", SkColorSetRGB(136, 136, 136),
     ui::mojom::BrowserColorVariant::kNeutral},
    {43, u"Aqua", SkColorSetRGB(38, 166, 154),
     ui::mojom::BrowserColorVariant::kTonalSpot},
    {44, u"Green", SkColorSetRGB(0, 255, 0),
     ui::mojom::BrowserColorVariant::kTonalSpot},
    {45, u"Viridian", SkColorSetRGB(135, 186, 129),
     ui::mojom::BrowserColorVariant::kNeutral},
    {46, u"Citron", SkColorSetRGB(250, 223, 115),
     ui::mojom::BrowserColorVariant::kTonalSpot},
    {47, u"Orange", SkColorSetRGB(255, 128, 0),
     ui::mojom::BrowserColorVariant::kTonalSpot},
    {48, u"Apricot", SkColorSetRGB(252, 219, 201),
     ui::mojom::BrowserColorVariant::kNeutral},
    {49, u"Rose", SkColorSetRGB(243, 178, 190),
     ui::mojom::BrowserColorVariant::kTonalSpot},
    {50, u"Pink", SkColorSetRGB(243, 178, 190),
     ui::mojom::BrowserColorVariant::kNeutral},
    {51, u"Fuchsia", SkColorSetRGB(255, 0, 255),
     ui::mojom::BrowserColorVariant::kTonalSpot},
    {52, u"Violet", SkColorSetRGB(229, 213, 252),
     ui::mojom::BrowserColorVariant::kTonalSpot},
};

// A filled 14px dot with a faint rim, drawn at 2x for Retina menus.
gfx::ImageSkia SwatchDot(SkColor color) {
  constexpr int kSize = 14;
  constexpr float kScale = 2.0f;
  SkBitmap bitmap;
  bitmap.allocN32Pixels(kSize * kScale, kSize * kScale);
  bitmap.eraseColor(SK_ColorTRANSPARENT);
  SkCanvas canvas(bitmap);
  SkPaint paint;
  paint.setAntiAlias(true);
  paint.setColor(color);
  const float r = kSize * kScale / 2.0f;
  canvas.drawCircle(r, r, r - 1.0f, paint);
  paint.setStyle(SkPaint::kStroke_Style);
  paint.setStrokeWidth(1.5f);
  paint.setColor(SkColorSetARGB(0x40, 0, 0, 0));
  canvas.drawCircle(r, r, r - 1.5f, paint);
  return gfx::ImageSkia::CreateFromBitmap(bitmap, kScale);
}

const PanelColor* FindPanelColor(int command) {
  for (const PanelColor& color : kPanelColors) {
    if (color.command == command) {
      return &color;
    }
  }
  return nullptr;
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

bool IsInternalPage(const GURL& url) {
  if (!url.is_valid() || url.IsAboutBlank() ||
      grok_companion::IsGrokHomeURL(url) ||
      url.SchemeIs(content::kChromeUIScheme) ||
      url.SchemeIs(content::kChromeUIUntrustedScheme)) {
    return true;
  }
  return url.SchemeIsHTTPOrHTTPS() &&
         (url.host() == "127.0.0.1" || url.host() == "localhost") &&
         (url.path() == "/welcome" || url.path() == "/search");
}

std::u16string EditableUrl(const GURL& url) {
  if (IsInternalPage(url))
    return {};
  return url_formatter::FormatUrl(
      url,
      url_formatter::kFormatUrlOmitDefaults | url_formatter::kFormatUrlOmitHTTPS,
      base::UnescapeRule::SPACES, nullptr, nullptr, nullptr);
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

  // Favorites: rows of up to kPinsPerRow tiles, then a "+" tile.
  pins_ = AddChildView(std::make_unique<views::View>());
  pins_->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets::VH(8, 8), 6));
  pins_->SetVisible(false);

  auto* header = AddChildView(std::make_unique<views::View>());
  auto* header_layout =
      header->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal, gfx::Insets::VH(4, 6),
          8));
  header_layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  header->SetProperty(views::kMarginsKey, kHeaderMargins);
  header->SetVisible(false);
  space_swatch_ = header->AddChildView(std::make_unique<XplorerSpaceSwatch>());
  auto* title = header->AddChildView(std::make_unique<views::Label>(
      base::UTF8ToUTF16(grok_companion::GetSpaceName())));
  space_title_ = title;
  title->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  title->SetFontList(title->font_list().DeriveWithSizeDelta(1).DeriveWithWeight(
      gfx::Font::Weight::SEMIBOLD));

  // Back, forward, reload, then the address field. This replaces the toolbar
  // that used to sit across the top of the page.
  auto* nav = AddChildView(std::make_unique<views::View>());
  auto* nav_layout = nav->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, gfx::Insets::TLBR(0, 8, 2, 8),
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

  // Chat and the extension icons. The top toolbar is hidden, so this row
  // is the only place those controls live.
  tools_ = AddChildView(std::make_unique<views::View>());
  auto* tools_layout =
      tools_->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal,
          gfx::Insets::TLBR(0, 6, 2, 6), 4));
  tools_layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  chat_button_ = tools_->AddChildView(std::make_unique<views::LabelButton>(
      base::BindRepeating(&XplorerSidebarChromeView::ToggleChat,
                          base::Unretained(this)),
      u"Chat"));
  chat_button_->SetTooltipText(u"Open Grok chat");
  extensions_button_ =
      tools_->AddChildView(std::make_unique<views::LabelButton>(
          base::BindRepeating(&XplorerSidebarChromeView::OpenExtensions,
                              base::Unretained(this)),
          u"Extensions"));
  extensions_button_->SetTooltipText(u"Extensions");
  create_button_ = tools_->AddChildView(std::make_unique<views::LabelButton>(
      base::BindRepeating(&XplorerSidebarChromeView::ShowCreateMenu,
                          base::Unretained(this), gfx::Point()),
      u"+"));
  create_button_->SetTooltipText(u"New tab, folder, or split");
  create_button_->SetAccessibleName(u"New");

  auto* tabs_label =
      AddChildView(std::make_unique<XplorerSidebarSectionLabel>(u"Today"));
  tabs_label->SetProperty(views::kMarginsKey, kSectionLabelMargins);
  today_label_ = tabs_label;

  if (browser_ && browser_->GetTabStripModel()) {
    browser_->GetTabStripModel()->AddObserver(this);
    observing_tabs_ = true;
  }
  onboarding_subscription_ = grok_companion::AddOnboardingChangedCallback(
      base::BindRepeating(&XplorerSidebarChromeView::ReloadChrome,
                          base::Unretained(this)));
  // The address is the thin strip at the top of the page.
  if (nav) {
    nav->SetVisible(false);
  }
  if (tools_)
    ReorderChildView(tools_, 1);
  if (pins_)
    ReorderChildView(pins_, 2);

  HostExtensions();
  set_context_menu_controller(this);
  tone_timer_.Start(FROM_HERE, base::Seconds(1),
                    base::BindRepeating(&XplorerSidebarChromeView::ApplySidebarTone,
                                        base::Unretained(this)));
  ReloadChrome();
  UpdateUrlField();
  UpdateNavButtons();
}

XplorerSidebarChromeView::~XplorerSidebarChromeView() {
  ReturnExtensions();
  CloseUrlPopup();
  if (observing_tabs_ && browser_ && browser_->GetTabStripModel())
    browser_->GetTabStripModel()->RemoveObserver(this);
}

void XplorerSidebarChromeView::FocusUrlField(bool user_initiated) {
  if (user_initiated)
    ShowUrlPopup();
}

void XplorerSidebarChromeView::OnUrlFieldBlur() {
  CloseUrlPopup();
}

bool XplorerSidebarChromeView::HandleKeyEvent(
    views::Textfield* sender,
    const ui::KeyEvent& key_event) {
  if (key_event.type() != ui::EventType::kKeyPressed)
    return false;
  if (sender == space_editor_) {
    if (key_event.key_code() == ui::VKEY_RETURN) {
      CommitRename();
      return true;
    }
    if (key_event.key_code() == ui::VKEY_ESCAPE) {
      if (space_editor_)
        space_editor_->SetVisible(false);
      if (space_title_)
        space_title_->SetVisible(true);
      return true;
    }
    return false;
  }
  if (sender != url_field_)
    return false;
  if (key_event.key_code() == ui::VKEY_RETURN) {
    editing_url_ = false;
    if (url_field_)
      url_field_->SetReadOnly(true);
    NavigateFromText(std::u16string(url_field_->GetText()));
    UpdateUrlField();
    return true;
  }
  if (key_event.key_code() == ui::VKEY_ESCAPE) {
    CloseUrlPopup();
    UpdateUrlField();
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
  UpdateTodayLabel();
  if (change.type() == TabStripModelChange::kInserted) {
    // Not from inside the observer call: moving tabs here re-enters the model.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&XplorerSidebarChromeView::KeepLooseTabsTogether,
                       weak_factory_.GetWeakPtr()));
  }
}

void XplorerSidebarChromeView::KeepLooseTabsTogether() {
  if (!browser_ || !browser_->GetTabStripModel())
    return;
  TabStripModel* model = browser_->GetTabStripModel();
  int first_group = -1;
  for (int i = 0; i < model->count(); ++i) {
    if (!model->IsTabPinned(i) && model->GetTabGroupForTab(i).has_value()) {
      first_group = i;
      break;
    }
  }
  if (first_group < 0)
    return;
  std::vector<content::WebContents*> stragglers;
  for (int i = first_group + 1; i < model->count(); ++i) {
    if (!model->IsTabPinned(i) && !model->GetTabGroupForTab(i).has_value())
      stragglers.push_back(model->GetWebContentsAt(i));
  }
  int target = first_group;
  for (content::WebContents* contents : stragglers) {
    const int index = model->GetIndexOfWebContents(contents);
    if (index == TabStripModel::kNoTab)
      continue;
    model->MoveWebContentsAt(index, target, /*select_after_move=*/false,
                             std::nullopt);
    ++target;
  }
  UpdateTodayLabel();
}

void XplorerSidebarChromeView::OnTabGroupChanged(const TabGroupChange& change) {
  UpdateTodayLabel();
}

void XplorerSidebarChromeView::UpdateTodayLabel() {
  if (!today_label_ || !browser_ || !browser_->GetTabStripModel())
    return;
  TabStripModel* model = browser_->GetTabStripModel();
  bool any_loose = false;
  for (int i = 0; i < model->count(); ++i) {
    if (!model->IsTabPinned(i) && !model->GetTabGroupForTab(i).has_value()) {
      any_loose = true;
      break;
    }
  }
  if (today_label_->GetVisible() != any_loose) {
    today_label_->SetVisible(any_loose);
    InvalidateLayout();
  }
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

void XplorerSidebarChromeView::ToggleChat() {
  grok_companion::ToggleGrokSidePanel(browser_);
}

void XplorerSidebarChromeView::OpenExtensions() {
  if (!browser_)
    return;
  if (auto* container = ExtensionsContainer::From(*browser_)) {
    container->ToggleExtensionsMenu();
    return;
  }
  ::ShowSingletonTab(browser_, GURL(chrome::kChromeUIExtensionsURL));
}

void XplorerSidebarChromeView::HostExtensions() {
  if (extensions_view_ || !tools_ || !browser_)
    return;
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser_);
  if (!browser_view || !browser_view->toolbar())
    return;
  ExtensionsToolbarDesktop* extensions =
      browser_view->toolbar()->extensions_container();
  if (!extensions || extensions->parent() == tools_)
    return;
  extensions->parent()->RemoveChildView(extensions);
  tools_->AddChildView(extensions);
  extensions->SetVisible(true);
  extensions_view_ = extensions;
  if (auto* layout = static_cast<views::BoxLayout*>(tools_->GetLayoutManager()))
    layout->SetFlexForView(extensions, 1);
}

void XplorerSidebarChromeView::ReturnExtensions() {
  if (!extensions_view_ || !browser_)
    return;
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser_);
  views::View* extensions = extensions_view_;
  extensions_view_ = nullptr;
  if (!browser_view || !browser_view->toolbar() ||
      extensions->parent() != tools_) {
    return;
  }
  tools_->RemoveChildView(extensions);
  browser_view->toolbar()->AddChildView(extensions);
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
  // Editing happens in the sidebar field. There is no floating popup; one
  // was sticking open on top of the space name.
  editing_url_ = false;
  if (url_field_)
    url_field_->SetReadOnly(true);
  UpdateUrlField();
}

void XplorerSidebarChromeView::OnWidgetActivationChanged(views::Widget* widget,
                                                         bool active) {}

void XplorerSidebarChromeView::OnWidgetDestroying(views::Widget* widget) {}

void XplorerSidebarChromeView::ShowUrlPopup() {
  if (!url_field_)
    return;
  editing_url_ = true;
  url_field_->SetReadOnly(false);
  url_field_->SetText(EditableUrl(full_url_));
  url_field_->RequestFocus();
  url_field_->SelectAll(false);
}

void XplorerSidebarChromeView::NavigateFromField() {
  NavigateFromText(EditableUrl(full_url_));
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
  if (!url_field_ || !browser_ || editing_url_)
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
  views::View* row = nullptr;
  auto next_slot = [&]() -> views::View* {
    if (!row || static_cast<int>(row->children().size()) >= kPinsPerRow) {
      row = pins_->AddChildView(std::make_unique<views::View>());
      row->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal, gfx::Insets(), 6));
    }
    return row;
  };
  for (const base::DictValue& app : grok_companion::GetPinnedAppConfigs()) {
    const std::string* url_text = app.FindString("url");
    const std::string* label = app.FindString("label");
    if (!url_text)
      continue;
    const GURL url(*url_text);
    if (!url.is_valid() || !url.SchemeIsHTTPOrHTTPS())
      continue;
    const std::u16string name =
        label ? base::UTF8ToUTF16(*label) : std::u16string(u"Pinned app");
    next_slot()->AddChildView(std::make_unique<PinnedAppButton>(
        this, profile_, url, name,
        base::BindRepeating(&XplorerSidebarChromeView::OpenPinned,
                            base::Unretained(this), url)));
    ++shown;
    if (shown >= kMaxPins)
      break;
  }
  pins_->SetVisible(shown > 0);
  InvalidateLayout();
}

void XplorerSidebarChromeView::ShowContextMenuForViewImpl(
    views::View* source,
    const gfx::Point& point,
    ui::mojom::MenuSourceType source_type) {
  (void)source;
  (void)source_type;
  ShowSpaceMenu(point);
}

bool XplorerSidebarChromeView::IsCommandIdEnabled(int command_id) const {
  if (command_id == 15) {
    content::WebContents* active = ActiveWebContents();
    return active && active->GetVisibleURL().SchemeIsHTTPOrHTTPS();
  }
  return true;
}

bool XplorerSidebarChromeView::IsCommandIdChecked(int command_id) const {
  if (command_id == 30) {
    return true;
  }
  if (command_id == 80) {
    return panel_variant_ == ui::mojom::BrowserColorVariant::kTonalSpot;
  }
  if (command_id == 81) {
    return panel_variant_ == ui::mojom::BrowserColorVariant::kNeutral;
  }
  if (command_id == 82) {
    return panel_variant_ == ui::mojom::BrowserColorVariant::kVibrant;
  }
  if (command_id == 83) {
    return panel_variant_ == ui::mojom::BrowserColorVariant::kExpressive;
  }
  ThemeService* theme =
      profile_ ? ThemeServiceFactory::GetForProfile(profile_) : nullptr;
  const std::optional<SkColor> current = theme ? theme->GetUserColor()
                                               : std::nullopt;
  if (command_id == 39) {
    return !current.has_value();
  }
  if (const PanelColor* color = FindPanelColor(command_id)) {
    return current.has_value() &&
           SkColorSetA(*current, SK_AlphaOPAQUE) == color->color &&
           panel_variant_ == color->variant;
  }
  return false;
}

void XplorerSidebarChromeView::ApplyPanelColor(SkColor color) {
  if (!profile_) {
    return;
  }
  if (ThemeService* theme = ThemeServiceFactory::GetForProfile(profile_)) {
    theme->SetUserColorAndBrowserColorVariant(color, panel_variant_);
  }
}

void XplorerSidebarChromeView::ShowSpaceMenu(const gfx::Point& point) {
  if (profile_) {
    if (ThemeService* theme = ThemeServiceFactory::GetForProfile(profile_)) {
      panel_variant_ = theme->GetBrowserColorVariant();
    }
  }
  theme_menu_ = std::make_unique<ui::SimpleMenuModel>(this);
  theme_menu_->AddCheckItem(39, u"Chrome default");
  theme_menu_->AddSeparator(ui::NORMAL_SEPARATOR);
  for (const PanelColor& choice : kPanelColors) {
    theme_menu_->AddCheckItem(choice.command, choice.name);
    // A swatch next to each name. Neutral variants come out greyer than the
    // seed, so show them that way.
    const SkColor swatch =
        choice.variant == ui::mojom::BrowserColorVariant::kNeutral
            ? color_utils::AlphaBlend(choice.color,
                                      SkColorSetRGB(0x9A, 0x9C, 0xA2), 0.45f)
            : choice.color;
    // Pre-rendered: the native macOS menu drops vector icons that need a
    // color provider.
    theme_menu_->SetIcon(theme_menu_->GetItemCount() - 1,
                         ui::ImageModel::FromImageSkia(SwatchDot(swatch)));
  }
  tone_menu_ = std::make_unique<ui::SimpleMenuModel>(this);
  tone_menu_->AddCheckItem(80, u"Tonal");
  tone_menu_->AddCheckItem(81, u"Neutral");
  tone_menu_->AddCheckItem(82, u"Vibrant");
  tone_menu_->AddCheckItem(83, u"Expressive");
  theme_menu_->AddSeparator(ui::NORMAL_SEPARATOR);
  theme_menu_->AddSubMenu(0, u"Tone", tone_menu_.get());

  std::u16string profile_name = u"Default";
  if (profile_ && g_browser_process && g_browser_process->profile_manager()) {
    ProfileAttributesEntry* entry =
        g_browser_process->profile_manager()
            ->GetProfileAttributesStorage()
            .GetProfileAttributesWithPath(profile_->GetPath());
    if (entry && !entry->GetName().empty()) {
      profile_name = entry->GetName();
    }
  }
  profile_menu_ = std::make_unique<ui::SimpleMenuModel>(this);
  profile_menu_->AddCheckItem(30, profile_name);
  profile_menu_->AddSeparator(ui::NORMAL_SEPARATOR);
  profile_menu_->AddItem(13, u"New Profile…");
  profile_menu_->AddItem(14, u"Manage Profiles");

  menu_model_ = std::make_unique<ui::SimpleMenuModel>(this);
  menu_model_->AddItem(15, u"Add Favorite");
  menu_model_->AddItem(10, u"New Folder");
  menu_model_->AddSubMenu(0, u"Panel Color", theme_menu_.get());
  menu_model_->AddSubMenu(0, u"Set Profile", profile_menu_.get());
  menu_model_->AddSeparator(ui::NORMAL_SEPARATOR);
  menu_model_->AddItem(16, u"Settings…");
  menu_runner_ = std::make_unique<views::MenuRunner>(
      menu_model_.get(), views::MenuRunner::CONTEXT_MENU);
  menu_runner_->RunMenuAt(GetWidget(), nullptr, gfx::Rect(point, gfx::Size()),
                          views::MenuAnchorPosition::kTopLeft,
                          ui::mojom::MenuSourceType::kMouse);
}

void XplorerSidebarChromeView::ShowCreateMenu(const gfx::Point& point) {
  gfx::Rect anchor(point, gfx::Size());
  if (create_button_ && create_button_->GetWidget()) {
    anchor = create_button_->GetBoundsInScreen();
    anchor.set_y(anchor.bottom());
    anchor.set_height(0);
  } else if (GetWidget()) {
    anchor.set_origin(GetWidget()->GetWindowBoundsInScreen().origin());
  }
  menu_model_ = std::make_unique<ui::SimpleMenuModel>(this);
  menu_model_->AddItem(11, u"New Tab");
  menu_model_->AddItem(10, u"New Folder");
  menu_model_->AddItem(15, u"Add Favorite");
  menu_model_->AddItem(12, u"New Split");
  menu_runner_ = std::make_unique<views::MenuRunner>(
      menu_model_.get(), views::MenuRunner::HAS_MNEMONICS);
  menu_runner_->RunMenuAt(GetWidget(), nullptr, anchor,
                          views::MenuAnchorPosition::kTopLeft,
                          ui::mojom::MenuSourceType::kMouse);
}

void XplorerSidebarChromeView::ExecuteCommand(int command_id, int event_flags) {
  switch (command_id) {
    case 1:
      BeginRename();
      break;
    case 16:
      OpenXplorerSettings(browser_, {}, /*in_new_tab=*/true);
      break;
    case 39:
      if (profile_) {
        if (ThemeService* theme =
                ThemeServiceFactory::GetForProfile(profile_)) {
          theme->SetUserColor(std::nullopt);
        }
      }
      break;
    case 80:
    case 81:
    case 82:
    case 83:
      panel_variant_ = command_id == 81
                           ? ui::mojom::BrowserColorVariant::kNeutral
                       : command_id == 82
                           ? ui::mojom::BrowserColorVariant::kVibrant
                       : command_id == 83
                           ? ui::mojom::BrowserColorVariant::kExpressive
                           : ui::mojom::BrowserColorVariant::kTonalSpot;
      if (profile_) {
        if (ThemeService* theme =
                ThemeServiceFactory::GetForProfile(profile_)) {
          if (std::optional<SkColor> current = theme->GetUserColor()) {
            theme->SetUserColorAndBrowserColorVariant(*current,
                                                       panel_variant_);
          }
        }
      }
      break;
    case 10:
      NewFolder();
      break;
    case 11:
      RunCommand(IDC_NEW_TAB);
      break;
    case 12:
      RunCommand(IDC_NEW_SPLIT_TAB);
      break;
    case 13:
      RunCommand(IDC_ADD_NEW_PROFILE);
      break;
    case 14:
      RunCommand(IDC_MANAGE_CHROME_PROFILES);
      break;
    case 15:
      AddFavoriteFromActiveTab();
      break;
    default:
      if (const PanelColor* color = FindPanelColor(command_id)) {
        panel_variant_ = color->variant;
        ApplyPanelColor(color->color);
      }
      break;
  }
}

void XplorerSidebarChromeView::NewFolder() {
  if (!browser_ || !browser_->GetTabStripModel()) {
    return;
  }
  RunCommand(IDC_NEW_TAB);
  TabStripModel* model = browser_->GetTabStripModel();
  const int index = model->active_index();
  if (index < 0) {
    return;
  }
  const tab_groups::TabGroupId group = model->AddToNewGroup({index});
  model->ChangeTabGroupVisuals(
      group, tab_groups::TabGroupVisualData(
                 u"Folder", tab_groups::TabGroupColorId::kGrey));
}

void XplorerSidebarChromeView::BeginRename() {
  if (!space_title_ || !space_title_->parent()) {
    return;
  }
  if (!space_editor_) {
    auto editor = std::make_unique<views::Textfield>();
    editor->SetPlaceholderText(u"Space name");
    editor->set_controller(this);
    space_editor_ = space_title_->parent()->AddChildView(std::move(editor));
  }
  space_editor_->SetText(space_title_->GetText());
  space_editor_->SetVisible(true);
  space_title_->SetVisible(false);
  space_editor_->RequestFocus();
  space_editor_->SelectAll(false);
}

void XplorerSidebarChromeView::CommitRename() {
  if (!space_editor_ || !space_title_) {
    return;
  }
  std::string name = base::UTF16ToUTF8(space_editor_->GetText());
  if (name.empty()) {
    name = "Xplor";
  }
  grok_companion::SetSpaceName(name);
  space_title_->SetText(base::UTF8ToUTF16(name));
  space_editor_->SetVisible(false);
  space_title_->SetVisible(true);
}

void XplorerSidebarChromeView::ApplySidebarTone() {
  views::View* surface = parent() ? parent() : this;
  if (surface->GetWidget()) {
    NoteSidebarSampleBounds(surface->GetBoundsInScreen());
  }
  const SkColor ink = SidebarInkColor();
  if (space_title_) {
    space_title_->SetEnabledColor(ink);
  }
  if (chat_button_) {
    chat_button_->SetEnabledTextColors(ink);
  }
  if (extensions_button_) {
    extensions_button_->SetEnabledTextColors(ink);
  }
  if (create_button_) {
    create_button_->SetEnabledTextColors(ink);
  }
  if (url_field_) {
    url_field_->SetColor(ink);
  }
  const auto tint = [ink](views::ImageButton* button,
                          const gfx::VectorIcon& icon) {
    if (!button) {
      return;
    }
    const ui::ImageModel model =
        ui::ImageModel::FromVectorIcon(icon, ink, 16);
    button->SetImageModel(views::Button::STATE_NORMAL, model);
    button->SetImageModel(views::Button::STATE_HOVERED, model);
    button->SetImageModel(views::Button::STATE_PRESSED, model);
    button->SetImageModel(
        views::Button::STATE_DISABLED,
        ui::ImageModel::FromVectorIcon(icon, SkColorSetA(ink, 0x66), 16));
  };
  tint(back_button_, vector_icons::kArrowBackIcon);
  tint(forward_button_, vector_icons::kArrowForwardIcon);
  tint(reload_button_, vector_icons::kRefreshIcon);
  views::View* root = parent() ? parent() : this;
  views::View::Views stack = {root};
  while (!stack.empty()) {
    views::View* view = stack.back();
    stack.pop_back();
    if (auto* label = views::AsViewClass<views::Label>(view)) {
      label->SetEnabledColor(ink);
    }
    if (auto* row = views::AsViewClass<XplorerSidebarRowButton>(view)) {
      row->SetEnabledTextColors(ink);
    }
    for (views::View* child : view->children()) {
      stack.push_back(child);
    }
  }
}

content::WebContents* XplorerSidebarChromeView::ActiveWebContents() const {
  if (!browser_ || !browser_->GetTabStripModel()) {
    return nullptr;
  }
  return browser_->GetTabStripModel()->GetActiveWebContents();
}

content::WebContents* XplorerSidebarChromeView::FindFavoriteTab(
    const GURL& url) const {
  TabStripModel* model = browser_ ? browser_->GetTabStripModel() : nullptr;
  if (!model || !url.is_valid() || url.host().empty()) {
    return nullptr;
  }
  for (int i = 0; i < model->count(); ++i) {
    content::WebContents* contents = model->GetWebContentsAt(i);
    if (contents && contents->GetVisibleURL().host() == url.host()) {
      return contents;
    }
  }
  return nullptr;
}

void XplorerSidebarChromeView::AddFavoriteFromActiveTab() {
  content::WebContents* active = ActiveWebContents();
  if (!active) {
    return;
  }
  const GURL url = active->GetVisibleURL();
  if (!url.is_valid() || !url.SchemeIsHTTPOrHTTPS()) {
    return;
  }
  std::vector<base::DictValue> apps = grok_companion::GetPinnedAppConfigs();
  for (const base::DictValue& app : apps) {
    const std::string* existing = app.FindString("url");
    if (existing && GURL(*existing).host() == url.host()) {
      return;
    }
  }
  base::DictValue added;
  added.Set("id", base::NumberToString(static_cast<int>(apps.size()) + 1));
  const std::u16string title = active->GetTitle();
  const std::u16string label =
      title.empty() ? base::UTF8ToUTF16(url.host()) : title;
  added.Set("label", base::UTF16ToUTF8(label));
  added.Set("url", url.spec());
  apps.push_back(std::move(added));
  grok_companion::SetPinnedAppConfigs(apps);
}

int XplorerSidebarChromeView::FavoriteIndex(const GURL& url,
                                            int* count) const {
  const std::vector<base::DictValue> apps =
      grok_companion::GetPinnedAppConfigs();
  if (count)
    *count = static_cast<int>(apps.size());
  for (size_t i = 0; i < apps.size(); ++i) {
    const std::string* existing = apps[i].FindString("url");
    if (existing && GURL(*existing) == url)
      return static_cast<int>(i);
  }
  return -1;
}

void XplorerSidebarChromeView::MoveFavoriteBy(const GURL& url, int delta) {
  std::vector<base::DictValue> apps = grok_companion::GetPinnedAppConfigs();
  int count = 0;
  const int from = FavoriteIndex(url, &count);
  const int to = from + delta;
  if (from < 0 || to < 0 || to >= count)
    return;
  std::swap(apps[from], apps[to]);
  grok_companion::SetPinnedAppConfigs(apps);
}

void XplorerSidebarChromeView::RemoveFavorite(const GURL& url) {
  std::vector<base::DictValue> kept;
  for (base::DictValue& app : grok_companion::GetPinnedAppConfigs()) {
    const std::string* existing = app.FindString("url");
    if (existing && GURL(*existing) == url) {
      continue;
    }
    kept.push_back(std::move(app));
  }
  grok_companion::SetPinnedAppConfigs(kept);
}

void XplorerSidebarChromeView::RenameFavorite(const GURL& url,
                                             const std::u16string& label) {
  std::vector<base::DictValue> apps = grok_companion::GetPinnedAppConfigs();
  for (base::DictValue& app : apps) {
    const std::string* existing = app.FindString("url");
    if (existing && GURL(*existing) == url) {
      app.Set("label", base::UTF16ToUTF8(label));
    }
  }
  grok_companion::SetPinnedAppConfigs(apps);
}

void XplorerSidebarChromeView::SetFavoriteUrl(const GURL& url,
                                             const GURL& new_url) {
  std::vector<base::DictValue> apps = grok_companion::GetPinnedAppConfigs();
  for (base::DictValue& app : apps) {
    const std::string* existing = app.FindString("url");
    if (existing && GURL(*existing) == url) {
      app.Set("url", new_url.spec());
    }
  }
  grok_companion::SetPinnedAppConfigs(apps);
}

void XplorerSidebarChromeView::OpenFavoriteInNewTab(const GURL& url) {
  if (!browser_ || !url.is_valid()) {
    return;
  }
  NavigateParams params(browser_, url, ui::PAGE_TRANSITION_LINK);
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  Navigate(&params);
}

void XplorerSidebarChromeView::OpenFavoriteInSplit(const GURL& url) {
  TabStripModel* model = browser_ ? browser_->GetTabStripModel() : nullptr;
  if (!model || !url.is_valid()) {
    return;
  }
  if (model->empty()) {
    OpenFavoriteInNewTab(url);
    return;
  }
  const int active = model->active_index();
  model->delegate()->AddTabAt(url, active + 1, true, std::nullopt, false);
  model->AddToNewSplit(
      {active},
      split_tabs::SplitTabVisualData(split_tabs::SplitTabLayout::kSideBySide),
      split_tabs::SplitTabCreatedSource::kTabContextMenu);
}

void XplorerSidebarChromeView::MoveFavoriteToFolder(
    const GURL& url,
    std::optional<tab_groups::TabGroupId> group) {
  TabStripModel* model = browser_ ? browser_->GetTabStripModel() : nullptr;
  if (!model || !url.is_valid()) {
    return;
  }
  model->delegate()->AddTabAt(url, -1, true, std::nullopt, false);
  const int index = model->active_index();
  if (index < 0) {
    return;
  }
  if (group.has_value()) {
    model->AddToExistingGroup({index}, *group);
  } else {
    const tab_groups::TabGroupId created = model->AddToNewGroup({index});
    model->ChangeTabGroupVisuals(
        created, tab_groups::TabGroupVisualData(
                     u"Folder", tab_groups::TabGroupColorId::kGrey));
  }
  RemoveFavorite(url);
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
