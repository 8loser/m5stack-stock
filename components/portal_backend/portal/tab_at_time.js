function ensureAtTimeNotEmpty() {
  if (s_at_time_items.length > 0) return;
  s_at_time_items.push({ id: s_at_time_next_id++, time: "09:00", prompt: "" });
}

function renderAtTimeList() {
  var box = document.getElementById("at_time_list");
  if (!box) return;
  ensureAtTimeNotEmpty();

  var html = "";
  s_at_time_items.forEach(function (item, idx) {
    var safePrompt = escHtml(item.prompt || "");
    var safeTime = normalizeAtTimeValue(item.time || "");
    html += "<div class='at-time-row'>" +
      "<div class='hint'>Entry " + (idx + 1) + "</div>" +
      "<div class='at-time-row-grid'>" +
      "<label>Time (24h)<div class='at-time-time-picker'>" +
      "<input class='at-time-input' type='time' step='60' value='" + safeTime + "' oninput=\"updateAtTimeField(" + item.id + ",'time',this.value)\"></div></label>" +
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
  s_at_time_items.push({ id: s_at_time_next_id++, time: "09:00", prompt: "" });
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
  if (field !== "time" && field !== "prompt") return;
  s_at_time_items.forEach(function (item) {
    if (item.id === id) {
      item[field] = field === "time" ? normalizeAtTimeValue(value || "") : (value || "");
    }
  });
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
