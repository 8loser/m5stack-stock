var PORTAL_INIT_TIMEOUT_MS = 10000;
var PORTAL_API_TIMEOUT_MS = 8000;
var s_init_generation = 0;
var s_init_timeout_timer = 0;

var s_saved_ssids = [];
var s_stock_items = {};
var s_stock_editing_symbol = "";
var s_at_time_items = [];
var s_at_time_next_id = 1;
var s_interval_items = [];
var s_interval_next_id = 1;

var s_tab_loaded = {
  wifi: false,
  ai: false,
  telegram: false,
  stocks: false,
  at_time: false,
  interval: false
};

var s_tab_loading_promise = {
  wifi: null,
  ai: null,
  telegram: null,
  stocks: null,
  at_time: null,
  interval: null
};

var PORTAL_TAB_IDS = ["wifi", "ai", "telegram", "stocks", "at_time", "interval"];
var PORTAL_TAB_PATHS = {
  wifi: "/wifi",
  ai: "/ai",
  telegram: "/telegram",
  stocks: "/stocks",
  at_time: "/at_time",
  interval: "/interval"
};

function setInitOverlayState(state) {
  var overlay = document.getElementById("init_overlay");
  if (!overlay) return;
  if (state !== "loading" && state !== "timeout_error") {
    overlay.className = "init-overlay hidden";
    return;
  }
  overlay.className = "init-overlay " + state;
}

function is_stale_generation(generation) {
  return typeof generation === "number" && generation !== s_init_generation;
}

function stockErr(code) {
  var m = {
    invalid_format: "Symbol 必須是 4 位數字",
    duplicate_symbol: "已在清單中",
    limit_exceeded: "最多 15 檔",
    not_found_or_not_tse: "找不到代號或非 TWSE 上市",
    validate_failed: "TWSE 驗證失敗，請稍後再試",
    not_found: "清單內找不到此代號",
    invalid_threshold: "門檻需為 -99.99..99.99 的數字",
    prompt_too_long: "Alert prompt 最多 512 bytes",
    test_failed: "AI/Telegram 測試流程失敗",
    scheduler_unavailable: "Scheduler 暫時不可用，請稍後重試",
    timeout: "測試逾時，請稍後重試"
  };
  return m[code] || ("Error: " + (code || "unknown"));
}

function aiErr(code) {
  var m = {
    prompt_too_long: "Global prompt 最多 300 bytes（約 100 中文字）",
    missing_provider: "缺少 provider",
    invalid_provider: "provider 無效",
    conflict_clear_and_set: "不可同時清除與設定同一 provider key",
    save_failed_prompt: "儲存 global prompt 失敗"
  };
  return m[code] || ("Error: " + (code || "unknown"));
}

function setStocksMsg(msg, ok) {
  var el = document.getElementById("stocks_msg");
  if (!el) return;
  el.textContent = msg || "";
  el.className = ok ? "ok" : "err";
}

function setAiMsg(msg, ok) {
  var el = document.getElementById("ai_msg");
  if (!el) return;
  el.textContent = msg || "";
  el.style.whiteSpace = "pre-line";
  el.className = ok ? "ok" : "err";
}

function setAiProviderMsg(provider, msg, ok) {
  var el = document.getElementById("ai_msg_" + provider);
  if (!el) return;
  el.textContent = msg || "";
  el.style.whiteSpace = "pre-line";
  el.className = ok ? "ok" : "err";
}

function setTelegramMsg(msg, ok) {
  var el = document.getElementById("tg_msg");
  if (!el) return;
  el.textContent = msg || "";
  el.className = ok ? "ok" : "err";
}

function setAtTimeMsg(msg, ok) {
  var el = document.getElementById("at_time_msg");
  if (!el) return;
  el.textContent = msg || "";
  el.className = ok ? "ok" : "err";
}

function setIntervalMsg(msg, ok) {
  var el = document.getElementById("interval_msg");
  if (!el) return;
  el.textContent = msg || "";
  el.className = ok ? "ok" : "err";
}

