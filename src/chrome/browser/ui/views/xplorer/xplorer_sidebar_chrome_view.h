// Copyright 2026 The Xplorer Authors.
// Use of this source code is governed by a BSD-style license.

#ifndef CHROME_BROWSER_UI_VIEWS_XPLORER_XPLORER_SIDEBAR_CHROME_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_XPLORER_XPLORER_SIDEBAR_CHROME_VIEW_H_

#include <optional>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "components/tab_groups/tab_group_id.h"

namespace content {
class WebContents;
}  // namespace content
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/mojom/themes.mojom.h"
#include "ui/menus/simple_menu_model.h"
#include "ui/views/context_menu_controller.h"
#include "base/memory/weak_ptr.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget_observer.h"
#include "url/gurl.h"

class BrowserWindowInterface;
class Profile;

namespace views {
class ImageButton;
class Label;
class LabelButton;
class MenuRunner;
class Textfield;
class View;
class Widget;
}  // namespace views

namespace xplorer {

// Arc-style chrome at the top of the vertical tab strip: pinned apps, the
// space name, then the address field. The window toolbar is hidden while
// vertical tabs are on, so this field is the location bar.
class XplorerSidebarChromeView : public views::View,
                                  public views::TextfieldController,
                                  public views::WidgetObserver,
                                  public views::ContextMenuController,
                                  public ui::SimpleMenuModel::Delegate,
                                  public TabStripModelObserver {
  METADATA_HEADER(XplorerSidebarChromeView, views::View)

 public:
  XplorerSidebarChromeView(BrowserWindowInterface* browser, Profile* profile);
  XplorerSidebarChromeView(const XplorerSidebarChromeView&) = delete;
  XplorerSidebarChromeView& operator=(const XplorerSidebarChromeView&) =
      delete;
  ~XplorerSidebarChromeView() override;

  // Cmd-L opens the URL popup. Startup focus must not, or the popup
  // sticks open on chrome://newtab.
  void FocusUrlField(bool user_initiated);
  void OnUrlFieldBlur();

  // Favorites are the icon row. Chrome has no Arc "space"; a favorite is a
  // saved URL, and a folder is a native tab group.
  BrowserWindowInterface* browser() const { return browser_; }
  content::WebContents* ActiveWebContents() const;

  void AddFavoriteFromActiveTab();
  void RemoveFavorite(const GURL& url);
  // Moves a favorite one slot left (-1) or right (+1) in the row.
  void MoveFavoriteBy(const GURL& url, int delta);
  // Position of |url| in the row and the number of favorites.
  int FavoriteIndex(const GURL& url, int* count) const;
  void RenameFavorite(const GURL& url, const std::u16string& label);
  void SetFavoriteUrl(const GURL& url, const GURL& new_url);
  void OpenFavoriteInNewTab(const GURL& url);
  void OpenFavoriteInSplit(const GURL& url);
  void MoveFavoriteToFolder(const GURL& url,
                            std::optional<tab_groups::TabGroupId> group);
  content::WebContents* FindFavoriteTab(const GURL& url) const;

  // views::TextfieldController:
  bool HandleKeyEvent(views::Textfield* sender,
                      const ui::KeyEvent& key_event) override;
  bool HandleMouseEvent(views::Textfield* sender,
                        const ui::MouseEvent& mouse_event) override;

  // views::WidgetObserver:
  void OnWidgetActivationChanged(views::Widget* widget, bool active) override;
  void OnWidgetDestroying(views::Widget* widget) override;

  // views::ContextMenuController:
  void ShowContextMenuForViewImpl(
      views::View* source,
      const gfx::Point& point,
      ui::mojom::MenuSourceType source_type) override;

  // ui::SimpleMenuModel::Delegate:
  void ExecuteCommand(int command_id, int event_flags) override;
  bool IsCommandIdEnabled(int command_id) const override;
  bool IsCommandIdChecked(int command_id) const override;

  // TabStripModelObserver:
  void OnTabGroupChanged(const TabGroupChange& change) override;
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;
  void OnTabChangedAt(tabs::TabInterface* tab,
                      TabChangeType change_type) override;
  void OnTabStripModelDestroyed(TabStripModel* tab_strip_model) override;

 private:
  void NavigateFromField();
  void NavigateFromText(const std::u16string& text);
  void ShowUrlPopup();
  void CloseUrlPopup();
  void UpdateUrlField();
  void UpdateNavButtons();
  void ReloadChrome();
  void OpenPinned(const GURL& url);
  void RunCommand(int command_id);
  void ToggleChat();
  void OpenExtensions();
  void HostExtensions();
  void ReturnExtensions();
  void ShowSpaceMenu(const gfx::Point& point);
  void ApplyPanelColor(SkColor color);
  void ShowCreateMenu(const gfx::Point& point);
  void NewFolder();
  void BeginRename();
  void CommitRename();
  void ApplySidebarTone();

  const raw_ptr<BrowserWindowInterface> browser_;
  const raw_ptr<Profile> profile_;
  raw_ptr<views::View> pins_ = nullptr;
  raw_ptr<views::View> space_swatch_ = nullptr;
  raw_ptr<views::Label> space_title_ = nullptr;
  raw_ptr<views::Textfield> space_editor_ = nullptr;
  raw_ptr<views::LabelButton> chat_button_ = nullptr;
  raw_ptr<views::LabelButton> extensions_button_ = nullptr;
  raw_ptr<views::LabelButton> create_button_ = nullptr;
  raw_ptr<views::ImageButton> back_button_ = nullptr;
  raw_ptr<views::ImageButton> forward_button_ = nullptr;
  raw_ptr<views::ImageButton> reload_button_ = nullptr;
  raw_ptr<views::Textfield> url_field_ = nullptr;
  raw_ptr<views::View> tools_ = nullptr;
  // "Today" heads the loose tabs. Hidden when every tab sits in a folder.
  raw_ptr<views::View> today_label_ = nullptr;
  void UpdateTodayLabel();
  // New tabs open at the end of the strip, below the folders. Move loose
  // tabs back up so they stay together under "Today".
  void KeepLooseTabsTogether();
  raw_ptr<views::View> extensions_view_ = nullptr;
  GURL full_url_;
  bool editing_url_ = false;
  bool showing_stop_ = false;
  bool observing_tabs_ = false;

  base::CallbackListSubscription onboarding_subscription_;
  std::unique_ptr<ui::SimpleMenuModel> menu_model_;
  std::unique_ptr<ui::SimpleMenuModel> theme_menu_;
  std::unique_ptr<ui::SimpleMenuModel> tone_menu_;
  std::unique_ptr<ui::SimpleMenuModel> profile_menu_;
  ui::mojom::BrowserColorVariant panel_variant_ =
      ui::mojom::BrowserColorVariant::kTonalSpot;
  std::unique_ptr<views::MenuRunner> menu_runner_;
  base::RepeatingTimer tone_timer_;

  base::WeakPtrFactory<XplorerSidebarChromeView> weak_factory_{this};
};

}  // namespace xplorer

#endif  // CHROME_BROWSER_UI_VIEWS_XPLORER_XPLORER_SIDEBAR_CHROME_VIEW_H_
