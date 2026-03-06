function tgErr(code) {
  var m = {
    missing_config: "Please configure and save Telegram settings first",
    missing_token: "Please set Bot Token first",
    no_internet: "Device not connected to WiFi",
    invalid_token: "Bot Token is invalid (401 Unauthorized)",
    send_failed: "Send failed (check network or Telegram config)",
    updates_failed: "Failed to fetch chats (check network or bot token)"
  };
  return m[code] || ("Error: " + (code || "unknown"));
}

function loadTelegram(generation) {
  return fetch("/api/telegram")
    .then(function (r) { return r.json(); })
    .then(function (c) {
      if (is_stale_generation(generation)) return;
      var enabled = !!c.enabled;
      var chat = c.chat_id || "";
      var masked = c.token_masked || "";
      document.getElementById("tg_enabled").checked = enabled;
      document.getElementById("tg_chat_id").value = chat;
      document.getElementById("tg_bot_token").value = "";
      document.getElementById("tg_bot_token").placeholder = masked || "123456789:AA...";
    })
    .catch(function () {
      if (is_stale_generation(generation)) return;
      setTelegramMsg("Load telegram config failed", false);
      throw new Error("load_telegram_failed");
    });
}

function loadTelegramChats() {
  fetch("/api/telegram/chats")
    .then(function (r) {
      return r.json().catch(function () { return {}; }).then(function (j) { return { ok: r.ok, body: j }; });
    })
    .then(function (x) {
      if (!x.ok || !x.body.ok) {
        setTelegramMsg(tgErr((x.body && x.body.error) || "unknown"), false);
        return;
      }
      var items = (x.body.items || []);
      var sel = document.getElementById("tg_chat_select");
      sel.options.length = 0;
      sel.add(new Option("-- Select discovered chat --", ""));
      items.forEach(function (it) {
        var label = (it.label || it.chat_id || "chat") + " [" + (it.chat_id || "") + "]";
        sel.add(new Option(label, it.chat_id || ""));
      });
      if (!items.length) {
        setTelegramMsg("No chats found. Send message to bot first.", false);
        return;
      }
      setTelegramMsg("Chats loaded. Select one to fill Chat ID.", true);
    })
    .catch(function (e) {
      setTelegramMsg("Load chats failed: " + (e && e.message ? e.message : "network"), false);
    });
}

function applySelectedTelegramChat() {
  var sel = document.getElementById("tg_chat_select");
  if (!sel) return;
  var chatId = sel.value || "";
  if (!chatId) return;
  document.getElementById("tg_chat_id").value = chatId;
  setTelegramMsg("Chat ID filled from discovered chats.", true);
}

function saveTelegram() {
  var enabled = document.getElementById("tg_enabled").checked ? "1" : "0";
  var token = (document.getElementById("tg_bot_token").value || "").trim();
  var chat = (document.getElementById("tg_chat_id").value || "").trim();
  var body = "enabled=" + encodeURIComponent(enabled) +
    "&bot_token=" + encodeURIComponent(token) +
    "&chat_id=" + encodeURIComponent(chat);

  fetch("/api/telegram", {
    method: "POST",
    headers: { "Content-Type": "application/x-www-form-urlencoded" },
    body: body
  })
    .then(function (r) {
      return r.json().catch(function () { return {}; }).then(function (j) { return { ok: r.ok, body: j }; });
    })
    .then(function (x) {
      if (!x.ok || !x.body.ok) {
        setTelegramMsg("Save failed: " + ((x.body && x.body.error) || "unknown"), false);
        return;
      }
      setTelegramMsg("Telegram settings saved", true);
      loadTelegram().catch(function () {});
    })
    .catch(function (e) {
      setTelegramMsg("Save failed: " + (e && e.message ? e.message : "network"), false);
    });
}

function sendTelegramTest() {
  fetch("/api/telegram/test", { method: "POST" })
    .then(function (r) {
      return r.json().catch(function () { return {}; }).then(function (j) { return { ok: r.ok, body: j }; });
    })
    .then(function (x) {
      if (!x.ok || !x.body.ok) {
        setTelegramMsg(tgErr((x.body && x.body.error) || "unknown"), false);
        return;
      }
      setTelegramMsg("Test message sent", true);
    })
    .catch(function (e) {
      setTelegramMsg("Test failed: " + (e && e.message ? e.message : "network"), false);
    });
}
