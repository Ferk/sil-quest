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

  // Renders one ANSI-like colored text line by grouping contiguous attribute runs.
  function renderColoredLine(text, attrs, fallbackAttr = 1) {
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

  // Renders ANSI-like colored text by grouping contiguous attribute runs.
  function renderColoredText(text, attrs, fallbackAttr = 1) {
    if (!text) return "";

    let html = "";
    let lineStart = 0;

    for (let i = 0; i <= text.length; i++) {
      if (i !== text.length && text[i] !== "\n") continue;

      if (i > lineStart) {
        html += renderColoredLine(
          text.slice(lineStart, i),
          attrs ? attrs.subarray(lineStart, i) : null,
          fallbackAttr
        );
      }

      if (i !== text.length) {
        html += "<br>";
      }
      lineStart = i + 1;
    }

    return html;
  }

  // Renders colored text as semantic blocks and lines instead of <br>-separated spans.
  function renderColoredBlocks(text, attrs, fallbackAttr = 1) {
    if (!text) return "";

    const blocks = [];
    let currentBlock = [];
    let lineStart = 0;

    const flushLine = (lineText, lineAttrs) => {
      if (!lineText) {
        if (currentBlock.length) {
          blocks.push(currentBlock);
          currentBlock = [];
        }
        return;
      }

      currentBlock.push(
        `<p class="term-line">${renderColoredLine(lineText, lineAttrs, fallbackAttr)}</p>`
      );
    };

    for (let i = 0; i <= text.length; i++) {
      if (i !== text.length && text[i] !== "\n") continue;

      flushLine(
        text.slice(lineStart, i),
        attrs ? attrs.subarray(lineStart, i) : null
      );
      lineStart = i + 1;
    }

    if (currentBlock.length) {
      blocks.push(currentBlock);
    }

    return blocks
      .map((block) => `<div class="term-block">${block.join("")}</div>`)
      .join("");
  }

  // Chooses CSS class for right-side panel values based on semantic label/value.
  function sideValueClass(label, valueRaw) {
    if (label === "Exp:") return "term-c13";
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

  // Normalizes one optional side-panel value for consistent filtering.
  function normalizeSideValue(value) {
    return String(value ?? "").trim();
  }

  // Returns whether one semantic side-panel value should be hidden as placeholder text.
  function isHiddenSideValue(value, skipValues = []) {
    const normalized = normalizeSideValue(value);
    return !normalized || skipValues.includes(normalized);
  }

  // Renders one compact item section for the side panel from a simple string list.
  function renderSideItemSection(title, items) {
    if (!Array.isArray(items) || !items.length) return [];

    const rows = [
      `<span class="term-c14">${escapeHtml(String(title || ""))}</span>`,
    ];

    for (const item of items) {
      const text = normalizeSideValue(item);
      if (!text) continue;
      rows.push(
        `<span class="term-c9">\u00a0${escapeHtml(text)}</span>`
      );
    }

    return rows.length > 1 ? rows : [];
  }

  // Renders side panel HTML from semantic player-state JSON.
  function renderSideState(state) {
    if (!state || Number(state.ready) !== 1) {
      return "<span class=\"term-c2\">(player state not ready)</span>";
    }

    const lines = [];
    const hiddenNormalOrNone = ["Normal", "(none)"];
    const appendSection = (rows) => {
      const visibleRows = rows.filter(Boolean);
      if (!visibleRows.length) return;
      if (lines.length) lines.push("");
      lines.push(...visibleRows);
    };
    const renderText = (text, klass = "term-c1", skipValues = []) => {
      const valueRaw = normalizeSideValue(text);
      if (isHiddenSideValue(valueRaw, skipValues)) return "";
      return `<span class="${klass}">${escapeHtml(valueRaw)}</span>`;
    };
    const renderPair = (label, value, skipValues = []) => {
      const valueRaw = normalizeSideValue(value);
      if (isHiddenSideValue(valueRaw, skipValues)) return "";
      const valueClass = sideValueClass(label, valueRaw);
      return (
        `<span class="term-c11">${escapeHtml(label)}</span>` +
        `<span class="${valueClass}"> ${escapeHtml(valueRaw)}</span>`
      );
    };

    appendSection([
      renderPair("Name:", state.name || "(unnamed)"),
      renderPair("Race:", state.race || "Unknown"),
      renderPair("House:", state.house || "Unknown"),
    ]);

    appendSection([
      renderPair("STR:", state.str || "?"),
      renderPair("DEX:", state.dex || "?"),
      renderPair("CON:", state.con || "?"),
      renderPair("GRA:", state.gra || "?"),
      renderPair("Exp:", state.exp || ""),
    ]);

    appendSection([
      renderPair("Health:", `${state.healthCur ?? 0}/${state.healthMax ?? 0}`),
      renderPair("Voice:", `${state.voiceCur ?? 0}/${state.voiceMax ?? 0}`),
      renderPair("Depth:", state.depthText || "Surface"),
    ]);

    appendSection([
      renderPair("Melee:", state.melee || "(none)", ["(none)"]),
      renderPair("Melee2:", state.melee2 || "(none)", ["(none)"]),
      renderPair("Ranged:", state.ranged || "(none)", ["(none)"]),
      renderPair("Armor:", state.armor || "(none)", ["(none)"]),
    ]);

    appendSection([
      renderPair("Song:", state.song || "(none)", hiddenNormalOrNone),
      renderPair("Theme:", state.theme || "(none)", hiddenNormalOrNone),
    ]);

    appendSection([
      renderPair("State:", state.state || "Normal", hiddenNormalOrNone),
      renderPair("Speed:", state.speed || "Normal", hiddenNormalOrNone),
      renderPair("Hunger:", state.hunger || "Normal", hiddenNormalOrNone),
      renderPair("Terrain:", state.terrain || "(none)", hiddenNormalOrNone),
      renderPair("Effects:", state.effects || "(none)", hiddenNormalOrNone),
    ]);

    appendSection([
      renderPair("Target:", state.targetName || "(none)", hiddenNormalOrNone),
      renderPair("HP:", `[${normalizeSideValue(state.targetHpBar)}]`, ["[]"]),
      renderText(state.targetAlert, "term-c12", hiddenNormalOrNone),
    ]);

    const equipmentTitle = state.equipmentWeight
      ? `Equipment (${normalizeSideValue(state.equipmentWeight)})`
      : "Equipment";
    appendSection(renderSideItemSection(equipmentTitle, state.equipment));
    appendSection(renderSideItemSection("Inventory", state.inventory));

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
    renderColoredBlocks,
    renderColoredText,
    renderSideState,
    setHtmlIfChanged,
  });
}

initWebHelpers(globalThis);
