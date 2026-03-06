function loadAiConfig(generation) {
  return fetch("/api/ai")
    .then(function (r) { return r.json(); })
    .then(function (c) {
      if (is_stale_generation(generation)) return;
      var gemini = document.getElementById("gemini_key");
      var claude = document.getElementById("claude_key");
      var openai = document.getElementById("openai_key");
      if (!gemini || !claude || !openai) return;

      gemini.value = "";
      claude.value = "";
      openai.value = "";
      gemini.placeholder = c.gemini_key_masked || "";
      claude.placeholder = c.claude_key_masked || "";
      openai.placeholder = c.openai_key_masked || "";
      document.getElementById("ai_global_prompt").value = c.global_prompt || "";
      setAiProviderSelection((c && c.provider) ? c.provider : "gemini");
    })
    .catch(function () {
      throw new Error("load_ai_config_failed");
    });
}

function getAiProviderSelection() {
  var selected = document.querySelector("input[name='ai_provider']:checked");
  return selected ? selected.value : "";
}

function setAiProviderSelection(provider) {
  var picked = (provider === "gemini" || provider === "claude" || provider === "openai")
    ? provider : "gemini";
  var target = document.querySelector("input[name='ai_provider'][value='" + picked + "']");
  if (target) target.checked = true;
}

function testAiProvider(provider) {
  var inputId = provider === "gemini" ? "gemini_key" :
    provider === "claude" ? "claude_key" :
      provider === "openai" ? "openai_key" : "";
  var apiKey = inputId ? (document.getElementById(inputId).value || "") : "";
  var body = "provider=" + encodeURIComponent(provider || "") +
    "&api_key=" + encodeURIComponent(apiKey);

  fetch("/api/ai/test", {
    method: "POST",
    headers: { "Content-Type": "application/x-www-form-urlencoded" },
    body: body
  })
    .then(function (r) {
      return r.json().catch(function () { return {}; }).then(function (j) { return { ok: r.ok, body: j }; });
    })
    .then(function (x) {
      if (!x.ok || !x.body.ok) {
        var err = (x.body && x.body.error) || "unknown";
        setAiProviderMsg(provider, "Test failed: " + err, false);
        return;
      }
      var result = (x.body.results && x.body.results[0]) ? x.body.results[0] : null;
      if (!result || !result.configured) {
        setAiProviderMsg(provider, "Test skipped: key not configured", false);
        return;
      }
      if (result.ok) {
        setAiProviderMsg(provider, "Test ok (HTTP " + (result.status || 0) + ")", true);
      } else {
        setAiProviderMsg(provider, "Test failed (" + (result.error || "unknown") + ", HTTP " + (result.status || 0) + ")", false);
      }
    })
    .catch(function (e) {
      setAiProviderMsg(provider, "Test failed: " + (e && e.message ? e.message : "network"), false);
    });
}

function saveAiSettings() {
  var provider = getAiProviderSelection();
  if (!provider) {
    setAiMsg("Save failed: missing provider", false);
    return;
  }

  var gemini = document.getElementById("gemini_key").value || "";
  var claude = document.getElementById("claude_key").value || "";
  var openai = document.getElementById("openai_key").value || "";
  var globalPrompt = document.getElementById("ai_global_prompt").value || "";
  var body = "provider=" + encodeURIComponent(provider) +
    "&gemini_key=" + encodeURIComponent(gemini) +
    "&claude_key=" + encodeURIComponent(claude) +
    "&openai_key=" + encodeURIComponent(openai) +
    "&global_prompt=" + encodeURIComponent(globalPrompt);

  fetch("/api/ai", {
    method: "POST",
    headers: { "Content-Type": "application/x-www-form-urlencoded" },
    body: body
  })
    .then(function (r) {
      return r.json().catch(function () { return {}; }).then(function (j) { return { ok: r.ok, body: j }; });
    })
    .then(function (x) {
      if (!x.ok || !x.body.ok) {
        setAiMsg("Save failed: " + aiErr((x.body && x.body.error) || "unknown"), false);
        return;
      }
      setAiMsg("AI settings saved", true);
      loadAiConfig().catch(function () {});
    })
    .catch(function (e) {
      setAiMsg("Save failed: " + (e && e.message ? e.message : "network"), false);
    });
}

function clearAiProvider(provider) {
  var fieldName = "clear_" + provider;
  var selectedProvider = getAiProviderSelection();
  if (!selectedProvider) {
    setAiProviderMsg(provider, "Clear failed: missing provider", false);
    return;
  }

  var body = "provider=" + encodeURIComponent(selectedProvider) + "&" + fieldName + "=1";
  fetch("/api/ai", {
    method: "POST",
    headers: { "Content-Type": "application/x-www-form-urlencoded" },
    body: body
  })
    .then(function (r) {
      return r.json().catch(function () { return {}; }).then(function (j) { return { ok: r.ok, body: j }; });
    })
    .then(function (x) {
      if (!x.ok || !x.body.ok) {
        setAiProviderMsg(provider, "Clear failed: " + ((x.body && x.body.error) || "unknown"), false);
        return;
      }
      loadAiConfig().catch(function () {});
      setAiProviderMsg(provider, "Cleared key", true);
    })
    .catch(function (e) {
      setAiProviderMsg(provider, "Clear failed: " + (e && e.message ? e.message : "network"), false);
    });
}
