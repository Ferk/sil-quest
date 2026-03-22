(() => {
    /* ==========================================================================
     * Setup And Shared Helpers
     * Reads globally provided helper utilities and caches core DOM references.
     * ========================================================================== */

    const {
      clamp,
      renderColoredText,
      renderSideState,
      setHtmlIfChanged,
    } = globalThis.WebHelpers;

    const appEl = document.querySelector(".app");
    const statusEl = document.getElementById("status");
    const actionFabsEl = document.getElementById("action-fabs");
    const adjacentActionRowEl = document.getElementById("adjacent-action-row");
    const mapLoadingEl = document.getElementById("map-loading");
    const mapLoadingStatusEl = document.getElementById("map-loading-status");
    const mapWrapEl = document.querySelector(".map-wrap");
    const mapCanvas = document.getElementById("map");
    const yesFabEl = document.getElementById("yes-fab");
    const closeFabEl = document.getElementById("close-fab");
    const inventoryFabEl = document.getElementById("inventory-fab");
    const tileActionFabEl = document.getElementById("tile-action-fab");
    const tileActionFabVisualEl = document.getElementById("tile-action-fab-visual");
    const adjacentActionFabEls = Array.from(
      document.querySelectorAll(".adjacent-action-fab")
    );
    const overlayModalEl = document.getElementById("overlay-modal");
    const hudBarsEl = document.getElementById("hud-bars");
    const sideEl = document.getElementById("side");
    const sideWrapEl = document.getElementById("side-wrap");
    const sideBackdropEl = document.getElementById("side-backdrop");
    const sideToggleEl = document.getElementById("side-toggle");
    const logEl = document.getElementById("log");
    const PERSIST_APEX_DIR = "/persist/apex";
    const PERSIST_SAVE_DIR = "/persist/save-web";
    const PERSIST_USER_DIR = "/persist/user";
    const PERSIST_DATA_DIR = "/persist/data";
    const IDB_SYNC_INTERVAL_MSEC = 5000;
    const ADJACENT_ACTION_DIRECTIONS = [7, 8, 9, 4, 6, 1, 2, 3];
    const ADJACENT_DIRECTION_NAMES = {
      1: "South-west",
      2: "South",
      3: "South-east",
      4: "West",
      6: "East",
      7: "North-west",
      8: "North",
      9: "North-east",
    };
    const adjacentActionFabByDir = new Map(
      adjacentActionFabEls.map((buttonEl) => [Number(buttonEl.dataset.adjDir), buttonEl])
    );

    /* ==========================================================================
     * Rendering Constants
     * Defines fixed values used by map drawing, cell formats, and UX behavior.
     * ========================================================================== */

    const ctx = mapCanvas.getContext("2d", { alpha: false });

    const WEB_CELL_EMPTY = 0;
    const WEB_CELL_TEXT = 1;
    const WEB_CELL_PICT = 2;
    const RESPONSIVE_BREAKPOINT = 960;
    const MAP_TILE_SCALE = 2;
    const MAP_MIN_ZOOM = 0.5;
    const MAP_FOLLOW_EDGE_RATIO = 0.30;
    const MAP_CLICK_DRAG_THRESHOLD = 8;

    const WEB_FLAG_ALERT = 0x01;
    const WEB_FLAG_GLOW = 0x02;
    const WEB_FLAG_FG_PICT = 0x04;
    const WEB_FLAG_BG_PICT = 0x08;
    const WEB_FLAG_MARK = 0x10;

    const colors = [
      "#000000", "#ffffff", "#9ca3af", "#f59e0b",
      "#ef4444", "#22c55e", "#3b82f6", "#b45309",
      "#4b5563", "#d1d5db", "#a855f7", "#fde047",
      "#fca5a5", "#86efac", "#93c5fd", "#fcd34d"
    ];

    /* ==========================================================================
     * Mutable Runtime State
     * Stores live frame, map, input, and actor-facing state across ticks.
     * ========================================================================== */

    let api = null;
    let lastFrameId = -1;
    let lastMenuRevision = -1;
    let lastModalRevision = -1;
    let cellStride = 8;
    let tileSrcW = 16;
    let tileSrcH = 16;
    let tileW = tileSrcW * MAP_TILE_SCALE;
    let tileH = tileSrcH * MAP_TILE_SCALE;
    let tileImageReady = false;
    let mapCols = 80;
    let mapRows = 24;
    let underlayCols = 0;
    let underlayRows = 0;
    let underlayCells = [];
    let prevActorFacing = new Map();
    let prevActorPositions = new Set();
    let prevPlayerWorldX = null;
    let playerFacingRight = 0;
    let fxDelaySeq = -1;
    let fxOverlayUntil = 0;
    let forceRedraw = true;
    let mapZoomSettings = computeMapZoomSettings();
    let mapZoom = mapZoomSettings.defaultZoom;
    let mapDisplayReady = false;
    let userAdjustedMapZoom = false;
    let lastPlayerMapX = -1;
    let lastPlayerMapY = -1;
    let initialViewportPlaced = false;
    let lastDepthText = null;
    let morePromptActive = false;
    let morePromptText = "";
    let topPromptActive = false;
    let topPromptText = "";
    let topPromptAttrs = null;
    let overlayMorePromptActive = false;
    let overlayMorePromptText = "";
    let lineMorePromptActive = false;
    let lineMorePromptText = "";
    let persistFsEnabled = false;
    let idbSyncTimer = null;
    let idbSyncInFlight = false;
    let idbSyncPending = false;
    let idbSyncHooksBound = false;
    let activeMenuItems = [];
    let activeMenuColumnX = null;
    let activeMenuText = "";
    let activeMenuTextAttrs = null;
    let activeMenuDetailsText = "";
    let activeMenuDetailsAttrs = null;
    let activeMenuDetailsWidth = null;
    let activeMenuDetailsVisual = null;
    let semanticMenuSnapshot = null;
    let hoveredMenuIndex = -1;
    let compactSideOpen = false;
    const iconMeta = {
      alertAttr: 0,
      alertChar: 0,
      glowAttr: 0,
      glowChar: 0,
    };
    const utf8Decoder = new TextDecoder("utf-8");

    // Disables IndexedDB-backed persistence and clears any related timers.
    function disablePersistFs(reason, error = null) {
      persistFsEnabled = false;
      idbSyncInFlight = false;
      idbSyncPending = false;

      if (idbSyncTimer !== null) {
        globalThis.clearInterval(idbSyncTimer);
        idbSyncTimer = null;
      }

      if (error) {
        console.warn(reason, error);
      } else {
        console.warn(reason);
      }
    }

    // Flushes persistent IDBFS state to IndexedDB with single-flight semantics.
    function flushPersistFs(force = false) {
      const FS = globalThis.FS;
      if (!persistFsEnabled || !FS || typeof FS.syncfs !== "function") return;

      if (idbSyncInFlight) {
        idbSyncPending = idbSyncPending || force;
        return;
      }

      idbSyncInFlight = true;
      FS.syncfs(false, (err) => {
        idbSyncInFlight = false;
        if (err) {
          disablePersistFs("syncfs flush failed; disabling persistence fallback", err);
          return;
        }

        if (idbSyncPending) {
          idbSyncPending = false;
          flushPersistFs(true);
        }
      });
    }

    // Starts periodic and lifecycle-triggered persistence flushes for IDBFS.
    function startPersistSyncLoop() {
      if (!persistFsEnabled) return;

      if (idbSyncTimer === null) {
        idbSyncTimer = globalThis.setInterval(() => {
          flushPersistFs(false);
        }, IDB_SYNC_INTERVAL_MSEC);
      }

      if (idbSyncHooksBound) return;
      idbSyncHooksBound = true;

      globalThis.addEventListener("pagehide", () => {
        flushPersistFs(true);
      });

      globalThis.addEventListener("beforeunload", () => {
        flushPersistFs(true);
      });

      document.addEventListener("visibilitychange", () => {
        if (document.visibilityState === "hidden") {
          flushPersistFs(true);
        }
      });
    }

    /* ==========================================================================
     * WASM Buffer Access
     * Reads primitive and structured values exported from the game runtime.
     * ========================================================================== */

    // Reads map layout metadata from WASM and applies safe fallbacks.
    function refreshLayoutMetadata(cols, rows) {
      if (!api) return;

      let mcols = api.getMapCols();
      let mrows = api.getMapRows();

      if (!Number.isFinite(mcols) || mcols <= 0 || mcols > cols) {
        mcols = Math.max(1, cols - 14);
      }
      if (!Number.isFinite(mrows) || mrows <= 0 || mrows > rows) {
        mrows = Math.max(1, rows - 2);
      }

      mapCols = mcols;
      mapRows = mrows;
    }

    // Refreshes special icon tile metadata used for alert and glow overlays.
    function refreshIconMetadata() {
      if (!api) return;
      iconMeta.alertAttr = api.getAlertAttr() & 0xff;
      iconMeta.alertChar = api.getAlertChar() & 0xff;
      iconMeta.glowAttr = api.getGlowAttr() & 0xff;
      iconMeta.glowChar = api.getGlowChar() & 0xff;
    }

    // Mirrors runtime status text to both the top bar and the centered loading panel.
    function setStatusText(text) {
      const nextText = String(text || "");
      statusEl.textContent = nextText;
      if (mapLoadingStatusEl) mapLoadingStatusEl.textContent = nextText;
    }

    // Swaps between the centered loading panel and the interactive map canvas.
    function setMapDisplayReady(ready) {
      mapDisplayReady = !!ready;
      mapCanvas.hidden = !mapDisplayReady;
      mapWrapEl.classList.toggle("map-ready", mapDisplayReady);
      if (mapLoadingEl) mapLoadingEl.hidden = mapDisplayReady;
    }

    // Returns the active Emscripten heap view used to read wasm-exported buffers.
    function getHeapU8() {
      if (typeof globalThis.Module !== "undefined" && globalThis.Module.HEAPU8) {
        return globalThis.Module.HEAPU8;
      }
      if (typeof globalThis.HEAPU8 !== "undefined") return globalThis.HEAPU8;
      return null;
    }

    // Reads one packed web_cell struct from wasm memory.
    function readCell(heap, ptr, cols, x, y) {
      const off = ptr + (y * cols + x) * cellStride;
      return {
        kind: heap[off + 0],
        textAttr: heap[off + 1],
        textChar: heap[off + 2],
        fgAttr: heap[off + 3],
        fgChar: heap[off + 4],
        bgAttr: heap[off + 5],
        bgChar: heap[off + 6],
        flags: heap[off + 7],
      };
    }

    // Decodes a UTF-8 string slice from wasm memory.
    function readUtf8(heap, ptr, len) {
      if (!ptr || !len || len <= 0) return "";
      const end = ptr + len;
      if (ptr < 0 || end > heap.length) return "";
      return utf8Decoder.decode(heap.subarray(ptr, end));
    }

    // Escapes plain text for safe HTML rendering inside semantic overlays.
    function escapeHtml(text) {
      return String(text)
        .replaceAll("&", "&amp;")
        .replaceAll("<", "&lt;")
        .replaceAll(">", "&gt;");
    }

    // Returns a byte slice from wasm memory for per-character color attributes.
    function readBytes(heap, ptr, len) {
      if (!ptr || !len || len <= 0) return null;
      const end = ptr + len;
      if (ptr < 0 || end > heap.length) return null;
      return heap.subarray(ptr, end);
    }

    // Copies an attribute buffer out of wasm-backed memory for stable reuse across frames.
    function cloneAttrBytes(attrs) {
      return attrs ? new Uint8Array(attrs) : null;
    }

    // Normalizes one optional menu-details visual payload exported by the frontend.
    function normalizeMenuDetailsVisual(kind, attr, chr) {
      if (!Number.isInteger(kind) || kind <= 0) return null;
      return {
        kind,
        attr: Number.isInteger(attr) ? attr : 0,
        chr: Number.isInteger(chr) ? chr : 0,
      };
    }

    // Normalizes one optional visual payload attached to a semantic menu row.
    function normalizeMenuItemVisual(kind, attr, chr) {
      return normalizeMenuDetailsVisual(kind, attr, chr);
    }

    // Items with a non-Enter activation key are treated as direct actions rather than hover-driven selections.
    function isDirectMenuItem(item) {
      return !!item && item.key > 0 && item.key !== 13;
    }

    // Groups menu items by their terminal x-position so multi-column menus can keep their parent/child structure.
    function getMenuColumns(items) {
      const columns = new Map();

      for (const item of items) {
        const key = String(item.x);
        if (!columns.has(key)) columns.set(key, []);
        columns.get(key).push(item);
      }

      return [...columns.entries()]
        .sort((a, b) => Number(a[0]) - Number(b[0]))
        .map((entry) => entry[1].sort((a, b) => a.y - b.y || a.x - b.x));
    }

    // The rightmost menu column is treated as the active submenu for passive hover.
    function getActiveMenuColumnX(items = activeMenuItems, activeColumnX = activeMenuColumnX) {
      if (!items.length) return null;

      if (Number.isInteger(activeColumnX) && activeColumnX < 0) {
        return null;
      }

      if (Number.isInteger(activeColumnX) && items.some((item) => item.x === activeColumnX)) {
        return activeColumnX;
      }

      let maxX = items[0].x;
      for (const item of items) {
        if (item.x > maxX) maxX = item.x;
      }
      return maxX;
    }

    // Only the active/rightmost submenu should react to passive hover; older columns remain clickable.
    function isPassiveHoverMenuItem(index) {
      if (
        index < 0 ||
        index >= activeMenuItems.length ||
        !Number.isInteger(index)
      ) {
        return false;
      }

      const activeColumnX = getActiveMenuColumnX();
      if (activeColumnX === null) return true;

      return activeMenuItems[index].x === activeColumnX;
    }

    // After a semantic menu redraw, keep the active column within the horizontal scroll viewport.
    function scrollMenuColumnsToActiveEdge(activeColumnX = activeMenuColumnX) {
      if (!overlayModalEl.classList.contains("overlay-menu")) return;
      const container = overlayModalEl.querySelector(".menu-columns");
      if (!container) return;

      let columnEl = null;
      if (Number.isInteger(activeColumnX)) {
        columnEl = container.querySelector(
          `.menu-column[data-menu-column="${activeColumnX}"]`
        );
      }
      if (!columnEl) {
        const columns = container.querySelectorAll(".menu-column");
        columnEl = columns.length ? columns[columns.length - 1] : null;
      }
      if (!columnEl) return;

      const columnRect = columnEl.getBoundingClientRect();
      const containerRect = container.getBoundingClientRect();
      const pad = 8;

      if (columnRect.left < containerRect.left) {
        container.scrollLeft -= (containerRect.left - columnRect.left) + pad;
      } else if (columnRect.right > containerRect.right) {
        container.scrollLeft += (columnRect.right - containerRect.right) + pad;
      }
    }

    // Keeps a selected overlay menu item within the visible scroll region of its menu container.
    function scrollMenuItemIntoView(itemEl) {
      const container = itemEl?.closest(".menu-columns");
      if (!container) return;

      const useOverlayVerticalScroll =
        overlayModalEl.classList.contains("overlay-menu") &&
        globalThis.innerWidth <= 700;
      const verticalContainer = useOverlayVerticalScroll ? overlayModalEl : container;

      const itemRect = itemEl.getBoundingClientRect();
      const verticalRect = verticalContainer.getBoundingClientRect();
      const containerRect = container.getBoundingClientRect();
      const pad = 8;

      if (itemRect.top < verticalRect.top) {
        verticalContainer.scrollTop -= (verticalRect.top - itemRect.top) + pad;
      } else if (itemRect.bottom > verticalRect.bottom) {
        verticalContainer.scrollTop += (itemRect.bottom - verticalRect.bottom) + pad;
      }

      if (itemRect.left < containerRect.left) {
        container.scrollLeft -= (containerRect.left - itemRect.left) + pad;
      } else if (itemRect.right > containerRect.right) {
        container.scrollLeft += (itemRect.right - containerRect.right) + pad;
      }
    }

    // After re-rendering a semantic menu, keeps the selected button visible inside the scroll area.
    function syncSelectedMenuItemsIntoView(activeColumnX = activeMenuColumnX) {
      if (!overlayModalEl.classList.contains("overlay-menu")) return;
      scrollMenuColumnsToActiveEdge(activeColumnX);
      let selectedItems = overlayModalEl.querySelectorAll(
        ".menu-column-active .menu-item-selected"
      );
      if (!selectedItems.length) {
        selectedItems = overlayModalEl.querySelectorAll(".menu-item-selected");
      }
      for (const itemEl of selectedItems) {
        scrollMenuItemIntoView(itemEl);
      }
    }

    // Saves the last live semantic menu so transient prompts can reuse it without falling back to terminal capture.
    function snapshotSemanticMenu(
      items,
      activeColumnX,
      text,
      textAttrs,
      detailsText,
      detailsAttrs,
      detailsWidth,
      detailsVisual
    ) {
      if (!items.length) return;
      semanticMenuSnapshot = {
        items: items.map((item) => ({ ...item })),
        activeColumnX,
        text,
        textAttrs: cloneAttrBytes(textAttrs),
        detailsText,
        detailsAttrs: cloneAttrBytes(detailsAttrs),
        detailsWidth,
        detailsVisual: detailsVisual ? { ...detailsVisual } : null,
      };
    }

    // Returns whether the top line is showing a prompt that still expects input.
    function isInteractiveTopPromptActive() {
      if (!topPromptActive) return false;

      const prompt = String(topPromptText || "").trimEnd();
      if (!prompt) return false;

      return /\[[^\]]+\]\s*$/.test(prompt) ||
        /\?\s*$/.test(prompt) ||
        /:\s*$/.test(prompt);
    }

    // Returns whether the top prompt is one of the blocking yes/no confirmations.
    function isYesNoTopPromptActive() {
      if (!isInteractiveTopPromptActive()) return false;

      const prompt = String(topPromptText || "").trimEnd();
      return /\[y\/n(?:\/[^\]]+)?\]\s*$/i.test(prompt);
    }

    // Only interactive prompts should keep the last semantic menu visible as context.
    function shouldKeepSemanticMenuSnapshot() {
      if (!semanticMenuSnapshot || activeMenuItems.length) {
        return false;
      }

      return isInteractiveTopPromptActive();
    }

    // Picks the menu state to render: live wasm-exported data, or the last semantic menu during a prompt.
    function getRenderableMenuState() {
      if (activeMenuItems.length) {
        snapshotSemanticMenu(
          activeMenuItems,
          activeMenuColumnX,
          activeMenuText,
          activeMenuTextAttrs,
          activeMenuDetailsText,
          activeMenuDetailsAttrs,
          activeMenuDetailsWidth,
          activeMenuDetailsVisual
        );
        return {
          items: activeMenuItems,
          activeColumnX: activeMenuColumnX,
          text: activeMenuText,
          textAttrs: activeMenuTextAttrs,
          detailsText: activeMenuDetailsText,
          detailsAttrs: activeMenuDetailsAttrs,
          detailsWidth: activeMenuDetailsWidth,
          detailsVisual: activeMenuDetailsVisual,
        };
      }

      if (shouldKeepSemanticMenuSnapshot()) {
        return semanticMenuSnapshot;
      }

      return null;
    }

    // Reads the active semantic menu items exported by the wasm frontend.
    function refreshMenuItems(heap) {
      if (
        !api ||
        typeof api.getMenuItemsPtr !== "function" ||
        typeof api.getMenuItemCount !== "function" ||
        typeof api.getMenuItemStride !== "function"
      ) {
        activeMenuItems = [];
        activeMenuColumnX = null;
        activeMenuText = "";
        activeMenuTextAttrs = null;
        activeMenuDetailsText = "";
        activeMenuDetailsAttrs = null;
        activeMenuDetailsWidth = null;
        activeMenuDetailsVisual = null;
        hoveredMenuIndex = -1;
        return;
      }

      const ptr = api.getMenuItemsPtr();
      const count = api.getMenuItemCount();
      const stride = api.getMenuItemStride();
      const byteLen = count * stride;
      const textPtr =
        typeof api.getMenuTextPtr === "function" ? api.getMenuTextPtr() : 0;
      const textLen =
        typeof api.getMenuTextLen === "function" ? api.getMenuTextLen() : 0;
      const attrPtr =
        typeof api.getMenuAttrsPtr === "function" ? api.getMenuAttrsPtr() : 0;
      const attrLen =
        typeof api.getMenuAttrsLen === "function" ? api.getMenuAttrsLen() : 0;
      const activeColumnX =
        typeof api.getMenuActiveX === "function" ? api.getMenuActiveX() : null;
      const detailsPtr =
        typeof api.getMenuDetailsPtr === "function" ? api.getMenuDetailsPtr() : 0;
      const detailsLen =
        typeof api.getMenuDetailsLen === "function" ? api.getMenuDetailsLen() : 0;
      const detailsAttrPtr =
        typeof api.getMenuDetailsAttrsPtr === "function" ? api.getMenuDetailsAttrsPtr() : 0;
      const detailsAttrLen =
        typeof api.getMenuDetailsAttrsLen === "function" ? api.getMenuDetailsAttrsLen() : 0;
      const detailsWidth =
        typeof api.getMenuDetailsWidth === "function" ? api.getMenuDetailsWidth() : 0;
      const detailsVisualKind =
        typeof api.getMenuDetailsVisualKind === "function"
          ? api.getMenuDetailsVisualKind()
          : 0;
      const detailsVisualAttr =
        typeof api.getMenuDetailsVisualAttr === "function"
          ? api.getMenuDetailsVisualAttr()
          : 0;
      const detailsVisualChar =
        typeof api.getMenuDetailsVisualChar === "function"
          ? api.getMenuDetailsVisualChar()
          : 0;

      activeMenuText = readUtf8(heap, textPtr, textLen);
      activeMenuTextAttrs = readBytes(heap, attrPtr, attrLen);
      activeMenuColumnX = Number.isInteger(activeColumnX) ? activeColumnX : null;
      activeMenuDetailsText = readUtf8(heap, detailsPtr, detailsLen);
      activeMenuDetailsAttrs = readBytes(heap, detailsAttrPtr, detailsAttrLen);
      activeMenuDetailsWidth =
        Number.isInteger(detailsWidth) && detailsWidth > 0 ? detailsWidth : null;
      activeMenuDetailsVisual = normalizeMenuDetailsVisual(
        detailsVisualKind,
        detailsVisualAttr,
        detailsVisualChar
      );

      if (!ptr || count <= 0 || stride < 40 || ptr < 0 || (ptr + byteLen) > heap.length) {
        activeMenuItems = [];
        activeMenuColumnX = null;
        activeMenuDetailsText = "";
        activeMenuDetailsAttrs = null;
        activeMenuDetailsWidth = null;
        activeMenuDetailsVisual = null;
        hoveredMenuIndex = -1;
        return;
      }

      const view = new DataView(heap.buffer, heap.byteOffset + ptr, byteLen);
      const items = new Array(count);

      for (let i = 0; i < count; i++) {
        const off = i * stride;
        const labelBytes = heap.subarray(ptr + off + 40, ptr + off + stride);
        const labelEnd = labelBytes.indexOf(0);
        items[i] = {
          x: view.getInt32(off + 0, true),
          y: view.getInt32(off + 4, true),
          w: view.getInt32(off + 8, true),
          h: view.getInt32(off + 12, true),
          key: view.getInt32(off + 16, true),
          selected: view.getInt32(off + 20, true) !== 0,
          attr: view.getInt32(off + 24, true),
          visual: normalizeMenuItemVisual(
            view.getInt32(off + 28, true),
            view.getInt32(off + 32, true),
            view.getInt32(off + 36, true)
          ),
          label: utf8Decoder.decode(
            labelBytes.subarray(0, labelEnd >= 0 ? labelEnd : labelBytes.length)
          ),
        };
      }

      activeMenuItems = items;
      if (hoveredMenuIndex >= activeMenuItems.length) {
        hoveredMenuIndex = -1;
      }
    }

    // Reads and parses semantic player-state payload exported by wasm.
    function readPlayerState(heap) {
      if (
        !api ||
        typeof api.getPlayerStatePtr !== "function" ||
        typeof api.getPlayerStateLen !== "function"
      ) {
        return null;
      }

      const ptr = api.getPlayerStatePtr();
      const len = api.getPlayerStateLen();
      const payload = readUtf8(heap, ptr, len);
      if (!payload) return null;

      try {
        return JSON.parse(payload);
      } catch (err) {
        console.warn("Invalid player-state payload from wasm:", err);
        return null;
      }
    }

    /* ==========================================================================
     * Tile And Sprite Helpers
     * Converts cell metadata into canvas draw calls and actor-facing metadata.
     * ========================================================================== */

    // Converts raw byte character values to JavaScript display characters.
    function cellToChar(chr) {
      if (chr === 0) return " ";
      return String.fromCharCode(chr);
    }

    // Draws one map cell in ASCII fallback mode.
    function drawAsciiCell(cell, x, y) {
      const ch = cellToChar(cell.textChar);
      ctx.fillStyle = colors[cell.textAttr & 0x0f] || "#ffffff";
      ctx.fillText(ch, x * tileW, y * tileH);
    }

    // Draws one explicit attr/char pair in ASCII fallback mode.
    function drawAsciiAttrChar(attr, chr, x, y) {
      const ch = cellToChar(chr);
      ctx.fillStyle = colors[attr & 0x0f] || "#ffffff";
      ctx.fillText(ch, x * tileW, y * tileH);
    }

    // Draws one tileset sprite by attr/char tile coordinates, optionally flipped.
    function drawTileByAttrChar(attr, chr, x, y, flipX = false) {
      if (!tileImageReady) return;
      const sx = (chr & 0x3f) * tileSrcW;
      const sy = (attr & 0x3f) * tileSrcH;

      if (!flipX) {
        ctx.drawImage(
          tileImage,
          sx,
          sy,
          tileSrcW,
          tileSrcH,
          x * tileW,
          y * tileH,
          tileW,
          tileH
        );
        return;
      }

      ctx.save();
      ctx.translate((x + 1) * tileW, y * tileH);
      ctx.scale(-1, 1);
      ctx.drawImage(
        tileImage,
        sx,
        sy,
        tileSrcW,
        tileSrcH,
        0,
        0,
        tileW,
        tileH
      );
      ctx.restore();
    }

    // Draws the optional semantic details visual shown beside menu copy.
    function renderMenuDetailsVisual(detailsVisual) {
      const visualEl = overlayModalEl.querySelector(".menu-item-details-visual");
      if (!visualEl) return;

      if (!detailsVisual || !detailsVisual.kind) {
        setHtmlIfChanged(visualEl, "");
        return;
      }

      if (detailsVisual.kind === 1) {
        const glyph = cellToChar(detailsVisual.chr) || "?";
        const color = colors[detailsVisual.attr & 0x0f] || "#ffffff";
        setHtmlIfChanged(
          visualEl,
          `<div class="menu-item-details-glyph" style="color:${color}">${escapeHtml(
            glyph
          )}</div>`
        );
        return;
      }

      setHtmlIfChanged(
        visualEl,
        `<canvas class="menu-item-details-visual-canvas" width="64" height="64" aria-hidden="true"></canvas>`
      );

      const canvas = visualEl.querySelector(".menu-item-details-visual-canvas");
      if (!canvas) return;

      const detailsCtx = canvas.getContext("2d", { alpha: true });
      if (!detailsCtx) return;

      detailsCtx.clearRect(0, 0, canvas.width, canvas.height);
      detailsCtx.imageSmoothingEnabled = false;

      if (!tileImageReady) return;

      const sx = (detailsVisual.chr & 0x3f) * tileSrcW;
      const sy = (detailsVisual.attr & 0x3f) * tileSrcH;
      detailsCtx.drawImage(
        tileImage,
        sx,
        sy,
        tileSrcW,
        tileSrcH,
        0,
        0,
        canvas.width,
        canvas.height
      );
    }

    // Draws any per-row menu visuals embedded inside semantic menu buttons.
    function renderMenuItemVisuals() {
      if (!overlayModalEl.classList.contains("overlay-menu")) return;

      const visualEls = overlayModalEl.querySelectorAll(".menu-item-visual");
      for (const visualEl of visualEls) {
        const kind = Number(visualEl.dataset.menuVisualKind);
        const attr = Number(visualEl.dataset.menuVisualAttr);
        const chr = Number(visualEl.dataset.menuVisualChar);
        const visual = normalizeMenuItemVisual(kind, attr, chr);

        if (!visual || !visual.kind) {
          setHtmlIfChanged(visualEl, "");
          continue;
        }

        if (visual.kind === 1) {
          const glyph = cellToChar(visual.chr) || "?";
          const color = colors[visual.attr & 0x0f] || "#ffffff";
          setHtmlIfChanged(
            visualEl,
            `<span class="menu-item-visual-glyph" style="color:${color}">${escapeHtml(
              glyph
            )}</span>`
          );
          continue;
        }

        setHtmlIfChanged(
          visualEl,
          `<canvas class="menu-item-visual-canvas" width="24" height="24" aria-hidden="true"></canvas>`
        );

        const canvas = visualEl.querySelector(".menu-item-visual-canvas");
        if (!canvas) continue;

        const visualCtx = canvas.getContext("2d", { alpha: true });
        if (!visualCtx) continue;

        visualCtx.clearRect(0, 0, canvas.width, canvas.height);
        visualCtx.imageSmoothingEnabled = false;

        if (!tileImageReady) continue;

        const sx = (visual.chr & 0x3f) * tileSrcW;
        const sy = (visual.attr & 0x3f) * tileSrcH;
        visualCtx.drawImage(
          tileImage,
          sx,
          sy,
          tileSrcW,
          tileSrcH,
          0,
          0,
          canvas.width,
          canvas.height
        );
      }
    }

    // Draws a composited pict cell in terrain -> glow -> actor -> alert order.
    function drawPictCell(cell, x, y, flipForeground = false) {
      if ((cell.flags & WEB_FLAG_BG_PICT) !== 0) {
        drawTileByAttrChar(cell.bgAttr, cell.bgChar, x, y);
      }

      if ((cell.flags & WEB_FLAG_GLOW) !== 0) {
        drawTileByAttrChar(iconMeta.glowAttr, iconMeta.glowChar, x, y);
      }

      if ((cell.flags & WEB_FLAG_FG_PICT) !== 0) {
        drawTileByAttrChar(cell.fgAttr, cell.fgChar, x, y, flipForeground);
      }

      if ((cell.flags & WEB_FLAG_ALERT) !== 0) {
        drawTileByAttrChar(iconMeta.alertAttr, iconMeta.alertChar, x, y);
      }
    }

    // Checks whether a cell provides a foreground pict sprite.
    function hasForegroundPict(cell) {
      return (
        cell &&
        cell.kind === WEB_CELL_PICT &&
        (cell.flags & WEB_FLAG_FG_PICT) !== 0
      );
    }

    // Checks whether a cell provides a background pict sprite.
    function hasBackgroundPict(cell) {
      return (
        cell &&
        cell.kind === WEB_CELL_PICT &&
        (cell.flags & WEB_FLAG_BG_PICT) !== 0
      );
    }

    // Detects actor-like cells whose foreground sprite can be mirrored.
    function isFlippableActorCell(cell) {
      if (!hasForegroundPict(cell) || !hasBackgroundPict(cell)) return false;

      /* Actor-like composited cell: foreground sprite over different terrain tile. */
      return (
        ((cell.fgAttr & 0x3f) !== (cell.bgAttr & 0x3f)) ||
        ((cell.fgChar & 0x3f) !== (cell.bgChar & 0x3f))
      );
    }

    // Builds a compact stable ID for an actor sprite using fg attr/char.
    function actorTileId(cell) {
      return ((cell.fgAttr & 0x3f) << 6) | (cell.fgChar & 0x3f);
    }

    // Creates a unique key for actor tracking in world coordinates.
    function actorPosKey(worldX, worldY, tileId) {
      return `${worldX},${worldY},${tileId}`;
    }

    /* ==========================================================================
     * Responsive Shell Helpers
     * Adapts the mobile HUD, side drawer, and zoom defaults to the viewport.
     * ========================================================================== */

    // Reports whether the layout should collapse into the touch-friendly mobile shell.
    function isCompactLayout() {
      return globalThis.innerWidth <= RESPONSIVE_BREAKPOINT;
    }

    // Detects touch-first devices so dense screens can start with a larger map zoom.
    function prefersCoarsePointer() {
      return (
        typeof globalThis.matchMedia === "function" &&
        globalThis.matchMedia("(pointer: coarse)").matches
      );
    }

    // Reads the root rem size so browser accessibility text scaling also informs map zoom.
    function getRootRemPixels() {
      const rootStyle = globalThis.getComputedStyle(document.documentElement);
      const fontSize = Number.parseFloat(rootStyle.fontSize);
      return Number.isFinite(fontSize) && fontSize > 0 ? fontSize : 16;
    }

    // Estimates the base fit scale before user zoom, even before the first full map draw.
    function estimateMapFitScale() {
      const wrapRect = mapWrapEl.getBoundingClientRect();
      const availW = Math.max(1, wrapRect.width - 2);
      const availH = Math.max(1, wrapRect.height - 2);
      const canvasW = Math.max(1, mapCols * tileW);
      const canvasH = Math.max(1, mapRows * tileH);

      return Math.min(1, availW / canvasW, availH / canvasH);
    }

    // Derives runtime zoom bounds from screen density and target readable tile size.
    function computeMapZoomSettings() {
      const dpr = Math.max(1, globalThis.devicePixelRatio || 1);
      const remPx = getRootRemPixels();
      const coarse = prefersCoarsePointer();
      // Counteracts the viewport fit shrink so the readable tile size is still driven by rem/DPI.
      const fitScale = Math.max(0.05, estimateMapFitScale());
      let targetTilePx = remPx * 2.5;

      targetTilePx += Math.min(remPx * 0.9, (dpr - 1) * remPx * 0.55);
      if (coarse) targetTilePx += remPx * 0.45;

      const minZoom = coarse ? 0.75 : MAP_MIN_ZOOM;
      const maxZoom = coarse ? 24 : 12;
      const defaultZoom = targetTilePx / Math.max(1, tileW * fitScale);

      return {
        defaultZoom: clamp(defaultZoom, minZoom, maxZoom),
        minZoom,
        maxZoom,
      };
    }

    // Applies the current compact side-panel state to the DOM and accessibility labels.
    function setCompactSideOpen(nextOpen) {
      const compact = isCompactLayout();
      const open = compact && !!nextOpen;

      compactSideOpen = open;
      appEl.classList.toggle("side-open", open);
      sideBackdropEl.hidden = !open;
      sideToggleEl.setAttribute("aria-expanded", open ? "true" : "false");
      sideToggleEl.textContent = open ? "Close" : "Stats";
    }

    // Recomputes responsive zoom presets and mobile chrome state after viewport changes.
    function syncResponsiveShell(forceZoomReset = false) {
      const compact = isCompactLayout();
      const prevSettings = mapZoomSettings;
      mapZoomSettings = computeMapZoomSettings();

      const zoomSettingsChanged =
        !prevSettings ||
        prevSettings.defaultZoom !== mapZoomSettings.defaultZoom ||
        prevSettings.minZoom !== mapZoomSettings.minZoom ||
        prevSettings.maxZoom !== mapZoomSettings.maxZoom;

      if (forceZoomReset || (!userAdjustedMapZoom && zoomSettingsChanged)) {
        mapZoom = mapZoomSettings.defaultZoom;
      } else {
        mapZoom = clampMapZoom(mapZoom);
      }

      if (!compact) compactSideOpen = false;
      setCompactSideOpen(compactSideOpen);
      fitMapToViewport();
    }

    /* ==========================================================================
     * Camera And FX Utilities
     * Manages viewport fitting, focus-follow behavior, and transient FX timing.
     * ========================================================================== */

    // Restricts user zoom to the supported range.
    function clampMapZoom(zoom) {
      return Math.min(mapZoomSettings.maxZoom, Math.max(mapZoomSettings.minZoom, zoom));
    }

    // Updates zoom level, optionally recording a user override, then refits the canvas.
    function setMapZoom(nextZoom, manual = false) {
      mapZoom = clampMapZoom(nextZoom);
      if (manual) userAdjustedMapZoom = true;
      fitMapToViewport();
    }

    // Fits the map canvas to container size while preserving scroll ratios.
    function fitMapToViewport() {
      if (!mapCanvas.width || !mapCanvas.height) return;

      const prevScrollMaxX = Math.max(1, mapWrapEl.scrollWidth - mapWrapEl.clientWidth);
      const prevScrollMaxY = Math.max(1, mapWrapEl.scrollHeight - mapWrapEl.clientHeight);
      const prevRatioX = mapWrapEl.scrollLeft / prevScrollMaxX;
      const prevRatioY = mapWrapEl.scrollTop / prevScrollMaxY;

      const wrapRect = mapWrapEl.getBoundingClientRect();
      const availW = Math.max(1, wrapRect.width - 2);
      const availH = Math.max(1, wrapRect.height - 2);
      const fitScale = Math.min(1, availW / mapCanvas.width, availH / mapCanvas.height);
      const scale = Math.max(0.05, fitScale * mapZoom);
      const cssW = Math.max(1, Math.floor(mapCanvas.width * scale));
      const cssH = Math.max(1, Math.floor(mapCanvas.height * scale));
      mapCanvas.style.width = `${cssW}px`;
      mapCanvas.style.height = `${cssH}px`;

      const nextScrollMaxX = Math.max(0, mapWrapEl.scrollWidth - mapWrapEl.clientWidth);
      const nextScrollMaxY = Math.max(0, mapWrapEl.scrollHeight - mapWrapEl.clientHeight);
      mapWrapEl.scrollLeft = Math.round(prevRatioX * nextScrollMaxX);
      mapWrapEl.scrollTop = Math.round(prevRatioY * nextScrollMaxY);
    }

    // Auto-scrolls the map container to keep the player near viewport center bands.
    function followPlayerInViewport(overlayActive, force = false) {
      if (!api || overlayActive) return;

      const playerX = api.getPlayerMapX();
      const playerY = api.getPlayerMapY();
      if (playerX < 0 || playerY < 0 || playerX >= mapCols || playerY >= mapRows) {
        return;
      }

      const moved = (playerX !== lastPlayerMapX) || (playerY !== lastPlayerMapY);
      if (!force && !moved) return;

      lastPlayerMapX = playerX;
      lastPlayerMapY = playerY;

      const viewW = mapWrapEl.clientWidth;
      const viewH = mapWrapEl.clientHeight;
      if (!viewW || !viewH || !mapCols || !mapRows) return;

      const maxScrollX = Math.max(0, mapWrapEl.scrollWidth - viewW);
      const maxScrollY = Math.max(0, mapWrapEl.scrollHeight - viewH);
      if (!maxScrollX && !maxScrollY) return;

      const mapRect = mapCanvas.getBoundingClientRect();
      const wrapRect = mapWrapEl.getBoundingClientRect();
      const cellW = mapRect.width / mapCols;
      const cellH = mapRect.height / mapRows;
      if (!Number.isFinite(cellW) || !Number.isFinite(cellH) || cellW <= 0 || cellH <= 0) {
        return;
      }

      const mapOffsetX = mapWrapEl.scrollLeft + (mapRect.left - wrapRect.left);
      const mapOffsetY = mapWrapEl.scrollTop + (mapRect.top - wrapRect.top);
      const playerCenterX = mapOffsetX + (playerX + 0.5) * cellW;
      const playerCenterY = mapOffsetY + (playerY + 0.5) * cellH;

      const marginX = viewW * MAP_FOLLOW_EDGE_RATIO;
      const marginY = viewH * MAP_FOLLOW_EDGE_RATIO;

      let nextScrollX = mapWrapEl.scrollLeft;
      let nextScrollY = mapWrapEl.scrollTop;

      const visibleLeft = nextScrollX;
      const visibleRight = nextScrollX + viewW;
      const visibleTop = nextScrollY;
      const visibleBottom = nextScrollY + viewH;

      if (playerCenterX < visibleLeft + marginX) {
        nextScrollX = playerCenterX - marginX;
      } else if (playerCenterX > visibleRight - marginX) {
        nextScrollX = playerCenterX - (viewW - marginX);
      }

      if (playerCenterY < visibleTop + marginY) {
        nextScrollY = playerCenterY - marginY;
      } else if (playerCenterY > visibleBottom - marginY) {
        nextScrollY = playerCenterY - (viewH - marginY);
      }

      mapWrapEl.scrollLeft = Math.round(clamp(nextScrollX, 0, maxScrollX));
      mapWrapEl.scrollTop = Math.round(clamp(nextScrollY, 0, maxScrollY));
    }

    // Tracks temporary FX overlay timing based on engine delay notifications.
    function updateMapFxWindow(now) {
      if (!api) return false;

      const seq = api.getFxDelaySeq();
      if (seq !== fxDelaySeq) {
        fxDelaySeq = seq;
        const delayMsec = Math.max(0, api.getFxDelayMsec());
        const holdMsec = Math.max(40, Math.min(600, delayMsec + 24));
        fxOverlayUntil = now + holdMsec;
      }

      return now <= fxOverlayUntil;
    }

    // Compares FX cell data against base map cell data to detect visual deltas.
    function mapCellMatchesBase(fxCell, baseCell) {
      if (!fxCell) return true;

      if (fxCell.kind === WEB_CELL_TEXT) {
        return (
          fxCell.textChar === baseCell.textChar &&
          ((fxCell.textAttr & 0x0f) === (baseCell.textAttr & 0x0f))
        );
      }

      if (fxCell.kind === WEB_CELL_PICT) {
        const hasPictFlags = (fxCell.flags & (WEB_FLAG_FG_PICT | WEB_FLAG_BG_PICT)) !== 0;
        if (!hasPictFlags) {
          return (
            fxCell.fgChar === baseCell.textChar &&
            ((fxCell.fgAttr & 0x0f) === (baseCell.textAttr & 0x0f))
          );
        }

        if (baseCell.kind !== WEB_CELL_PICT) return false;
        return (
          fxCell.fgAttr === baseCell.fgAttr &&
          fxCell.fgChar === baseCell.fgChar &&
          fxCell.bgAttr === baseCell.bgAttr &&
          fxCell.bgChar === baseCell.bgChar &&
          fxCell.flags === baseCell.flags
        );
      }

      return true;
    }

    /* ==========================================================================
     * Main Map Rendering
     * Draws map layers, applies actor flipping, and persists frame tracking.
     * ========================================================================== */

    // Renders the full map layer, actor facing flips, and transient FX overlays.
    function drawMap(mapPtr, fxPtr, fxCols, fxRows, fxBlocked, heap, overlayActive) {
      const drawCols = Math.max(0, mapCols);
      const drawRows = Math.max(0, mapRows);
      const cellCount = drawCols * drawRows;
      const currentCells = new Array(cellCount);
      const facingRight = new Uint8Array(cellCount);
      const actorEntries = [];
      const currentActorPositions = new Set();
      const mapCol = api ? api.getMapCol() : 0;
      const mapRow = api ? api.getMapRow() : 0;
      const mapXStep = api ? Math.max(1, api.getMapXStep()) : 1;
      const mapWorldX = api ? api.getMapWorldX() : 0;
      const mapWorldY = api ? api.getMapWorldY() : 0;
      const playerMapX = api ? api.getPlayerMapX() : -1;
      const playerMapY = api ? api.getPlayerMapY() : -1;
      const cursorVisible = api ? (!overlayActive && api.getCursorVisible() !== 0) : false;
      let cursorMapX = -1;
      let cursorMapY = -1;
      const playerVisible =
        playerMapX >= 0 &&
        playerMapX < drawCols &&
        playerMapY >= 0 &&
        playerMapY < drawRows;
      const playerWorldX = playerVisible ? (mapWorldX + playerMapX) : null;

      if (playerWorldX !== null && prevPlayerWorldX !== null) {
        const pdx = playerWorldX - prevPlayerWorldX;
        if (pdx > 0) playerFacingRight = 1;
        else if (pdx < 0) playerFacingRight = 0;
      }

      if (cursorVisible) {
        const cursorX = api.getCursorX();
        const cursorY = api.getCursorY();
        const relX = cursorX - mapCol;
        const relY = cursorY - mapRow;
        if (relY >= 0 && relY < drawRows && relX >= 0) {
          const mx = Math.floor(relX / mapXStep);
          if (mx >= 0 && mx < drawCols) {
            cursorMapX = mx;
            cursorMapY = relY;
          }
        }
      }

      mapCanvas.width = drawCols * tileW;
      mapCanvas.height = drawRows * tileH;
      ctx.imageSmoothingEnabled = false;

      ctx.fillStyle = "#000";
      ctx.fillRect(0, 0, mapCanvas.width, mapCanvas.height);
      ctx.font = `${Math.max(12, tileH - 1)}px "Unscii Fantasy", monospace`;
      ctx.textBaseline = "top";

      if (underlayCols !== drawCols || underlayRows !== drawRows) {
        underlayCols = drawCols;
        underlayRows = drawRows;
        underlayCells = new Array(drawCols * drawRows).fill(null);
      }

      for (let y = 0; y < drawRows; y++) {
        for (let x = 0; x < drawCols; x++) {
          const idx = y * drawCols + x;
          if (!mapPtr) {
            currentCells[idx] = {
              kind: WEB_CELL_EMPTY,
              textAttr: 0,
              textChar: 0,
              fgAttr: 0,
              fgChar: 0,
              bgAttr: 0,
              bgChar: 0,
              flags: 0,
            };
            continue;
          }
          currentCells[idx] = readCell(heap, mapPtr, drawCols, x, y);
        }
      }

      for (let y = 0; y < drawRows; y++) {
        for (let x = 0; x < drawCols; x++) {
          const idx = y * drawCols + x;
          const cell = currentCells[idx];
          if (!isFlippableActorCell(cell)) continue;

          const worldX = mapWorldX + x;
          const worldY = mapWorldY + y;
          const tileId = actorTileId(cell);
          const posKey = actorPosKey(worldX, worldY, tileId);

          actorEntries.push({ idx, worldX, worldY, tileId, posKey });
          currentActorPositions.add(posKey);
        }
      }

      for (const actor of actorEntries) {
        let facing = prevActorFacing.get(actor.posKey) ? 1 : 0;

        let movedFromLeft = false;
        let movedFromRight = false;

        for (let dy = -1; dy <= 1; dy++) {
          const leftPrevKey =
            actorPosKey(actor.worldX - 1, actor.worldY + dy, actor.tileId);
          const rightPrevKey =
            actorPosKey(actor.worldX + 1, actor.worldY + dy, actor.tileId);

          if (
            prevActorPositions.has(leftPrevKey) &&
            !currentActorPositions.has(leftPrevKey)
          ) {
            movedFromLeft = true;
          }
          if (
            prevActorPositions.has(rightPrevKey) &&
            !currentActorPositions.has(rightPrevKey)
          ) {
            movedFromRight = true;
          }
        }

        if (movedFromLeft && !movedFromRight) {
          facing = 1;
        } else if (movedFromRight && !movedFromLeft) {
          facing = 0;
        }

        facingRight[actor.idx] = facing;
      }

      if (playerVisible) {
        const pIdx = playerMapY * drawCols + playerMapX;
        if (isFlippableActorCell(currentCells[pIdx])) {
          facingRight[pIdx] = playerFacingRight ? 1 : 0;
        }
      }

      for (let y = 0; y < drawRows; y++) {
        for (let x = 0; x < drawCols; x++) {
          const idx = y * drawCols + x;
          const cell = currentCells[idx];
          const under = underlayCells[idx];
          const flipActor = facingRight[idx] === 1 && isFlippableActorCell(cell);

          if (cell.kind === WEB_CELL_PICT) {
            if (tileImageReady) {
              drawPictCell(cell, x, y, flipActor);
              underlayCells[idx] = cell;
            } else {
              drawAsciiCell(cell, x, y);
              underlayCells[idx] = null;
            }
          } else if (cell.kind === WEB_CELL_TEXT) {
            drawAsciiCell(cell, x, y);
            underlayCells[idx] = null;
          } else if (under && tileImageReady) {
            drawPictCell(under, x, y);
          }

          if (!fxBlocked && fxPtr && fxCols === drawCols && fxRows === drawRows) {
            const fxCell = readCell(heap, fxPtr, fxCols, x, y);
            if (!mapCellMatchesBase(fxCell, cell)) {
              if (fxCell.kind === WEB_CELL_PICT) {
                const hasPictFlags = (fxCell.flags & (WEB_FLAG_FG_PICT | WEB_FLAG_BG_PICT)) !== 0;
                if (hasPictFlags && tileImageReady) {
                  const flipFxActor = facingRight[idx] === 1 && isFlippableActorCell(fxCell);
                  drawPictCell(fxCell, x, y, flipFxActor);
                } else {
                  drawAsciiAttrChar(fxCell.fgAttr, fxCell.fgChar, x, y);
                }
              } else if (fxCell.kind === WEB_CELL_TEXT) {
                drawAsciiCell(fxCell, x, y);
              }
            }
          }

          if ((cell.flags & WEB_FLAG_MARK) !== 0) {
            drawAsciiAttrChar(cell.textAttr, cell.textChar, x, y);
          }
        }
      }

      if (cursorMapX >= 0 && cursorMapY >= 0) {
        const base = Math.max(1, Math.floor(Math.min(tileW, tileH) * 0.08));
        ctx.save();
        ctx.strokeStyle = "#00ffff";
        ctx.lineWidth = Math.max(1, base);
        ctx.strokeRect(
          cursorMapX * tileW + 0.5,
          cursorMapY * tileH + 0.5,
          tileW - 1,
          tileH - 1
        );
        ctx.restore();
      }

      prevPlayerWorldX = playerWorldX;

      const nextActorFacing = new Map();
      for (const actor of actorEntries) {
        nextActorFacing.set(actor.posKey, facingRight[actor.idx] ? 1 : 0);
      }
      prevActorFacing = nextActorFacing;
      prevActorPositions = new Set(nextActorFacing.keys());

      // Re-resolves the default zoom once the real map canvas size is known.
      if (!userAdjustedMapZoom && !initialViewportPlaced) {
        mapZoomSettings = computeMapZoomSettings();
        mapZoom = mapZoomSettings.defaultZoom;
      }

      fitMapToViewport();
      return { drawCols, drawRows };
    }

    /* ==========================================================================
     * Semantic Panel Rendering
     * Renders overlays, side panel, and log from semantic text buffers.
     * ========================================================================== */

    // Merges overlay-driven and top-line-driven more prompts into one active prompt.
    function syncMorePromptState() {
      if (lineMorePromptActive) {
        morePromptActive = true;
        morePromptText = lineMorePromptText || "-more-";
        return;
      }

      if (overlayMorePromptActive) {
        morePromptActive = true;
        morePromptText = overlayMorePromptText || "-more-";
        return;
      }

      morePromptActive = false;
      morePromptText = "";
    }

    // Extracts live top-line prompt text from terminal row 0 (input prompts and -more-).
    function updateToplinePrompt(heap, cols) {
      topPromptActive = false;
      topPromptText = "";
      topPromptAttrs = null;
      lineMorePromptActive = false;
      lineMorePromptText = "";

      if (!api || !heap || cols <= 0) {
        syncMorePromptState();
        return;
      }

      const cellPtr = api.getCellsPtr();
      if (!cellPtr) {
        syncMorePromptState();
        return;
      }

      const chars = [];
      const attrs = new Uint8Array(cols);
      let lastNonSpace = -1;

      for (let x = 0; x < cols; x++) {
        const cell = readCell(heap, cellPtr, cols, x, 0);
        let ch = " ";
        let attr = 1;

        if (cell.kind === WEB_CELL_TEXT) {
          const code = cell.textChar & 0xff;
          if (code >= 32 && code <= 126) {
            ch = String.fromCharCode(code);
          }
          attr = cell.textAttr & 0x0f;
        }

        chars.push(ch);
        attrs[x] = attr;
        if (ch !== " ") {
          lastNonSpace = x;
        }
      }

      if (lastNonSpace < 0) {
        syncMorePromptState();
        return;
      }

      const rawText = chars.slice(0, lastNonSpace + 1).join("");
      const rawAttrs = attrs.slice(0, lastNonSpace + 1);

      if (/-more-/i.test(rawText)) {
        lineMorePromptActive = true;
        lineMorePromptText =
          rawText.replace(/\s*-more-\s*/gi, " ").trim() || "-more-";
      } else {
        topPromptActive = true;
        topPromptText = rawText.trimEnd();
        topPromptAttrs = rawAttrs;
      }

      syncMorePromptState();
    }

    // Renders an active semantic menu as an HTML overlay instead of raw captured text.
    function updateMenuSemantic() {
      const menuState = getRenderableMenuState();
      if (!menuState) return false;

      const columns = getMenuColumns(menuState.items);
      const activeColumnX = getActiveMenuColumnX(
        menuState.items,
        menuState.activeColumnX
      );
      const hasCopy = !!menuState.text;
      const hasDetails = !!menuState.detailsText;
      const hasDetailsVisual =
        !!menuState.detailsVisual && Number.isInteger(menuState.detailsVisual.kind);
      const hasDetailsPane = hasDetails || hasDetailsVisual;
      const renderMenuButton = (item, index) => {
        const selectedClass = item.selected ? " menu-item-selected" : "";
        const attrClass = item.selected ? "" : ` term-c${item.attr & 0x0f}`;
        const directClass = isDirectMenuItem(item) ? " menu-item-direct" : "";
        const label = escapeHtml(item.label || String.fromCharCode(item.key));
        const visualHtml = item.visual
          ? `<span class="menu-item-visual" data-menu-visual-kind="${item.visual.kind}" data-menu-visual-attr="${item.visual.attr}" data-menu-visual-char="${item.visual.chr}"></span>`
          : "";
        return `<button type="button" class="menu-item${selectedClass}${attrClass}${directClass}" data-menu-index="${index}">${visualHtml}<span class="menu-item-label">${label}</span></button>`;
      };
      const introHtml = `<div class="menu-copy${hasCopy ? "" : " menu-copy-empty"}">${
        hasCopy ? renderColoredText(menuState.text, menuState.textAttrs, 1) : ""
      }</div>`;
      const detailsHtml = `<div class="menu-item-details${hasDetailsPane ? "" : " menu-item-details-empty"}"><div class="menu-item-details-visual"></div><div class="menu-item-details-copy${
        hasDetails ? "" : " menu-item-details-copy-empty"
      }">${
        hasDetails
          ? renderColoredText(menuState.detailsText, menuState.detailsAttrs, 1)
          : ""
      }</div></div>`;
      const menuColumnsHtml = columns
        .map((column, columnIndex) => {
          const columnX = column.length ? column[0].x : columnIndex;
          const columnStateClass =
            columns.length > 1 &&
            Number.isInteger(activeColumnX) &&
            columnX !== activeColumnX
              ? " menu-column-inactive"
              : " menu-column-active";
          const itemsHtml = column
            .map((item, itemIndex) => renderMenuButton(item, menuState.items.indexOf(item)))
            .join("");
          return `<div class="menu-column${columnStateClass}" data-menu-column="${columnX}">${itemsHtml}</div>`;
        })
        .join("");
      const shellClass = hasCopy ? "menu-shell-with-copy" : "menu-shell-no-copy";
      const bodyClass = hasDetailsPane ? "menu-body-with-details" : "menu-body-no-details";
      const menuHtml = `<div class="menu-shell ${shellClass}">${introHtml}<div class="menu-body ${bodyClass}"><div class="menu-columns-wrap"><div class="menu-columns">${menuColumnsHtml}</div></div>${detailsHtml}</div></div>`;

      overlayMorePromptActive = false;
      overlayMorePromptText = "";
      syncMorePromptState();
      overlayModalEl.style.display = "block";
      overlayModalEl.classList.remove("overlay-dismissable");
      overlayModalEl.classList.add("overlay-menu");
      if (hasDetailsPane && Number.isInteger(menuState.detailsWidth) && menuState.detailsWidth > 0) {
        overlayModalEl.style.setProperty(
          "--menu-details-width",
          `${menuState.detailsWidth}ch`
        );
      } else {
        overlayModalEl.style.removeProperty("--menu-details-width");
      }
      setHtmlIfChanged(overlayModalEl, menuHtml);
      renderMenuItemVisuals();
      renderMenuDetailsVisual(menuState.detailsVisual);
      syncSelectedMenuItemsIntoView(activeColumnX);
      return true;
    }

    // Renders an active semantic modal dialog as an HTML overlay.
    function updateModalSemantic(heap) {
      if (
        !api ||
        typeof api.getModalTextPtr !== "function" ||
        typeof api.getModalTextLen !== "function" ||
        typeof api.getModalAttrsPtr !== "function" ||
        typeof api.getModalAttrsLen !== "function"
      ) {
        return false;
      }

      const textPtr = api.getModalTextPtr();
      const textLen = api.getModalTextLen();
      const attrPtr = api.getModalAttrsPtr();
      const attrLen = api.getModalAttrsLen();
      const dismissKey =
        typeof api.getModalDismissKey === "function" ? api.getModalDismissKey() : 0;
      const text = readUtf8(heap, textPtr, textLen);
      const attrs = readBytes(heap, attrPtr, attrLen);

      if (!text) {
        overlayModalEl.classList.remove("overlay-dismissable");
        return false;
      }

      overlayMorePromptActive = false;
      overlayMorePromptText = "";
      syncMorePromptState();

      overlayModalEl.style.display = "block";
      overlayModalEl.classList.remove("overlay-menu");
      overlayModalEl.classList.toggle("overlay-dismissable", dismissKey > 0);
      setHtmlIfChanged(overlayModalEl, renderColoredText(text, attrs, 1));
      return true;
    }

    // Updates modal overlay text from semantic wasm buffers.
    function updateOverlaySemantic(heap) {
      if (updateModalSemantic(heap)) {
        return true;
      }

      if (updateMenuSemantic()) {
        return true;
      }

      if (!activeMenuItems.length && !shouldKeepSemanticMenuSnapshot()) {
        semanticMenuSnapshot = null;
      }

      if (!api) return false;

      const mode = api.getOverlayMode();
      if (!mode) {
        overlayMorePromptActive = false;
        overlayMorePromptText = "";
        syncMorePromptState();
        overlayModalEl.style.display = "none";
        overlayModalEl.classList.remove("overlay-dismissable");
        overlayModalEl.classList.remove("overlay-menu");
        setHtmlIfChanged(overlayModalEl, "");
        return false;
      }

      const textPtr = api.getOverlayTextPtr();
      const textLen = api.getOverlayTextLen();
      const attrPtr = api.getOverlayAttrsPtr();
      const attrLen = api.getOverlayAttrsLen();
      const text = readUtf8(heap, textPtr, textLen);
      const attrs = readBytes(heap, attrPtr, attrLen);

      if (!text) {
        overlayMorePromptActive = false;
        overlayMorePromptText = "";
        syncMorePromptState();
        overlayModalEl.style.display = "none";
        overlayModalEl.classList.remove("overlay-dismissable");
        setHtmlIfChanged(overlayModalEl, "");
        return false;
      }

      const moreLines = text.split("\n");
      let hasMorePrompt = false;
      const cleanedLines = moreLines.map((lineRaw) => {
        const line = lineRaw.replace(/\r/g, "");
        if (/-more-\s*$/i.test(line)) {
          hasMorePrompt = true;
          return line.replace(/\s*-more-\s*$/i, "").trimEnd();
        }
        return line;
      });

      if (hasMorePrompt) {
        overlayMorePromptActive = true;
        overlayMorePromptText = cleanedLines.join("\n").trim() || "-more-";
        syncMorePromptState();
        overlayModalEl.style.display = "none";
        overlayModalEl.classList.remove("overlay-dismissable");
        overlayModalEl.classList.remove("overlay-menu");
        setHtmlIfChanged(overlayModalEl, "");
        return false;
      }

      overlayMorePromptActive = false;
      overlayMorePromptText = "";
      syncMorePromptState();

      overlayModalEl.style.display = "block";
      overlayModalEl.classList.remove("overlay-dismissable");
      overlayModalEl.classList.remove("overlay-menu");
      setHtmlIfChanged(overlayModalEl, renderColoredText(text, attrs, 1));
      return true;
    }

    // Builds one compact resource bar used by the floating mobile HUD.
    function renderMobileHudBar(label, shortLabel, current, maximum, toneClass) {
      const safeMax = Number.isFinite(maximum) && maximum > 0 ? maximum : 0;
      const safeCur = Number.isFinite(current) ? Math.max(0, current) : 0;
      const fillPercent = safeMax > 0
        ? Math.round(clamp(safeCur / safeMax, 0, 1) * 100)
        : 0;

      return (
        `<div class="hud-bar ${toneClass}" title="${escapeHtml(label)} ${safeCur}/${safeMax}">` +
          `<span class="hud-bar-fill" style="--bar-fill:${fillPercent}%"></span>` +
          "<span class=\"hud-bar-copy\">" +
            `<span class="hud-bar-label">${escapeHtml(shortLabel)}</span>` +
            `<span class="hud-bar-value">${safeCur}/${safeMax}</span>` +
          "</span>" +
        "</div>"
      );
    }

    // Updates the compact top HUD shown when the side panel collapses on narrow screens.
    function updateMobileHud(state) {
      if (!hudBarsEl) return;

      if (!state || Number(state.ready) !== 1) {
        const placeholderHtml =
          renderMobileHudBar("Health", "HP", 0, 0, "hud-bar-empty") +
          renderMobileHudBar("Voice", "Voice", 0, 0, "hud-bar-empty");
        setHtmlIfChanged(hudBarsEl, placeholderHtml);
        sideToggleEl.title = "Character panel";
        return;
      }

      const healthCur = Number(state.healthCur ?? 0);
      const healthMax = Number(state.healthMax ?? 0);
      const voiceCur = Number(state.voiceCur ?? 0);
      const voiceMax = Number(state.voiceMax ?? 0);
      const hudHtml =
        renderMobileHudBar("Health", "HP", healthCur, healthMax, "hud-bar-health") +
        renderMobileHudBar("Voice", "Voice", voiceCur, voiceMax, "hud-bar-voice");
      const titleBits = [state.name, state.depthText].filter(Boolean);

      setHtmlIfChanged(hudBarsEl, hudHtml);
      sideToggleEl.title = titleBits.length ? titleBits.join(" - ") : "Character panel";
    }

    // Updates side and log panels using semantic text and color attributes.
    function updateSemanticPanels(heap, state = null) {
      if (!api) return;
      if (!state) state = readPlayerState(heap);
      const logPtr = api.getLogTextPtr();
      const logLen = api.getLogTextLen();
      const logAttrPtr = api.getLogAttrsPtr();
      const logAttrLen = api.getLogAttrsLen();

      const logText = readUtf8(heap, logPtr, logLen).trimEnd();
      const logAttrs = readBytes(heap, logAttrPtr, logAttrLen);
      const logHasMoreToken = /-more-/i.test(logText);
      const cleanedLogText = logHasMoreToken
        ? logText.replace(/\s*-more-\s*/gi, " ").trimEnd()
        : logText;

      const sideHtml = state
        ? renderSideState(state)
        : "<span class=\"term-c2\">(side panel empty)</span>";
      const hasLogText = Boolean(cleanedLogText);
      let logHtml = hasLogText
        ? renderColoredText(cleanedLogText, logHasMoreToken ? null : logAttrs, 1)
        : "<span class=\"term-c2\">(message panel empty)</span>";

      if (topPromptActive && topPromptText) {
        const promptHtml = renderColoredText(topPromptText, topPromptAttrs, 11);
        logHtml = hasLogText ? `${logHtml}<br>${promptHtml}` : promptHtml;
      }

      if (morePromptActive) {
        const promptHtml = `${renderColoredText(morePromptText || "-more-", null, 11)}<span class="more-indicator">-more-</span>`;
        logHtml = hasLogText ? `${logHtml}<br>${promptHtml}` : promptHtml;
      }

      updateMobileHud(state);
      syncFloatingActionButtons(state);
      setHtmlIfChanged(sideEl, sideHtml);
      setHtmlIfChanged(logEl, logHtml, true);
    }

    /* ==========================================================================
     * Frame Loop
     * Coordinates one render tick by pulling state and redrawing visible UI.
     * ========================================================================== */

    // Executes one render tick by syncing metadata and redrawing visible layers.
    function render() {
      if (!api) return;

      const now = performance.now();
      const mapFxActive = updateMapFxWindow(now);
      const frameId = api.getFrameId();
      const menuRevision =
        typeof api.getMenuRevision === "function" ? api.getMenuRevision() : 0;
      const modalRevision =
        typeof api.getModalRevision === "function" ? api.getModalRevision() : 0;
      if (
        !forceRedraw &&
        frameId === lastFrameId &&
        menuRevision === lastMenuRevision &&
        modalRevision === lastModalRevision &&
        !mapFxActive
      ) {
        return;
      }
      forceRedraw = false;
      lastFrameId = frameId;
      lastMenuRevision = menuRevision;
      lastModalRevision = modalRevision;

      const cols = api.getCols();
      const rows = api.getRows();
      const heap = getHeapU8();
      if (!heap) return;

      if (cols <= 0 || rows <= 0) return;

      refreshLayoutMetadata(cols, rows);
      refreshIconMetadata();
      refreshMenuItems(heap);
      updateToplinePrompt(heap, cols);

      const overlayActive = updateOverlaySemantic(heap);
      const state = readPlayerState(heap);
      const stateReady = !!state && Number(state.ready) === 1;
      if (stateReady) {
        const depthText = String(state.depthText || "");
        if (lastDepthText !== null && depthText !== lastDepthText) {
          initialViewportPlaced = false;
          lastPlayerMapX = -1;
          lastPlayerMapY = -1;
        }
        lastDepthText = depthText;
      }
      const mapPtr = api.getMapCellsPtr();
      let fxPtr = 0;
      let fxCols = 0;
      let fxRows = 0;

      if (!overlayActive && mapFxActive) {
        fxPtr = api.getFxCellsPtr();
        fxCols = api.getFxCellsCols();
        fxRows = api.getFxCellsRows();
      }

      const blockFx = overlayActive || !mapFxActive;
      drawMap(mapPtr, fxPtr, fxCols, fxRows, blockFx, heap, overlayActive);
      setMapDisplayReady(stateReady && mapCanvas.width > 0 && mapCanvas.height > 0);
      updateSemanticPanels(heap, state);

      if (!initialViewportPlaced) {
        followPlayerInViewport(false, true);
        initialViewportPlaced = true;
      }

      followPlayerInViewport(overlayActive);
    }

    /* ==========================================================================
     * Input Handling
     * Binds keyboard, wheel, touch, and pointer interactions for gameplay.
     * ========================================================================== */

    // Pushes one input key byte into the engine input queue.
    function pushAscii(code) {
      if (!api) return;
      api.pushKey(code & 0xff);
    }

    // Converts a client-space pointer position into one map cell and world-grid location.
    function clientPointToMapWorld(clientX, clientY) {
      if (!api || !mapCols || !mapRows) return null;

      const rect = mapCanvas.getBoundingClientRect();
      if (!rect.width || !rect.height) return null;
      if (
        clientX < rect.left ||
        clientX >= rect.right ||
        clientY < rect.top ||
        clientY >= rect.bottom
      ) {
        return null;
      }

      const mapX = Math.floor(((clientX - rect.left) / rect.width) * mapCols);
      const mapY = Math.floor(((clientY - rect.top) / rect.height) * mapRows);
      if (mapX < 0 || mapX >= mapCols || mapY < 0 || mapY >= mapRows) {
        return null;
      }

      return {
        mapX,
        mapY,
        worldX: api.getMapWorldX() + mapX,
        worldY: api.getMapWorldY() + mapY,
      };
    }

    // Returns whether the gameplay view is idle enough for floating map actions.
    function isGameplayViewIdle() {
      return !activeMenuItems.length &&
        !overlayModalEl.classList.contains("overlay-menu") &&
        !overlayModalEl.classList.contains("overlay-dismissable") &&
        !isInteractiveTopPromptActive() &&
        !lineMorePromptActive &&
        !overlayMorePromptActive &&
        !morePromptActive;
    }

    // Returns whether the current frontend state allows raw map clicks to start travel.
    function canUseMapTravel() {
      return !!api &&
        typeof api.travelTo === "function" &&
        isGameplayViewIdle();
    }

    // Returns whether the floating inventory button should currently be enabled.
    function canUseInventoryFab(state = null) {
      return !!api &&
        typeof api.openInventory === "function" &&
        !!state &&
        Number(state.ready) === 1 &&
        isGameplayViewIdle();
    }

    // Returns whether the contextual current-tile action button should be visible.
    function canUseTileActionFab(state = null) {
      return !!api &&
        typeof api.actHere === "function" &&
        !!state &&
        Number(state.ready) === 1 &&
        Number(state.tileActionVisible) === 1 &&
        isGameplayViewIdle();
    }

    // Returns whether the adjacent alter-action row should currently be shown.
    function canUseAdjacentActionRow(state = null) {
      return !!api &&
        typeof api.actAdjacent === "function" &&
        !!state &&
        Number(state.ready) === 1 &&
        ADJACENT_ACTION_DIRECTIONS.some((dir) =>
          Number(state[`adjAction${dir}Visible`] ?? 0) === 1
        ) &&
        isGameplayViewIdle();
    }

    // Returns whether the floating close button should replace the inventory button.
    function canUseCloseFab() {
      return !!api &&
        !morePromptActive &&
        (
          activeMenuItems.length > 0 ||
          overlayModalEl.classList.contains("overlay-menu") ||
          isInteractiveTopPromptActive()
        );
    }

    // Draws one floating action icon using the same tile/glyph payload as menus.
    function renderActionFabVisual(targetEl, visual = null) {
      if (!targetEl) return;

      if (!visual || !visual.kind) {
        setHtmlIfChanged(targetEl, "");
        return;
      }

      if (visual.kind === 1) {
        const glyph = cellToChar(visual.chr) || "?";
        const color = colors[visual.attr & 0x0f] || "#ffffff";
        setHtmlIfChanged(
          targetEl,
          `<span class="action-fab-glyph" style="color:${color}">${escapeHtml(
            glyph
          )}</span>`
        );
        return;
      }

      setHtmlIfChanged(
        targetEl,
        `<canvas class="action-fab-canvas" width="32" height="32" aria-hidden="true"></canvas>`
      );

      const canvas = targetEl.querySelector(".action-fab-canvas");
      if (!canvas) return;

      const visualCtx = canvas.getContext("2d", { alpha: true });
      if (!visualCtx) return;

      visualCtx.clearRect(0, 0, canvas.width, canvas.height);
      visualCtx.imageSmoothingEnabled = false;

      if (!tileImageReady) return;

      const sx = (visual.chr & 0x3f) * tileSrcW;
      const sy = (visual.attr & 0x3f) * tileSrcH;
      visualCtx.drawImage(
        tileImage,
        sx,
        sy,
        tileSrcW,
        tileSrcH,
        0,
        0,
        canvas.width,
        canvas.height
      );
    }

    // Reads one adjacent alter-action payload from the semantic player-state blob.
    function getAdjacentActionState(state, dir) {
      if (!state) {
        return {
          visible: false,
          label: "",
          attack: false,
          visual: null,
        };
      }

      return {
        visible: Number(state[`adjAction${dir}Visible`] ?? 0) === 1,
        label: String(state[`adjAction${dir}Label`] || ""),
        attack: Number(state[`adjAction${dir}Attack`] ?? 0) === 1,
        visual: normalizeMenuItemVisual(
          Number(state[`adjAction${dir}VisualKind`] ?? 0),
          Number(state[`adjAction${dir}VisualAttr`] ?? 0),
          Number(state[`adjAction${dir}VisualChar`] ?? 0)
        ),
      };
    }

    // Draws the current-tile action icon using the shared tile/glyph renderer.
    function renderTileActionFabVisual(state = null) {
      const visual = state && Number(state.tileActionVisible) === 1
        ? normalizeMenuItemVisual(
            Number(state.tileActionVisualKind ?? 0),
            Number(state.tileActionVisualAttr ?? 0),
            Number(state.tileActionVisualChar ?? 0)
          )
        : null;

      renderActionFabVisual(tileActionFabVisualEl, visual);
    }

    // Keeps the adjacent directional action buttons aligned with current tile context.
    function syncAdjacentActionButtons(state = null, shouldShow = false) {
      if (!adjacentActionRowEl || !adjacentActionFabEls.length) return false;

      let anyVisible = false;

      for (const dir of ADJACENT_ACTION_DIRECTIONS) {
        const buttonEl = adjacentActionFabByDir.get(dir);
        const visualEl = buttonEl?.querySelector(".action-fab-visual");
        const action = getAdjacentActionState(state, dir);
        const visible = shouldShow && action.visible;
        const dirName = ADJACENT_DIRECTION_NAMES[dir] || `Dir ${dir}`;
        const label = action.label || "Interact";

        if (!buttonEl) continue;

        buttonEl.hidden = !visible;
        buttonEl.classList.toggle("adjacent-action-fab-danger", visible && action.attack);
        buttonEl.title = `${dirName}: ${label}`;
        buttonEl.setAttribute("aria-label", `${dirName}: ${label}`);
        renderActionFabVisual(visualEl, visible ? action.visual : null);

        if (visible) anyVisible = true;
      }

      adjacentActionRowEl.hidden = !shouldShow || !anyVisible;
      return anyVisible;
    }

    // Keeps floating action buttons visually in sync with the current gameplay state.
    function syncFloatingActionButtons(state = null) {
      if (!actionFabsEl || !yesFabEl || !inventoryFabEl || !tileActionFabEl || !closeFabEl) {
        return;
      }

      const yesNoPromptActive = isYesNoTopPromptActive();
      const showClose = !compactSideOpen && canUseCloseFab();
      const showYes = showClose && yesNoPromptActive;
      const showInventory = !compactSideOpen && !showClose && canUseInventoryFab(state);
      const showTileAction = !compactSideOpen && !showClose && canUseTileActionFab(state);
      const showAdjacentActions =
        !compactSideOpen && !showClose && canUseAdjacentActionRow(state);
      const tileActionLabel = state && Number(state.tileActionVisible) === 1
        ? String(state.tileActionLabel || "Act here")
        : "Act here";

      renderTileActionFabVisual(state);
      const anyAdjacentActions = syncAdjacentActionButtons(state, showAdjacentActions);

      actionFabsEl.hidden =
        !showYes && !showClose && !showInventory && !showTileAction && !anyAdjacentActions;
      yesFabEl.hidden = !showYes;
      yesFabEl.title = "Yes (y)";
      yesFabEl.setAttribute("aria-label", "Yes");
      inventoryFabEl.hidden = !showInventory;
      tileActionFabEl.hidden = !showTileAction;
      tileActionFabEl.title = `${tileActionLabel} (,)`;
      tileActionFabEl.setAttribute("aria-label", tileActionLabel);
      closeFabEl.hidden = !showClose;
      closeFabEl.title = yesNoPromptActive ? "No (n)" : "Close (Esc)";
      closeFabEl.setAttribute("aria-label", yesNoPromptActive ? "No" : "Close");
    }

    // Starts automatic travel for one clicked map tile when the gameplay view is idle.
    function tryMapTravelAtClientPoint(clientX, clientY) {
      if (!canUseMapTravel()) return false;

      const hit = clientPointToMapWorld(clientX, clientY);
      if (!hit) return false;
      if (!api.travelTo(hit.worldY, hit.worldX)) return false;

      hoveredMenuIndex = -1;
      forceRedraw = true;
      return true;
    }

    // Converts a client-space pointer position into an active semantic menu item index.
    function findMenuItemIndexAtClientPoint(clientX, clientY) {
      if (!activeMenuItems.length || !mapCols || !mapRows) return -1;

      const rect = mapCanvas.getBoundingClientRect();
      if (!rect.width || !rect.height) return -1;
      if (
        clientX < rect.left ||
        clientX >= rect.right ||
        clientY < rect.top ||
        clientY >= rect.bottom
      ) {
        return -1;
      }

      const cellX = Math.floor(((clientX - rect.left) / rect.width) * mapCols);
      const cellY = Math.floor(((clientY - rect.top) / rect.height) * mapRows);

      for (let i = 0; i < activeMenuItems.length; i++) {
        const item = activeMenuItems[i];
        if (
          cellX >= item.x &&
          cellX < item.x + item.w &&
          cellY >= item.y &&
          cellY < item.y + item.h
        ) {
          return i;
        }
      }

      return -1;
    }

    // Binds semantic hover/click handling for menus exported by the wasm frontend.
    function bindMenuPointerInput() {
      const hoverMenuAtPoint = (clientX, clientY) => {
        if (isInteractiveTopPromptActive() || lineMorePromptActive) {
          hoveredMenuIndex = -1;
          return -1;
        }

        const index = findMenuItemIndexAtClientPoint(clientX, clientY);
        if (
          index < 0 ||
          !isPassiveHoverMenuItem(index) ||
          !api ||
          typeof api.menuHover !== "function"
        ) {
          hoveredMenuIndex = -1;
          return index;
        }

        if (index === hoveredMenuIndex) return index;

        hoveredMenuIndex = index;

        if (api.menuHover(index)) {
          forceRedraw = true;
        }

        return index;
      };

      mapCanvas.addEventListener("pointermove", (ev) => {
        if (ev.pointerType !== "mouse") return;
        hoverMenuAtPoint(ev.clientX, ev.clientY);
      });

      mapCanvas.addEventListener("pointerleave", () => {
        hoveredMenuIndex = -1;
      });

      mapCanvas.addEventListener("pointerdown", (ev) => {
        if (ev.button !== 0) return;
        if (isInteractiveTopPromptActive() || lineMorePromptActive) return;

        const index = findMenuItemIndexAtClientPoint(ev.clientX, ev.clientY);
        if (index < 0 || !api) return;

        hoveredMenuIndex = index;
        if (typeof api.menuActivate === "function" && api.menuActivate(index)) {
          activeMenuItems = [];
          activeMenuColumnX = null;
          hoveredMenuIndex = -1;
          forceRedraw = true;
        }

        ev.preventDefault();
        ev.stopPropagation();
      });
    }

    // Binds semantic hover/click handling for HTML menu overlays.
    function bindOverlayMenuInput() {
      const hoverFromOverlayEvent = (target) => {
        if (isInteractiveTopPromptActive() || lineMorePromptActive) {
          hoveredMenuIndex = -1;
          return;
        }

        const itemEl = target.closest("[data-menu-index]");
        if (!itemEl || !api || typeof api.menuHover !== "function") return;

        const index = Number(itemEl.dataset.menuIndex);
        if (
          !Number.isInteger(index) ||
          !isPassiveHoverMenuItem(index)
        ) {
          hoveredMenuIndex = -1;
          return;
        }

        if (index === hoveredMenuIndex) {
          return;
        }

        hoveredMenuIndex = index;
        if (api.menuHover(index)) {
          forceRedraw = true;
        }
      };

      overlayModalEl.addEventListener("pointermove", (ev) => {
        if (!overlayModalEl.classList.contains("overlay-menu")) return;
        if (ev.pointerType !== "mouse") return;
        hoverFromOverlayEvent(ev.target);
      });

      overlayModalEl.addEventListener("pointerover", (ev) => {
        if (!overlayModalEl.classList.contains("overlay-menu")) return;
        if (ev.pointerType !== "mouse") return;
        hoverFromOverlayEvent(ev.target);
      });

      overlayModalEl.addEventListener("click", (ev) => {
        if (!overlayModalEl.classList.contains("overlay-menu")) return;
        if (isInteractiveTopPromptActive() || lineMorePromptActive) return;

        const itemEl = ev.target.closest("[data-menu-index]");
        if (!itemEl || !api || typeof api.menuActivate !== "function") return;

        const index = Number(itemEl.dataset.menuIndex);
        if (!Number.isInteger(index)) return;

        hoveredMenuIndex = index;
        if (api.menuActivate(index)) {
          activeMenuItems = [];
          activeMenuColumnX = null;
          hoveredMenuIndex = -1;
          forceRedraw = true;
        }

        ev.preventDefault();
        ev.stopPropagation();
      });
    }

    // Binds click-to-dismiss for semantic modal dialogs such as discovered notes.
    function bindOverlayModalInput() {
      overlayModalEl.addEventListener("click", (ev) => {
        if (overlayModalEl.classList.contains("overlay-menu")) return;
        if (!overlayModalEl.classList.contains("overlay-dismissable")) return;
        if (!api || typeof api.modalActivate !== "function") return;

        if (api.modalActivate()) {
          overlayModalEl.style.display = "none";
          overlayModalEl.classList.remove("overlay-dismissable");
          overlayModalEl.classList.remove("overlay-menu");
          setHtmlIfChanged(overlayModalEl, "");
          forceRedraw = true;
          ev.preventDefault();
          ev.stopPropagation();
        }
      });
    }

    // Binds keyboard input and maps browser keys to Sil command bytes.
    function bindInput() {
      const keyMap = {
        Escape: 27,
        Enter: 13,
        Backspace: 8,
        Tab: 9,
        ArrowUp: "8".charCodeAt(0),
        ArrowDown: "2".charCodeAt(0),
        ArrowLeft: "4".charCodeAt(0),
        ArrowRight: "6".charCodeAt(0),
        Home: "7".charCodeAt(0),
        End: "1".charCodeAt(0),
        PageUp: "9".charCodeAt(0),
        PageDown: "3".charCodeAt(0),
      };

      const codeMap = {
        Numpad0: "0".charCodeAt(0),
        Numpad1: "1".charCodeAt(0),
        Numpad2: "2".charCodeAt(0),
        Numpad3: "3".charCodeAt(0),
        Numpad4: "4".charCodeAt(0),
        Numpad5: "5".charCodeAt(0),
        Numpad6: "6".charCodeAt(0),
        Numpad7: "7".charCodeAt(0),
        Numpad8: "8".charCodeAt(0),
        Numpad9: "9".charCodeAt(0),
      };

      const isEditableTarget = (target) => {
        if (!(target instanceof Element)) return false;
        if (target.isContentEditable) return true;
        const tag = target.tagName;
        return tag === "INPUT" || tag === "TEXTAREA" || tag === "SELECT";
      };

      // Advances an active -more- prompt using one supplied or default key.
      const advanceMorePrompt = (keyCode) => {
        overlayMorePromptActive = false;
        overlayMorePromptText = "";
        lineMorePromptActive = false;
        lineMorePromptText = "";
        syncMorePromptState();
        forceRedraw = true;
        pushAscii(keyCode !== null ? keyCode : " ".charCodeAt(0));
      };

      // Maps one browser key event to a single Sil ASCII command byte.
      const mapKeyEventToAscii = (ev) => {
        if (Object.hasOwn(keyMap, ev.key)) return keyMap[ev.key];
        if (Object.hasOwn(codeMap, ev.code)) return codeMap[ev.code];
        const altGraph =
          typeof ev.getModifierState === "function" &&
          ev.getModifierState("AltGraph");

        if (ev.ctrlKey && !ev.metaKey && !altGraph && ev.key.length === 1) {
          const upper = ev.key.toUpperCase();
          const code = upper.charCodeAt(0);
          if (code >= 64 && code <= 95) {
            return code & 0x1f;
          }
        }

        if (ev.key.length === 1 && !ev.metaKey && (!ev.ctrlKey || altGraph)) {
          return ev.key.charCodeAt(0);
        }
        return null;
      };

      document.addEventListener("keydown", (ev) => {
        if (!api) return;
        if (isEditableTarget(ev.target)) return;
        const keyCode = mapKeyEventToAscii(ev);

        if (morePromptActive) {
          advanceMorePrompt(keyCode);
          ev.preventDefault();
          return;
        }

        if (keyCode !== null) {
          pushAscii(keyCode);
          ev.preventDefault();
        }
      }, { capture: true });

      document.addEventListener("pointerdown", (ev) => {
        if (!api || !morePromptActive) return;
        if (ev.pointerType === "mouse" && ev.button !== 0) return;
        if (isEditableTarget(ev.target)) return;

        advanceMorePrompt(" ".charCodeAt(0));
        ev.preventDefault();
        ev.stopPropagation();
      }, { capture: true });
    }

    // Binds the compact mobile side drawer toggle and backdrop dismissal controls.
    function bindResponsiveShellInput() {
      sideToggleEl.addEventListener("pointerdown", (ev) => {
        ev.stopPropagation();
      });

      sideToggleEl.addEventListener("click", (ev) => {
        ev.preventDefault();
        ev.stopPropagation();
        setCompactSideOpen(!compactSideOpen);
      });

      sideBackdropEl.addEventListener("pointerdown", (ev) => {
        ev.preventDefault();
        ev.stopPropagation();
        setCompactSideOpen(false);
      });

      sideWrapEl.addEventListener("pointerdown", (ev) => {
        ev.stopPropagation();
      });
    }

    // Binds the floating backpack/close buttons to their current contextual actions.
    function bindFloatingActionInput() {
      for (const buttonEl of adjacentActionFabEls) {
        buttonEl.addEventListener("pointerdown", (ev) => {
          ev.stopPropagation();
        });

        buttonEl.addEventListener("click", (ev) => {
          ev.preventDefault();
          ev.stopPropagation();

          const dir = Number(buttonEl.dataset.adjDir);
          const heap = getHeapU8();
          const state = heap ? readPlayerState(heap) : null;
          const action = getAdjacentActionState(state, dir);
          if (!action.visible || !api || typeof api.actAdjacent !== "function") return;

          setCompactSideOpen(false);
          if (api.actAdjacent(dir)) {
            hoveredMenuIndex = -1;
            forceRedraw = true;
          }
        });
      }

      inventoryFabEl.addEventListener("pointerdown", (ev) => {
        ev.stopPropagation();
      });

      yesFabEl.addEventListener("pointerdown", (ev) => {
        ev.stopPropagation();
      });

      yesFabEl.addEventListener("click", (ev) => {
        ev.preventDefault();
        ev.stopPropagation();
        if (!isYesNoTopPromptActive()) return;

        setCompactSideOpen(false);
        pushAscii("y".charCodeAt(0));
        forceRedraw = true;
      });

      inventoryFabEl.addEventListener("click", (ev) => {
        ev.preventDefault();
        ev.stopPropagation();
        const heap = getHeapU8();
        const state = heap ? readPlayerState(heap) : null;
        if (!canUseInventoryFab(state)) return;

        setCompactSideOpen(false);
        if (api.openInventory()) {
          hoveredMenuIndex = -1;
          forceRedraw = true;
        }
      });

      tileActionFabEl.addEventListener("pointerdown", (ev) => {
        ev.stopPropagation();
      });

      tileActionFabEl.addEventListener("click", (ev) => {
        ev.preventDefault();
        ev.stopPropagation();
        const heap = getHeapU8();
        const state = heap ? readPlayerState(heap) : null;
        if (!canUseTileActionFab(state)) return;

        setCompactSideOpen(false);
        if (api.actHere()) {
          hoveredMenuIndex = -1;
          forceRedraw = true;
        }
      });

      closeFabEl.addEventListener("pointerdown", (ev) => {
        ev.stopPropagation();
      });

      closeFabEl.addEventListener("click", (ev) => {
        ev.preventDefault();
        ev.stopPropagation();
        if (!canUseCloseFab()) return;

        setCompactSideOpen(false);
        pushAscii(isYesNoTopPromptActive() ? "n".charCodeAt(0) : 27);
        forceRedraw = true;
      });
    }

    // Binds mouse/touch interactions for map zooming and drag-to-pan.
    function bindMapZoomInput() {
      mapWrapEl.addEventListener("wheel", (ev) => {
        if (!mapCanvas.width || !mapCanvas.height) return;
        ev.preventDefault();
        const factor = Math.exp((-ev.deltaY) * 0.0015);
        setMapZoom(mapZoom * factor, true);
      }, { passive: false });

      const touchPoints = new Map();
      let pinchBaseDistance = 0;
      let pinchBaseZoom = mapZoom;
      let dragPointerId = null;
      let dragLastX = 0;
      let dragLastY = 0;
      let touchTapPointerId = null;
      let touchTapStartX = 0;
      let touchTapStartY = 0;
      let touchTapEligible = false;
      let mouseClickPointerId = null;
      let mouseClickStartX = 0;
      let mouseClickStartY = 0;
      let mouseClickEligible = false;
      const mouseClickThresholdSq = MAP_CLICK_DRAG_THRESHOLD * MAP_CLICK_DRAG_THRESHOLD;
      const touchTapThresholdSq =
        Math.max(18, MAP_CLICK_DRAG_THRESHOLD * 2.5) ** 2;

      const resetPinch = () => {
        if (touchPoints.size < 2) pinchBaseDistance = 0;
      };

      const updatePinchBase = () => {
        if (touchPoints.size !== 2) return;
        const pts = [...touchPoints.values()];
        pinchBaseDistance = Math.hypot(pts[0].x - pts[1].x, pts[0].y - pts[1].y);
        pinchBaseZoom = mapZoom;
      };

      const startDrag = (pointerId, x, y) => {
        dragPointerId = pointerId;
        dragLastX = x;
        dragLastY = y;
        mapWrapEl.classList.add("dragging");
      };

      const stopDrag = (pointerId = null) => {
        if (pointerId !== null && dragPointerId !== pointerId) return;
        dragPointerId = null;
        mapWrapEl.classList.remove("dragging");
      };

      const updateDrag = (x, y) => {
        if (dragPointerId === null) return;
        const dx = x - dragLastX;
        const dy = y - dragLastY;
        dragLastX = x;
        dragLastY = y;
        mapWrapEl.scrollLeft -= dx;
        mapWrapEl.scrollTop -= dy;
      };

      const resumeSingleTouchDrag = () => {
        if (touchPoints.size !== 1) return;
        const [pointerId, pt] = touchPoints.entries().next().value;
        startDrag(pointerId, pt.x, pt.y);
      };

      mapWrapEl.addEventListener("pointerdown", (ev) => {
        if (ev.pointerType === "touch") {
          touchPoints.set(ev.pointerId, { x: ev.clientX, y: ev.clientY });
          if (touchPoints.size === 1) {
            touchTapPointerId = ev.pointerId;
            touchTapStartX = ev.clientX;
            touchTapStartY = ev.clientY;
            touchTapEligible = true;
            startDrag(ev.pointerId, ev.clientX, ev.clientY);
          } else if (touchPoints.size === 2) {
            touchTapPointerId = null;
            touchTapEligible = false;
            stopDrag();
            updatePinchBase();
          }
          ev.preventDefault();
        } else if (ev.pointerType === "mouse") {
          if (ev.button !== 0) return;
          mouseClickPointerId = ev.pointerId;
          mouseClickStartX = ev.clientX;
          mouseClickStartY = ev.clientY;
          mouseClickEligible = true;
          startDrag(ev.pointerId, ev.clientX, ev.clientY);
          ev.preventDefault();
        }

        try {
          mapWrapEl.setPointerCapture(ev.pointerId);
        } catch (_) {}
      });

      mapWrapEl.addEventListener("pointermove", (ev) => {
        if (ev.pointerType === "touch") {
          if (!touchPoints.has(ev.pointerId)) return;
          touchPoints.set(ev.pointerId, { x: ev.clientX, y: ev.clientY });

          if (touchPoints.size === 2 && pinchBaseDistance > 0) {
            const pts = [...touchPoints.values()];
            const distance = Math.hypot(pts[0].x - pts[1].x, pts[0].y - pts[1].y);
            if (distance > 0) {
              setMapZoom(pinchBaseZoom * (distance / pinchBaseDistance), true);
            }
            ev.preventDefault();
            return;
          }

          if (touchPoints.size === 1 && dragPointerId === ev.pointerId) {
            if (touchTapEligible && touchTapPointerId === ev.pointerId) {
              const dx = ev.clientX - touchTapStartX;
              const dy = ev.clientY - touchTapStartY;
              if ((dx * dx) + (dy * dy) > touchTapThresholdSq) {
                touchTapEligible = false;
              } else {
                ev.preventDefault();
                return;
              }
            }
            updateDrag(ev.clientX, ev.clientY);
            ev.preventDefault();
          }
          return;
        }

        if (ev.pointerType === "mouse" && dragPointerId === ev.pointerId) {
          if (mouseClickEligible && mouseClickPointerId === ev.pointerId) {
            const dx = ev.clientX - mouseClickStartX;
            const dy = ev.clientY - mouseClickStartY;
            if ((dx * dx) + (dy * dy) > mouseClickThresholdSq) {
              mouseClickEligible = false;
            }
          }
          updateDrag(ev.clientX, ev.clientY);
          ev.preventDefault();
        }
      }, { passive: false });

      const maybeCommitMouseClick = (ev) => {
        if (ev.pointerType !== "mouse") return;
        if (mouseClickPointerId !== ev.pointerId) return;

        const eligible = mouseClickEligible;
        mouseClickPointerId = null;
        mouseClickEligible = false;

        if (eligible) {
          tryMapTravelAtClientPoint(ev.clientX, ev.clientY);
        }
      };

      const commitTouchTapAt = (clientX, clientY) => {
        const eligible = touchTapEligible;
        touchTapPointerId = null;
        touchTapEligible = false;

        if (eligible) {
          tryMapTravelAtClientPoint(clientX, clientY);
        }
      };

      const maybeCommitTouchTap = (ev) => {
        if (ev.pointerType !== "touch") return;
        if (touchTapPointerId !== ev.pointerId) return;
        commitTouchTapAt(ev.clientX, ev.clientY);
      };

      const clearPointer = (ev) => {
        stopDrag(ev.pointerId);
        if (touchTapPointerId === ev.pointerId) {
          touchTapPointerId = null;
          touchTapEligible = false;
        }
        if (mouseClickPointerId === ev.pointerId) {
          mouseClickPointerId = null;
          mouseClickEligible = false;
        }
        if (touchPoints.has(ev.pointerId)) {
          touchPoints.delete(ev.pointerId);
          resetPinch();
          resumeSingleTouchDrag();
        }
      };

      mapWrapEl.addEventListener("pointerup", (ev) => {
        maybeCommitTouchTap(ev);
        maybeCommitMouseClick(ev);
        clearPointer(ev);
      });
      mapWrapEl.addEventListener("lostpointercapture", (ev) => {
        // Some mobile browsers release capture before pointerup; keep a valid tap.
        maybeCommitTouchTap(ev);
        maybeCommitMouseClick(ev);
        clearPointer(ev);
      });
      mapWrapEl.addEventListener("pointercancel", clearPointer);
      mapWrapEl.addEventListener("touchend", (ev) => {
        if (!touchTapEligible || !ev.changedTouches.length) return;
        const touch = ev.changedTouches[0];
        commitTouchTapAt(touch.clientX, touch.clientY);
      }, { passive: true });
    }

    /* ==========================================================================
     * WASM Bootstrapping
     * Loads tiles, configures Emscripten Module hooks, and binds exported API.
     * ========================================================================== */

    const tileImage = new Image();
    tileImage.onload = () => {
      tileImageReady = true;
      forceRedraw = true;
      lastFrameId = -1;
    };
    tileImage.onerror = () => {
      console.warn("Tiles PNG not found in web bundle, using ASCII fallback");
      tileImageReady = false;
      forceRedraw = true;
      lastFrameId = -1;
    };
    tileImage.src = "./lib/16x16_microchasm.png";

    window.Module = {
      locateFile: (path) => `./lib/${path}`,
      arguments: [
        "-mweb",
        "-g",
        `-da=${PERSIST_APEX_DIR}`,
        `-ds=${PERSIST_SAVE_DIR}`,
        `-du=${PERSIST_USER_DIR}`,
        `-dd=${PERSIST_DATA_DIR}`
      ],
      preRun: [() => {
        const FS = globalThis.FS;
        const IDBFS = globalThis.IDBFS;

        if (!FS) {
          console.error("Emscripten FS runtime is not available");
          return;
        }

        const ensureDir = (path) => {
          try { FS.mkdir(path); } catch (_) {}
        };
        const ensurePersistDirs = () => {
          ensureDir(PERSIST_APEX_DIR);
          ensureDir(PERSIST_SAVE_DIR);
          ensureDir(PERSIST_USER_DIR);
          ensureDir(PERSIST_DATA_DIR);
        };

        ensureDir("/persist");

        if (!IDBFS) {
          ensurePersistDirs();
          console.warn("IndexedDB persistence is unavailable; continuing with session storage only");
          return;
        }

        if (!globalThis.isSecureContext) {
          ensurePersistDirs();
          console.warn("IndexedDB persistence requires a secure origin; continuing with session storage only");
          return;
        }

        FS.mount(IDBFS, {}, "/persist");

        Module.addRunDependency("syncfs");
        FS.syncfs(true, (err) => {
          ensurePersistDirs();
          if (err) {
            disablePersistFs("syncfs init failed; continuing with session storage only", err);
          } else {
            persistFsEnabled = true;
            flushPersistFs(true);
          }
          Module.removeRunDependency("syncfs");
        });
      }],
      print: (text) => console.log(text),
      printErr: (text) => console.error(text),
      setStatus: (text) => { setStatusText(text); },
      onRuntimeInitialized: () => {
        const hasAPI =
          typeof Module._web_get_cols === "function" &&
          typeof Module._web_get_rows === "function" &&
          typeof Module._web_get_cells_ptr === "function" &&
          typeof Module._web_get_cell_stride === "function" &&
          typeof Module._web_get_tile_wid === "function" &&
          typeof Module._web_get_tile_hgt === "function" &&
          typeof Module._web_get_map_cols === "function" &&
          typeof Module._web_get_map_rows === "function" &&
          typeof Module._web_get_map_col === "function" &&
          typeof Module._web_get_map_row === "function" &&
          typeof Module._web_get_map_x_step === "function" &&
          typeof Module._web_get_map_world_x === "function" &&
          typeof Module._web_get_map_world_y === "function" &&
          typeof Module._web_get_map_cells_ptr === "function" &&
          typeof Module._web_get_player_map_x === "function" &&
          typeof Module._web_get_player_map_y === "function" &&
          typeof Module._web_get_cursor_x === "function" &&
          typeof Module._web_get_cursor_y === "function" &&
          typeof Module._web_get_cursor_visible === "function" &&
          typeof Module._web_get_icon_alert_attr === "function" &&
          typeof Module._web_get_icon_alert_char === "function" &&
          typeof Module._web_get_icon_glow_attr === "function" &&
          typeof Module._web_get_icon_glow_char === "function" &&
          typeof Module._web_get_frame_id === "function" &&
          typeof Module._web_push_key === "function" &&
          typeof Module._web_get_player_state_ptr === "function" &&
          typeof Module._web_get_player_state_len === "function" &&
          typeof Module._web_get_log_text_ptr === "function" &&
          typeof Module._web_get_log_text_len === "function" &&
          typeof Module._web_get_log_attrs_ptr === "function" &&
          typeof Module._web_get_log_attrs_len === "function" &&
          typeof Module._web_get_overlay_mode === "function" &&
          typeof Module._web_get_overlay_text_ptr === "function" &&
          typeof Module._web_get_overlay_text_len === "function" &&
          typeof Module._web_get_overlay_attrs_ptr === "function" &&
          typeof Module._web_get_overlay_attrs_len === "function" &&
          typeof Module._web_get_fx_cells_ptr === "function" &&
          typeof Module._web_get_fx_cells_cols === "function" &&
          typeof Module._web_get_fx_cells_rows === "function" &&
          typeof Module._web_get_fx_delay_seq === "function" &&
          typeof Module._web_get_fx_delay_msec === "function" &&
          typeof Module._web_get_menu_items_ptr === "function" &&
          typeof Module._web_get_menu_item_count === "function" &&
          typeof Module._web_get_menu_item_stride === "function" &&
          typeof Module._web_get_menu_text_ptr === "function" &&
          typeof Module._web_get_menu_text_len === "function" &&
          typeof Module._web_get_menu_attrs_ptr === "function" &&
          typeof Module._web_get_menu_attrs_len === "function" &&
          typeof Module._web_get_menu_details_ptr === "function" &&
          typeof Module._web_get_menu_details_len === "function" &&
          typeof Module._web_get_menu_details_attrs_ptr === "function" &&
          typeof Module._web_get_menu_details_attrs_len === "function" &&
          typeof Module._web_get_modal_text_ptr === "function" &&
          typeof Module._web_get_modal_text_len === "function" &&
          typeof Module._web_get_modal_attrs_ptr === "function" &&
          typeof Module._web_get_modal_attrs_len === "function" &&
          typeof Module._web_get_modal_dismiss_key === "function" &&
          typeof Module._web_get_modal_revision === "function" &&
          typeof Module._web_menu_hover === "function" &&
          typeof Module._web_menu_activate === "function" &&
          typeof Module._web_modal_activate === "function";

        if (!hasAPI) {
          setStatusText("WASM started, but web_* API missing");
          return;
        }

        api = {
          getCols: Module._web_get_cols,
          getRows: Module._web_get_rows,
          getCellsPtr: Module._web_get_cells_ptr,
          getCellStride: Module._web_get_cell_stride,
          getTileWid: Module._web_get_tile_wid,
          getTileHgt: Module._web_get_tile_hgt,
          getMapCol: Module._web_get_map_col,
          getMapRow: Module._web_get_map_row,
          getMapCols: Module._web_get_map_cols,
          getMapRows: Module._web_get_map_rows,
          getMapXStep: Module._web_get_map_x_step,
          getMapWorldX: Module._web_get_map_world_x,
          getMapWorldY: Module._web_get_map_world_y,
          getMapCellsPtr: Module._web_get_map_cells_ptr,
          getPlayerMapX: Module._web_get_player_map_x,
          getPlayerMapY: Module._web_get_player_map_y,
          getCursorX: Module._web_get_cursor_x,
          getCursorY: Module._web_get_cursor_y,
          getCursorVisible: Module._web_get_cursor_visible,
          actHere:
            typeof Module._web_act_here === "function"
              ? Module._web_act_here
              : null,
          actAdjacent:
            typeof Module._web_act_adjacent === "function"
              ? Module._web_act_adjacent
              : null,
          openInventory:
            typeof Module._web_open_inventory === "function"
              ? Module._web_open_inventory
              : null,
          travelTo:
            typeof Module._web_travel_to === "function"
              ? Module._web_travel_to
              : null,
          getAlertAttr: Module._web_get_icon_alert_attr,
          getAlertChar: Module._web_get_icon_alert_char,
          getGlowAttr: Module._web_get_icon_glow_attr,
          getGlowChar: Module._web_get_icon_glow_char,
          getFrameId: Module._web_get_frame_id,
          pushKey: Module._web_push_key,
          getPlayerStatePtr: Module._web_get_player_state_ptr,
          getPlayerStateLen: Module._web_get_player_state_len,
          getLogTextPtr: Module._web_get_log_text_ptr,
          getLogTextLen: Module._web_get_log_text_len,
          getLogAttrsPtr: Module._web_get_log_attrs_ptr,
          getLogAttrsLen: Module._web_get_log_attrs_len,
          getOverlayMode: Module._web_get_overlay_mode,
          getOverlayTextPtr: Module._web_get_overlay_text_ptr,
          getOverlayTextLen: Module._web_get_overlay_text_len,
          getOverlayAttrsPtr: Module._web_get_overlay_attrs_ptr,
          getOverlayAttrsLen: Module._web_get_overlay_attrs_len,
          getFxCellsPtr: Module._web_get_fx_cells_ptr,
          getFxCellsCols: Module._web_get_fx_cells_cols,
          getFxCellsRows: Module._web_get_fx_cells_rows,
          getFxDelaySeq: Module._web_get_fx_delay_seq,
          getFxDelayMsec: Module._web_get_fx_delay_msec,
          getMenuItemsPtr: Module._web_get_menu_items_ptr,
          getMenuItemCount: Module._web_get_menu_item_count,
          getMenuItemStride: Module._web_get_menu_item_stride,
          getMenuTextPtr: Module._web_get_menu_text_ptr,
          getMenuTextLen: Module._web_get_menu_text_len,
          getMenuAttrsPtr: Module._web_get_menu_attrs_ptr,
          getMenuAttrsLen: Module._web_get_menu_attrs_len,
          getMenuActiveX: Module._web_get_menu_active_x,
          getMenuRevision: Module._web_get_menu_revision,
          getMenuDetailsPtr: Module._web_get_menu_details_ptr,
          getMenuDetailsLen: Module._web_get_menu_details_len,
          getMenuDetailsAttrsPtr: Module._web_get_menu_details_attrs_ptr,
          getMenuDetailsAttrsLen: Module._web_get_menu_details_attrs_len,
          getMenuDetailsWidth:
            typeof Module._web_get_menu_details_width === "function"
              ? Module._web_get_menu_details_width
              : null,
          getMenuDetailsVisualKind:
            typeof Module._web_get_menu_details_visual_kind === "function"
              ? Module._web_get_menu_details_visual_kind
              : null,
          getMenuDetailsVisualAttr:
            typeof Module._web_get_menu_details_visual_attr === "function"
              ? Module._web_get_menu_details_visual_attr
              : null,
          getMenuDetailsVisualChar:
            typeof Module._web_get_menu_details_visual_char === "function"
              ? Module._web_get_menu_details_visual_char
              : null,
          getModalTextPtr: Module._web_get_modal_text_ptr,
          getModalTextLen: Module._web_get_modal_text_len,
          getModalAttrsPtr: Module._web_get_modal_attrs_ptr,
          getModalAttrsLen: Module._web_get_modal_attrs_len,
          getModalDismissKey: Module._web_get_modal_dismiss_key,
          getModalRevision: Module._web_get_modal_revision,
          menuHover: Module._web_menu_hover,
          menuActivate: Module._web_menu_activate,
          modalActivate: Module._web_modal_activate,
        };

        cellStride = api.getCellStride();
        tileSrcW = api.getTileWid();
        tileSrcH = api.getTileHgt();
        tileW = tileSrcW * MAP_TILE_SCALE;
        tileH = tileSrcH * MAP_TILE_SCALE;
        refreshLayoutMetadata(api.getCols(), api.getRows());

        setStatusText("Running (-mweb). Tap or click the map to travel. Pinch or wheel to zoom. The backpack button opens inventory.");
        bindInput();
        bindResponsiveShellInput();
        bindFloatingActionInput();
        bindMenuPointerInput();
        bindOverlayMenuInput();
        bindOverlayModalInput();
        bindMapZoomInput();
        startPersistSyncLoop();
        syncResponsiveShell(true);

        // Schedules continuous rendering with requestAnimationFrame.
        function tick() {
          render();
          requestAnimationFrame(tick);
        }
        tick();
      },
    };

    /* ==========================================================================
     * Global Event Hooks
     * Reflows map sizing and follow-camera state when the viewport changes.
     * ========================================================================== */
    
    window.addEventListener("resize", () => {
      syncResponsiveShell(false);
      if (api) {
        const heap = getHeapU8();
        let overlayActive = false;
        if (heap) overlayActive = updateOverlaySemantic(heap);
        followPlayerInViewport(overlayActive, true);
      }
    });

})();
