var AT_TIME_WEEKDAYS = [
  { value: 1, label: "Mon" },
  { value: 2, label: "Tue" },
  { value: 3, label: "Wed" },
  { value: 4, label: "Thu" },
  { value: 5, label: "Fri" },
  { value: 6, label: "Sat" },
  { value: 0, label: "Sun" }
];

function defaultAtTimeWeekdays() {
  return [1, 2, 3, 4, 5, 6, 0];
}

function normalizeAtTimeWeekdays(value) {
  var source = Array.isArray(value) ? value : [];
  var uniq = {};
  var out = [];
  source.forEach(function (day) {
    var n = Number(day);
    if (!isFinite(n)) return;
    n = Math.round(n);
    if (n < 0 || n > 6) return;
    if (uniq[n]) return;
    uniq[n] = true;
    out.push(n);
  });
  if (out.length === 0) return defaultAtTimeWeekdays();
  return out;
}

function weekdaysBitmaskToArray(mask) {
  var arr = [];
  for (var i = 0; i < 7; i++) {
    if (mask & (1 << i)) arr.push(i);
  }
  return arr.length > 0 ? arr : defaultAtTimeWeekdays();
}

function weekdaysArrayToBitmask(arr) {
  var mask = 0;
  arr.forEach(function (day) {
    var n = Number(day);
    if (n >= 0 && n <= 6) mask |= (1 << n);
  });
  return mask || 127;
}

function renderAtTimeList() {
  var box = document.getElementById("at_time_list");
  if (!box) return;

  if (s_at_time_items.length === 0) {
    box.innerHTML = "<div class='hint'>No entries. Click Add Time to create one.</div>";
    return;
  }

  var html = "";
  s_at_time_items.forEach(function (item, idx) {
    var safePrompt = escHtml(item.prompt || "");
    var safeTime = normalizeAtTimeValue(item.time || "");
    var enabled = item.enabled !== false;
    var weekdaySet = {};
    normalizeAtTimeWeekdays(item.weekdays).forEach(function (day) {
      weekdaySet[day] = true;
    });
    var weekdaysHtml = "";
    AT_TIME_WEEKDAYS.forEach(function (day) {
      var isActive = !!weekdaySet[day.value];
      weekdaysHtml += "<button type='button' class='weekday-btn" +
        (isActive ? " active" : "") + "' " +
        "aria-pressed='" + (isActive ? "true" : "false") + "' " +
        "onclick='toggleAtTimeWeekday(" + item.id + "," + day.value + ")'>" +
        "<span class='weekday-name'>" + day.label + "</span>" +
        "</button>";
    });

    var isNew = typeof item.serverIdx === "undefined";
    var entryLabel = isNew ? "New entry" : "Entry " + (idx + 1);

    html += "<div class='at-time-row'>" +
      "<div class='at-time-head'>" +
      "<div class='hint'>" + entryLabel + "</div>" +
      "<div class='at-time-meta'>" +
      "<label class='at-time-enabled'><input type='checkbox' " +
      (enabled ? "checked " : "") +
      "onchange=\"updateAtTimeField(" + item.id + ",'enabled',this.checked)\">Enable entry</label>" +
      "</div>" +
      "</div>" +
      "<div class='at-time-row-grid'>" +
      "<label>Time<div class='at-time-time-picker'>" +
      "<input class='at-time-input' type='time' step='60' value='" + safeTime + "' oninput=\"updateAtTimeField(" + item.id + ",'time',this.value)\"></div></label>" +
      "<label>Weekdays<div class='weekday-grid'>" + weekdaysHtml + "</div></label>" +
      "<label class='at-time-prompt'>Prompt<textarea maxlength='512' oninput=\"updateAtTimeField(" + item.id + ",'prompt',this.value)\">" +
      safePrompt + "</textarea></label>" +
      "</div>" +
      "<div class='at-time-actions'>" +
      "<button type='button' class='btn-remove-time' onclick='removeAtTimeRow(" + item.id + ")'>Remove</button>" +
      "<button type='button' class='btn-save-time' onclick='saveAtTimeRow(" + item.id + ")'>Save</button>" +
      "</div>" +
      "</div>";
  });
  box.innerHTML = html;
}

function addAtTimeRow() {
  if (s_at_time_items.length >= 8) {
    setAtTimeMsg("Maximum 8 entries.", false);
    return;
  }
  s_at_time_items.push({
    id: s_at_time_next_id++,
    enabled: true,
    time: "09:00",
    weekdays: defaultAtTimeWeekdays(),
    prompt: ""
  });
  renderAtTimeList();
  setAtTimeMsg("New entry added. Click Save to write to device.", true);
}

