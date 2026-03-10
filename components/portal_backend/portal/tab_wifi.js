function setScanLoadingState(text) {
  var s = document.getElementById("ss");
  if (!s) return;
  s.options.length = 0;
  s.add(new Option(text || "Scanning...", ""));
  var manualBox = document.getElementById("m");
  if (manualBox) manualBox.style.display = "none";
}

function setWifiMsg(msg, ok) {
  var el = document.getElementById("wifi_msg");
  if (!el) return;
  el.textContent = msg || "";
  el.className = ok ? "ok" : "err";
}

function setWifiSubmitting(submitting) {
  var btn = document.getElementById("wifi_connect_btn");
  if (!btn) return;
  btn.disabled = !!submitting;
  btn.textContent = submitting ? "Connecting..." : "Connect";
}

function wifiConnectErrMsg(code, status) {
  if (code === "already_connecting") return "Another connect is in progress. Please wait.";
  if (code === "connect_failed") return "Connect failed. Check SSID/password and try again.";
  if (code === "timeout") return "Connect timeout. Device may still be switching network.";
  if (code === "fetch_timeout") return "Request timeout. Reconnect to portal WiFi and check device network state.";
  if (code === "network_reset") return "Connection dropped while switching network. Reconnect to portal WiFi and retry.";
  if (code === "ssid_required") return "SSID is required";
  if (code === "internal_error") {
    return "System busy. Please retry in a moment.";
  }
  if (status) return "Connect failed: HTTP " + status;
  return "Connect failed: network";
}

function connectWifiNow() {
  var btn = document.getElementById("wifi_connect_btn");
  if (btn && btn.disabled) return;

  var ss = document.getElementById("ss");
  var manual = document.getElementById("ssid_manual");
  var pw = document.getElementById("wifi_password");
  if (!ss || !manual || !pw) return;

  var selectedSsid = ss.value || "";
  var manualSsid = (manual.value || "").trim();
  var finalSsid = (selectedSsid === "__manual__" || selectedSsid === "") ? manualSsid : selectedSsid;
  if (!finalSsid) {
    setWifiMsg("SSID is required", false);
    return;
  }

  var body = "ssid=" + encodeURIComponent(selectedSsid || "__manual__") +
    "&ssid_manual=" + encodeURIComponent(manualSsid) +
    "&password=" + encodeURIComponent(pw.value || "");

  setWifiSubmitting(true);
  setWifiMsg("Submitting WiFi connect request...", true);
  fetchWithTimeout("/wifi", {
    method: "POST",
    headers: { "Content-Type": "application/x-www-form-urlencoded" },
    body: body
  }, 35000)
    .then(function (r) {
      return r.json().catch(function () { return {}; }).then(function (data) {
        var ok = !!(data && data.ok);
        if (!r.ok || !ok) {
          var err = new Error("connect_failed");
          err.code = (data && data.error) ? data.error : "internal_error";
          err.status = r.status;
          throw err;
        }
        return data || {};
      });
    })
    .then(function (data) {
      var ssid = (data && data.ssid) ? data.ssid : "";
      var msg = ssid ?
        ("Connected to " + ssid + ". Core2 will switch back to STA mode.") :
        "Connected. Core2 will switch back to STA mode.";
      setWifiMsg(msg, true);
    })
    .catch(function (e) {
      if (e && e.message === "fetch_timeout") {
        e.code = "fetch_timeout";
      } else if (e && !e.code) {
        e.code = "network_reset";
      }
      setWifiMsg(wifiConnectErrMsg(e && e.code, e && e.status), false);
    })
    .finally(function () {
      setWifiSubmitting(false);
    });
}

function loadSavedAps(generation) {
  return fetchWithTimeout("/api/saved_aps", null, PORTAL_API_TIMEOUT_MS)
    .then(function (r) { return r.json(); })
    .then(function (a) {
      if (is_stale_generation(generation)) return;
      s_saved_ssids = (a || []).map(function (x) { return x.ssid || ""; }).filter(function (x) { return x.length > 0; });

      var box = document.getElementById("saved_aps_list");
      if (!box) return;
      if (!s_saved_ssids.length) {
        box.innerHTML = "<div class='hint'>No saved networks</div>";
        return;
      }

      var h = "";
      s_saved_ssids.forEach(function (ssid) {
        var safe = ssid.replace(/'/g, "\\'");
        h += "<div class='stock-row'><div><span class='stock-symbol'>" + escHtml(ssid) + "</span></div>" +
          "<button type='button' style='width:auto;padding:6px 10px;background:#c33' onclick=\"removeSavedAp('" + safe + "')\">Remove</button></div>";
      });
      box.innerHTML = h;
    })
    .catch(function () {
      if (is_stale_generation(generation)) return;
      var box = document.getElementById("saved_aps_list");
      if (box) box.innerHTML = "<div class='err'>Load failed</div>";
      throw new Error("load_saved_aps_failed");
    });
}

function loadScan(generation) {
  return fetchWithTimeout("/api/scan", null, PORTAL_API_TIMEOUT_MS)
    .then(function (r) { return r.json(); })
    .then(function (a) {
      if (is_stale_generation(generation)) return;
      var s = document.getElementById("ss");
      if (!s) return;
      s.options.length = 0;
      s.add(new Option("-- Select AP --", ""));

      a.forEach(function (x) {
        var saved = s_saved_ssids.indexOf(x.ssid) >= 0;
        var o = new Option();
        o.textContent = x.ssid + (saved ? " [Saved]" : "") + "  (" + x.rssi + "dBm)";
        o.value = x.ssid;
        s.add(o);
      });
      s.add(new Option("Other (manual)", "__manual__"));
    })
    .catch(function () {
      if (is_stale_generation(generation)) return;
      var s = document.getElementById("ss");
      if (!s) return;
      s.options.length = 0;
      s.add(new Option("Scan failed", "__manual__"));
      var manualBox = document.getElementById("m");
      if (manualBox) manualBox.style.display = "block";
      throw new Error("load_scan_failed");
    });
}

function loadWifiData(generation) {
  return loadSavedAps(generation)
    .catch(function () {});
}

function scanWifiNow(generation) {
  setScanLoadingState("Scanning...");
  return loadScan(generation).catch(function () {});
}

function removeSavedAp(ssid) {
  var body = "ssid=" + encodeURIComponent(ssid || "");
  fetch("/api/saved_aps/remove", {
    method: "POST",
    headers: { "Content-Type": "application/x-www-form-urlencoded" },
    body: body
  })
    .then(function (r) {
      return r.json().catch(function () { return {}; }).then(function (j) { return { ok: r.ok, body: j }; });
    })
    .then(function (x) {
      if (!x.ok || !x.body.ok) return;
      return loadWifiData();
    })
    .catch(function () {});
}

function chk(s) {
  var manual = document.getElementById("m");
  if (!manual || !s) return;
  manual.style.display = (s.value === "__manual__") ? "block" : "none";
}
