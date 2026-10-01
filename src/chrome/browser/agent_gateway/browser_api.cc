// Copyright 2026 The Xplorer Authors.
// Use of this source code is governed by a BSD-style license.

#include "chrome/browser/agent_gateway/browser_api.h"

#include <algorithm>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/cancelable_task_tracker.h"
#include "chrome/browser/agent_gateway/tab_ownership.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "ui/base/base_window.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/views/widget/widget.h"
#include "chrome/common/chrome_isolated_world_ids.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/render_frame_host.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/tabs/split_tab_metrics.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/tabs/public/tab_group.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_types.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

namespace agent_gateway {
namespace {

TabStripModel* FindTabStrip(const std::string& tab_id, int* out_index) {
  std::vector<std::string> parts;
  size_t colon = tab_id.find(':');
  if (colon == std::string::npos)
    return nullptr;
  int sid = 0, index = 0;
  if (!base::StringToInt(tab_id.substr(0, colon), &sid) ||
      !base::StringToInt(tab_id.substr(colon + 1), &index)) {
    return nullptr;
  }
  for (BrowserWindowInterface* browser : GetAllBrowserWindowInterfaces()) {
    if (browser->GetSessionID().id() != sid)
      continue;
    TabStripModel* model = browser->GetTabStripModel();
    if (index >= 0 && index < model->count()) {
      *out_index = index;
      return model;
    }
  }
  return nullptr;
}

void AppendBookmarkNode(const bookmarks::BookmarkNode* node,
                        base::ListValue& out,
                        int depth) {
  if (!node)
    return;
  if (node->is_url()) {
    base::DictValue b;
    b.Set("id", base::NumberToString(node->id()));
    b.Set("type", "url");
    b.Set("title", base::UTF16ToUTF8(node->GetTitle()));
    b.Set("url", node->url().spec());
    b.Set("parent_id", base::NumberToString(node->parent()->id()));
    b.Set("depth", depth);
    out.Append(std::move(b));
  } else {
    base::DictValue f;
    f.Set("id", base::NumberToString(node->id()));
    f.Set("type", "folder");
    f.Set("title", base::UTF16ToUTF8(node->GetTitle()));
    f.Set("parent_id",
          node->parent() ? base::NumberToString(node->parent()->id()) : "0");
    f.Set("depth", depth);
    out.Append(std::move(f));
    for (const auto& child : node->children())
      AppendBookmarkNode(child.get(), out, depth + 1);
  }
}

const bookmarks::BookmarkNode* FindBookmarkNode(
    bookmarks::BookmarkModel* model,
    const std::string& id_str) {
  int64_t id = 0;
  if (!base::StringToInt64(id_str, &id))
    return nullptr;
  return bookmarks::GetBookmarkNodeByID(model, id);
}

}  // namespace

void BrowserApi::ListBookmarks(DictCallback callback) {
  Profile* profile = ProfileManager::GetLastUsedProfile();
  bookmarks::BookmarkModel* model =
      BookmarkModelFactory::GetForBrowserContext(profile);
  base::DictValue result;
  base::ListValue items;
  if (!model || !model->loaded()) {
    result.Set("error", "bookmark model not loaded");
    std::move(callback).Run(std::move(result));
    return;
  }
  AppendBookmarkNode(model->bookmark_bar_node(), items, 0);
  AppendBookmarkNode(model->other_node(), items, 0);
  result.Set("bookmarks", std::move(items));
  std::move(callback).Run(std::move(result));
}

void BrowserApi::AddBookmark(const std::string& url,
                             const std::string& title,
                             const std::string& parent_id,
                             DictCallback callback) {
  Profile* profile = ProfileManager::GetLastUsedProfile();
  bookmarks::BookmarkModel* model =
      BookmarkModelFactory::GetForBrowserContext(profile);
  base::DictValue result;
  if (!model || !model->loaded()) {
    result.Set("error", "bookmark model not loaded");
    std::move(callback).Run(std::move(result));
    return;
  }
  const bookmarks::BookmarkNode* parent = model->bookmark_bar_node();
  if (!parent_id.empty()) {
    parent = FindBookmarkNode(model, parent_id);
    if (!parent || !parent->is_folder()) {
      result.Set("error", "parent folder not found");
      std::move(callback).Run(std::move(result));
      return;
    }
  }
  GURL gurl(url);
  if (!gurl.is_valid()) {
    result.Set("error", "invalid url");
    std::move(callback).Run(std::move(result));
    return;
  }
  std::u16string u_title = base::UTF8ToUTF16(title);
  if (u_title.empty())
    u_title = base::UTF8ToUTF16(gurl.host());
  const bookmarks::BookmarkNode* node =
      model->AddURL(parent, parent->children().size(), u_title, gurl);
  result.Set("ok", true);
  result.Set("id", base::NumberToString(node->id()));
  std::move(callback).Run(std::move(result));
}

void BrowserApi::RemoveBookmark(const std::string& id,
                                DictCallback callback) {
  Profile* profile = ProfileManager::GetLastUsedProfile();
  bookmarks::BookmarkModel* model =
      BookmarkModelFactory::GetForBrowserContext(profile);
  base::DictValue result;
  const bookmarks::BookmarkNode* node = FindBookmarkNode(model, id);
  if (!node) {
    result.Set("error", "bookmark not found");
    std::move(callback).Run(std::move(result));
    return;
  }
  model->Remove(node, bookmarks::metrics::BookmarkEditSource::kOther,
                FROM_HERE);
  result.Set("ok", true);
  std::move(callback).Run(std::move(result));
}

void BrowserApi::QueryHistory(const std::string& query,
                              int limit,
                              DictCallback callback) {
  Profile* profile = ProfileManager::GetLastUsedProfile();
  history::HistoryService* history =
      HistoryServiceFactory::GetForProfile(profile,
                                           ServiceAccessType::EXPLICIT_ACCESS);
  base::DictValue result;
  if (!history) {
    result.Set("error", "history service unavailable");
    std::move(callback).Run(std::move(result));
    return;
  }
  if (limit <= 0)
    limit = 50;
  auto* tracker = new base::CancelableTaskTracker();
  history->QueryHistory(
      base::UTF8ToUTF16(query), history::QueryOptions(),
      base::BindOnce(
          [](DictCallback cb, int lim, base::CancelableTaskTracker* trk,
             history::QueryResults results) {
            delete trk;
            base::DictValue out;
            base::ListValue entries;
            int count = 0;
            for (const auto& row : results) {
              if (count++ >= lim)
                break;
              base::DictValue e;
              e.Set("url", row.url().spec());
              e.Set("title", base::UTF16ToUTF8(row.title()));
              e.Set("visit_count", static_cast<int>(row.visit_count()));
              e.Set("typed_count", static_cast<int>(row.typed_count()));
              e.Set("last_visit",
                    static_cast<double>(
                        row.last_visit().InSecondsFSinceUnixEpoch()));
              entries.Append(std::move(e));
            }
            out.Set("history", std::move(entries));
            std::move(cb).Run(std::move(out));
          },
          std::move(callback), limit, tracker),
      tracker);
}

void BrowserApi::ActivateTab(const std::string& tab_id,
                             DictCallback callback) {
  base::DictValue result;
  int index = 0;
  TabStripModel* model = FindTabStrip(tab_id, &index);
  if (!model) {
    result.Set("error", "tab not found");
    std::move(callback).Run(std::move(result));
    return;
  }
  model->ActivateTabAt(index);
  result.Set("ok", true);
  std::move(callback).Run(std::move(result));
}

void BrowserApi::CloseTab(const std::string& tab_id, DictCallback callback) {
  base::DictValue result;
  int index = 0;
  TabStripModel* model = FindTabStrip(tab_id, &index);
  if (!model) {
    result.Set("error", "tab not found");
    std::move(callback).Run(std::move(result));
    return;
  }
  // A persistent bookmark tab (the xAI "Bookmarks" group) must NOT be closeable
  // via the API: an agent or scheduled task could otherwise close it, which drops
  // it from the bookmark config -- the recurring bookmark-config drift was a
  // scheduled-task agent DELETE'ing bookmark tabs over idle gaps. The user can
  // still close one via the native tab X (a different, non-API path), which is the
  // intended closeable behavior; agents manage bookmarks via /api/settings.
  content::WebContents* wc = model->GetWebContentsAt(index);
  TabOwnership* own = wc ? TabOwnership::Get(wc) : nullptr;
  if (own && own->bookmark_node_id != 0) {
    result.Set("error",
               "cannot close a bookmark tab via the API; manage bookmarks via "
               "/api/settings");
    std::move(callback).Run(std::move(result));
    return;
  }
  model->CloseWebContentsAt(index, TabCloseTypes::CLOSE_USER_GESTURE);
  result.Set("ok", true);
  std::move(callback).Run(std::move(result));
}

bool HostIs(const std::string& host, const char* domain) {
  if (host == domain)
    return true;
  return base::EndsWith(host, std::string(".") + domain,
                        base::CompareCase::SENSITIVE);
}

std::string BareHost(const std::string& host) {
  if (base::StartsWith(host, "www.", base::CompareCase::SENSITIVE))
    return host.substr(4);
  return host;
}

// Named folder, or empty when the tab should only join a folder if another
// open tab shares its site.
std::string TabCategory(const GURL& url, const std::string& title) {
  const std::string spec = base::ToLowerASCII(url.spec());
  const std::string host = base::ToLowerASCII(url.host());
  const std::string lower_title = base::ToLowerASCII(title);
  if (url.SchemeIs("chrome") || url.SchemeIs("chrome-untrusted"))
    return "Browser";
  if (host == "localhost" ||
      (host == "127.0.0.1" && url.port() != "9334"))
    return "Code";
  if (host == "127.0.0.1" &&
      (url.port() == "9334" || spec.find(":9334") != std::string::npos))
    return "Xplor";
  if (HostIs(host, "grok.com") || HostIs(host, "grok.x.ai"))
    return "Grok";
  if (HostIs(host, "x.com") || HostIs(host, "twitter.com"))
    return "X";
  if (HostIs(host, "youtube.com") || HostIs(host, "youtu.be"))
    return "Video";
  if (HostIs(host, "gmail.com") || HostIs(host, "mail.google.com") ||
      HostIs(host, "outlook.live.com") || HostIs(host, "outlook.office.com") ||
      spec.find("mail.google.com") != std::string::npos ||
      spec.find("/gmail") != std::string::npos)
    return "Mail";
  if (spec.find("docs.google.com") != std::string::npos ||
      spec.find("drive.google.com") != std::string::npos ||
      HostIs(host, "notion.so") || HostIs(host, "notion.site"))
    return "Docs";
  if (spec.find("maps.google.") != std::string::npos ||
      HostIs(host, "maps.google.com"))
    return "Maps";
  if (HostIs(host, "google.com") || HostIs(host, "bing.com") ||
      HostIs(host, "duckduckgo.com"))
    return "Search";
  if (HostIs(host, "github.com") || HostIs(host, "gitlab.com") ||
      HostIs(host, "stackoverflow.com"))
    return "Code";
  if (HostIs(host, "wikipedia.org") || HostIs(host, "grokipedia.com"))
    return "Reference";
  if (HostIs(host, "amazon.com") || HostIs(host, "amazon.co.uk") ||
      HostIs(host, "ebay.com"))
    return "Shopping";
  if (HostIs(host, "reddit.com") || HostIs(host, "facebook.com") ||
      HostIs(host, "instagram.com") || HostIs(host, "linkedin.com") ||
      HostIs(host, "tiktok.com"))
    return "Social";
  if (host.find("news") != std::string::npos || HostIs(host, "cnn.com") ||
      host.find("bbc.") != std::string::npos || HostIs(host, "nytimes.com") ||
      HostIs(host, "theguardian.com") || HostIs(host, "reuters.com") ||
      HostIs(host, "apnews.com") ||
      lower_title.find("news") != std::string::npos)
    return "News";
  if (spec.find("/travel/") != std::string::npos ||
      spec.find("flight") != std::string::npos ||
      lower_title.find("flight") != std::string::npos ||
      HostIs(host, "airbnb.com") || HostIs(host, "booking.com"))
    return "Travel";
  if (HostIs(host, "nature.org") || HostIs(host, "worldwildlife.org") ||
      HostIs(host, "nationalgeographic.com") || HostIs(host, "nps.gov") ||
      HostIs(host, "audubon.org") || HostIs(host, "conservation.org") ||
      HostIs(host, "iucn.org") || HostIs(host, "fs.usda.gov") ||
      host.find("earthobservatory") != std::string::npos ||
      lower_title.find("wildlife") != std::string::npos ||
      lower_title.find("conservation") != std::string::npos)
    return "Nature";
  return std::string();
}

tab_groups::TabGroupColorId ColorForCategory(const std::string& category) {
  if (category == "Grok" || category == "X")
    return tab_groups::TabGroupColorId::kPurple;
  if (category == "Xplor" || category == "Search" || category == "Mail")
    return tab_groups::TabGroupColorId::kBlue;
  if (category == "News" || category == "Video")
    return tab_groups::TabGroupColorId::kRed;
  if (category == "Travel" || category == "Maps" || category == "Nature")
    return tab_groups::TabGroupColorId::kGreen;
  if (category == "Code" || category == "Development")
    return tab_groups::TabGroupColorId::kYellow;
  if (category == "Reference" || category == "Wiki")
    return tab_groups::TabGroupColorId::kCyan;
  if (category == "Social")
    return tab_groups::TabGroupColorId::kPink;
  if (category == "Shopping")
    return tab_groups::TabGroupColorId::kOrange;
  if (category == "Browser" || category == "Docs")
    return tab_groups::TabGroupColorId::kGrey;
  return tab_groups::TabGroupColorId::kGrey;
}

namespace {
// TabStripModel::AddToNewGroup() CHECK-aborts the entire browser process on
// duplicate or out-of-range indices. A model driving the browser can easily pass
// repeated, reordered, or stale tab ids (especially after opening many tabs), so
// always dedupe, drop anything no longer in the strip, and sort ascending before
// grouping — a bad tool argument must degrade gracefully, never crash.
std::optional<tab_groups::TabGroupId> FindGroupTitled(
    TabStripModel* model,
    const std::string& title) {
  TabGroupModel* groups = model->group_model();
  if (!groups)
    return std::nullopt;
  for (const tab_groups::TabGroupId& id : groups->ListTabGroups()) {
    const TabGroup* group = groups->GetTabGroup(id);
    if (!group || !group->visual_data())
      continue;
    if (base::UTF16ToUTF8(group->visual_data()->title()) == title)
      return id;
  }
  return std::nullopt;
}

std::vector<int> SanitizeTabIndices(TabStripModel* model,
                                    std::vector<int> indices) {
  std::vector<int> clean;
  for (int i : indices) {
    if (model->ContainsIndex(i) &&
        std::find(clean.begin(), clean.end(), i) == clean.end()) {
      clean.push_back(i);
    }
  }
  std::sort(clean.begin(), clean.end());
  return clean;
}
}  // namespace

base::ListValue BrowserApi::SnapshotOrganizableTabs() {
  base::ListValue tabs;
  for (BrowserWindowInterface* browser : GetAllBrowserWindowInterfaces()) {
    TabStripModel* model = browser->GetTabStripModel();
    const int sid = browser->GetSessionID().id();
    for (int i = 0; i < model->count(); ++i) {
      content::WebContents* wc = model->GetWebContentsAt(i);
      if (!wc)
        continue;
      TabOwnership* own = TabOwnership::Get(wc);
      if (own && (own->bookmark_node_id != 0 || !own->owner.empty() ||
                  !own->task_id.empty())) {
        continue;
      }
      base::DictValue t;
      t.Set("id", base::NumberToString(sid) + ":" + base::NumberToString(i));
      t.Set("url", wc->GetLastCommittedURL().spec());
      t.Set("title", base::UTF16ToUTF8(wc->GetTitle()));
      tabs.Append(std::move(t));
    }
  }
  return tabs;
}

void BrowserApi::ApplyModelTabGroups(base::ListValue groups,
                                     DictCallback callback) {
  std::map<std::string, content::WebContents*> by_id;
  for (BrowserWindowInterface* browser : GetAllBrowserWindowInterfaces()) {
    TabStripModel* model = browser->GetTabStripModel();
    const int sid = browser->GetSessionID().id();
    for (int i = 0; i < model->count(); ++i) {
      content::WebContents* wc = model->GetWebContentsAt(i);
      if (wc)
        by_id[base::NumberToString(sid) + ":" + base::NumberToString(i)] = wc;
    }
  }

  base::DictValue result;
  base::ListValue groups_out;
  int groups_created = 0;
  int assigned = 0;
  std::vector<content::WebContents*> used;
  for (const auto& v : groups) {
    if (!v.is_dict())
      continue;
    const std::string* title = v.GetDict().FindString("title");
    const base::ListValue* ids = v.GetDict().FindList("tab_ids");
    if (!title || title->empty() || !ids)
      continue;
    std::string folder = *title;
    if (folder.size() > 48)
      folder.resize(48);
    std::vector<content::WebContents*> members;
    for (const auto& id_value : *ids) {
      if (!id_value.is_string())
        continue;
      auto it = by_id.find(id_value.GetString());
      if (it == by_id.end() || !it->second)
        continue;
      if (std::find(used.begin(), used.end(), it->second) != used.end())
        continue;
      if (std::find(members.begin(), members.end(), it->second) !=
          members.end())
        continue;
      members.push_back(it->second);
    }
    if (members.empty())
      continue;
    TabStripModel* strip = nullptr;
    for (BrowserWindowInterface* browser : GetAllBrowserWindowInterfaces()) {
      TabStripModel* candidate = browser->GetTabStripModel();
      for (int i = 0; i < candidate->count(); ++i) {
        if (std::find(members.begin(), members.end(),
                      candidate->GetWebContentsAt(i)) != members.end()) {
          strip = candidate;
          break;
        }
      }
      if (strip)
        break;
    }
    if (!strip)
      continue;
    std::vector<int> indices;
    for (int i = 0; i < strip->count(); ++i) {
      content::WebContents* wc = strip->GetWebContentsAt(i);
      if (std::find(members.begin(), members.end(), wc) != members.end())
        indices.push_back(i);
    }
    indices = SanitizeTabIndices(strip, std::move(indices));
    if (indices.empty())
      continue;
    std::optional<tab_groups::TabGroupId> existing =
        FindGroupTitled(strip, folder);
    if (existing) {
      strip->AddToExistingGroup(indices, *existing, /*add_to_end=*/true);
    } else {
      existing = strip->AddToNewGroup(indices);
      ++groups_created;
    }
    const tab_groups::TabGroupId group = *existing;
    tab_groups::TabGroupVisualData visual_data(
        base::UTF8ToUTF16(folder), ColorForCategory(folder));
    strip->ChangeTabGroupVisuals(group, visual_data);
    for (int idx : indices) {
      if (content::WebContents* wc = strip->GetWebContentsAt(idx))
        used.push_back(wc);
    }
    assigned += static_cast<int>(indices.size());
    base::DictValue g;
    g.Set("title", folder);
    g.Set("group_id", group.ToString());
    base::ListValue tab_ids;
    const int sid = [&] {
      for (BrowserWindowInterface* browser : GetAllBrowserWindowInterfaces()) {
        if (browser->GetTabStripModel() == strip)
          return browser->GetSessionID().id();
      }
      return 0;
    }();
    for (int idx : indices) {
      tab_ids.Append(base::NumberToString(sid) + ":" +
                     base::NumberToString(idx));
    }
    g.Set("tab_ids", std::move(tab_ids));
    groups_out.Append(std::move(g));
  }
  result.Set("ok", true);
  result.Set("tabs", assigned);
  result.Set("groups_created", groups_created);
  result.Set("groups", std::move(groups_out));
  std::move(callback).Run(std::move(result));
}

void BrowserApi::OrganizeTabs(DictCallback callback) {
  base::DictValue result;
  base::ListValue groups_out;
  int total_tabs = 0;
  int groups_created = 0;

  for (BrowserWindowInterface* browser : GetAllBrowserWindowInterfaces()) {
    TabStripModel* model = browser->GetTabStripModel();
    // Keyed by folder title. Values are the contents to move; indices are
    // resolved after each move because grouping reorders the strip.
    std::map<std::string, std::vector<content::WebContents*>> buckets;
    std::map<std::string, bool> named_folder;
    for (int i = 0; i < model->count(); ++i) {
      content::WebContents* wc = model->GetWebContentsAt(i);
      if (!wc)
        continue;
      // Skip managed tabs (bookmarks / agent-owned / scheduled-task). They belong
      // to AgentTabGrouper's persistent Bookmarks/Agent/Scheduled groups; letting
      // organize bucket them by category scatters them out of those groups and the
      // grouper does not reclaim a tab already in a non-managed group.
      TabOwnership* own = TabOwnership::Get(wc);
      if (own && (own->bookmark_node_id != 0 || !own->owner.empty() ||
                  !own->task_id.empty())) {
        continue;
      }
      ++total_tabs;
      const GURL& url = wc->GetLastCommittedURL();
      const std::string named =
          TabCategory(url, base::UTF16ToUTF8(wc->GetTitle()));
      std::string key = named;
      if (key.empty())
        key = BareHost(base::ToLowerASCII(url.host()));
      if (key.empty())
        continue;
      named_folder[key] = !named.empty();
      buckets[key].push_back(wc);
    }
    const int sid = browser->GetSessionID().id();
    for (auto& [category, contents] : buckets) {
      // A lone unrecognized site stays loose. Two of the same site, or any
      // recognized kind of page, become a folder.
      if (!named_folder[category] && contents.size() < 2)
        continue;
      std::vector<int> indices;
      for (int i = 0; i < model->count(); ++i) {
        content::WebContents* wc = model->GetWebContentsAt(i);
        if (std::find(contents.begin(), contents.end(), wc) != contents.end())
          indices.push_back(i);
      }
      indices = SanitizeTabIndices(model, std::move(indices));
      if (indices.empty())
        continue;
      std::optional<tab_groups::TabGroupId> existing =
          FindGroupTitled(model, category);
      if (existing) {
        model->AddToExistingGroup(indices, *existing, /*add_to_end=*/true);
      } else {
        existing = model->AddToNewGroup(indices);
        ++groups_created;
      }
      const tab_groups::TabGroupId group = *existing;
      tab_groups::TabGroupVisualData visual_data(
          base::UTF8ToUTF16(category), ColorForCategory(category));
      model->ChangeTabGroupVisuals(group, visual_data);

      base::DictValue g;
      g.Set("title", category);
      g.Set("group_id", group.ToString());
      base::ListValue tab_ids;
      for (int idx : indices) {
        tab_ids.Append(base::NumberToString(sid) + ":" +
                       base::NumberToString(idx));
      }
      g.Set("tab_ids", std::move(tab_ids));
      groups_out.Append(std::move(g));
    }
  }

  result.Set("ok", true);
  result.Set("tabs", total_tabs);
  result.Set("groups_created", groups_created);
  result.Set("groups", std::move(groups_out));
  std::move(callback).Run(std::move(result));
}

void BrowserApi::GroupTabs(const std::vector<std::string>& tab_ids,
                            const std::string& title,
                            DictCallback callback) {
  base::DictValue result;
  if (tab_ids.empty()) {
    result.Set("error", "no tabs specified");
    std::move(callback).Run(std::move(result));
    return;
  }
  TabStripModel* model = nullptr;
  std::vector<int> indices;
  for (const std::string& id : tab_ids) {
    int index = 0;
    TabStripModel* m = FindTabStrip(id, &index);
    if (!m) {
      result.Set("error", "tab not found: " + id);
      std::move(callback).Run(std::move(result));
      return;
    }
    if (!model)
      model = m;
    else if (model != m) {
      result.Set("error", "tabs must be in the same window");
      std::move(callback).Run(std::move(result));
      return;
    }
    indices.push_back(index);
  }
  indices = SanitizeTabIndices(model, std::move(indices));
  if (indices.empty()) {
    result.Set("error", "no valid tabs to group");
    std::move(callback).Run(std::move(result));
    return;
  }
  tab_groups::TabGroupId group = model->AddToNewGroup(indices);
  if (!title.empty()) {
    tab_groups::TabGroupVisualData visual_data(base::UTF8ToUTF16(title),
                                               tab_groups::TabGroupColorId::kGrey);
    model->ChangeTabGroupVisuals(group, visual_data);
  }
  result.Set("ok", true);
  result.Set("group_id", group.ToString());
  std::move(callback).Run(std::move(result));
}

void BrowserApi::SplitTab(const std::string& tab_id,
                          const std::string& layout,
                          DictCallback callback) {
  base::DictValue result;
  int index = 0;
  TabStripModel* model = FindTabStrip(tab_id, &index);
  if (!model) {
    result.Set("error", "tab not found");
    std::move(callback).Run(std::move(result));
    return;
  }
  model->ActivateTabAt(index);
  BrowserWindowInterface* browser = nullptr;
  for (BrowserWindowInterface* b : GetAllBrowserWindowInterfaces()) {
    if (b->GetTabStripModel() == model) {
      browser = b;
      break;
    }
  }
  if (!browser) {
    result.Set("error", "browser window not found");
    std::move(callback).Run(std::move(result));
    return;
  }
  split_tabs::SplitTabLayout split_layout =
      layout == "stacked" ? split_tabs::SplitTabLayout::kStacked
                          : split_tabs::SplitTabLayout::kSideBySide;
  chrome::NewSplitTab(browser, split_layout,
                      split_tabs::SplitTabCreatedSource::kExtensionsApi);
  result.Set("ok", true);
  std::move(callback).Run(std::move(result));
}

namespace {

// Same readability-style pass the agent gateway uses: drop page chrome, keep
// the main article text.
constexpr char16_t kReadPageJs[] = uR"js(
(() => {
  const kill = 'script,style,noscript,svg,nav,footer,header,aside,iframe';
  const doc = document.cloneNode(true);
  doc.querySelectorAll(kill).forEach(n => n.remove());
  const main = doc.querySelector('main,article,[role=main]') || doc.body;
  return (main ? main.innerText : '').replace(/\n{3,}/g, '\n\n').trim();
})()
)js";

// Enough for a long article without blowing up the request.
constexpr size_t kMaxPageTextBytes = 24000;

}  // namespace

