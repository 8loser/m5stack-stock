function ensureIntervalNotEmpty() {
  if (s_interval_items.length > 0) return;
  s_interval_items.push({ id: s_interval_next_id++, minutes: 30, prompt: "" });
}

function renderIntervalList() {
  var box = document.getElementById("interval_list");
  if (!box) return;
  ensureIntervalNotEmpty();

  var html = "";
  s_interval_items.forEach(function (item, idx) {
    var safePrompt = escHtml(item.prompt || "");
    var safeMinutes = Number(item.minutes) || 1;
    if (safeMinutes < 1) safeMinutes = 1;
    if (safeMinutes > 1440) safeMinutes = 1440;

    html += "<div class='at-time-row'>" +
      "<div class='hint'>Entry " + (idx + 1) + "</div>" +
      "<div class='interval-row-grid'>" +
      "<label>Every (minutes)" +
      "<input type='number' min='1' max='1440' step='1' value='" + safeMinutes + "' " +
      "oninput=\"updateIntervalMinutes(" + item.id + ", this.value)\"></label>" +
      "<label>Prompt<textarea maxlength='512' oninput=\"updateIntervalPrompt(" + item.id + ", this.value)\">" +
      safePrompt + "</textarea></label>" +
      "</div>" +
      "<div class='at-time-actions'>" +
      "<button type='button' class='btn-remove-time' onclick='removeIntervalRow(" + item.id + ")'>Remove</button>" +
      "</div>" +
      "</div>";
  });
  box.innerHTML = html;
}

function addIntervalRow() {
  s_interval_items.push({ id: s_interval_next_id++, minutes: 30, prompt: "" });
  renderIntervalList();
  setIntervalMsg("Added one interval entry.", true);
}

function removeIntervalRow(id) {
  s_interval_items = s_interval_items.filter(function (item) { return item.id !== id; });
  ensureIntervalNotEmpty();
  renderIntervalList();
  setIntervalMsg("Entry removed.", true);
}

function updateIntervalMinutes(id, value) {
  var n = Number(value);
  if (!isFinite(n)) return;
  n = Math.round(n);
  if (n < 1) n = 1;
  if (n > 1440) n = 1440;
  s_interval_items.forEach(function (item) {
    if (item.id === id) item.minutes = n;
  });
}

function updateIntervalPrompt(id, value) {
  s_interval_items.forEach(function (item) {
    if (item.id === id) item.prompt = value || "";
  });
}

function initIntervalPage() {
  s_interval_items = [];
  s_interval_next_id = 1;
  ensureIntervalNotEmpty();
  renderIntervalList();
  setIntervalMsg("", true);
}

