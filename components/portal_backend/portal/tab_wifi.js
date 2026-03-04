function loadSavedAps(generation) {
  return fetch("/saved_aps")
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
  return fetch("/scan")
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
    .then(function () {
      return loadScan(generation);
    });
}

function removeSavedAp(ssid) {
  var body = "ssid=" + encodeURIComponent(ssid || "");
  fetch("/saved_aps/remove", {
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
