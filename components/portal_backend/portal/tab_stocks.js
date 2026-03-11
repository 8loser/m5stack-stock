function formatPrice(v) {
  return (typeof v === "number" && isFinite(v)) ? v.toFixed(2) : "N/A";
}

function formatPercent(v) {
  if (typeof v !== "number" || !isFinite(v)) return "N/A";
  if (v > 0) return "↗" + v.toFixed(2) + "%";
  if (v < 0) return "↘" + Math.abs(v).toFixed(2) + "%";
  return "0.00%";
}

function quoteDeltaClass(q) {
  if (!q || typeof q.change_percent !== "number" || !isFinite(q.change_percent)) return "";
  if (q.change_percent > 0) return " stock-change-up";
  if (q.change_percent < 0) return " stock-change-down";
  return " stock-change-flat";
}

function quoteSummary(q) {
  if (!q || !q.available) {
    return "N/A | N/A";
  }
  return formatPrice(q.price) + " | " + formatPercent(q.change_percent);
}

function alertSummary(cfg) {
  cfg = cfg || {};
  var threshold = (typeof cfg.threshold_pct === "number" && isFinite(cfg.threshold_pct)) ? (Math.round(cfg.threshold_pct * 100) / 100) : 0;
  var p = cfg.alert_prompt || cfg.ai_prompt || "";
  var promptText = p ? ("Prompt: " + p) : "Prompt: (empty)";
  if (threshold > 0) {
    return "告警: 啟用 | 上漲 +" + threshold.toFixed(2) + "% | " + promptText;
  }
  if (threshold < 0) {
    return "告警: 啟用 | 下跌 " + threshold.toFixed(2) + "% | " + promptText;
  }
  return "告警: 停用 | 門檻 0.00% | " + promptText;
}

function hasAlertConfigForDisplay(cfg) {
  if (!cfg) return false;
  var threshold = (typeof cfg.threshold_pct === "number" && isFinite(cfg.threshold_pct)) ? cfg.threshold_pct : 0;
  var prompt = (cfg.alert_prompt || cfg.ai_prompt || "").trim();
  return threshold !== 0 || !!prompt;
}

function thresholdInputValue(v) {
  if (typeof v !== "number" || !isFinite(v)) return "0.00";
  var rounded = Math.round(v * 100) / 100;
  if (Math.abs(rounded) < 0.005) rounded = 0;
  return rounded.toFixed(2);
}

function clampThresholdValue(v) {
  if (!isFinite(v)) return 0;
  if (v < -99.99) return -99.99;
  if (v > 99.99) return 99.99;
  return Math.round(v * 100) / 100;
}

