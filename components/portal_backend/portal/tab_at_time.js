function ensureAtTimeNotEmpty() {
  if (s_at_time_items.length > 0) return;
  s_at_time_items.push({ id: s_at_time_next_id++, time: "", prompt: "" });
}

function renderAtTimeList() {
  var box = document.getElementById("at_time_list");
  if (!box) return;
  ensureAtTimeNotEmpty();

  var html = "";
  s_at_time_items.forEach(function (item, idx) {
    var safePrompt = escHtml(item.prompt || "");
    var parts = getAtTimeParts(item.time || "");
    html += "<div class='at-time-row'>" +
      "<div class='hint'>Entry " + (idx + 1) + "</div>" +
      "<div class='at-time-row-grid'>" +
      "<label>Time (24h)<div class='at-time-time-picker'>" +
      "<select class='at-time-select' onchange=\"updateAtTimePart(" + item.id + ",'hour',this.value)\">" +
      buildAtTimeNumberOptions(23, parts.hour) +
      "</select><span class='at-time-time-sep'>:</span>" +
      "<select class='at-time-select' onchange=\"updateAtTimePart(" + item.id + ",'minute',this.value)\">" +
      buildAtTimeNumberOptions(59, parts.minute) +
      "</select></div></label>" +
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
  s_at_time_items.push({ id: s_at_time_next_id++, time: "", prompt: "" });
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
      item[field] = value || "";
    }
  });
}

function getAtTimeParts(value) {
  var m = /^([01][0-9]|2[0-3]):([0-5][0-9])$/.exec(value || "");
  if (!m) {
    return { hour: "09", minute: "00" };
  }
  return { hour: m[1], minute: m[2] };
}

function buildAtTimeNumberOptions(max, selected) {
  var html = "";
  var i = 0;
  var target = String(selected || "00");
  for (i = 0; i <= max; i++) {
    var v = i < 10 ? ("0" + i) : String(i);
    html += "<option value='" + v + "'" + (v === target ? " selected" : "") + ">" + v + "</option>";
  }
  return html;
}

function updateAtTimePart(id, part, value) {
  if (part !== "hour" && part !== "minute") return;
  s_at_time_items.forEach(function (item) {
    if (item.id !== id) return;
    var p = getAtTimeParts(item.time || "");
    if (part === "hour") p.hour = value;
    if (part === "minute") p.minute = value;
    item.time = p.hour + ":" + p.minute;
  });
}

function initAtTimePage() {
  s_at_time_items = [];
  s_at_time_next_id = 1;
  ensureAtTimeNotEmpty();
  renderAtTimeList();
  setAtTimeMsg("", true);
}

initAtTimePage();
initPortalApp();
