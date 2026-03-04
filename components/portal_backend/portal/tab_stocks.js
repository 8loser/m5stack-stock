function formatPrice(v) {
  return (typeof v === "number" && isFinite(v)) ? v.toFixed(2) : "N/A";
}

function formatPercent(v) {
  if (typeof v !== "number" || !isFinite(v)) return "N/A";
  var s = v > 0 ? "+" : "";
  return s + v.toFixed(2) + "%";
}

function quoteSummary(q) {
  if (!q || !q.available) {
    return "最新: N/A | 漲幅: N/A";
  }
  var s = "最新: " + formatPrice(q.price) + " | 漲幅: " + formatPercent(q.change_percent);
  if (q.limit_status === "up") s += " | 漲停";
  if (q.limit_status === "down") s += " | 跌停";
  return s;
}

function alertSummary(cfg) {
  cfg = cfg || {};
  var enabled = !!cfg.enabled;
  var up = (typeof cfg.up_threshold_pct === "number" && isFinite(cfg.up_threshold_pct)) ? cfg.up_threshold_pct.toFixed(2) : "0.00";
  var down = (typeof cfg.down_threshold_pct === "number" && isFinite(cfg.down_threshold_pct)) ? cfg.down_threshold_pct.toFixed(2) : "0.00";
  var p = cfg.ai_prompt || "";
  var promptText = p ? ("Prompt: " + p) : "Prompt: (empty)";
  return (enabled ? "告警: 啟用" : "告警: 停用") + " | 上漲 " + up + "% | 下跌 " + down + "% | " + promptText;
}

function thresholdInputValue(v) {
  if (typeof v !== "number" || !isFinite(v)) return "0.00";
  return (Math.round(v * 100) / 100).toFixed(2);
}

function renderStocksList() {
  var box = document.getElementById("stocks_list");
  if (!box) return;

  var syms = Object.keys(s_stock_items);
  if (!syms.length) {
    box.innerHTML = "<div class='hint'>No stocks configured</div>";
    return;
  }

  var html = "";
  syms.forEach(function (sym) {
    var it = s_stock_items[sym];
    var cfg = it.alert_config || {};
    html += "<div class='stock-row'><div><span class='stock-symbol'>" +
      escHtml(sym) + "</span><br><span>" + escHtml(it.name || "") + "</span><br><span class='hint'>" +
      escHtml(it.industry || "") + "</span><div class='stock-quote'>" + escHtml(quoteSummary(it.quote)) +
      "</div><div class='stock-alert'>" + escHtml(alertSummary(cfg)) + "</div></div>" +
      "<div class='stock-actions'>" +
      "<button type='button' class='btn-edit' onclick=\"editStock('" + sym + "')\">Edit</button>" +
      "<button type='button' class='btn-danger' onclick=\"removeStock('" + sym + "')\">Remove</button>" +
      "</div></div>";

    if (s_stock_editing_symbol === sym) {
      html += "<div class='stock-edit'>" +
        "<label class='stock-edit-toggle'><input id='alert_enabled_" + sym + "' type='checkbox'" + (cfg.enabled ? " checked" : "") + ">Enable Alert</label>" +
        "<div class='stock-edit-grid'>" +
          "<label>Up Threshold (%)<input id='alert_up_" + sym + "' type='number' min='0' max='99.99' step='0.01' value='" + escHtml(thresholdInputValue(cfg.up_threshold_pct)) + "'></label>" +
          "<label>Down Threshold (%)<input id='alert_down_" + sym + "' type='number' min='0' max='99.99' step='0.01' value='" + escHtml(thresholdInputValue(cfg.down_threshold_pct)) + "'></label>" +
        "</div>" +
        "<label>AI Prompt<textarea id='alert_prompt_" + sym + "' maxlength='512'>" + escHtml(cfg.ai_prompt || "") + "</textarea></label>" +
        "<div class='stock-edit-actions'>" +
          "<button type='button' class='secondary' onclick=\"saveStockEdit('" + sym + "')\">Save</button>" +
          "<button type='button' class='btn-warning' onclick=\"clearStockAlert('" + sym + "')\">Clear Alert</button>" +
          "<button type='button' class='btn-cancel' onclick=\"cancelStockEdit()\">Cancel</button>" +
        "</div>" +
      "</div>";
    }
  });

  box.innerHTML = html;
}

function editStock(sym) {
  s_stock_editing_symbol = sym || "";
  renderStocksList();
}

function cancelStockEdit() {
  s_stock_editing_symbol = "";
  renderStocksList();
}

