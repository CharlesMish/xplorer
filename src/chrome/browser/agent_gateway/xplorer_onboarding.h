// Copyright 2026 The Xplorer Authors.
// Use of this source code is governed by a BSD-style license.

#ifndef CHROME_BROWSER_AGENT_GATEWAY_XPLORER_ONBOARDING_H_
#define CHROME_BROWSER_AGENT_GATEWAY_XPLORER_ONBOARDING_H_

#include "base/functional/callback.h"
#include "base/values.h"

namespace xplorer {

// UI thread. Detects Safari, Firefox, and other browsers Chromium can import
// from, then runs |callback| with a list of {name,index,profileName,history,
// favorites,passwords,search}.
void DetectImportBrowsers(base::OnceCallback<void(base::ListValue)> callback);

// UI thread. |index| is an index from the last DetectImportBrowsers result.
// Imports the selected item kinds. Returns {ok,error?,running}.
base::DictValue StartBrowserImport(int index,
                                   bool bookmarks,
                                   bool history,
                                   bool passwords,
                                   bool search_engines);

// UI thread. {running, done, ok, error?}
base::DictValue BrowserImportStatus();

// Callbacks run on the sequence that starts the check (post to the UI thread
// before calling). |is_default| is true only when this install is the default.
void CheckDefaultBrowser(
    base::OnceCallback<void(bool is_default, const std::string& state)>
        callback);
void SetDefaultBrowser(
    base::OnceCallback<void(bool is_default, const std::string& state)>
        callback);

}  // namespace xplorer

#endif  // CHROME_BROWSER_AGENT_GATEWAY_XPLORER_ONBOARDING_H_
