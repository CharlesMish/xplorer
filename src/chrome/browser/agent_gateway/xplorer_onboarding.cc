// Copyright 2026 The Xplorer Authors.
// Use of this source code is governed by a BSD-style license.

#include "chrome/browser/agent_gateway/xplorer_onboarding.h"

#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/importer/external_process_importer_host.h"
#include "chrome/browser/importer/importer_list.h"
#include "chrome/browser/importer/profile_writer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/shell_integration.h"
#include "components/user_data_importer/common/importer_data_types.h"
#include "content/public/browser/browser_thread.h"

namespace xplorer {
namespace {

std::string DefaultStateName(shell_integration::DefaultWebClientState state) {
  switch (state) {
    case shell_integration::IS_DEFAULT:
      return "default";
    case shell_integration::NOT_DEFAULT:
      return "not_default";
    case shell_integration::OTHER_MODE_IS_DEFAULT:
      return "other_mode";
    case shell_integration::UNKNOWN_DEFAULT:
    case shell_integration::NUM_DEFAULT_STATES:
      return "unknown";
  }
  return "unknown";
}

class OnboardingController : public importer::ImporterProgressObserver {
 public:
  static OnboardingController* Get() {
    static base::NoDestructor<OnboardingController> controller;
    return controller.get();
  }

  void Detect(base::OnceCallback<void(base::ListValue)> callback) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    pending_.push_back(std::move(callback));
    if (detecting_)
      return;
    detecting_ = true;
    list_ = std::make_unique<ImporterList>();
    list_->DetectSourceProfiles(
        g_browser_process->GetApplicationLocale(),
        /*include_interactive_profiles=*/true,
        base::BindOnce(&OnboardingController::OnDetected,
                       base::Unretained(this)));
  }

  base::DictValue StartImport(int index,
                              bool bookmarks,
                              bool history,
                              bool passwords,
                              bool search_engines) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    base::DictValue result;
    if (!list_ || index < 0 || index >= static_cast<int>(list_->count())) {
      result.Set("ok", false);
      result.Set("error", "Choose a browser from the list, then try again.");
      return result;
    }
    Profile* profile = ProfileManager::GetLastUsedProfile();
    if (!profile) {
      result.Set("ok", false);
      result.Set("error", "No browser profile is open.");
      return result;
    }
    const user_data_importer::SourceProfile& source =
        list_->GetSourceProfileAt(static_cast<size_t>(index));
    uint16_t items = user_data_importer::NONE;
    if (bookmarks)
      items |= user_data_importer::FAVORITES;
    if (history)
      items |= user_data_importer::HISTORY;
    if (passwords)
      items |= user_data_importer::PASSWORDS;
    if (search_engines)
      items |= user_data_importer::SEARCH_ENGINES;
    items &= source.services_supported;
    if (!items) {
      result.Set("ok", false);
      result.Set("error", "That browser has none of the selected data.");
      return result;
    }
    if (host_)
      host_->set_observer(nullptr);
    running_ = true;
    done_ = false;
    ok_ = false;
    error_.clear();
    host_ = new ExternalProcessImporterHost();
    host_->set_headless();
    host_->set_observer(this);
    host_->StartImportSettings(source, profile, items,
                               new ProfileWriter(profile));
    result.Set("ok", true);
    result.Set("running", true);
    result.Set("browser", source.importer_name);
    return result;
  }

  base::DictValue Status() const {
    base::DictValue d;
    d.Set("running", running_);
    d.Set("done", done_);
    d.Set("ok", ok_);
    if (!error_.empty())
      d.Set("error", error_);
    return d;
  }

  void ImportStarted() override {}
  void ImportItemStarted(user_data_importer::ImportItem item) override {}
  void ImportItemEnded(user_data_importer::ImportItem item) override {
    ok_ = true;
  }
  void ImportEnded() override {
    running_ = false;
    done_ = true;
    if (!ok_ && error_.empty())
      error_ = "Import finished without bringing anything over.";
    if (host_)
      host_->set_observer(nullptr);
    host_ = nullptr;
  }

 private:
  void OnDetected() {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    detecting_ = false;
    base::ListValue browsers;
    if (list_) {
      for (size_t i = 0; i < list_->count(); ++i) {
        const user_data_importer::SourceProfile& source =
            list_->GetSourceProfileAt(i);
        const uint16_t services = source.services_supported;
        base::DictValue row;
        row.Set("name", source.importer_name);
        row.Set("index", static_cast<int>(i));
        row.Set("profileName", source.profile);
        row.Set("history",
                (services & user_data_importer::HISTORY) != 0);
        row.Set("favorites",
                (services & user_data_importer::FAVORITES) != 0);
        row.Set("passwords",
                (services & user_data_importer::PASSWORDS) != 0);
        row.Set("search",
                (services & user_data_importer::SEARCH_ENGINES) != 0);
        browsers.Append(std::move(row));
      }
    }
    std::vector<base::OnceCallback<void(base::ListValue)>> pending;
    pending.swap(pending_);
    for (auto& callback : pending)
      std::move(callback).Run(browsers.Clone());
  }

  std::unique_ptr<ImporterList> list_;
  raw_ptr<ExternalProcessImporterHost> host_ = nullptr;
  std::vector<base::OnceCallback<void(base::ListValue)>> pending_;
  bool detecting_ = false;
  bool running_ = false;
  bool done_ = false;
  bool ok_ = false;
  std::string error_;
};

}  // namespace

void DetectImportBrowsers(base::OnceCallback<void(base::ListValue)> callback) {
  OnboardingController::Get()->Detect(std::move(callback));
}

base::DictValue StartBrowserImport(int index,
                                   bool bookmarks,
                                   bool history,
                                   bool passwords,
                                   bool search_engines) {
  return OnboardingController::Get()->StartImport(
      index, bookmarks, history, passwords, search_engines);
}

base::DictValue BrowserImportStatus() {
  return OnboardingController::Get()->Status();
}

void CheckDefaultBrowser(
    base::OnceCallback<void(bool, const std::string&)> callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto worker = base::MakeRefCounted<shell_integration::DefaultBrowserWorker>();
  worker->StartCheckIsDefault(base::BindOnce(
      [](base::OnceCallback<void(bool, const std::string&)> callback,
         shell_integration::DefaultWebClientState state) {
        std::move(callback).Run(state == shell_integration::IS_DEFAULT,
                                DefaultStateName(state));
      },
      std::move(callback)));
}

void SetDefaultBrowser(
    base::OnceCallback<void(bool, const std::string&)> callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto worker = base::MakeRefCounted<shell_integration::DefaultBrowserWorker>();
  worker->set_interactive_permitted(true);
  worker->StartSetAsDefault(base::BindOnce(
      [](base::OnceCallback<void(bool, const std::string&)> callback,
         shell_integration::DefaultWebClientState state) {
        std::move(callback).Run(state == shell_integration::IS_DEFAULT,
                                DefaultStateName(state));
      },
      std::move(callback)));
}

}  // namespace xplorer