function saveStockEdit(sym) {
  var enabledEl = document.getElementById("alert_enabled_" + sym);
  var upEl = document.getElementById("alert_up_" + sym);
  var downEl = document.getElementById("alert_down_" + sym);
  var promptEl = document.getElementById("alert_prompt_" + sym);
  if (!enabledEl || !upEl || !downEl || !promptEl) return;

  var up = parseFloat(upEl.value);
  var down = parseFloat(downEl.value);
  if (!isFinite(up) || !isFinite(down) || up < 0 || down < 0 || up > 99.99 || down > 99.99) {
    setStocksMsg(stockErr("invalid_threshold"), false);
    return;
  }

  var cfg = {
    enabled: !!enabledEl.checked,
    up_threshold_pct: up,
    down_threshold_pct: down,
    ai_prompt: (promptEl.value || "").trim()
  };

  if (!cfg.ai_prompt) {
    setStocksMsg("AI Prompt 不可空白；若要移除 Alert，請使用 Clear Alert。", false);
    return;
  }

  fetch("/stocks/update", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ symbol: sym, alert_config: cfg })
  })
    .then(function (r) {
      return r.json().catch(function () { return {}; }).then(function (b) { return { ok: r.ok, body: b }; });
    })
    .then(function (x) {
      if (!x.ok || !x.body.ok) {
        setStocksMsg(stockErr(x.body.error), false);
        return;
      }
      var item = x.body.item || {};
      s_stock_items[sym] = {
        name: item.name || "",
        industry: item.industry || "",
        quote: item.quote || null,
        alert_config: item.alert_config || cfg
      };
      s_stock_editing_symbol = "";
      renderStocksList();
      setStocksMsg("更新成功：" + sym + " | " + alertSummary(s_stock_items[sym].alert_config), true);
    })
    .catch(function (e) {
      setStocksMsg("Request failed: " + (e && e.message ? e.message : "network"), false);
    });
}

function clearStockAlert(sym) {
  if (!confirm("確定要清除 " + sym + " 的 Alert 設定？")) {
    return;
  }
  fetch("/stocks/update", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ symbol: sym, clear_alert: true })
  })
    .then(function (r) {
      return r.json().catch(function () { return {}; }).then(function (b) { return { ok: r.ok, body: b }; });
    })
    .then(function (x) {
      if (!x.ok || !x.body.ok) {
        setStocksMsg(stockErr(x.body.error), false);
        return;
      }
      var item = x.body.item || {};
      s_stock_items[sym] = {
        name: item.name || "",
        industry: item.industry || "",
        quote: item.quote || null,
        alert_config: item.alert_config || { enabled: false, up_threshold_pct: 0, down_threshold_pct: 0, ai_prompt: "" }
      };
      s_stock_editing_symbol = "";
      renderStocksList();
      setStocksMsg("已清除 Alert：" + sym, true);
    })
    .catch(function (e) {
      setStocksMsg("Request failed: " + (e && e.message ? e.message : "network"), false);
    });
}

function loadStocks(generation) {
  return fetch("/stocks")
    .then(function (r) { return r.json(); })
    .then(function (d) {
      if (is_stale_generation(generation)) return;
      s_stock_items = {};
      if (!d.items || !d.items.length) {
        document.getElementById("stocks_list").innerHTML = "<div class='hint'>No stocks configured</div>";
        return;
      }
      d.items.forEach(function (it) {
        s_stock_items[it.symbol] = {
          name: it.name || "",
          industry: it.industry || "",
          quote: it.quote || null,
          alert_config: it.alert_config || { enabled: false, up_threshold_pct: 0, down_threshold_pct: 0, ai_prompt: "" }
        };
      });
      if (s_stock_editing_symbol && !s_stock_items[s_stock_editing_symbol]) {
        s_stock_editing_symbol = "";
      }
      renderStocksList();
    })
    .catch(function () {
      if (is_stale_generation(generation)) return;
      document.getElementById("stocks_list").innerHTML = "<div class='err'>Load failed</div>";
      throw new Error("load_stocks_failed");
    });
}

function addStock() {
  var sym = (document.getElementById("stock_symbol").value || "").trim();
  fetch("/stocks/add", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ symbol: sym })
  })
    .then(function (r) {
      return r.json().catch(function () { return {}; }).then(function (b) { return { ok: r.ok, body: b }; });
    })
    .then(function (x) {
      if (!x.ok || !x.body.ok) {
        setStocksMsg(stockErr(x.body.error), false);
        return;
      }
      setStocksMsg("新增成功：" + x.body.item.symbol + " " + (x.body.item.name || "") +
        " / " + (x.body.item.industry || "") + " | " + quoteSummary(x.body.item.quote), true);
      document.getElementById("stock_symbol").value = "";
      s_stock_editing_symbol = "";
      loadStocks().catch(function () {});
    })
    .catch(function (e) {
      setStocksMsg("Request failed: " + (e && e.message ? e.message : "network"), false);
    });
}

function removeStock(sym) {
  if (!confirm("確定要移除股票 " + sym + " 嗎？此操作也會刪除該股 Alert 設定。")) {
    return;
  }
  fetch("/stocks/remove", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ symbol: sym })
  })
    .then(function (r) {
      return r.json().catch(function () { return {}; }).then(function (b) { return { ok: r.ok, body: b }; });
    })
    .then(function (x) {
      if (!x.ok || !x.body.ok) {
        setStocksMsg(stockErr(x.body.error), false);
        return;
      }
      var it = s_stock_items[sym] || {};
      setStocksMsg("已移除：" + sym + " " + (it.name || "") + " / " + (it.industry || ""), true);
      if (s_stock_editing_symbol === sym) {
        s_stock_editing_symbol = "";
      }
      loadStocks().catch(function () {});
    })
    .catch(function (e) {
      setStocksMsg("Request failed: " + (e && e.message ? e.message : "network"), false);
    });
}
