/* global globalThis */

// Initializes the global helper registry used by the web frontend.
function initWebHelpers(root) {
  // Escapes user-facing text so it can be safely injected into HTML.
  function escapeHtml(text) {
    return text
      .replaceAll("&", "&amp;")
      .replaceAll("<", "&lt;")
      .replaceAll(">", "&gt;");
  }

  // Renders ANSI-like colored text by grouping contiguous attribute runs.
  function renderColoredText(text, attrs, fallbackAttr = 1) {
    if (!text) return "";

    let html = "";
    let runAttr = -1;
    let runStart = 0;

    // Emits the current attribute run as one span for efficient DOM output.
    const flushRun = (end) => {
      if (end <= runStart) return;
      const segment = escapeHtml(text.slice(runStart, end));
      const attr = runAttr >= 0 ? runAttr : fallbackAttr;
      html += `<span class="term-c${attr & 0x0f}">${segment}</span>`;
    };

    for (let i = 0; i < text.length; i++) {
      const ch = text[i];
      if (ch === "\n") {
        flushRun(i);
        html += "<br>";
        runAttr = -1;
        runStart = i + 1;
        continue;
      }

      const attr = (attrs && i < attrs.length) ? attrs[i] : fallbackAttr;
      if (runAttr < 0) {
        runAttr = attr;
        runStart = i;
        continue;
      }
      if (attr !== runAttr) {
        flushRun(i);
        runAttr = attr;
        runStart = i;
      }
    }

    flushRun(text.length);
    return html;
  }

  // Chooses CSS class for right-side panel values based on semantic label/value.
  function sideValueClass(label, valueRaw) {
    if (label === "Health:") {
      const m = valueRaw.match(/(-?\d+)\s*\/\s*(-?\d+)/);
      if (!m) return "term-c9";
      const cur = Number.parseInt(m[1], 10);
      const max = Number.parseInt(m[2], 10);
      if (!Number.isFinite(cur) || !Number.isFinite(max) || max <= 0) return "term-c9";
      const ratio = Math.max(0, cur) / max;
      if (ratio <= 0.25) return "term-c4";
      if (ratio <= 0.5) return "term-c3";
      return "term-c13";
    }
    if (label === "Voice:") return "term-c14";
    if (label === "Depth:") return "term-c10";
    if (label === "Melee:" || label === "Melee2:") return "term-c1";
    if (label === "Ranged:") return "term-c7";
    if (label === "Armor:") return "term-c2";
    if (label === "State:") {
      if (valueRaw.includes("Entranced")) return "term-c4";
      if (valueRaw.includes("Stealth")) return "term-c13";
      return "term-c14";
    }
    if (label === "Speed:") {
      if (valueRaw.includes("Fast")) return "term-c13";
      if (valueRaw.includes("Slow")) return "term-c3";
      return "term-c9";
    }
    if (label === "Hunger:") {
      if (valueRaw.includes("Starving")) return "term-c4";
      if (valueRaw.includes("Weak")) return "term-c3";
      if (valueRaw.includes("Hungry")) return "term-c11";
      if (valueRaw.includes("Full")) return "term-c13";
      return "term-c9";
    }
    if (label === "Terrain:") {
      if (valueRaw.includes("Sunlight")) return "term-c11";
      if (valueRaw.includes("Pit") || valueRaw.includes("Web")) return "term-c3";
      return "term-c9";
    }
    if (label === "Effects:") {
      if (valueRaw.includes("(none)")) return "term-c8";
      return "term-c12";
    }
    if (label === "Song:" || label === "Theme:") return "term-c15";
    return "term-c9";
  }

  // Renders side panel HTML from semantic player-state JSON.
  function renderSideState(state) {
    if (!state || Number(state.ready) !== 1) {
      return "<span class=\"term-c2\">(player state not ready)</span>";
    }

    const lines = [];
    const emitBlank = () => { lines.push(""); };
    const emitText = (text, klass = "term-c1") => {
      lines.push(`<span class="${klass}">${escapeHtml(String(text ?? ""))}</span>`);
    };
    const emitPair = (label, value) => {
      const valueRaw = String(value ?? "");
      const valueClass = sideValueClass(label, valueRaw);
      lines.push(
        `<span class="term-c11">${escapeHtml(label)}</span>` +
        `<span class="${valueClass}"> ${escapeHtml(valueRaw)}</span>`
      );
    };

    emitPair("Name:", state.name || "(unnamed)");
    emitPair("Race:", state.race || "Unknown");
    emitPair("House:", state.house || "Unknown");
    emitBlank();

    emitPair("STR:", state.str || "?");
    emitPair("DEX:", state.dex || "?");
    emitPair("CON:", state.con || "?");
    emitPair("GRA:", state.gra || "?");
    emitBlank();

    emitPair("Health:", `${state.healthCur ?? 0}/${state.healthMax ?? 0}`);
    emitPair("Voice:", `${state.voiceCur ?? 0}/${state.voiceMax ?? 0}`);
    emitPair("Depth:", state.depthText || "Surface");
    emitBlank();

    emitPair("Melee:", state.melee || "(none)");
    emitPair("Melee2:", state.melee2 || "(none)");
    emitPair("Ranged:", state.ranged || "(none)");
    emitPair("Armor:", state.armor || "(none)");
    emitBlank();

    emitPair("Song:", state.song || "(none)");
    emitPair("Theme:", state.theme || "(none)");
    emitBlank();

    emitPair("State:", state.state || "Normal");
    emitPair("Speed:", state.speed || "Normal");
    emitPair("Hunger:", state.hunger || "Normal");
    emitPair("Terrain:", state.terrain || "(none)");
    emitPair("Effects:", state.effects || "(none)");

    if (state.targetName) {
      emitBlank();
      emitPair("Target:", state.targetName);
      if (state.targetHpBar) emitPair("HP:", `[${state.targetHpBar}]`);
      if (state.targetAlert) emitText(state.targetAlert, "term-c12");
    }

    return lines.join("<br>");
  }

  // Updates element HTML only when content changes, optionally pinning scroll bottom.
  function setHtmlIfChanged(el, html, keepBottom = false) {
    if (el.dataset.htmlCache === html) return;

    el.innerHTML = html;
    el.dataset.htmlCache = html;

    if (keepBottom) {
      el.scrollTop = el.scrollHeight;
    }
  }

  // Restricts a numeric value to the inclusive range [min, max].
  function clamp(value, min, max) {
    return Math.min(max, Math.max(min, value));
  }

  root.WebHelpers = Object.freeze({
    clamp,
    renderColoredText,
    renderSideState,
    setHtmlIfChanged,
  });
}

initWebHelpers(globalThis);