function adjustStockThreshold(sym, delta) {
  var inputId = "alert_threshold_" + sym;
  var inputEl = document.getElementById(inputId);
  if (!inputEl) return;

  var curr = parseFloat(inputEl.value);
  if (!isFinite(curr)) curr = 0;
  var next = clampThresholdValue(curr + delta);
  inputEl.value = next.toFixed(2);
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
    var stockName = (it.name || "").trim();
    var alertHtml = hasAlertConfigForDisplay(cfg)
      ? ("<div class='stock-alert'>" + escHtml(alertSummary(cfg)) + "</div>")
      : "";
    var quote = it.quote || null;
    var quoteText = quoteSummary(quote);
    var parts = quoteText.split(" | ");
    var priceText = parts.length ? parts[0] : "N/A";
    var deltaText = parts.length > 1 ? parts[1] : "N/A";
    var toneClass = quoteDeltaClass(quote);
    var limitHtml = "";
    if (quote && quote.limit_status === "up") {
      limitHtml = " <span class='stock-limit-badge stock-limit-up'>漲停</span>";
    } else if (quote && quote.limit_status === "down") {
      limitHtml = " <span class='stock-limit-badge stock-limit-down'>跌停</span>";
    }
    var quoteHtml = "<span class='stock-price" + toneClass + "'>" + escHtml(priceText) + "</span> | " +
      "<span class='stock-change" + toneClass + "'>" + escHtml(deltaText) + "</span>" + limitHtml;
    html += "<div class='stock-item'>" +
      "<div class='stock-row'><div><div class='stock-head'><div><span class='stock-symbol'>" +
      escHtml(sym) + "</span>" + (stockName ? (" <span class='stock-name'>" + escHtml(stockName) + "</span>") : "") +
      "</div><span class='hint stock-industry'>" + escHtml(it.industry || "") + "</span></div>" +
      "<div class='stock-quote'>" + quoteHtml + "</div>" + alertHtml + "</div>" +
      "<div class='stock-actions'>" +
      "<button type='button' class='btn-edit' onclick=\"editStock('" + sym + "')\">Edit</button>" +
      "<button type='button' class='btn-danger' onclick=\"removeStock('" + sym + "')\">Remove</button>" +
      "</div></div>";

    if (s_stock_editing_symbol === sym) {
      html += "<div class='stock-edit'>" +
        "<div class='stock-edit-group'>" +
          "<div class='stock-threshold-grid'>" +
            "<label class='threshold-label' for='alert_threshold_" + sym + "'>漲跌幅門檻 (%)</label>" +
            "<div class='threshold-input-inline'>" +
              "<input id='alert_threshold_" + sym + "' type='number' min='-99.99' max='99.99' step='0.01' value='" + escHtml(thresholdInputValue(cfg.threshold_pct)) + "'>" +
              "<div class='threshold-stepper'>" +
                "<button type='button' class='threshold-step-btn threshold-step-minus' title='-0.01' aria-label='decrease by 0.01' onclick=\"adjustStockThreshold('" + sym + "',-0.01)\">−</button>" +
                "<button type='button' class='threshold-step-btn threshold-step-plus' title='+0.01' aria-label='increase by 0.01' onclick=\"adjustStockThreshold('" + sym + "',0.01)\">+</button>" +
              "</div>" +
            "</div>" +
          "</div>" +
          "<div class='hint'>輸入正值代表上漲門檻，負值代表下跌門檻；0 代表停用。</div>" +
          "<label class='prompt-group-label'>Alert Prompt<textarea id='alert_prompt_" + sym + "' maxlength='512'>" + escHtml(cfg.alert_prompt || cfg.ai_prompt || "") + "</textarea></label>" +
        "</div>" +
        "<div class='stock-edit-actions'>" +
          "<button type='button' class='secondary' onclick=\"saveStockEdit('" + sym + "')\">Save</button>" +
          "<button type='button' class='btn-warning' onclick=\"clearStockAlert('" + sym + "')\">Clear</button>" +
          "<button type='button' class='btn-cancel' onclick=\"cancelStockEdit()\">Cancel</button>" +
          "<button type='button' class='secondary' onclick=\"testStockEdit('" + sym + "')\">Test</button>" +
        "</div>" +
      "</div>";
    }

    html += "</div>";
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
  var thresholdEl = document.getElementById("alert_threshold_" + sym);
  var promptEl = document.getElementById("alert_prompt_" + sym);
  if (!thresholdEl || !promptEl) return;

  var threshold = parseFloat(thresholdEl.value);
  if (!isFinite(threshold) || threshold < -99.99 || threshold > 99.99) {
    setStocksMsg(stockErr("invalid_threshold"), false);
    return;
  }
  threshold = Math.round(threshold * 100) / 100;
  if (Math.abs(threshold) < 0.005) threshold = 0;

  var prompt = (promptEl.value || "").trim();

  if (threshold !== 0 && !prompt) {
    setStocksMsg("門檻非 0 時 Alert Prompt 不可空白。", false);
    return;
  }

  var requestBody = (threshold === 0 && !prompt)
    ? { symbol: sym, clear_alert: true }
    : { symbol: sym, alert_config: { threshold_pct: threshold, alert_prompt: prompt } };

  fetch("/api/stocks/update", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify(requestBody)
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
        alert_config: item.alert_config || { threshold_pct: threshold, alert_prompt: prompt }
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
  fetch("/api/stocks/update", {
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
        alert_config: item.alert_config || { threshold_pct: 0, alert_prompt: "" }
      };
      s_stock_editing_symbol = "";
      renderStocksList();
      setStocksMsg("已清除 Alert：" + sym, true);
    })
    .catch(function (e) {
      setStocksMsg("Request failed: " + (e && e.message ? e.message : "network"), false);
    });
}

function setStocksTestOverlay(visible) {
  var el = document.getElementById("stocks_test_overlay");
  if (!el) return;
  el.className = visible ? "init-overlay loading" : "init-overlay hidden";
}

function testStockEdit(sym) {
  var thresholdEl = document.getElementById("alert_threshold_" + sym);
  var promptEl = document.getElementById("alert_prompt_" + sym);
  if (!thresholdEl || !promptEl) return;

  var threshold = parseFloat(thresholdEl.value);
  if (!isFinite(threshold) || threshold < -99.99 || threshold > 99.99) {
    setStocksMsg(stockErr("invalid_threshold"), false);
    return;
  }
  threshold = Math.round(threshold * 100) / 100;
  if (Math.abs(threshold) < 0.005) threshold = 0;

  var prompt = (promptEl.value || "").trim();
  if (!prompt) {
    setStocksMsg("Test 需要 Alert Prompt。", false);
    return;
  }

  setStocksMsg("測試中：呼叫 AI 並發送 Telegram...", true);
  setStocksTestOverlay(true);
  fetchWithTimeout("/api/stocks/test", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({
      symbol: sym,
      alert_config: {
        threshold_pct: threshold,
        alert_prompt: prompt
      }
    })
  }, 75000)
    .then(function (r) {
      return r.json().catch(function () { return {}; }).then(function (b) { return { ok: r.ok, body: b }; });
    })
    .then(function (x) {
      if (!x.ok || !x.body.ok) {
        var detail = x.body && x.body.detail ? (" | " + x.body.detail) : "";
        var msg = stockErr(x.body && x.body.error);
        setStocksMsg("測試失敗：" + msg + detail, false);
        return;
      }
      setStocksMsg("測試成功：AI 呼叫完成，Telegram 已送出。", true);
    })
    .catch(function (e) {
      setStocksMsg("Request failed: " + (e && e.message ? e.message : "network"), false);
    })
    .finally(function () {
      setStocksTestOverlay(false);
    });
}

function loadStocks(generation) {
  return fetch("/api/stocks")
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
          alert_config: it.alert_config || { threshold_pct: 0, alert_prompt: "" }
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
  fetch("/api/stocks/add", {
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
  fetch("/api/stocks/remove", {
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
