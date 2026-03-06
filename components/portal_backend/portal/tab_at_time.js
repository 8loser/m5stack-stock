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

  if (out.length === 0) {
    return defaultAtTimeWeekdays();
  }

  return out;
}

function ensureAtTimeNotEmpty() {
  if (s_at_time_items.length > 0) return;
  s_at_time_items.push({
    id: s_at_time_next_id++,
    enabled: true,
    time: "09:00",
    weekdays: defaultAtTimeWeekdays(),
    prompt: ""
  });
}

function renderAtTimeList() {
  var box = document.getElementById("at_time_list");
  if (!box) return;
  ensureAtTimeNotEmpty();

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

    html += "<div class='at-time-row'>" +
      "<div class='hint'>Entry " + (idx + 1) + "</div>" +
      "<div class='at-time-meta'>" +
      "<label class='at-time-enabled'><input type='checkbox' " +
      (enabled ? "checked " : "") +
      "onchange=\"updateAtTimeField(" + item.id + ",'enabled',this.checked)\">Enable entry</label>" +
      "</div>" +
      "<div class='at-time-row-grid'>" +
      "<label>Time (24h)<div class='at-time-time-picker'>" +
      "<input class='at-time-input' type='time' step='60' value='" + safeTime + "' oninput=\"updateAtTimeField(" + item.id + ",'time',this.value)\"></div></label>" +
      "<label>Weekdays<div class='weekday-grid'>" + weekdaysHtml + "</div></label>" +
      "<label>Prompt<textarea maxlength='512' oninput=\"updateAtTimeField(" + item.id + ",'prompt',this.value)\">" +
      safePrompt + "</textarea></label>" +
      "</div>" +
      "<div class='at-time-actions'>" +
      "<button type='button' class='btn-remove-time' onclick='removeAtTimeRow(" + item.id + ")'>Remove</button>" +
      "</div>" +
      "</div>";
  });
  box.innerHTML = html;
}

function addAtTimeRow() {
  s_at_time_items.push({
    id: s_at_time_next_id++,
    enabled: true,
    time: "09:00",
    weekdays: defaultAtTimeWeekdays(),
    prompt: ""
  });
  renderAtTimeList();
  setAtTimeMsg("Added one time entry.", true);
}

function removeAtTimeRow(id) {
  s_at_time_items = s_at_time_items.filter(function (item) { return item.id !== id; });
  ensureAtTimeNotEmpty();
  renderAtTimeList();
  setAtTimeMsg("Entry removed.", true);
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
  ensureAtTimeNotEmpty();
  renderAtTimeList();
  setAtTimeMsg("", true);
}

initAtTimePage();