void BrowserApi::ReadActiveTab(DictCallback callback) {
  base::DictValue result;
  BrowserWindowInterface* bwi =
      GetLastActiveBrowserWindowInterfaceWithAnyProfile();
  tabs::TabInterface* tab = bwi ? bwi->GetActiveTabInterface() : nullptr;
  content::WebContents* wc = tab ? tab->GetContents() : nullptr;
  if (!wc) {
    result.Set("error", "no active tab");
    std::move(callback).Run(std::move(result));
    return;
  }
  const GURL url = wc->GetLastCommittedURL();
  result.Set("title", base::UTF16ToUTF8(wc->GetTitle()));
  result.Set("url", url.spec());
  if (!url.SchemeIsHTTPOrHTTPS()) {
    result.Set("text", "");
    std::move(callback).Run(std::move(result));
    return;
  }
  wc->GetPrimaryMainFrame()->ExecuteJavaScriptInIsolatedWorld(
      kReadPageJs,
      base::BindOnce(
          [](base::DictValue result, DictCallback callback, base::Value value) {
            std::string text = value.is_string() ? value.GetString() : "";
            base::TruncateUTF8ToByteSize(text, kMaxPageTextBytes, &text);
            result.Set("text", std::move(text));
            std::move(callback).Run(std::move(result));
          },
          std::move(result), std::move(callback)),
      ISOLATED_WORLD_ID_CHROME_INTERNAL);
}