function escHtml(v) {
  return String(v || "")
    .replace(/&/g, "&amp;")
    .replace(/</g, "&lt;")
    .replace(/>/g, "&gt;")
    .replace(/\"/g, "&quot;")
    .replace(/'/g, "&#39;");
}

function fetchWithTimeout(url, options, timeoutMs) {
  var timeout = (typeof timeoutMs === "number" && timeoutMs > 0) ? timeoutMs : PORTAL_API_TIMEOUT_MS;
  return Promise.race([
    fetch(url, options),
    new Promise(function (_, reject) {
      setTimeout(function () {
        reject(new Error("fetch_timeout"));
      }, timeout);
    })
  ]);
}

function loadTabData(tab, generation) {
  if (tab === "wifi") return loadWifiData(generation);
  if (tab === "ai") return loadAiConfig(generation);
  if (tab === "telegram") return loadTelegram(generation);
  if (tab === "stocks") return loadStocks(generation);
  if (tab === "at_time") {
    return initAtTimePage();
  }
  if (tab === "interval") {
    initIntervalPage();
    return Promise.resolve();
  }
  return Promise.reject(new Error("unknown tab"));
}

function getCurrentPageTab() {
  var tab = window.PORTAL_PAGE_TAB || "wifi";
  return PORTAL_TAB_IDS.indexOf(tab) >= 0 ? tab : "wifi";
}

function ensureTabLoaded(tab, generation, force_reload) {
  if (!force_reload && s_tab_loaded[tab]) {
    return Promise.resolve();
  }
  if (!force_reload && s_tab_loading_promise[tab]) {
    return s_tab_loading_promise[tab];
  }

  var p = Promise.resolve()
    .then(function () {
      return loadTabData(tab, generation);
    })
    .then(function () {
      s_tab_loaded[tab] = true;
    })
    .finally(function () {
      s_tab_loading_promise[tab] = null;
    });

  s_tab_loading_promise[tab] = p;
  return p;
}

function showTab(tab, skip_auto_load) {
  PORTAL_TAB_IDS.forEach(function (x) {
    var card = document.getElementById("card_" + x);
    var btn = document.getElementById("tab_" + x);
    if (card) card.className = "card section-card" + (x === tab ? " active" : "");
    if (btn) btn.className = "menu-btn" + (x === tab ? " active" : "");
  });

  if (tab !== getCurrentPageTab()) {
    var nextPath = PORTAL_TAB_PATHS[tab];
    if (nextPath) {
      window.location.assign(nextPath);
    }
    return;
  }

  if (skip_auto_load) {
    return;
  }
  ensureTabLoaded(tab).catch(function () {});
}

function beginBootstrapInit() {
  var tab = getCurrentPageTab();
  var generation = ++s_init_generation;
  var timed_out = false;

  if (s_init_timeout_timer) {
    clearTimeout(s_init_timeout_timer);
    s_init_timeout_timer = 0;
  }

  s_tab_loaded[tab] = false;

  setInitOverlayState("loading");
  showTab(tab, true);

  s_init_timeout_timer = setTimeout(function () {
    if (generation !== s_init_generation) return;
    timed_out = true;
    setInitOverlayState("timeout_error");
  }, PORTAL_INIT_TIMEOUT_MS);

  ensureTabLoaded(tab, generation, true)
    .catch(function () {})
    .finally(function () {
      if (generation !== s_init_generation) return;
      if (timed_out) return;
      clearTimeout(s_init_timeout_timer);
      s_init_timeout_timer = 0;
      setInitOverlayState("hidden");
    });
}

function initPortalApp() {
  var retryBtn = document.getElementById("init_retry_btn");
  if (retryBtn) {
    retryBtn.addEventListener("click", beginBootstrapInit);
  }
  beginBootstrapInit();
}

if (document.readyState === "loading") {
  document.addEventListener("DOMContentLoaded", initPortalApp);
} else {
  initPortalApp();
}
