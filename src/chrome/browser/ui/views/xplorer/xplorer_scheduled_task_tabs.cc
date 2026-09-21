// Copyright 2026 The Xplorer Authors.
// Use of this source code is governed by a BSD-style license.

#include "chrome/browser/ui/views/xplorer/xplorer_scheduled_task_tabs.h"

#include "chrome/browser/ui/browser.h"
#include "components/tabs/public/tab_interface.h"

namespace xplorer {

void SetTabRowVisible(Browser* browser,
                      tabs::TabInterface* tab,
                      bool visible) {
  // M153 removed VerticalTabStripView. Hiding a tab row from the vertical
  // strip needs a port onto views/tabs/common/TabStripView. Until then the
  // row stays visible; grouping still works.
  (void)browser;
  (void)tab;
  (void)visible;
}

}  // namespace xplorer