void BrowserApi::GetTheme(DictCallback callback) {
  Profile* profile = ProfileManager::GetLastUsedProfile();
  ThemeService* theme = ThemeServiceFactory::GetForProfile(profile);
  base::DictValue result;
  if (!theme) {
    result.Set("error", "theme service unavailable");
    std::move(callback).Run(std::move(result));
    return;
  }
  switch (theme->GetBrowserColorScheme()) {
    case ThemeService::BrowserColorScheme::kDark:
      result.Set("color_scheme", "dark");
      break;
    case ThemeService::BrowserColorScheme::kLight:
      result.Set("color_scheme", "light");
      break;
    case ThemeService::BrowserColorScheme::kSystem:
    default:
      result.Set("color_scheme", "system");
      break;
  }
  result.Set("using_custom_theme", theme->UsingExtensionTheme());

  // The colors Chrome computed for the current theme, so Xplor's own pages
  // (chat, settings, new tab) can match the window like Chrome's side panels
  // and new-tab page do. Absent for the default theme: those pages keep their
  // neutral black/white look.
  const bool themed = theme->GetUserColor().has_value() ||
                      theme->UsingExtensionTheme() ||
                      theme->UsingAutogeneratedTheme();
  result.Set("themed", themed);
  BrowserWindowInterface* bwi =
      GetLastActiveBrowserWindowInterfaceWithAnyProfile();
  views::Widget* widget =
      bwi && bwi->GetWindow()
          ? views::Widget::GetWidgetForNativeWindow(
                bwi->GetWindow()->GetNativeWindow())
          : nullptr;
  const ui::ColorProvider* colors =
      widget ? widget->GetColorProvider() : nullptr;
  if (themed && colors) {
    auto hex = [colors](ui::ColorId id) {
      const SkColor c = colors->GetColor(id);
      return base::StringPrintf("#%02x%02x%02x", SkColorGetR(c),
                                SkColorGetG(c), SkColorGetB(c));
    };
    base::DictValue palette;
    palette.Set("frame", hex(ui::kColorSysBase));
    palette.Set("surface", hex(ui::kColorSysBaseContainer));
    palette.Set("surface_elevated", hex(ui::kColorSysBaseContainerElevated));
    palette.Set("text", hex(ui::kColorSysOnSurface));
    palette.Set("text_secondary", hex(ui::kColorSysOnSurfaceSubtle));
    palette.Set("divider", hex(ui::kColorSysDivider));
    palette.Set("primary", hex(ui::kColorSysPrimary));
    palette.Set("on_primary", hex(ui::kColorSysOnPrimary));
    palette.Set("toolbar", hex(kColorToolbar));
    result.Set("palette", std::move(palette));
  }
  std::move(callback).Run(std::move(result));
}

void BrowserApi::SetTheme(const std::string& color_scheme,
                          DictCallback callback) {
  Profile* profile = ProfileManager::GetLastUsedProfile();
  ThemeService* theme = ThemeServiceFactory::GetForProfile(profile);
  base::DictValue result;
  if (!theme) {
    result.Set("error", "theme service unavailable");
    std::move(callback).Run(std::move(result));
    return;
  }
  ThemeService::BrowserColorScheme scheme =
      ThemeService::BrowserColorScheme::kSystem;
  if (color_scheme == "dark")
    scheme = ThemeService::BrowserColorScheme::kDark;
  else if (color_scheme == "light")
    scheme = ThemeService::BrowserColorScheme::kLight;
  theme->SetBrowserColorScheme(scheme);
  result.Set("ok", true);
  result.Set("color_scheme", color_scheme);
  std::move(callback).Run(std::move(result));
}

}  // namespace agent_gateway