function removeAtTimeRow(id) {
  var item = null;
  var itemIdx = -1;
  s_at_time_items.forEach(function (it, i) {
    if (it.id === id) { item = it; itemIdx = i; }
  });
  if (!item) return;

  if (!window.confirm("Remove this entry?")) return;

  if (typeof item.serverIdx === "undefined") {
    s_at_time_items.splice(itemIdx, 1);
    renderAtTimeList();
    setAtTimeMsg("Entry removed (was unsaved).", true);
    return;
  }

  setAtTimeMsg("Removing...", true);
  fetchWithTimeout("/api/at_time/remove", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ idx: item.serverIdx })
  }).then(function (r) { return r.json(); })
    .then(function (data) {
      if (!data.ok) {
        setAtTimeMsg("Remove failed: " + (data.error || "unknown"), false);
        return;
      }
      setAtTimeMsg("Entry removed.", true);
      return reloadAtTimeFromServer();
    })
    .catch(function (err) {
      setAtTimeMsg("Remove failed: " + err.message, false);
    });
}

function saveAtTimeRow(id) {
  var item = null;
  var itemIdx = -1;
  s_at_time_items.forEach(function (it, i) {
    if (it.id === id) { item = it; itemIdx = i; }
  });
  if (!item) return;

  var timeParts = normalizeAtTimeValue(item.time || "").split(":");
  var hour = parseInt(timeParts[0], 10);
  var minute = parseInt(timeParts[1], 10);
  var weekdays = weekdaysArrayToBitmask(normalizeAtTimeWeekdays(item.weekdays));

  var serverIdx = typeof item.serverIdx === "undefined" ? itemIdx : item.serverIdx;

  setAtTimeMsg("Saving entry " + (itemIdx + 1) + "...", true);
  fetchWithTimeout("/api/at_time/save", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({
      idx: serverIdx,
      enabled: item.enabled !== false,
      hour: hour,
      minute: minute,
      weekdays: weekdays,
      prompt: item.prompt || ""
    })
  }).then(function (r) { return r.json(); })
    .then(function (data) {
      if (!data.ok) {
        setAtTimeMsg("Save failed: " + (data.error || "unknown"), false);
        return;
      }
      setAtTimeMsg("Entry " + (itemIdx + 1) + " saved.", true);
      return reloadAtTimeFromServer();
    })
    .catch(function (err) {
      setAtTimeMsg("Save failed: " + err.message, false);
    });
}

function reloadAtTimeFromServer() {
  return fetchWithTimeout("/api/at_time")
    .then(function (r) { return r.json(); })
    .then(function (data) {
      if (!data.ok) return;
      s_at_time_items = [];
      s_at_time_next_id = 1;
      var items = data.items || [];
      for (var i = 0; i < items.length; i++) {
        var entry = items[i];
        var hh = String(entry.hour);
        var mm = String(entry.minute);
        if (hh.length < 2) hh = "0" + hh;
        if (mm.length < 2) mm = "0" + mm;
        s_at_time_items.push({
          id: s_at_time_next_id++,
          serverIdx: entry.idx,
          enabled: entry.enabled,
          time: hh + ":" + mm,
          weekdays: weekdaysBitmaskToArray(entry.weekdays),
          prompt: entry.prompt || ""
        });
      }
      renderAtTimeList();
    });
}

function updateAtTimeField(id, field, value) {
  if (field !== "time" && field !== "prompt" && field !== "enabled") return;
  s_at_time_items.forEach(function (item) {
    if (item.id === id) {
      if (field === "time") {
        item.time = normalizeAtTimeValue(value || "");
      } else if (field === "enabled") {
        item.enabled = !!value;
      } else {
        item.prompt = value || "";
      }
    }
  });
}

function toggleAtTimeWeekday(id, weekday) {
  s_at_time_items.forEach(function (item) {
    if (item.id !== id) return;
    var weekdays = normalizeAtTimeWeekdays(item.weekdays);
    var idx = weekdays.indexOf(weekday);
    if (idx >= 0) {
      weekdays.splice(idx, 1);
    } else {
      weekdays.push(weekday);
    }
    item.weekdays = normalizeAtTimeWeekdays(weekdays);
  });
  renderAtTimeList();
}

function normalizeAtTimeValue(value) {
  var m = /^([01][0-9]|2[0-3]):([0-5][0-9])$/.exec(value || "");
  return m ? (m[1] + ":" + m[2]) : "09:00";
}

function initAtTimePage() {
  s_at_time_items = [];
  s_at_time_next_id = 1;
  setAtTimeMsg("Loading...", true);
  return reloadAtTimeFromServer()
    .then(function () {
      setAtTimeMsg("", true);
    })
    .catch(function (err) {
      setAtTimeMsg("Load failed: " + err.message, false);
      renderAtTimeList();
    });
}

initAtTimePage();
