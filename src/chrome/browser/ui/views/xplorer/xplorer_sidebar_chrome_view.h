// Copyright 2026 The Xplorer Authors.
// Use of this source code is governed by a BSD-style license.

#ifndef CHROME_BROWSER_UI_VIEWS_XPLORER_XPLORER_SIDEBAR_CHROME_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_XPLORER_XPLORER_SIDEBAR_CHROME_VIEW_H_

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/view.h"
#include "url/gurl.h"

class BrowserWindowInterface;
class Profile;

namespace views {
class ImageButton;
class Textfield;
class View;
}  // namespace views

namespace xplorer {

// Arc-style chrome at the top of the vertical tab strip: pinned apps, the
// space name, then the address field. The window toolbar is hidden while
// vertical tabs are on, so this field is the location bar.
class XplorerSidebarChromeView : public views::View,
                                  public views::TextfieldController,
                                  public TabStripModelObserver {
  METADATA_HEADER(XplorerSidebarChromeView, views::View)

 public:
  XplorerSidebarChromeView(BrowserWindowInterface* browser, Profile* profile);
  XplorerSidebarChromeView(const XplorerSidebarChromeView&) = delete;
  XplorerSidebarChromeView& operator=(const XplorerSidebarChromeView&) =
      delete;
  ~XplorerSidebarChromeView() override;

  // Cmd-L / Ctrl-L. Selects the current text so typing replaces it.
  void FocusUrlField();
  void OnUrlFieldBlur();

  // views::TextfieldController:
  bool HandleKeyEvent(views::Textfield* sender,
                      const ui::KeyEvent& key_event) override;

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;
  void OnTabChangedAt(tabs::TabInterface* tab,
                      TabChangeType change_type) override;
  void OnTabStripModelDestroyed(TabStripModel* tab_strip_model) override;

 private:
  void NavigateFromField();
  void UpdateUrlField();
  void UpdateNavButtons();
  void ReloadChrome();
  void OpenPinned(const GURL& url);
  void RunCommand(int command_id);

  const raw_ptr<BrowserWindowInterface> browser_;
  const raw_ptr<Profile> profile_;
  raw_ptr<views::View> pins_ = nullptr;
  raw_ptr<views::View> space_swatch_ = nullptr;
  raw_ptr<views::ImageButton> back_button_ = nullptr;
  raw_ptr<views::ImageButton> forward_button_ = nullptr;
  raw_ptr<views::ImageButton> reload_button_ = nullptr;
  raw_ptr<views::Textfield> url_field_ = nullptr;
  bool showing_stop_ = false;
  bool observing_tabs_ = false;

  base::CallbackListSubscription onboarding_subscription_;
};

}  // namespace xplorer

#endif  // CHROME_BROWSER_UI_VIEWS_XPLORER_XPLORER_SIDEBAR_CHROME_VIEW_H_
