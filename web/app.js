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
    const rangedFabEl = document.getElementById("ranged-fab");
    const rangedFabVisualEl = document.getElementById("ranged-fab-visual");
    const stateFabWrapEl = document.getElementById("state-fab-wrap");
    const stateFabEl = document.getElementById("state-fab");
    const stateFabVisualEl = document.getElementById("state-fab-visual");
    const stateFabLabelEl = document.getElementById("state-fab-label");
    const songFabWrapEl = document.getElementById("song-fab-wrap");
    const songFabEl = document.getElementById("song-fab");
    const songFabLabelEl = document.getElementById("song-fab-label");
    const tileActionFabEl = document.getElementById("tile-action-fab");
    const tileActionFabVisualEl = document.getElementById("tile-action-fab-visual");
    const hudCharacterButtonEl = document.getElementById("hud-character-button");
    const hudCharacterVisualEl = document.getElementById("hud-character-button-visual");
    const adjacentActionFabEls = Array.from(
      document.querySelectorAll(".adjacent-action-fab")
    );
    const overlayModalEl = document.getElementById("overlay-modal");
    const hudBarsEl = document.getElementById("hud-bars");
    const sideEl = document.getElementById("side");
    const sideWrapEl = document.getElementById("side-wrap");
    const sideCollapseButtonEl = document.getElementById("side-collapse-button");
    const sideLogButtonEl = document.getElementById("side-log-button");
    const sideKnowledgeButtonEl = document.getElementById("side-knowledge-button");
    const sideCharacterButtonEl = document.getElementById("side-character-button");
    const sideCharacterVisualEl = document.getElementById("side-character-button-visual");
    const sideBackdropEl = document.getElementById("side-backdrop");
    const sideToggleEl = document.getElementById("side-toggle");
    const logEl = document.getElementById("log");
    const PERSIST_APEX_DIR = "/persist/apex";
    const PERSIST_SAVE_DIR = "/persist/save-web";
    const PERSIST_USER_DIR = "/persist/user";
    const PERSIST_DATA_DIR = "/persist/data";
    const PERSIST_AUTO_RESUME_MARKER = `${PERSIST_USER_DIR}/web-autoresume.txt`;
    const IDB_SYNC_INTERVAL_MSEC = 5000;
    const AUTOSAVE_IDLE_MSEC = 8000;
    const AUTOSAVE_MIN_INTERVAL_MSEC = 30000;
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
    const TARGET_MARK_ALPHA = 0.5;
    const TILE_CONTEXT_LONG_PRESS_MSEC = 450;

    const WEB_FLAG_ALERT = 0x01;
    const WEB_FLAG_GLOW = 0x02;
    const WEB_FLAG_FG_PICT = 0x04;
    const WEB_FLAG_BG_PICT = 0x08;
    const WEB_FLAG_MARK = 0x10;
    const UI_PROMPT_KIND_NONE = 0;
    const UI_PROMPT_KIND_MESSAGE = 1;
    const UI_PROMPT_KIND_GENERIC = 2;
    const UI_PROMPT_KIND_YES_NO = 3;
    const UI_PROMPT_KIND_TARGET = 4;
    const UI_PROMPT_KIND_MORE = 5;

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
    let lastPromptRevision = -1;
    let lastBirthStateRevision = -1;
    let lastCharacterSheetRevision = -1;
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
    let lastMapFxActive = false;
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
    let morePromptAttrs = [];
    let topPromptActive = false;
    let topPromptKind = UI_PROMPT_KIND_NONE;
    let topPromptMoreHint = false;
    let topPromptText = "";
    let topPromptAttrs = null;
    let persistFsEnabled = false;
    let idbSyncTimer = null;
    let idbSyncInFlight = false;
    let idbSyncPending = false;
    let idbSyncHooksBound = false;
    let autosaveTimer = null;
    let autosaveLastAt = 0;
    let autosavePrimed = false;
    let activeMenuItems = [];
    let activeMenuColumnX = null;
    let activeMenuText = "";
    let activeMenuTextAttrs = null;
    let activeMenuDetailsText = "";
    let activeMenuDetailsAttrs = null;
    let activeMenuSummaryText = "";
    let activeMenuSummaryAttrs = null;
    let activeMenuDetailsWidth = null;
    let activeMenuSummaryRows = null;
    let activeMenuDetailsVisual = null;
    let activeBirthState = null;
    let activeCharacterSheetState = null;
    let activePlayerState = null;
    let activeTileContextState = null;
    let semanticMenuSnapshot = null;
    let hoveredMenuIndex = -1;
    let compactSideOpen = false;
    let sideCollapsed = false;
    let wasCompactLayout = false;
    let birthTextDraft = "";
    let birthTextSourceText = "";
    let birthTextSourceKind = "";
    let birthTextDirty = false;
    let birthAhwDraft = createEmptyBirthAhwDraft();
    let birthAhwSource = createEmptyBirthAhwDraft();
    let birthAhwDirty = false;
    const alternateFabLongPressTimers = new WeakMap();
    const alternateFabSuppressedClicks = new WeakSet();
    const iconMeta = {
      alertAttr: 0,
      alertChar: 0,
      glowAttr: 0,
      glowChar: 0,
    };
    const utf8Decoder = new TextDecoder("utf-8");
    const utf8Encoder = new TextEncoder();
    const MAX_PENDING_SOUND_EVENTS = 64;
    const FRONTEND_NOISE_EVENT_ID = 0;
    const FRONTEND_NOISE_SAMPLE_PATH = "./assets/bell.flac";
    let soundPlayer = null;

    // Disables IndexedDB-backed persistence and clears any related timers.
    function disablePersistFs(reason, error = null) {
      persistFsEnabled = false;
      idbSyncInFlight = false;
      idbSyncPending = false;
      autosavePrimed = false;

      if (autosaveTimer !== null) {
        globalThis.clearTimeout(autosaveTimer);
        autosaveTimer = null;
      }

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

    // Clears any pending delayed autosave request.
    function clearAutosaveTimer() {
      if (autosaveTimer === null) return;
      globalThis.clearTimeout(autosaveTimer);
      autosaveTimer = null;
    }

    // Returns whether the current runtime state can be persisted as a gameplay save.
    function canAutosaveGame(state = activePlayerState) {
      return persistFsEnabled &&
        !!api &&
        typeof api.saveGameAutomatically === "function" &&
        !!state &&
        Number(state.ready) === 1;
    }

    // Performs one real in-engine autosave and flushes the persisted filesystem.
    function performAutosave(reason, { force = false, flush = true } = {}) {
      clearAutosaveTimer();

      const heap = getHeapU8();
      const state = heap ? readPlayerState(heap) : activePlayerState;
      if (!canAutosaveGame(state)) {
        if (flush) flushPersistFs(true);
        return false;
      }

      const now = Date.now();
      const elapsed = autosaveLastAt > 0 ? now - autosaveLastAt : AUTOSAVE_MIN_INTERVAL_MSEC;
      if (!force && elapsed < AUTOSAVE_MIN_INTERVAL_MSEC) {
        scheduleAutosave(reason, AUTOSAVE_MIN_INTERVAL_MSEC - elapsed);
        return false;
      }

      let saved = false;
      try {
        saved = api.saveGameAutomatically() !== 0;
      } catch (error) {
        console.warn(`Autosave failed during ${reason}:`, error);
      }

      if (saved) {
        autosaveLastAt = Date.now();
      }

      if (flush || saved) {
        flushPersistFs(true);
      }

      return saved;
    }

    // Schedules one debounced gameplay autosave after recent activity settles.
    function scheduleAutosave(reason, delay = AUTOSAVE_IDLE_MSEC) {
      if (!persistFsEnabled || !api || typeof api.saveGameAutomatically !== "function") {
        return;
      }

      clearAutosaveTimer();
      autosaveTimer = globalThis.setTimeout(() => {
        autosaveTimer = null;
        performAutosave(reason, { force: false, flush: true });
      }, Math.max(0, Math.floor(delay)));
    }

    // Tracks gameplay frame activity so active runs are saved in the background too.
    function noteAutosaveGameplayActivity(frameChanged, stateReady) {
      if (!persistFsEnabled) return;

      if (!stateReady) {
        autosavePrimed = false;
        clearAutosaveTimer();
        return;
      }

      if (!frameChanged) return;

      if (!autosavePrimed) {
        autosavePrimed = true;
        return;
      }

      scheduleAutosave("gameplay");
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
        performAutosave("pagehide", { force: true, flush: true });
      });

      globalThis.addEventListener("beforeunload", () => {
        performAutosave("beforeunload", { force: true, flush: true });
      });

      document.addEventListener("visibilitychange", () => {
        if (document.visibilityState === "hidden") {
          performAutosave("visibilitychange", { force: true, flush: true });
        }
      });

      document.addEventListener("freeze", () => {
        performAutosave("freeze", { force: true, flush: true });
      });
    }

    // Returns the saved-character base name to auto-open, if one was persisted.
    function readPersistedAutoResumeBaseName(FS) {
      if (!FS || typeof FS.readFile !== "function") return "";

      let resumePath = "";
      try {
        resumePath = String(
          FS.readFile(PERSIST_AUTO_RESUME_MARKER, { encoding: "utf8" }) || ""
        ).trim();
      } catch (_) {
        return "";
      }

      if (!resumePath.startsWith(`${PERSIST_SAVE_DIR}/`)) {
        return "";
      }

      const stat = FS.analyzePath(resumePath);
      if (!stat || !stat.exists) {
        return "";
      }

      const parts = resumePath.split("/");
      return String(parts[parts.length - 1] || "").trim();
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
      if (!mapDisplayReady) compactSideOpen = false;
      syncResponsiveShellChrome();
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

    // Parses sound.cfg into a map from event name to one or more sample files.
    function parseSoundConfig(text) {
      const samplesByName = new Map();
      let inSoundSection = false;

      for (const rawLine of String(text || "").split(/\r?\n/)) {
        const line = rawLine.trim();
        if (!line || line.startsWith("#") || line.startsWith(";")) continue;

        if (line.startsWith("[") && line.endsWith("]")) {
          inSoundSection = line.slice(1, -1).trim().toLowerCase() === "sound";
          continue;
        }

        if (!inSoundSection) continue;

        const eq = line.indexOf("=");
        if (eq < 0) continue;

        const name = line.slice(0, eq).trim().toLowerCase();
        const sampleNames = line
          .slice(eq + 1)
          .trim()
          .split(/\s+/)
          .filter(Boolean);

        if (!name || !sampleNames.length) continue;
        samplesByName.set(name, sampleNames);
      }

      return samplesByName;
    }

    // Wraps browser-specific decodeAudioData variants behind one promise API.
    function decodeAudioDataCompat(context, arrayBuffer) {
      return new Promise((resolve, reject) => {
        const promise = context.decodeAudioData(arrayBuffer, resolve, reject);
        if (promise && typeof promise.then === "function") {
          promise.then(resolve, reject);
        }
      });
    }

    class WebSoundPlayer {
      constructor(module, options) {
        this.module = module;
        this.getSoundCount = options.getSoundCount;
        this.getSoundNamePtr = options.getSoundNamePtr;
        this.getSoundNameLen = options.getSoundNameLen;
        this.noiseSamplePath = options.noiseSamplePath || FRONTEND_NOISE_SAMPLE_PATH;
        this.pendingEvents = [];
        this.samplePathsByEvent = null;
        this.samplePathsPromise = null;
        this.audioContext = null;
        this.masterGain = null;
        this.bufferCache = new Map();
        this.bufferPromiseCache = new Map();
        this.flushPromise = null;
        this.unlockHandlersBound = false;
        this.warnedPaths = new Set();
      }

      bindUnlockGestures(target = document) {
        if (
          this.unlockHandlersBound ||
          !target ||
          typeof target.addEventListener !== "function"
        ) {
          return;
        }

        this.unlockHandlersBound = true;
        const unlock = () => {
          void this.unlockAudio();
        };

        target.addEventListener("pointerdown", unlock, {
          capture: true,
          passive: true,
        });
        target.addEventListener("keydown", unlock, { capture: true });
      }

      ensureAudioContext() {
        const AudioContextCtor =
          globalThis.AudioContext || globalThis.webkitAudioContext;
        if (!AudioContextCtor) return null;

        if (!this.audioContext) {
          this.audioContext = new AudioContextCtor();
          this.masterGain = this.audioContext.createGain();
          this.masterGain.gain.value = 0.75;
          this.masterGain.connect(this.audioContext.destination);
        }

        return this.audioContext;
      }

      async unlockAudio() {
        const context = this.ensureAudioContext();
        if (!context) return false;

        if (context.state === "suspended") {
          try {
            await context.resume();
          } catch (error) {
            console.warn("Failed to resume web audio:", error);
            return false;
          }
        }

        void this.flushPendingEvents();
        return context.state === "running";
      }

      enqueueEvent(eventId) {
        if (!Number.isInteger(eventId) || eventId < FRONTEND_NOISE_EVENT_ID) return;

        if (this.pendingEvents.length >= MAX_PENDING_SOUND_EVENTS) {
          this.pendingEvents.shift();
        }

        this.pendingEvents.push(eventId);
        void this.flushPendingEvents();
      }

      async ensureSamplePathsByEvent() {
        if (this.samplePathsByEvent) return this.samplePathsByEvent;
        if (this.samplePathsPromise) return this.samplePathsPromise;

        this.samplePathsPromise = this.loadSamplePathsByEvent()
          .then((pathsByEvent) => {
            this.samplePathsByEvent = pathsByEvent;
            return pathsByEvent;
          })
          .finally(() => {
            this.samplePathsPromise = null;
          });

        return this.samplePathsPromise;
      }

      async loadSamplePathsByEvent() {
        const FS = this.module?.FS || globalThis.FS;
        const heap = this.module?.HEAPU8 || globalThis.HEAPU8;
        if (
          !FS ||
          !heap ||
          typeof this.getSoundCount !== "function" ||
          typeof this.getSoundNamePtr !== "function" ||
          typeof this.getSoundNameLen !== "function"
        ) {
          return new Map();
        }

        let cfgText = "";
        try {
          const cfgBytes = FS.readFile("/lib/xtra/sound/sound.cfg");
          cfgText = typeof cfgBytes === "string"
            ? cfgBytes
            : utf8Decoder.decode(cfgBytes);
        } catch (error) {
          console.warn("Failed to load /lib/xtra/sound/sound.cfg:", error);
          return new Map();
        }

        const samplesByName = parseSoundConfig(cfgText);
        const pathsByEvent = new Map();
        const soundCount = Number(this.getSoundCount()) || 0;

        for (let i = 0; i < soundCount; i++) {
          const namePtr = Number(this.getSoundNamePtr(i)) || 0;
          const nameLen = Number(this.getSoundNameLen(i)) || 0;
          const eventName = readUtf8(heap, namePtr, nameLen).trim().toLowerCase();
          if (!eventName) continue;

          const sampleNames = samplesByName.get(eventName);
          if (!sampleNames || !sampleNames.length) continue;

          pathsByEvent.set(
            i,
            sampleNames.map((sampleName) => `/lib/xtra/sound/${sampleName}`)
          );
        }

        return pathsByEvent;
      }

      async flushPendingEvents() {
        if (this.flushPromise) return this.flushPromise;

        this.flushPromise = this.runPendingEvents()
          .finally(() => {
            this.flushPromise = null;
          });

        return this.flushPromise;
      }

      async runPendingEvents() {
        const context = this.ensureAudioContext();
        if (!context || context.state !== "running" || !this.pendingEvents.length) {
          return;
        }

        const samplePathsByEvent = await this.ensureSamplePathsByEvent();
        while (this.pendingEvents.length && context.state === "running") {
          const eventId = this.pendingEvents.shift();
          if (eventId === FRONTEND_NOISE_EVENT_ID) {
            await this.playFrontendNoise();
            continue;
          }

          const samplePaths = samplePathsByEvent.get(eventId);
          if (!samplePaths || !samplePaths.length) continue;

          const samplePath =
            samplePaths[Math.floor(Math.random() * samplePaths.length)];
          const buffer = await this.loadBuffer(samplePath);
          if (!buffer) continue;

          this.playBuffer(buffer);
        }
      }

      async loadBuffer(path) {
        if (this.bufferCache.has(path)) {
          return this.bufferCache.get(path);
        }
        if (this.bufferPromiseCache.has(path)) {
          return this.bufferPromiseCache.get(path);
        }

        const bufferPromise = this.readAndDecodeBuffer(path)
          .then((buffer) => {
            if (buffer) this.bufferCache.set(path, buffer);
            return buffer;
          })
          .finally(() => {
            this.bufferPromiseCache.delete(path);
          });

        this.bufferPromiseCache.set(path, bufferPromise);
        return bufferPromise;
      }

      async playFrontendNoise() {
        if (this.noiseSamplePath) {
          const buffer = await this.loadBuffer(this.noiseSamplePath);
          if (buffer) {
            this.playBuffer(buffer);
            return;
          }
        }

        this.playFallbackBeep();
      }

      async readAndDecodeBuffer(path) {
        if (typeof path !== "string" || !path) return null;

        if (!path.startsWith("/")) {
          return this.fetchAndDecodeBuffer(path);
        }

        const context = this.ensureAudioContext();
        const FS = this.module?.FS || globalThis.FS;
        if (!context || !FS) return null;

        let bytes;
        try {
          bytes = FS.readFile(path);
        } catch (error) {
          if (!this.warnedPaths.has(path)) {
            this.warnedPaths.add(path);
            console.warn(`Failed to read sound asset ${path}:`, error);
          }
          return null;
        }

        const audioBytes = bytes instanceof Uint8Array ? bytes : new Uint8Array(bytes);
        const audioData = audioBytes.buffer.slice(
          audioBytes.byteOffset,
          audioBytes.byteOffset + audioBytes.byteLength
        );

        try {
          return await decodeAudioDataCompat(context, audioData);
        } catch (error) {
          if (!this.warnedPaths.has(path)) {
            this.warnedPaths.add(path);
            console.warn(`Failed to decode sound asset ${path}:`, error);
          }
          return null;
        }
      }

      async fetchAndDecodeBuffer(path) {
        const context = this.ensureAudioContext();
        if (!context) return null;

        let response;
        try {
          response = await globalThis.fetch(path);
        } catch (error) {
          if (!this.warnedPaths.has(path)) {
            this.warnedPaths.add(path);
            console.warn(`Failed to fetch sound asset ${path}:`, error);
          }
          return null;
        }

        if (!response.ok) {
          if (!this.warnedPaths.has(path)) {
            this.warnedPaths.add(path);
            console.warn(`Failed to fetch sound asset ${path}: HTTP ${response.status}`);
          }
          return null;
        }

        try {
          return await decodeAudioDataCompat(context, await response.arrayBuffer());
        } catch (error) {
          if (!this.warnedPaths.has(path)) {
            this.warnedPaths.add(path);
            console.warn(`Failed to decode sound asset ${path}:`, error);
          }
          return null;
        }
      }

      playBuffer(buffer) {
        if (!this.audioContext || !this.masterGain) return;

        const source = this.audioContext.createBufferSource();
        source.buffer = buffer;
        source.connect(this.masterGain);
        source.start();
      }

      playFallbackBeep() {
        const context = this.ensureAudioContext();
        if (!context || !this.masterGain) return;

        const oscillator = context.createOscillator();
        const gain = context.createGain();
        const startedAt = context.currentTime;

        oscillator.type = "triangle";
        oscillator.frequency.setValueAtTime(880, startedAt);
        gain.gain.setValueAtTime(0.0001, startedAt);
        gain.gain.exponentialRampToValueAtTime(0.12, startedAt + 0.01);
        gain.gain.exponentialRampToValueAtTime(0.0001, startedAt + 0.18);

        oscillator.connect(gain);
        gain.connect(this.masterGain);
        oscillator.start(startedAt);
        oscillator.stop(startedAt + 0.2);
      }
    }

    function createWebSoundPlayer(options) {
      return new WebSoundPlayer(options.module, options);
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

    // Projects one map cell into the shared glyph/tile visual payload used by toolbar buttons.
    function normalizeCellVisual(cell) {
      if (!cell) return null;

      if (cell.kind === WEB_CELL_PICT) {
        if ((cell.flags & WEB_FLAG_FG_PICT) !== 0) {
          return normalizeMenuItemVisual(WEB_CELL_PICT, cell.fgAttr, cell.fgChar);
        }
        if ((cell.flags & WEB_FLAG_BG_PICT) !== 0) {
          return normalizeMenuItemVisual(WEB_CELL_PICT, cell.bgAttr, cell.bgChar);
        }
      }

      if (cell.kind === WEB_CELL_TEXT) {
        return normalizeMenuItemVisual(WEB_CELL_TEXT, cell.textAttr, cell.textChar);
      }

      return null;
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

    // Returns the backend-selected active menu column for styling and scroll-follow.
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

    // Only the rightmost submenu column should react to passive hover; older columns remain click-only.
    function isPassiveHoverMenuItem(index) {
      if (
        index < 0 ||
        index >= activeMenuItems.length ||
        !Number.isInteger(index)
      ) {
        return false;
      }

      let rightmostColumnX = activeMenuItems[0].x;
      for (const item of activeMenuItems) {
        if (item.x > rightmostColumnX) rightmostColumnX = item.x;
      }

      return activeMenuItems[index].x === rightmostColumnX;
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

    // Keeps one selected editor row visible within the overlay scroll area.
    function scrollOverlayRowIntoView(itemEl) {
      const container = overlayModalEl;
      if (!itemEl || !container) return;

      const itemRect = itemEl.getBoundingClientRect();
      const containerRect = container.getBoundingClientRect();
      const pad = 8;

      if (itemRect.top < containerRect.top + pad) {
        container.scrollTop -= (containerRect.top + pad) - itemRect.top;
      } else if (itemRect.bottom > containerRect.bottom - pad) {
        container.scrollTop += itemRect.bottom - (containerRect.bottom - pad);
      }
    }

    // Keeps one overlay editor section visible by scrolling its containing card into view.
    function syncOverlayEditorSectionIntoView(overlayClass, fieldSelector, sectionSelector) {
      if (!overlayModalEl.classList.contains(overlayClass)) return;
      const fieldEl = overlayModalEl.querySelector(fieldSelector);
      if (!fieldEl) return;

      const targetEl = sectionSelector
        ? fieldEl.closest(sectionSelector) || fieldEl
        : fieldEl;
      scrollOverlayRowIntoView(targetEl);
    }

    // After re-rendering the birth A/H/W overlay, keeps the physical section visible.
    function syncBirthAhwEditorIntoView() {
      syncOverlayEditorSectionIntoView(
        "overlay-birth-ahw",
        '[data-birth-ahw-field="age"]',
        ".character-sheet-meta-card"
      );
    }

    // After re-rendering the birth-stat overlay, keeps the selected stat in view.
    function syncSelectedBirthStatIntoView() {
      if (!overlayModalEl.classList.contains("overlay-birth-stats")) return;
      const selectedItem = overlayModalEl.querySelector(
        ".character-sheet-stat-row-selected"
      );
      scrollOverlayRowIntoView(selectedItem);
    }

    // After re-rendering the skill editor overlay, keeps the selected skill in view.
    function syncSelectedCharacterSkillIntoView() {
      if (!overlayModalEl.classList.contains("overlay-character-skills")) return;
      const selectedItem = overlayModalEl.querySelector(
        ".character-sheet-skill-row-selected"
      );
      scrollOverlayRowIntoView(selectedItem);
    }

    // Saves the last live semantic menu so transient prompts can reuse it without falling back to terminal capture.
    function snapshotSemanticMenu(
      items,
      activeColumnX,
      text,
      textAttrs,
      detailsText,
      detailsAttrs,
      summaryText,
      summaryAttrs,
      detailsWidth,
      summaryRows,
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
        summaryText,
        summaryAttrs: cloneAttrBytes(summaryAttrs),
        detailsWidth,
        summaryRows,
        detailsVisual: detailsVisual ? { ...detailsVisual } : null,
      };
    }

    // Returns whether the top line is showing a prompt that still expects input.
    function isInteractiveTopPromptActive() {
      return topPromptActive && (
        topPromptKind === UI_PROMPT_KIND_GENERIC ||
        topPromptKind === UI_PROMPT_KIND_YES_NO ||
        topPromptKind === UI_PROMPT_KIND_TARGET
      );
    }

    // Returns whether the top prompt is one of the blocking yes/no confirmations.
    function isYesNoTopPromptActive() {
      return topPromptActive && topPromptKind === UI_PROMPT_KIND_YES_NO;
    }

    // Returns whether the top prompt represents an active target-preview state.
    function isTargetPreviewTopPromptActive() {
      return topPromptActive && topPromptKind === UI_PROMPT_KIND_TARGET;
    }

    // Returns whether any shared semantic birth screen is active.
    function isBirthScreenActive() {
      return !!activeBirthState && Number(activeBirthState.active) === 1;
    }

    // Returns the active semantic birth-screen kind, if any.
    function getBirthScreenKind() {
      if (!isBirthScreenActive()) return "";
      return String(activeBirthState.kind || "stats");
    }

    // Returns whether the shared semantic birth stat-allocation screen is active.
    function isBirthStatsScreenActive() {
      return getBirthScreenKind() === "stats";
    }

    // Returns whether the shared semantic birth age/height/weight editor is active.
    function isBirthAhwScreenActive() {
      return getBirthScreenKind() === "ahw";
    }

    // Returns whether one shared birth text-editing screen is active.
    function isBirthTextScreenActive() {
      return isBirthTextScreenKind(getBirthScreenKind());
    }

    // Returns whether the shared semantic character-sheet screen is active.
    function isCharacterSheetActive() {
      return !!activeCharacterSheetState && Number(activeCharacterSheetState.active) === 1;
    }

    // Returns whether the shared semantic character sheet is in skill-editor mode.
    function isCharacterSkillEditorActive() {
      return !!activeCharacterSheetState &&
        Number(activeCharacterSheetState.active) === 1 &&
        Number(activeCharacterSheetState.skillEditorActive ?? 0) === 1;
    }

    // Only interactive prompts should keep the last semantic menu visible as context.
    function shouldKeepSemanticMenuSnapshot() {
      if (!semanticMenuSnapshot || activeMenuItems.length) {
        return false;
      }

      if (
        api &&
        typeof api.getMenuSnapshotRetained === "function" &&
        !api.getMenuSnapshotRetained()
      ) {
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
          activeMenuSummaryText,
          activeMenuSummaryAttrs,
          activeMenuDetailsWidth,
          activeMenuSummaryRows,
          activeMenuDetailsVisual
        );
        return {
          items: activeMenuItems,
          activeColumnX: activeMenuColumnX,
          text: activeMenuText,
          textAttrs: activeMenuTextAttrs,
          detailsText: activeMenuDetailsText,
          detailsAttrs: activeMenuDetailsAttrs,
          summaryText: activeMenuSummaryText,
          summaryAttrs: activeMenuSummaryAttrs,
          detailsWidth: activeMenuDetailsWidth,
          summaryRows: activeMenuSummaryRows,
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
        activeMenuSummaryText = "";
        activeMenuSummaryAttrs = null;
        activeMenuDetailsWidth = null;
        activeMenuSummaryRows = null;
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
      const summaryPtr =
        typeof api.getMenuSummaryPtr === "function" ? api.getMenuSummaryPtr() : 0;
      const summaryLen =
        typeof api.getMenuSummaryLen === "function" ? api.getMenuSummaryLen() : 0;
      const summaryAttrPtr =
        typeof api.getMenuSummaryAttrsPtr === "function" ? api.getMenuSummaryAttrsPtr() : 0;
      const summaryAttrLen =
        typeof api.getMenuSummaryAttrsLen === "function" ? api.getMenuSummaryAttrsLen() : 0;
      const detailsWidth =
        typeof api.getMenuDetailsWidth === "function" ? api.getMenuDetailsWidth() : 0;
      const summaryRows =
        typeof api.getMenuSummaryRows === "function" ? api.getMenuSummaryRows() : 0;
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
      activeMenuSummaryText = readUtf8(heap, summaryPtr, summaryLen);
      activeMenuSummaryAttrs = readBytes(heap, summaryAttrPtr, summaryAttrLen);
      activeMenuDetailsWidth =
        Number.isInteger(detailsWidth) && detailsWidth > 0 ? detailsWidth : null;
      activeMenuSummaryRows =
        Number.isInteger(summaryRows) && summaryRows > 0 ? summaryRows : null;
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
        activeMenuSummaryText = "";
        activeMenuSummaryAttrs = null;
        activeMenuDetailsWidth = null;
        activeMenuSummaryRows = null;
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

    // Reads and parses the active semantic birth-screen payload exported by wasm.
    function readBirthState(heap) {
      if (
        !api ||
        typeof api.getBirthStatePtr !== "function" ||
        typeof api.getBirthStateLen !== "function"
      ) {
        activeBirthState = null;
        return null;
      }

      const ptr = api.getBirthStatePtr();
      const len = api.getBirthStateLen();
      const payload = readUtf8(heap, ptr, len);
      if (!payload) {
        activeBirthState = null;
        return null;
      }

      try {
        const state = JSON.parse(payload);
        activeBirthState =
          state && Number(state.active) === 1 ? state : null;
        if (!isBirthTextScreenKind(String(activeBirthState?.kind || ""))) {
          resetBirthTextDraft();
        }
        if (!activeBirthState || String(activeBirthState.kind || "") !== "ahw") {
          resetBirthAhwDraft();
        }
        return activeBirthState;
      } catch (err) {
        console.warn("Invalid birth-state payload from wasm:", err);
        activeBirthState = null;
        resetBirthTextDraft();
        resetBirthAhwDraft();
        return null;
      }
    }

    // Returns whether one birth-screen kind uses the shared text editor.
    function isBirthTextScreenKind(kind) {
      return kind === "name" || kind === "history";
    }

    // Describes one shared birth text-editing screen for renderer and input reuse.
    function getBirthTextScreenConfig(kind = getBirthScreenKind()) {
      switch (String(kind || "")) {
        case "name":
          return {
            kind: "name",
            overlayClass: "overlay-birth-name",
            inputSelector: '[data-birth-text-input="name"]',
            sectionSelector: ".character-sheet-meta-card",
            rerollKeyCode: 9,
            acceptsSubmitShortcut: (ev) => ev.key === "Enter",
            acceptFabTitle: "Accept name (Enter)",
            acceptFabAria: "Accept name",
          };
        case "history":
          return {
            kind: "history",
            overlayClass: "overlay-birth-history",
            inputSelector: '[data-birth-text-input="history"]',
            sectionSelector: ".character-sheet-history-card",
            rerollKeyCode: " ".charCodeAt(0),
            acceptsSubmitShortcut: (ev) =>
              ev.key === "Enter" && (ev.ctrlKey || ev.metaKey),
            acceptFabTitle: "Accept history (Enter)",
            acceptFabAria: "Accept history",
          };
        default:
          return null;
      }
    }

    // Clears the local shared text editor draft state.
    function resetBirthTextDraft() {
      birthTextDraft = "";
      birthTextSourceText = "";
      birthTextSourceKind = "";
      birthTextDirty = false;
    }

    // Returns a fresh empty birth age/height/weight draft object.
    function createEmptyBirthAhwDraft() {
      return { age: "", height: "", weight: "" };
    }

    // Clears the local age/height/weight editor draft state.
    function resetBirthAhwDraft() {
      birthAhwDraft = createEmptyBirthAhwDraft();
      birthAhwSource = createEmptyBirthAhwDraft();
      birthAhwDirty = false;
    }

    // Submits one accepted birth form through the shared wasm adapter interface.
    function submitBirthForm(kind, payload = {}) {
      const kindCode =
        { stats: 1, history: 2, ahw: 3, name: 4 }[String(kind || "")] || 0;
      if (!api || typeof api.submitBirthForm !== "function" || kindCode <= 0) {
        return false;
      }

      if (typeof payload.text !== "undefined" && payload.text !== null) {
        const maxLength = Number(activeBirthState?.maxLength ?? 0);
        const heap = getHeapU8();
        const ptr =
          typeof api.getBirthSubmitTextPtr === "function"
            ? Number(api.getBirthSubmitTextPtr() || 0)
            : 0;
        if (!heap || ptr <= 0 || maxLength <= 0) return false;

        const encoded = utf8Encoder.encode(String(payload.text ?? ""));
        const writeLen = Math.min(encoded.length, maxLength);
        heap.set(encoded.subarray(0, writeLen), ptr);
        heap[ptr + writeLen] = 0;
      }

      return (
        api.submitBirthForm(
          kindCode,
          parseBirthNumberInput(payload.age),
          parseBirthNumberInput(payload.height),
          parseBirthNumberInput(payload.weight)
        ) !== 0
      );
    }

    // Keeps the local shared birth-text draft aligned with backend state.
    function syncBirthTextDraft(state, force = false) {
      const nextKind = String(state?.kind || "");
      const nextText = String(state?.text || "");

      if (
        force ||
        nextKind !== birthTextSourceKind ||
        !birthTextDirty ||
        nextText !== birthTextSourceText
      ) {
        birthTextDraft = nextText;
        birthTextSourceText = nextText;
        birthTextSourceKind = nextKind;
        birthTextDirty = false;
      }
    }

    // Commits one shared birth-text draft value to local state only.
    function commitBirthTextDraft(text = birthTextDraft) {
      birthTextDraft = String(text ?? "");
      birthTextDirty =
        (getBirthScreenKind() !== birthTextSourceKind) ||
        (birthTextDraft !== birthTextSourceText);
      return birthTextDraft;
    }

    // Reads the current shared birth-text draft directly from one overlay field.
    function readBirthTextDraftFromDom(selector) {
      return overlayModalEl.querySelector(selector)?.value ?? birthTextDraft;
    }

    // Commits the current shared birth-text draft from one overlay field.
    function commitBirthTextDraftFromDom(selector) {
      return commitBirthTextDraft(readBirthTextDraftFromDom(selector));
    }

    // Mirrors the shared birth-text draft into the active overlay field.
    function syncBirthTextEditorValue(kind = getBirthScreenKind()) {
      const config = getBirthTextScreenConfig(kind);
      if (!config) return;

      const inputEl = overlayModalEl.querySelector(config.inputSelector);
      if (inputEl && inputEl.value !== birthTextDraft) {
        inputEl.value = birthTextDraft;
      }
    }

    // Keeps the active shared birth-text editor card visible in the overlay.
    function syncBirthTextEditorIntoView(kind = getBirthScreenKind()) {
      const config = getBirthTextScreenConfig(kind);
      if (!config) return;

      syncOverlayEditorSectionIntoView(
        config.overlayClass,
        config.inputSelector,
        config.sectionSelector
      );
    }

    // Commits and submits the active birth form on accept.
    function submitActiveBirthDraft(
      kind = getBirthScreenKind(),
      textOverride = null
    ) {
      if (isBirthTextScreenKind(kind)) {
        const config = getBirthTextScreenConfig(kind);
        const text =
          textOverride === null
            ? (config ? commitBirthTextDraftFromDom(config.inputSelector) : "")
            : commitBirthTextDraft(textOverride);
        return !!config && submitBirthForm(kind, { text });
      }

      if (kind === "ahw") {
        return submitBirthForm(kind, commitBirthAhwDraftFromDom());
      }

      return false;
    }

    // Dispatches one shared birth-text editor action through the engine input queue.
    function dispatchBirthTextScreenAction(
      action,
      kind = getBirthScreenKind(),
      textOverride = null
    ) {
      const config = getBirthTextScreenConfig(kind);
      if (!config) return false;

      if (action === "accept") {
        let nextText = textOverride;

        if (textOverride === null) {
          nextText = commitBirthTextDraftFromDom(config.inputSelector);
        } else {
          nextText = commitBirthTextDraft(textOverride);
        }
        if (!submitActiveBirthDraft(kind, nextText)) return false;
        pushAscii(13);
        forceRedraw = true;
        return true;
      }

      if (action === "reroll") {
        birthTextDirty = false;
        pushAscii(config.rerollKeyCode);
        forceRedraw = true;
        return true;
      }

      if (action === "cancel") {
        pushAscii(27);
        forceRedraw = true;
        return true;
      }

      return false;
    }

    // Normalizes one numeric birth-input field for the wasm bridge.
    function parseBirthNumberInput(value) {
      const text = String(value ?? "").trim();
      if (!text) return 0;

      const number = Number.parseInt(text, 10);
      return Number.isFinite(number) ? number : 0;
    }

    // Commits one age/height/weight draft value to local state only.
    function commitBirthAhwDraft(draft = birthAhwDraft) {
      birthAhwDraft = {
        age: String(draft?.age ?? ""),
        height: String(draft?.height ?? ""),
        weight: String(draft?.weight ?? ""),
      };
      birthAhwDirty =
        birthAhwDraft.age !== birthAhwSource.age ||
        birthAhwDraft.height !== birthAhwSource.height ||
        birthAhwDraft.weight !== birthAhwSource.weight;
      return birthAhwDraft;
    }

    // Reads the current age/height/weight draft directly from the overlay DOM.
    function readBirthAhwDraftFromDom() {
      return {
        age: overlayModalEl.querySelector('[data-birth-ahw-field="age"]')?.value ?? birthAhwDraft.age,
        height: overlayModalEl.querySelector('[data-birth-ahw-field="height"]')?.value ?? birthAhwDraft.height,
        weight: overlayModalEl.querySelector('[data-birth-ahw-field="weight"]')?.value ?? birthAhwDraft.weight,
      };
    }

    // Commits the current age/height/weight draft from the overlay DOM into local state.
    function commitBirthAhwDraftFromDom() {
      return commitBirthAhwDraft(readBirthAhwDraftFromDom());
    }

    // Keeps the local age/height/weight draft aligned with backend state when it changes.
    function syncBirthAhwDraft(state, force = false) {
      const nextDraft = {
        age: String(state?.age ?? ""),
        height: String(state?.height ?? ""),
        weight: String(state?.weight ?? ""),
      };

      if (
        force ||
        !birthAhwDirty ||
        nextDraft.age !== birthAhwSource.age ||
        nextDraft.height !== birthAhwSource.height ||
        nextDraft.weight !== birthAhwSource.weight
      ) {
        birthAhwDraft = nextDraft;
        birthAhwSource = { ...nextDraft };
        birthAhwDirty = false;
      }
    }

    // Reads and parses the semantic character-sheet payload exported by wasm.
    function readCharacterSheetState(heap) {
      if (
        !api ||
        typeof api.getCharacterSheetStatePtr !== "function" ||
        typeof api.getCharacterSheetStateLen !== "function"
      ) {
        activeCharacterSheetState = null;
        return null;
      }

      const ptr = api.getCharacterSheetStatePtr();
      const len = api.getCharacterSheetStateLen();
      const payload = readUtf8(heap, ptr, len);
      if (!payload) {
        activeCharacterSheetState = null;
        return null;
      }

      try {
        const state = JSON.parse(payload);
        activeCharacterSheetState =
          state && Number(state.active) === 1 ? state : null;
        return activeCharacterSheetState;
      } catch (err) {
        console.warn("Invalid character-sheet payload from wasm:", err);
        activeCharacterSheetState = null;
        return null;
      }
    }

    // Reads and parses the active tile-context popup payload exported by wasm.
    function readTileContextState(heap, dir) {
      if (
        !api ||
        typeof api.getTileContextStatePtr !== "function" ||
        typeof api.getTileContextStateLen !== "function"
      ) {
        return null;
      }

      const ptr = api.getTileContextStatePtr(dir);
      const len = api.getTileContextStateLen(dir);
      const payload = readUtf8(heap, ptr, len);
      if (!payload) return null;

      try {
        const state = JSON.parse(payload);
        if (!state || Number(state.valid) !== 1) {
          return null;
        }

        return {
          valid: true,
          dir: Number(state.dir ?? dir),
          description: String(state.description || ""),
          actions: Array.isArray(state.actions)
            ? state.actions
              .map((action) => ({
                id: Number(action?.id ?? 0),
                label: String(action?.label || ""),
              }))
              .filter((action) => Number.isInteger(action.id) && action.id > 0)
            : [],
        };
      } catch (err) {
        console.warn("Invalid tile-context payload from wasm:", err);
        return null;
      }
    }

    // Formats one action key label for the custom character-sheet footer.
    function formatCharacterActionKey(key) {
      const value = Number(key);

      if (value === 27) return "Esc";
      if (value === 9) return "Tab";
      if (value === 13) return "Enter";
      if (value === 32) return "Space";
      if (!Number.isInteger(value) || value <= 0 || value > 255) return "";

      return String.fromCharCode(value).toUpperCase();
    }

    // Builds HTML rows for visible label/value character fields.
    function renderCharacterFields(fields, valueClass = "character-sheet-field-value") {
      return (Array.isArray(fields) ? fields : [])
        .filter((field) => Number(field?.visible ?? 0) === 1)
        .map(
          (field) =>
            `<div class="character-sheet-field">` +
              `<span class="character-sheet-field-label">${escapeHtml(String(field.label || ""))}</span>` +
              `<strong class="${valueClass}">${escapeHtml(String(field.value || ""))}</strong>` +
            `</div>`
        )
        .join("");
    }

    // Builds the standard stat rows used by the semantic character-sheet view.
    function renderCharacterStaticStats(stats) {
      return (Array.isArray(stats) ? stats : [])
        .map((stat) => {
          const showBase = Number(stat?.showBase ?? 0) === 1;
          const reduced = Number(stat?.reduced ?? 0) === 1;
          const modHtml = [];

          if (showBase) {
            modHtml.push(
              `<span class="character-sheet-stat-mod character-sheet-stat-base">= ${escapeHtml(String(stat.base || ""))}</span>`
            );
          }
          if (Number(stat?.equipMod ?? 0) !== 0) {
            modHtml.push(
              `<span class="character-sheet-stat-mod">Eq ${escapeHtml(String(stat.equipMod > 0 ? `+${stat.equipMod}` : stat.equipMod))}</span>`
            );
          }
          if (Number(stat?.drainMod ?? 0) !== 0) {
            modHtml.push(
              `<span class="character-sheet-stat-mod">Drain ${escapeHtml(String(stat.drainMod > 0 ? `+${stat.drainMod}` : stat.drainMod))}</span>`
            );
          }
          if (Number(stat?.miscMod ?? 0) !== 0) {
            modHtml.push(
              `<span class="character-sheet-stat-mod">Misc ${escapeHtml(String(stat.miscMod > 0 ? `+${stat.miscMod}` : stat.miscMod))}</span>`
            );
          }

          return (
            `<div class="character-sheet-stat-row${reduced ? " character-sheet-stat-row-reduced" : ""}">` +
              `<div class="character-sheet-stat-main">` +
                `<span class="character-sheet-stat-label">${escapeHtml(String(stat.label || ""))}</span>` +
                `<strong class="character-sheet-stat-current">${escapeHtml(String(stat.current || ""))}</strong>` +
              `</div>` +
              `<div class="character-sheet-stat-mods">${modHtml.join("")}</div>` +
            `</div>`
          );
        })
        .join("");
    }

    // Builds the editable stat rows used during birth stat allocation.
    function renderCharacterBirthStats(stats) {
      return (Array.isArray(stats) ? stats : [])
        .map((stat) => {
          const index = Number(stat.index ?? -1);
          const selected = Number(stat.selected ?? 0) === 1;
          const canDecrease = Number(stat.canDecrease ?? 0) === 1;
          const canIncrease = Number(stat.canIncrease ?? 0) === 1;

          return (
            `<div class="character-sheet-stat-row character-sheet-stat-row-editable${selected ? " character-sheet-stat-row-selected" : ""}" data-birth-stat-index="${index}">` +
              `<button type="button" class="character-sheet-stat-select" data-birth-stat-index="${index}" aria-pressed="${selected ? "true" : "false"}">` +
                `<div class="character-sheet-stat-main">` +
                  `<span class="character-sheet-stat-label">${escapeHtml(String(stat.name || ""))}</span>` +
                  `<strong class="character-sheet-stat-current">${escapeHtml(String(stat.value || ""))}</strong>` +
                `</div>` +
              `</button>` +
              `<div class="character-sheet-stat-controls">` +
                `<button type="button" class="character-sheet-stat-adjust" data-birth-stat-index="${index}" data-birth-adjust="-1"${canDecrease ? "" : " disabled"} aria-label="Decrease ${escapeHtml(String(stat.name || "stat"))}">-</button>` +
                `<button type="button" class="character-sheet-stat-adjust" data-birth-stat-index="${index}" data-birth-adjust="1"${canIncrease ? "" : " disabled"} aria-label="Increase ${escapeHtml(String(stat.name || "stat"))}">+</button>` +
                `<span class="character-sheet-stat-cost">Cost ${escapeHtml(String(stat.cost ?? 0))}</span>` +
              `</div>` +
            `</div>`
          );
        })
        .join("");
    }

    // Formats one signed modifier the same way the terminal character sheet does.
    function formatCharacterSkillModifier(value) {
      const number = Number(value ?? 0);

      if (!Number.isFinite(number) || number === 0) return "";
      return number > 0 ? `+${number}` : String(number);
    }

    // Builds the skill equation HTML and hover breakdown for one skill row.
    function buildCharacterSkillEquation(skill) {
      const base = Number(skill?.base ?? 0);
      const value = Number(skill?.value ?? 0);
      const statMod = Number(skill?.statMod ?? 0);
      const equipMod = Number(skill?.equipMod ?? 0);
      const miscMod = Number(skill?.miscMod ?? 0);
      const mods = [
        { label: "Stat", value: statMod },
        { label: "Eq", value: equipMod },
        { label: "Misc", value: miscMod },
      ];
      const titleTerms = [`Base ${base}`];

      for (const mod of mods) {
        if (mod.value === 0) continue;
        titleTerms.push(`${mod.label} ${mod.value > 0 ? "+" : ""}${mod.value}`);
      }

      return {
        html:
          `<span class="character-sheet-skill-cell character-sheet-skill-value term-c13">${escapeHtml(String(value))}</span>` +
          `<span class="character-sheet-skill-cell character-sheet-skill-equals term-c2">=</span>` +
          `<span class="character-sheet-skill-cell character-sheet-skill-base term-c5">${escapeHtml(String(base))}</span>` +
          `<span class="character-sheet-skill-cell character-sheet-skill-mod term-c2">${escapeHtml(formatCharacterSkillModifier(statMod))}</span>` +
          `<span class="character-sheet-skill-cell character-sheet-skill-mod term-c2">${escapeHtml(formatCharacterSkillModifier(equipMod))}</span>` +
          `<span class="character-sheet-skill-cell character-sheet-skill-mod term-c2">${escapeHtml(formatCharacterSkillModifier(miscMod))}</span>`,
        title: titleTerms.join(" | "),
      };
    }

    // Builds the skill rows used by the semantic character-sheet view.
    function renderCharacterSkills(skills) {
      return (Array.isArray(skills) ? skills : [])
        .map((skill) => {
          const equation = buildCharacterSkillEquation(skill);

          return (
            `<div class="character-sheet-skill-row" title="${escapeHtml(equation.title)}">` +
              `<div class="character-sheet-skill-main">` +
                `<span class="character-sheet-skill-label">${escapeHtml(String(skill.label || ""))}</span>` +
                `${equation.html}` +
              `</div>` +
            `</div>`
          );
        })
        .join("");
    }

    // Builds the editable skill rows used during semantic skill allocation.
    function renderCharacterEditableSkills(skills) {
      return (Array.isArray(skills) ? skills : [])
        .map((skill, index) => {
          const selected = Number(skill?.selected ?? 0) === 1;
          const canDecrease = Number(skill?.canDecrease ?? 0) === 1;
          const canIncrease = Number(skill?.canIncrease ?? 0) === 1;
          const equation = buildCharacterSkillEquation(skill);

          return (
            `<div class="character-sheet-skill-row character-sheet-skill-row-editable${selected ? " character-sheet-skill-row-selected" : ""}" data-skill-editor-index="${index}" title="${escapeHtml(equation.title)}">` +
              `<button type="button" class="character-sheet-skill-select" data-skill-editor-index="${index}" aria-pressed="${selected ? "true" : "false"}">` +
                `<div class="character-sheet-skill-main">` +
                  `<span class="character-sheet-skill-label">${escapeHtml(String(skill.label || ""))}</span>` +
                  `${equation.html}` +
                `</div>` +
              `</button>` +
              `<div class="character-sheet-skill-controls">` +
                `<button type="button" class="character-sheet-stat-adjust" data-skill-editor-index="${index}" data-skill-editor-adjust="-1"${canDecrease ? "" : " disabled"} aria-label="Decrease ${escapeHtml(String(skill.label || "skill"))}">-</button>` +
                `<button type="button" class="character-sheet-stat-adjust" data-skill-editor-index="${index}" data-skill-editor-adjust="1"${canIncrease ? "" : " disabled"} aria-label="Increase ${escapeHtml(String(skill.label || "skill"))}">+</button>` +
                `<span class="character-sheet-skill-cost">Cost ${escapeHtml(String(skill.cost ?? 0))}</span>` +
              `</div>` +
            `</div>`
          );
        })
        .join("");
    }

    // Builds the footer action buttons used by the semantic character-sheet view.
    function renderCharacterActions(actions) {
      return (Array.isArray(actions) ? actions : [])
        .map((action) => {
          const key = Number(action?.key ?? 0);
          const keyLabel = formatCharacterActionKey(key);

          return (
            `<button type="button" class="character-sheet-action" data-character-action-key="${key}">` +
              `<span class="character-sheet-action-key">${escapeHtml(keyLabel)}</span>` +
              `<span class="character-sheet-action-label">${escapeHtml(String(action?.label || ""))}</span>` +
            `</button>`
          );
        })
        .join("");
    }

    // Normalizes one song/theme name from the semantic player-state blob.
    function normalizeSongStateName(raw) {
      const text = String(raw || "").trim();
      return text && text !== "(none)" ? text : "";
    }

    // Trims the common song prefix so the compact FAB caption can stay short.
    function getCompactSongName(raw) {
      const text = normalizeSongStateName(raw);
      return text.replace(/^(?:Song|Theme) of /i, "").replace(/^the /i, "").trim();
    }

    // Builds the active/inactive presentation state for the floating song button.
    function getSongFabState(state = null) {
      const songName = normalizeSongStateName(state?.song);
      const themeName = normalizeSongStateName(state?.theme);
      const activeName = songName || themeName;
      const compactName = getCompactSongName(activeName);
      let title = "Songs";
      let ariaLabel = "Songs";

      if (songName && themeName) {
        const compactSong = getCompactSongName(songName);
        const compactTheme = getCompactSongName(themeName);
        title = `Stop singing ${compactSong} + ${compactTheme}`;
        ariaLabel = `Stop singing ${compactSong} and ${compactTheme}`;
      } else if (compactName) {
        title = `Stop singing ${compactName}`;
        ariaLabel = `Stop singing ${compactName}`;
      }

      return {
        active: !!activeName,
        label: compactName,
        title,
        ariaLabel,
      };
    }

    // Builds the active/inactive presentation state for the floating stealth/state button.
    function getStateFabState(state = null) {
      const rawState = String(state?.state || "").trim();
      const label = rawState && rawState !== "Normal" ? rawState : "";
      const active = Number(state?.stealthActive ?? 0) === 1 || rawState === "Stealth";
      let title = active ? "Leave stealth mode (S)" : "Enter stealth mode (S)";
      let ariaLabel = active ? "Leave stealth mode" : "Enter stealth mode";

      if (!active && label) {
        title = `${title} - current state: ${label}`;
        ariaLabel = `${ariaLabel}, current state ${label}`;
      }

      return {
        active,
        label,
        title,
        ariaLabel,
      };
    }

    // Builds the current presentation state for the floating ranged-weapon button.
    function getRangedFabState(state = null) {
      const visible = !!state && Number(state.rangedActionVisible ?? 0) === 1;
      const label = String(state?.rangedActionLabel || "").trim();
      const ready = !!state && Number(state.rangedActionReady ?? 0) === 1;
      const quiver = Number(state?.rangedActionQuiver ?? 0);
      const quiverLabel = quiver === 2 ? "2nd quiver" : "1st quiver";
      const visual = visible
        ? normalizeMenuItemVisual(
            Number(state?.rangedActionVisualKind ?? 0),
            Number(state?.rangedActionVisualAttr ?? 0),
            Number(state?.rangedActionVisualChar ?? 0)
          )
        : null;
      let title = "Aim and fire";
      let ariaLabel = "Aim and fire";

      if (label) {
        title = `Aim and fire with ${label}`;
        ariaLabel = `Aim and fire with ${label}`;
      }

      if (ready) {
        title = `${title} (${quiverLabel})`;
        ariaLabel = `${ariaLabel} using the ${quiverLabel}`;
      } else if (visible) {
        title = label
          ? `${label} ready, but no arrows are loaded`
          : "No arrows are loaded";
        ariaLabel = title;
      }

      return {
        visible,
        ready,
        title,
        ariaLabel,
        visual,
      };
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

    // Reports whether one attr/char pair encodes a tileset pict rather than plain text.
    function isTileAttrChar(attr, chr) {
      return ((attr & 0x80) !== 0) && ((chr & 0x80) !== 0);
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

    // Mirrors the current shell state into sidebar, compact HUD, and accessibility attributes.
    function syncResponsiveShellChrome() {
      const visible = mapDisplayReady;
      const compact = isCompactLayout();
      const open = visible && compact && !!compactSideOpen;
      const collapsed = visible && !compact && !!sideCollapsed;

      appEl.classList.toggle("shell-visible", visible);
      appEl.classList.toggle("side-open", open);
      appEl.classList.toggle("side-collapsed", collapsed);
      sideBackdropEl.hidden = !open;
      sideWrapEl.setAttribute(
        "aria-hidden",
        (!visible || (compact ? !open : collapsed)) ? "true" : "false"
      );

      sideToggleEl.setAttribute("aria-expanded", compact ? (open ? "true" : "false") : (collapsed ? "false" : "true"));
      sideToggleEl.textContent = "☰";
      sideToggleEl.setAttribute("aria-label", compact && open ? "Hide sidebar" : "Show sidebar");
      sideToggleEl.title = compact && open ? "Hide sidebar" : "Show sidebar";

      sideCollapseButtonEl.setAttribute("aria-expanded", compact ? (open ? "true" : "false") : (collapsed ? "false" : "true"));
      sideCollapseButtonEl.textContent = "<";
      sideCollapseButtonEl.setAttribute("aria-label", compact ? "Hide sidebar" : "Hide sidebar");
      sideCollapseButtonEl.title = compact ? "Hide sidebar" : "Hide sidebar";
    }

    // Applies the current compact side-panel state to the DOM and accessibility labels.
    function setCompactSideOpen(nextOpen) {
      const compact = isCompactLayout();
      compactSideOpen = compact && !!nextOpen;
      syncResponsiveShellChrome();
    }

    // Toggles the persistent desktop sidebar collapse mode.
    function setSideCollapsed(nextCollapsed) {
      sideCollapsed = !!nextCollapsed;
      syncResponsiveShellChrome();
    }

    // Recomputes responsive zoom presets and mobile chrome state after viewport changes.
    function syncResponsiveShell(forceZoomReset = false) {
      const compact = isCompactLayout();
      const layoutChanged = compact !== wasCompactLayout;
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
      if (layoutChanged && compact) compactSideOpen = false;
      wasCompactLayout = compact;
      syncResponsiveShellChrome();
      syncSidebarToolbar(activePlayerState, getHeapU8());
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
            ctx.save();
            ctx.globalAlpha = TARGET_MARK_ALPHA;
            if (tileImageReady && isTileAttrChar(cell.textAttr, cell.textChar)) {
              drawTileByAttrChar(cell.textAttr, cell.textChar, x, y);
            } else if (isTileAttrChar(cell.textAttr, cell.textChar)) {
              drawAsciiAttrChar(1, 42, x, y);
            } else {
              drawAsciiAttrChar(cell.textAttr, cell.textChar, x, y);
            }
            ctx.restore();
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

    const semanticOverlayClasses = [
      "overlay-birth-name",
      "overlay-birth-ahw",
      "overlay-birth-history",
      "overlay-birth-stats",
      "overlay-character-skills",
      "overlay-character-sheet",
      "overlay-tile-context",
      "overlay-menu",
      "overlay-dismissable",
    ];

    // Clears any semantic overlay mode classes from the shared overlay element.
    function resetSemanticOverlayModes() {
      for (const className of semanticOverlayClasses) {
        overlayModalEl.classList.remove(className);
      }
    }

    // Shows one semantic overlay mode with consistent class and layout reset behavior.
    function showSemanticOverlay(modeClass, html, { dismissable = false } = {}) {
      overlayModalEl.style.display = "block";
      resetSemanticOverlayModes();
      if (modeClass) {
        overlayModalEl.classList.add(modeClass);
      }
      if (dismissable) {
        overlayModalEl.classList.add("overlay-dismissable");
      }
      overlayModalEl.style.removeProperty("--menu-details-width");
      overlayModalEl.style.removeProperty("--menu-summary-min-height");
      setHtmlIfChanged(overlayModalEl, html);
    }

    // Hides the shared semantic overlay and clears any mode-specific classes.
    function hideSemanticOverlay() {
      overlayModalEl.style.display = "none";
      resetSemanticOverlayModes();
      overlayModalEl.style.removeProperty("--menu-details-width");
      overlayModalEl.style.removeProperty("--menu-summary-min-height");
      setHtmlIfChanged(overlayModalEl, "");
    }

    // Returns whether the shared overlay is currently visible.
    function isSemanticOverlayVisible() {
      return overlayModalEl.style.display !== "none";
    }

    // Returns whether the active overlay is a dismissable semantic modal dialog.
    function isDismissableModalOverlayActive() {
      return (
        isSemanticOverlayVisible() &&
        overlayModalEl.classList.contains("overlay-dismissable")
      );
    }

    // Returns whether the custom tile-context action popup is currently visible.
    function isTileContextOverlayActive() {
      return (
        !!activeTileContextState &&
        isSemanticOverlayVisible() &&
        overlayModalEl.classList.contains("overlay-tile-context")
      );
    }

    // Dismisses the custom tile-context popup without sending input to the backend.
    function hideTileContextOverlay({ redraw = true } = {}) {
      if (!activeTileContextState && !overlayModalEl.classList.contains("overlay-tile-context")) {
        return false;
      }

      activeTileContextState = null;
      if (overlayModalEl.classList.contains("overlay-tile-context")) {
        hideSemanticOverlay();
      }
      if (redraw) {
        forceRedraw = true;
      }
      return true;
    }

    // Returns whether the active overlay is plain captured text without its own UI mode.
    function isGenericCapturedOverlayActive() {
      if (
        !api ||
        typeof api.getOverlayMode !== "function" ||
        !isSemanticOverlayVisible()
      ) {
        return false;
      }

      if (Number(api.getOverlayMode()) <= 0) {
        return false;
      }

      return !semanticOverlayClasses.some((className) =>
        overlayModalEl.classList.contains(className)
      );
    }

    // Tries to dismiss the current overlay via its native activation path.
    function tryDismissActiveOverlay() {
      if (isTileContextOverlayActive()) {
        return hideTileContextOverlay();
      }

      if (isDismissableModalOverlayActive()) {
        if (!api || typeof api.modalActivate !== "function") return false;

        if (!api.modalActivate()) {
          return false;
        }

        hideSemanticOverlay();
        forceRedraw = true;
        return true;
      }

      if (isGenericCapturedOverlayActive()) {
        if (!pushAscii(27)) {
          return false;
        }

        forceRedraw = true;
        return true;
      }

      return false;
    }

    // Returns one human-readable heading for a current-tile or adjacent-tile popup.
    function getTileContextHeading(dir) {
      if (dir === 5) return "Current Tile";
      return `${ADJACENT_DIRECTION_NAMES[dir] || "Adjacent"} Tile`;
    }

    // Reuses the existing action-button visuals for one tile-context popup target.
    function getTileContextVisual(state, dir) {
      if (!state) return null;

      if (dir === 5 && Number(state.tileActionVisible ?? 0) === 1) {
        return normalizeMenuItemVisual(
          Number(state.tileActionVisualKind ?? 0),
          Number(state.tileActionVisualAttr ?? 0),
          Number(state.tileActionVisualChar ?? 0)
        );
      }

      const adjacentState = getAdjacentActionState(state, dir);
      return adjacentState.visible ? adjacentState.visual : null;
    }

    // Opens one semantic tile-context popup using the current gameplay state and wasm payload.
    function openTileContextOverlay(dir) {
      const heap = getHeapU8();
      if (!heap) return false;

      const playerState = readPlayerState(heap);
      if (!playerState || Number(playerState.ready) !== 1) {
        return false;
      }

      const tileContextState = readTileContextState(heap, dir);
      if (!tileContextState || !tileContextState.actions.length) {
        return false;
      }

      activeTileContextState = {
        ...tileContextState,
        visual: getTileContextVisual(playerState, Number(tileContextState.dir ?? dir)),
      };
      setCompactSideOpen(false);
      forceRedraw = true;
      return true;
    }

    // Renders the frontend-owned popup for contextual tile inspection and actions.
    function updateTileContextSemantic() {
      const state = activeTileContextState;
      if (!state || !Array.isArray(state.actions) || !state.actions.length) {
        if (overlayModalEl.classList.contains("overlay-tile-context")) {
          hideSemanticOverlay();
        }
        activeTileContextState = null;
        return false;
      }

      const dir = Number(state.dir ?? 5);
      const description = String(state.description || "You see nothing of interest.");
      const actionsHtml = state.actions
        .map((action, index) => (
          `<button type="button" class="tile-context-action${index === 0 ? " tile-context-action-primary" : ""}" ` +
            `data-tile-context-action="${action.id}" data-tile-context-dir="${dir}">` +
            `<span class="tile-context-action-label">${escapeHtml(action.label || "Act")}</span>` +
            `${index === 0 ? '<span class="tile-context-action-tag">Default</span>' : ""}` +
          "</button>"
        ))
        .join("");

      const html =
        `<div class="tile-context-shell">` +
          `<div class="tile-context-header">` +
            `<div class="tile-context-visual-frame">` +
              `<div class="tile-context-visual" data-tile-context-visual></div>` +
            `</div>` +
            `<div class="tile-context-copy">` +
              `<h2>${escapeHtml(getTileContextHeading(dir))}</h2>` +
              `<p>Inspect the tile and choose an action.</p>` +
            `</div>` +
          `</div>` +
          `<div class="tile-context-description">${escapeHtml(description)}</div>` +
          `<div class="tile-context-actions">${actionsHtml}</div>` +
        `</div>`;

      showSemanticOverlay("overlay-tile-context", html);
      overlayModalEl.scrollTop = 0;
      renderActionFabVisual(
        overlayModalEl.querySelector("[data-tile-context-visual]"),
        state.visual
      );
      return true;
    }

    // Builds the reusable identity/physical/progress/combat context shared by sheet-style overlays.
    function buildCharacterSheetContext(sheet) {
      return {
        identityHtml: renderCharacterFields(
          sheet.identity,
          "character-sheet-identity-value"
        ),
        physicalHtml: renderCharacterFields(sheet.physical),
        progressHtml: renderCharacterFields(
          sheet.progress,
          "character-sheet-progress-value"
        ),
        combatHtml: renderCharacterFields(
          sheet.combat,
          "character-sheet-combat-value"
        ),
      };
    }

    // Renders the shared character-sheet shell used by birth and character overlays.
    function renderCharacterSheetShell({
      shellClass = "",
      title = "",
      note = "",
      bannerHtml = "",
      layoutClass = "",
      context,
      identitySectionTitle = "Identity",
      identitySectionHeadHtml = "",
      identitySectionBodyHtml = "",
      physicalSectionTitle = "Physical",
      physicalSectionHeadHtml = "",
      physicalSectionBodyHtml = "",
      statsHtml = "",
      skillsHtml = "",
      detailSectionHtml = "",
      footerHtml = "",
    }) {
      const shellClassAttr = shellClass ? ` ${shellClass}` : "";
      const layoutClassAttr = layoutClass ? ` ${layoutClass}` : "";
      const identityBodyHtml = identitySectionBodyHtml ||
        `<div class="character-sheet-field-list">${context.identityHtml}</div>`;
      const identityTitleHtml = escapeHtml(String(identitySectionTitle || "Identity"));
      const identityHeaderHtml = identitySectionHeadHtml
        ? `<div class="character-sheet-card-head"><h3>${identityTitleHtml}</h3>${identitySectionHeadHtml}</div>`
        : `<h3>${identityTitleHtml}</h3>`;
      const physicalBodyHtml = physicalSectionBodyHtml ||
        `<div class="character-sheet-field-list">${context.physicalHtml}</div>`;
      const physicalTitleHtml = escapeHtml(String(physicalSectionTitle || "Physical"));
      const physicalHeaderHtml = physicalSectionHeadHtml
        ? `<div class="character-sheet-card-head"><h3>${physicalTitleHtml}</h3>${physicalSectionHeadHtml}</div>`
        : `<h3>${physicalTitleHtml}</h3>`;
      const noteHtml = note
        ? `<p class="character-sheet-editor-note">${escapeHtml(String(note))}</p>`
        : "";
      const footerWrapHtml = footerHtml
        ? `<footer class="character-sheet-actions">${footerHtml}</footer>`
        : "";

      return (
        `<div class="character-sheet-shell${shellClassAttr}">` +
          `<header class="character-sheet-header">` +
            `<div class="character-sheet-heading">` +
              `<h2>${escapeHtml(String(title))}</h2>` +
              `${noteHtml}` +
            `</div>` +
            `${bannerHtml}` +
            `<div class="character-sheet-meta">` +
              `<section class="character-sheet-card character-sheet-meta-card">` +
                `${identityHeaderHtml}` +
                `${identityBodyHtml}` +
              `</section>` +
              `<section class="character-sheet-card character-sheet-meta-card">` +
                `${physicalHeaderHtml}` +
                `${physicalBodyHtml}` +
              `</section>` +
              `<section class="character-sheet-card character-sheet-stats-card">` +
                `<h3>Stats</h3>` +
                `<div class="character-sheet-stat-list">${statsHtml}</div>` +
              `</section>` +
            `</div>` +
          `</header>` +
          `<div class="character-sheet-layout${layoutClassAttr}">` +
            `<section class="character-sheet-card character-sheet-overview-card">` +
              `<div class="character-sheet-subsection">` +
                `<h3>Progress</h3>` +
                `<div class="character-sheet-field-list">${context.progressHtml}</div>` +
              `</div>` +
              `<div class="character-sheet-subsection">` +
                `<h3>Combat</h3>` +
                `<div class="character-sheet-field-list">${context.combatHtml}</div>` +
              `</div>` +
            `</section>` +
            `<section class="character-sheet-card character-sheet-skills-card">` +
              `<h3>Skills</h3>` +
              `<div class="character-sheet-skill-list">${skillsHtml}</div>` +
            `</section>` +
            `${detailSectionHtml}` +
          `</div>` +
          `${footerWrapHtml}` +
        `</div>`
      );
    }

    // Reads the semantic prompt exported by wasm instead of scraping terminal text.
    function updatePromptState(heap) {
      topPromptKind = UI_PROMPT_KIND_NONE;
      topPromptMoreHint = false;
      topPromptActive = false;
      topPromptText = "";
      topPromptAttrs = null;
      morePromptActive = false;
      morePromptText = "";
      morePromptAttrs = [];

      if (
        !api ||
        !heap ||
        typeof api.getPromptKind !== "function" ||
        typeof api.getPromptTextPtr !== "function" ||
        typeof api.getPromptTextLen !== "function" ||
        typeof api.getPromptAttrsPtr !== "function" ||
        typeof api.getPromptAttrsLen !== "function"
      ) {
        return;
      }

      const promptKind = Number(api.getPromptKind()) || UI_PROMPT_KIND_NONE;
      const promptText = readUtf8(heap, api.getPromptTextPtr(), api.getPromptTextLen()).trimEnd();
      const promptAttrs = readBytes(heap, api.getPromptAttrsPtr(), api.getPromptAttrsLen());
      const promptMoreHint =
        typeof api.getPromptMoreHint === "function" && api.getPromptMoreHint() !== 0;

      if (promptKind === UI_PROMPT_KIND_MORE) {
        morePromptActive = true;
        morePromptText = promptText;
        morePromptAttrs = promptAttrs;
        return;
      }

      if (!promptText) return;

      topPromptKind = promptKind;
      topPromptMoreHint = promptMoreHint;
      topPromptActive = true;
      topPromptText = promptText;
      topPromptAttrs = promptAttrs;
    }

    // Formats one inches value as feet and inches for the age/height/weight editor.
    function formatBirthHeight(value) {
      const inches = parseBirthNumberInput(value);
      const feet = Math.floor(inches / 12);
      const remainder = inches % 12;

      if (inches <= 0 || feet <= 0) return "";
      return remainder > 0 ? `${feet}'${remainder}"` : `${feet}'`;
    }

    // Renders the shared birth name editor as a responsive HTML overlay.
    function updateBirthNameSemantic() {
      const state = activeBirthState;
      const sheet = activeCharacterSheetState;
      if (getBirthScreenKind() !== "name" || !sheet) {
        return false;
      }

      syncBirthTextDraft(state);
      semanticMenuSnapshot = null;

      const context = buildCharacterSheetContext(sheet);
      const identityFields = Array.isArray(sheet.identity) ? sheet.identity : [];
      const identityRemainderHtml = renderCharacterFields(
        identityFields.filter((field) => String(field?.label || "") !== "Name"),
        "character-sheet-identity-value"
      );
      const statsHtml = renderCharacterStaticStats(Array.isArray(sheet.stats) ? sheet.stats : []);
      const skillsHtml = renderCharacterSkills(Array.isArray(sheet.skills) ? sheet.skills : []);
      const maxLength = Math.max(1, Number(state.maxLength ?? 31));
      const identitySectionHeadHtml =
        `<button type="button" class="character-sheet-action character-sheet-action-inline" data-birth-text-action="reroll">` +
          `<span class="character-sheet-action-key">Tab</span>` +
          `<span class="character-sheet-action-label">Random</span>` +
        `</button>`;
      const identitySectionBodyHtml =
        `<div class="character-birth-form">` +
          `<label class="character-birth-field">` +
            `<span class="character-birth-field-head">` +
              `<span class="character-birth-field-label">Name</span>` +
            `</span>` +
            `<input class="character-birth-input" data-birth-text-input="name" type="text" spellcheck="false" autocomplete="off" maxlength="${maxLength}" value="${escapeHtml(birthTextDraft)}" />` +
            `<span class="character-birth-field-range">${escapeHtml(`Up to ${maxLength} characters`)}</span>` +
          `</label>` +
        `</div>` +
        `<div class="character-sheet-field-list">${identityRemainderHtml}</div>`;
      const detailSectionHtml =
        `<section class="character-sheet-card character-sheet-history-card">` +
          `<h3>History</h3>` +
          `<div class="character-sheet-history">${escapeHtml(String(sheet.history || ""))}</div>` +
        `</section>`;
      const footerHtml =
        `<button type="button" class="character-sheet-action" data-birth-text-action="accept">` +
          `<span class="character-sheet-action-key">Enter</span>` +
          `<span class="character-sheet-action-label">Accept</span>` +
        `</button>` +
        `<button type="button" class="character-sheet-action" data-birth-text-action="cancel">` +
          `<span class="character-sheet-action-key">Esc</span>` +
          `<span class="character-sheet-action-label">Back</span>` +
        `</button>`;
      const html = renderCharacterSheetShell({
        shellClass: "character-sheet-shell-birth-name",
        title: String(state.title || "Choose your name"),
        note: String(state.help || ""),
        context,
        identitySectionHeadHtml,
        identitySectionBodyHtml,
        statsHtml,
        skillsHtml,
        detailSectionHtml,
        footerHtml,
      });

      showSemanticOverlay("overlay-birth-name", html);
      syncBirthTextEditorValue("name");
      syncBirthTextEditorIntoView("name");

      return true;
    }

    // Renders the shared birth age/height/weight editor as a responsive HTML overlay.
    function updateBirthAhwSemantic() {
      const state = activeBirthState;
      const sheet = activeCharacterSheetState;
      if (!isBirthAhwScreenActive() || !sheet) {
        return false;
      }

      syncBirthAhwDraft(state);
      semanticMenuSnapshot = null;

      const context = buildCharacterSheetContext(sheet);
      const statsHtml = renderCharacterStaticStats(Array.isArray(sheet.stats) ? sheet.stats : []);
      const skillsHtml = renderCharacterSkills(Array.isArray(sheet.skills) ? sheet.skills : []);
      const fields = [
        {
          key: "age",
          label: "Age",
          value: birthAhwDraft.age,
          min: Number(state.ageMin ?? 0),
          max: Number(state.ageMax ?? 0),
          suffix: "years",
          preview: "",
        },
        {
          key: "height",
          label: "Height",
          value: birthAhwDraft.height,
          min: Number(state.heightMin ?? 0),
          max: Number(state.heightMax ?? 0),
          suffix: "inches",
          preview: formatBirthHeight(birthAhwDraft.height),
        },
        {
          key: "weight",
          label: "Weight",
          value: birthAhwDraft.weight,
          min: Number(state.weightMin ?? 0),
          max: Number(state.weightMax ?? 0),
          suffix: "pounds",
          preview: "",
        },
      ];
      const fieldHtml = fields
        .map((field) => {
          const previewHtml = field.preview
            ? `<span class="character-birth-field-preview">${escapeHtml(field.preview)}</span>`
            : "";

          return (
            `<label class="character-birth-field">` +
              `<span class="character-birth-field-head">` +
                `<span class="character-birth-field-label">${escapeHtml(field.label)}</span>` +
                `${previewHtml}` +
              `</span>` +
              `<input class="character-birth-input" data-birth-ahw-field="${escapeHtml(field.key)}" type="number" inputmode="numeric" step="1" min="${field.min}" max="${field.max}" value="${escapeHtml(field.value)}" />` +
              `<span class="character-birth-field-range">${escapeHtml(`${field.min}-${field.max} ${field.suffix}`)}</span>` +
            `</label>`
          );
        })
        .join("");

      const detailSectionHtml =
        `<section class="character-sheet-card character-sheet-history-card">` +
          `<h3>History</h3>` +
          `<div class="character-sheet-history">${escapeHtml(String(sheet.history || ""))}</div>` +
        `</section>`;
      const physicalSectionHeadHtml =
        `<button type="button" class="character-sheet-action character-sheet-action-inline" data-birth-ahw-action="reroll">` +
          `<span class="character-sheet-action-key">Space</span>` +
          `<span class="character-sheet-action-label">Reroll</span>` +
        `</button>`;
      const physicalSectionBodyHtml =
        `<div class="character-birth-form">${fieldHtml}</div>`;
      const footerHtml =
        `<button type="button" class="character-sheet-action" data-birth-ahw-action="accept">` +
          `<span class="character-sheet-action-key">Enter</span>` +
          `<span class="character-sheet-action-label">Accept</span>` +
        `</button>` +
        `<button type="button" class="character-sheet-action" data-birth-ahw-action="cancel">` +
          `<span class="character-sheet-action-key">Esc</span>` +
          `<span class="character-sheet-action-label">Back</span>` +
        `</button>`;
      const html = renderCharacterSheetShell({
        shellClass: "character-sheet-shell-birth-ahw",
        title: String(state.title || "Set your age, height, and weight"),
        note: String(state.help || ""),
        context,
        physicalSectionHeadHtml,
        physicalSectionBodyHtml,
        statsHtml,
        skillsHtml,
        detailSectionHtml,
        footerHtml,
      });

      showSemanticOverlay("overlay-birth-ahw", html);

      for (const field of fields) {
        const inputEl = overlayModalEl.querySelector(
          `[data-birth-ahw-field="${field.key}"]`
        );
        if (inputEl && inputEl.value !== field.value) {
          inputEl.value = field.value;
        }
      }
      syncBirthAhwEditorIntoView();

      return true;
    }

    // Renders the shared birth-history editor as a responsive HTML overlay.
    function updateBirthHistorySemantic() {
      const state = activeBirthState;
      const sheet = activeCharacterSheetState;
      if (getBirthScreenKind() !== "history" || !sheet) {
        return false;
      }

      syncBirthTextDraft(state);
      semanticMenuSnapshot = null;

      const context = buildCharacterSheetContext(sheet);
      const statsHtml = renderCharacterStaticStats(Array.isArray(sheet.stats) ? sheet.stats : []);
      const skillsHtml = renderCharacterSkills(Array.isArray(sheet.skills) ? sheet.skills : []);
      const helpText = String(
        state.help ||
        "Edit the text directly, use Reroll for a random history, Accept when you are happy, or Escape to go back."
      );

      const detailSectionHtml =
        `<section class="character-sheet-card character-sheet-history-card character-sheet-history-editor-card">` +
          `<div class="character-sheet-history-editor-head">` +
            `<h3>History</h3>` +
            `<button type="button" class="character-sheet-action character-sheet-action-inline" data-birth-text-action="reroll">` +
              `<span class="character-sheet-action-key">Space</span>` +
              `<span class="character-sheet-action-label">Reroll</span>` +
            `</button>` +
          `</div>` +
          `<div class="character-history-editor">` +
            `<textarea class="character-history-textarea" data-birth-text-input="history" spellcheck="false" aria-label="Character history"></textarea>` +
          `</div>` +
        `</section>`;
      const footerHtml =
        `<button type="button" class="character-sheet-action" data-birth-text-action="accept">` +
          `<span class="character-sheet-action-key">Enter</span>` +
          `<span class="character-sheet-action-label">Accept</span>` +
        `</button>` +
        `<button type="button" class="character-sheet-action" data-birth-text-action="cancel">` +
          `<span class="character-sheet-action-key">Esc</span>` +
          `<span class="character-sheet-action-label">Back</span>` +
        `</button>`;
      const html = renderCharacterSheetShell({
        shellClass: "character-sheet-shell-birth-history",
        title: String(state.title || "Shape your history"),
        note: helpText,
        layoutClass: "character-sheet-layout-history-editor",
        context,
        statsHtml,
        skillsHtml,
        detailSectionHtml,
        footerHtml,
      });

      showSemanticOverlay("overlay-birth-history", html);
      syncBirthTextEditorValue("history");
      syncBirthTextEditorIntoView("history");

      return true;
    }

    // Renders the shared birth stat-allocation screen as a responsive HTML overlay.
    function updateBirthStatsSemantic() {
      const state = activeBirthState;
      const sheet = activeCharacterSheetState;
      const stats = Array.isArray(state?.stats) ? state.stats : [];
      if (!isBirthStatsScreenActive() || !stats.length || !sheet) {
        return false;
      }

      semanticMenuSnapshot = null;

      const context = buildCharacterSheetContext(sheet);
      const statRowsHtml = renderCharacterBirthStats(stats);
      const skillsHtml = renderCharacterSkills(sheet.skills);
      const bannerHtml =
        `<div class="character-sheet-editor-banner">` +
          `<span class="character-sheet-editor-points-label">Points left</span>` +
          `<strong class="character-sheet-editor-points-value">${escapeHtml(String(state.pointsLeft ?? 0))}</strong>` +
        `</div>`;
      const detailSectionHtml =
        `<section class="character-sheet-card character-sheet-history-card">` +
          `<h3>History</h3>` +
          `<div class="character-sheet-history">${escapeHtml(String(sheet.history || ""))}</div>` +
        `</section>`;
      const html = renderCharacterSheetShell({
        shellClass: "character-sheet-shell-birth",
        title: String(state.title || "Allocate your attributes"),
        note: String(state.help || ""),
        bannerHtml,
        context,
        statsHtml: statRowsHtml,
        skillsHtml,
        detailSectionHtml,
      });

      showSemanticOverlay("overlay-birth-stats", html);
      syncSelectedBirthStatIntoView();
      return true;
    }

    // Returns the currently selected birth stat row from the semantic overlay state.
    function getSelectedBirthStatIndex() {
      const stats = Array.isArray(activeBirthState?.stats)
        ? activeBirthState.stats
        : [];
      const selectedStat = stats.find((stat) => Number(stat?.selected ?? 0) === 1);
      const index = Number(selectedStat?.index ?? -1);

      return Number.isInteger(index) ? index : -1;
    }

    // Queues the movement keys needed to move birth stat selection to one row.
    function queueBirthStatSelection(index) {
      let selected = getSelectedBirthStatIndex();
      let step = 0;

      if (!Array.isArray(activeBirthState?.stats)) return false;
      if (!Number.isInteger(index)) return false;
      if (index < 0 || index >= activeBirthState.stats.length) return false;

      if (selected < 0) selected = 0;
      if (selected === index) return true;

      step = index > selected ? 1 : -1;
      while (selected !== index) {
        if (!pushAscii(step > 0 ? "2".charCodeAt(0) : "8".charCodeAt(0))) {
          return false;
        }
        selected += step;
      }

      return true;
    }

    // Queues one birth stat adjustment after ensuring the intended row is selected.
    function queueBirthStatAdjustment(index, delta) {
      const stats = Array.isArray(activeBirthState?.stats)
        ? activeBirthState.stats
        : [];
      const stat = stats.find((entry) => Number(entry?.index ?? -1) === index);

      if (!Number.isInteger(index) || !Number.isInteger(delta) || delta === 0) {
        return false;
      }
      if (!stat) return false;
      if (delta < 0 && Number(stat.canDecrease ?? 0) !== 1) return false;
      if (delta > 0 && Number(stat.canIncrease ?? 0) !== 1) return false;
      if (!queueBirthStatSelection(index)) return false;

      return pushAscii(delta < 0 ? "4".charCodeAt(0) : "6".charCodeAt(0));
    }

    // Returns the currently selected skill row from the semantic character-sheet editor state.
    function getSelectedCharacterSkillIndex() {
      const skills = Array.isArray(activeCharacterSheetState?.skills)
        ? activeCharacterSheetState.skills
        : [];
      const selectedSkill = skills.find((skill) => Number(skill?.selected ?? 0) === 1);

      return skills.indexOf(selectedSkill);
    }

    // Queues the movement keys needed to move skill-editor selection to one row.
    function queueCharacterSkillSelection(index) {
      const skills = Array.isArray(activeCharacterSheetState?.skills)
        ? activeCharacterSheetState.skills
        : [];
      let selected = getSelectedCharacterSkillIndex();
      let step = 0;

      if (!isCharacterSkillEditorActive()) return false;
      if (!Number.isInteger(index) || index < 0 || index >= skills.length) return false;

      if (selected < 0) selected = 0;
      if (selected === index) return true;

      step = index > selected ? 1 : -1;
      while (selected !== index) {
        if (!pushAscii(step > 0 ? "2".charCodeAt(0) : "8".charCodeAt(0))) {
          return false;
        }
        selected += step;
      }

      return true;
    }

    // Queues one skill-editor adjustment after ensuring the intended row is selected.
    function queueCharacterSkillAdjustment(index, delta) {
      const skills = Array.isArray(activeCharacterSheetState?.skills)
        ? activeCharacterSheetState.skills
        : [];
      const skill = skills[index];

      if (!isCharacterSkillEditorActive()) return false;
      if (!Number.isInteger(index) || !Number.isInteger(delta) || delta === 0) {
        return false;
      }
      if (!skill || Number(skill.editable ?? 0) !== 1) return false;
      if (delta < 0 && Number(skill.canDecrease ?? 0) !== 1) return false;
      if (delta > 0 && Number(skill.canIncrease ?? 0) !== 1) return false;
      if (!queueCharacterSkillSelection(index)) return false;

      return pushAscii(delta < 0 ? "4".charCodeAt(0) : "6".charCodeAt(0));
    }

    // Renders the semantic skill-allocation screen using the shared character-sheet layout.
    function updateCharacterSkillEditorSemantic() {
      const state = activeCharacterSheetState;
      if (!state || Number(state.active) !== 1 || Number(state.skillEditorActive ?? 0) !== 1) {
        return false;
      }

      const context = buildCharacterSheetContext(state);
      const stats = Array.isArray(state.stats) ? state.stats : [];
      const skills = Array.isArray(state.skills) ? state.skills : [];

      const statsHtml = renderCharacterStaticStats(stats);
      const skillsHtml = renderCharacterEditableSkills(skills);
      const bannerHtml =
        `<div class="character-sheet-editor-banner">` +
          `<span class="character-sheet-editor-points-label">Exp left</span>` +
          `<strong class="character-sheet-editor-points-value">${escapeHtml(String(state.skillPointsLeft ?? 0))}</strong>` +
        `</div>`;
      const detailSectionHtml =
        `<section class="character-sheet-card character-sheet-history-card">` +
          `<h3>History</h3>` +
          `<div class="character-sheet-history">${escapeHtml(String(state.history || ""))}</div>` +
        `</section>`;
      const html = renderCharacterSheetShell({
        title: String(state.skillEditorTitle || "Increase your skills"),
        note: String(state.skillEditorHelp || ""),
        bannerHtml,
        context,
        statsHtml,
        skillsHtml,
        detailSectionHtml,
      });

      semanticMenuSnapshot = null;

      showSemanticOverlay("overlay-character-skills", html);
      syncSelectedCharacterSkillIntoView();
      return true;
    }

    // Renders the semantic character sheet as a responsive HTML overlay.
    function updateCharacterSheetSemantic() {
      const state = activeCharacterSheetState;
      if (!state || Number(state.active) !== 1 || Number(state.skillEditorActive ?? 0) === 1) {
        return false;
      }

      const context = buildCharacterSheetContext(state);
      const stats = Array.isArray(state.stats) ? state.stats : [];
      const skills = Array.isArray(state.skills) ? state.skills : [];
      const actions = Array.isArray(state.actions) ? state.actions : [];

      const statsHtml = renderCharacterStaticStats(stats);
      const skillsHtml = renderCharacterSkills(skills);
      const actionsHtml = renderCharacterActions(actions);
      const detailSectionHtml =
        `<section class="character-sheet-card character-sheet-history-card">` +
          `<h3>History</h3>` +
          `<div class="character-sheet-history">${escapeHtml(String(state.history || ""))}</div>` +
        `</section>`;
      const html = renderCharacterSheetShell({
        title: String(state.title || "Character Sheet"),
        context,
        statsHtml,
        skillsHtml,
        detailSectionHtml,
        footerHtml: actionsHtml,
      });

      semanticMenuSnapshot = null;

      showSemanticOverlay("overlay-character-sheet", html);
      return true;
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
      const hasSummary = !!menuState.summaryText;
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
      const summaryHtml = `<div class="menu-summary${hasSummary ? "" : " menu-summary-empty"}">${
        hasSummary
          ? renderColoredText(menuState.summaryText, menuState.summaryAttrs, 1)
          : ""
      }</div>`;
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
      const menuHtml = `<div class="menu-shell ${shellClass}">${introHtml}<div class="menu-body ${bodyClass}"><div class="menu-columns-wrap"><div class="menu-columns">${menuColumnsHtml}</div></div>${detailsHtml}</div>${summaryHtml}</div>`;

      showSemanticOverlay("overlay-menu", menuHtml);
      if (hasDetailsPane && Number.isInteger(menuState.detailsWidth) && menuState.detailsWidth > 0) {
        overlayModalEl.style.setProperty(
          "--menu-details-width",
          `${menuState.detailsWidth}ch`
        );
      } else if (hasDetailsPane && Number.isInteger(menuState.detailsHeight) && menuState.detailsHeight > 0) {
        overlayModalEl.style.setProperty(
          "--menu-details-min-height",
          `${menuState.detailsRows}ch`
        );
      } else {
        overlayModalEl.style.removeProperty("--menu-details-width");
      }
      if (hasSummary && Number.isInteger(menuState.summaryRows) && menuState.summaryRows > 0) {
        overlayModalEl.style.setProperty(
          "--menu-summary-min-height",
          `calc(${menuState.summaryRows} * 1.25em + 1.5rem)`
        );
      } else {
        overlayModalEl.style.removeProperty("--menu-summary-min-height");
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
        resetSemanticOverlayModes();
        return false;
      }

      showSemanticOverlay(null, renderColoredText(text, attrs, 1), {
        dismissable: dismissKey > 0,
      });
      return true;
    }

    // Updates modal overlay text from semantic wasm buffers.
    function updateOverlaySemantic(heap) {
      if (updateModalSemantic(heap)) {
        activeTileContextState = null;
        return true;
      }

      if (updateBirthNameSemantic()) {
        activeTileContextState = null;
        return true;
      }

      if (updateBirthAhwSemantic()) {
        activeTileContextState = null;
        return true;
      }

      if (updateBirthHistorySemantic()) {
        activeTileContextState = null;
        return true;
      }

      if (updateBirthStatsSemantic()) {
        activeTileContextState = null;
        return true;
      }

      if (updateCharacterSkillEditorSemantic()) {
        activeTileContextState = null;
        return true;
      }

      if (updateCharacterSheetSemantic()) {
        activeTileContextState = null;
        return true;
      }

      if (updateTileContextSemantic()) {
        return true;
      }

      if (updateMenuSemantic()) {
        activeTileContextState = null;
        return true;
      }

      if (!activeMenuItems.length && !shouldKeepSemanticMenuSnapshot()) {
        semanticMenuSnapshot = null;
      }

      if (!api) return false;

      const mode = api.getOverlayMode();
      if (!mode) {
        hideSemanticOverlay();
        return false;
      }

      activeTileContextState = null;

      const textPtr = api.getOverlayTextPtr();
      const textLen = api.getOverlayTextLen();
      const attrPtr = api.getOverlayAttrsPtr();
      const attrLen = api.getOverlayAttrsLen();
      const text = readUtf8(heap, textPtr, textLen);
      const attrs = readBytes(heap, attrPtr, attrLen);

      if (!text) {
        hideSemanticOverlay();
        return false;
      }

      showSemanticOverlay(null, renderColoredText(text, attrs, 1));
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

    // Reads the current player map cell so shell toolbar buttons can reuse the live avatar visual.
    function getPlayerToolbarVisual(heap, state = null) {
      const fallback = normalizeMenuItemVisual(WEB_CELL_TEXT, 14, "@".charCodeAt(0));

      if (!state || Number(state.ready) !== 1 || !api || !heap) return fallback;

      const mapPtr = typeof api.getMapCellsPtr === "function" ? api.getMapCellsPtr() : 0;
      const playerX = typeof api.getPlayerMapX === "function" ? api.getPlayerMapX() : -1;
      const playerY = typeof api.getPlayerMapY === "function" ? api.getPlayerMapY() : -1;

      if (!mapPtr || playerX < 0 || playerY < 0 || playerX >= mapCols || playerY >= mapRows) {
        return fallback;
      }

      return normalizeCellVisual(readCell(heap, mapPtr, mapCols, playerX, playerY)) || fallback;
    }

    // Updates the sidebar and compact HUD toolbar buttons from the current gameplay state.
    function syncSidebarToolbar(state = null, heap = null) {
      const ready = !!state && Number(state.ready) === 1;
      const enabled = canUseShellToolbarScreenButton(state);
      const visual = getPlayerToolbarVisual(heap, state);
      const characterButtons = [hudCharacterButtonEl, sideCharacterButtonEl];
      const characterVisuals = [hudCharacterVisualEl, sideCharacterVisualEl];

      for (let i = 0; i < characterButtons.length; i += 1) {
        const buttonEl = characterButtons[i];
        const visualEl = characterVisuals[i];
        if (!buttonEl || !visualEl) continue;

        renderActionFabVisual(visualEl, ready ? visual : null);
        buttonEl.disabled = !enabled;
        buttonEl.classList.toggle("shell-toolbar-button-disabled", !enabled);
        buttonEl.setAttribute("aria-disabled", enabled ? "false" : "true");
        buttonEl.title = enabled ? "Open character sheet (@)" : "Character sheet unavailable";
      }

      for (const buttonEl of [sideLogButtonEl, sideKnowledgeButtonEl]) {
        if (!buttonEl) continue;
        buttonEl.disabled = !enabled;
        buttonEl.classList.toggle("shell-toolbar-button-disabled", !enabled);
        buttonEl.setAttribute("aria-disabled", enabled ? "false" : "true");
      }

      if (sideLogButtonEl) {
        sideLogButtonEl.title = enabled ? "Open message history (Ctrl-P)" : "Message history unavailable";
      }
      if (sideKnowledgeButtonEl) {
        sideKnowledgeButtonEl.title = enabled ? "Open knowledge browser (~)" : "Knowledge browser unavailable";
      }
    }

    // Updates the compact top HUD shown when the side panel collapses on narrow screens.
    function updateMobileHud(state) {
      if (!hudBarsEl) return;

      if (!state || Number(state.ready) !== 1) {
        const placeholderHtml =
          renderMobileHudBar("Health", "HP", 0, 0, "hud-bar-empty") +
          renderMobileHudBar("Voice", "Voice", 0, 0, "hud-bar-empty");
        setHtmlIfChanged(hudBarsEl, placeholderHtml);
        hudBarsEl.title = "Character panel";
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
      hudBarsEl.title = titleBits.length ? titleBits.join(" - ") : "Character panel";
    }

    // Updates side and log panels using semantic text and color attributes.
    function updateSemanticPanels(heap, state = null) {
      if (!api) return;
      if (!state) state = readPlayerState(heap);
      activePlayerState = state;
      const logPtr = api.getLogTextPtr();
      const logLen = api.getLogTextLen();
      const logAttrPtr = api.getLogAttrsPtr();
      const logAttrLen = api.getLogAttrsLen();

      const logText = readUtf8(heap, logPtr, logLen).trimEnd();
      const logAttrs = readBytes(heap, logAttrPtr, logAttrLen);

      const sideHtml = state
        ? renderSideState(state)
        : "<span class=\"term-c2\">(side panel empty)</span>";
      const hasLogText = Boolean(logText);
      let logHtml = hasLogText
        ? renderColoredText(logText, logAttrs, 1)
        : "<span class=\"term-c2\">(message panel empty)</span>";

      if (
        topPromptActive &&
        topPromptText &&
        topPromptKind !== UI_PROMPT_KIND_MESSAGE
      ) {
        const promptHtml = `${renderColoredText(topPromptText, topPromptAttrs, 11)}${
          topPromptMoreHint ? "<span class=\"more-indicator\">-more-</span>" : ""
        }`;
        logHtml = hasLogText ? `${logHtml}<br>${promptHtml}` : promptHtml;
      }

      if (morePromptActive) {
        const promptLead = morePromptText
          ? `${renderColoredText(morePromptText, morePromptAttrs, 11)} `
          : "";
        const promptHtml = `${promptLead}<span class="more-indicator">-more-</span>`;
        logHtml = hasLogText ? `${logHtml}<br>${promptHtml}` : promptHtml;
      }

      updateMobileHud(state);
      syncSidebarToolbar(state, heap);
      syncFloatingActionButtons(state, heap);
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

      syncSoundPlayback();

      const now = performance.now();
      const mapFxActive = updateMapFxWindow(now);
      const mapFxStateChanged = mapFxActive !== lastMapFxActive;
      const frameId = api.getFrameId();
      const menuRevision =
        typeof api.getMenuRevision === "function" ? api.getMenuRevision() : 0;
      const modalRevision =
        typeof api.getModalRevision === "function" ? api.getModalRevision() : 0;
      const promptRevision =
        typeof api.getPromptRevision === "function" ? api.getPromptRevision() : 0;
      const birthStateRevision =
        typeof api.getBirthStateRevision === "function" ? api.getBirthStateRevision() : 0;
      const characterSheetRevision =
        typeof api.getCharacterSheetStateRevision === "function"
          ? api.getCharacterSheetStateRevision()
          : 0;
      const frameChanged = frameId !== lastFrameId;
      if (
        !forceRedraw &&
        frameId === lastFrameId &&
        menuRevision === lastMenuRevision &&
        modalRevision === lastModalRevision &&
        promptRevision === lastPromptRevision &&
        birthStateRevision === lastBirthStateRevision &&
        characterSheetRevision === lastCharacterSheetRevision &&
        !mapFxStateChanged &&
        !mapFxActive
      ) {
        return;
      }
      forceRedraw = false;
      lastFrameId = frameId;
      lastMenuRevision = menuRevision;
      lastModalRevision = modalRevision;
      lastPromptRevision = promptRevision;
      lastBirthStateRevision = birthStateRevision;
      lastCharacterSheetRevision = characterSheetRevision;
      lastMapFxActive = mapFxActive;

      const cols = api.getCols();
      const rows = api.getRows();
      const heap = getHeapU8();
      if (!heap) return;

      if (cols <= 0 || rows <= 0) return;

      refreshLayoutMetadata(cols, rows);
      refreshIconMetadata();
      refreshMenuItems(heap);
      readBirthState(heap);
      readCharacterSheetState(heap);
      updatePromptState(heap);

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
      noteAutosaveGameplayActivity(frameChanged, stateReady);

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
      if (!api || typeof api.pushKey !== "function") return false;
      return api.pushKey(code & 0xff) !== 0;
    }

    // Drains any queued engine sound events into the browser audio player.
    function syncSoundPlayback() {
      if (!api || !soundPlayer || typeof api.popSoundEvent !== "function") return;

      for (;;) {
        const eventId = Number(api.popSoundEvent());
        if (!Number.isInteger(eventId) || eventId < 0) break;
        soundPlayer.enqueueEvent(eventId);
      }
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
        !activeTileContextState &&
        !isBirthScreenActive() &&
        !isCharacterSkillEditorActive() &&
        !isCharacterSheetActive() &&
        !overlayModalEl.classList.contains("overlay-tile-context") &&
        !overlayModalEl.classList.contains("overlay-menu") &&
        !overlayModalEl.classList.contains("overlay-birth-name") &&
        !overlayModalEl.classList.contains("overlay-birth-ahw") &&
        !overlayModalEl.classList.contains("overlay-birth-history") &&
        !overlayModalEl.classList.contains("overlay-birth-stats") &&
        !overlayModalEl.classList.contains("overlay-character-skills") &&
        !overlayModalEl.classList.contains("overlay-character-sheet") &&
        !overlayModalEl.classList.contains("overlay-dismissable") &&
        !isInteractiveTopPromptActive() &&
        !morePromptActive;
    }

    // Returns whether the current frontend state allows raw map clicks to start travel.
    function canUseMapTravel() {
      return !!api &&
        typeof api.travelTo === "function" &&
        isGameplayViewIdle();
    }

    // Returns whether the shell toolbar can open a non-action gameplay screen right now.
    function canUseShellToolbarScreenButton(state = null) {
      return !!api &&
        !!state &&
        Number(state.ready) === 1 &&
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

    // Returns whether the floating ranged-action button should currently be enabled.
    function canUseRangedFab(state = null) {
      return !!api &&
        typeof api.openRangedTarget === "function" &&
        !!state &&
        Number(state.rangedActionReady ?? 0) === 1 &&
        Number(state.rangedActionVisible) === 1 &&
        isGameplayViewIdle();
    }

    // Returns whether the floating stealth/state button should currently be enabled.
    function canUseStateFab(state = null) {
      return !!api &&
        typeof api.toggleStealth === "function" &&
        !!state &&
        Number(state.ready) === 1 &&
        isGameplayViewIdle();
    }

    // Returns whether the floating song button should currently be enabled.
    function canUseSongFab(state = null) {
      return !!api &&
        typeof api.openSongMenu === "function" &&
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
          !!activeTileContextState ||
          activeMenuItems.length > 0 ||
          isBirthScreenActive() ||
          isCharacterSkillEditorActive() ||
          isCharacterSheetActive() ||
          overlayModalEl.classList.contains("overlay-tile-context") ||
          overlayModalEl.classList.contains("overlay-birth-name") ||
          overlayModalEl.classList.contains("overlay-birth-ahw") ||
          overlayModalEl.classList.contains("overlay-birth-history") ||
          overlayModalEl.classList.contains("overlay-birth-stats") ||
          overlayModalEl.classList.contains("overlay-character-skills") ||
          overlayModalEl.classList.contains("overlay-character-sheet") ||
          isDismissableModalOverlayActive() ||
          isGenericCapturedOverlayActive() ||
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

    // Returns the visible confirm-FAB label metadata for the current overlay state.
    function getConfirmFabMeta({
      yesNoPromptActive,
      birthTextConfig,
      birthAhwActive,
      birthStatsActive,
      skillEditorActive,
    }) {
      if (yesNoPromptActive) {
        return { title: "Yes (y)", ariaLabel: "Yes" };
      }
      if (birthTextConfig) {
        return {
          title: birthTextConfig.acceptFabTitle,
          ariaLabel: birthTextConfig.acceptFabAria,
        };
      }
      if (birthAhwActive) {
        return { title: "Accept details (Enter)", ariaLabel: "Accept details" };
      }
      if (birthStatsActive) {
        return { title: "Accept attributes (Enter)", ariaLabel: "Accept attributes" };
      }
      if (skillEditorActive) {
        return { title: "Accept skills (Enter)", ariaLabel: "Accept skills" };
      }
      return { title: "Confirm target (Enter)", ariaLabel: "Confirm target" };
    }

    // Returns the visible close-FAB label metadata for the current overlay state.
    function getCloseFabMeta({
      yesNoPromptActive,
      tileContextActive,
      birthTextActive,
      birthAhwActive,
      birthStatsActive,
      skillEditorActive,
      targetPreviewPromptActive,
      dismissableModalActive,
      genericOverlayActive,
    }) {
      if (yesNoPromptActive) {
        return { title: "No (n)", ariaLabel: "No" };
      }
      if (tileContextActive) {
        return { title: "Dismiss (Esc)", ariaLabel: "Dismiss" };
      }
      if (birthTextActive || birthAhwActive || birthStatsActive || skillEditorActive) {
        return { title: "Back (Esc)", ariaLabel: "Back" };
      }
      if (targetPreviewPromptActive) {
        return { title: "Cancel target (Esc)", ariaLabel: "Cancel target" };
      }
      if (dismissableModalActive || genericOverlayActive) {
        return { title: "Dismiss (Esc)", ariaLabel: "Dismiss" };
      }
      return { title: "Close (Esc)", ariaLabel: "Close" };
    }

    // Keeps floating action buttons visually in sync with the current gameplay state.
    function syncFloatingActionButtons(state = null, heap = null) {
      if (!actionFabsEl || !yesFabEl || !inventoryFabEl || !rangedFabEl || !rangedFabVisualEl || !stateFabWrapEl || !stateFabEl || !stateFabVisualEl || !stateFabLabelEl || !songFabWrapEl || !songFabEl || !songFabLabelEl || !tileActionFabEl || !closeFabEl) {
        return;
      }

      const yesNoPromptActive = isYesNoTopPromptActive();
      const targetPreviewPromptActive = isTargetPreviewTopPromptActive();
      const birthTextActive = isBirthTextScreenActive();
      const birthTextKind = birthTextActive ? getBirthScreenKind() : "";
      const birthTextConfig = birthTextActive ? getBirthTextScreenConfig(birthTextKind) : null;
      const birthAhwActive = isBirthAhwScreenActive();
      const birthStatsActive = isBirthStatsScreenActive();
      const skillEditorActive = isCharacterSkillEditorActive();
      const tileContextActive = isTileContextOverlayActive();
      const dismissableModalActive = isDismissableModalOverlayActive();
      const genericOverlayActive = isGenericCapturedOverlayActive();
      const showClose = !compactSideOpen && canUseCloseFab();
      const showYes =
        showClose &&
        (yesNoPromptActive || targetPreviewPromptActive || birthTextActive || birthAhwActive || birthStatsActive || skillEditorActive);
      const showInventory = !compactSideOpen && !showClose && canUseInventoryFab(state);
      const showRanged = !compactSideOpen && !showClose && canUseRangedFab(state);
      const showState = !compactSideOpen && !showClose && canUseStateFab(state);
      const showSong = !compactSideOpen && !showClose && canUseSongFab(state);
      const showTileAction = !compactSideOpen && !showClose && canUseTileActionFab(state);
      const showAdjacentActions =
        !compactSideOpen && !showClose && canUseAdjacentActionRow(state);
      const tileActionLabel = state && Number(state.tileActionVisible) === 1
        ? String(state.tileActionLabel || "Act here")
        : "Act here";
      const playerVisual = getPlayerToolbarVisual(heap || getHeapU8(), state);
      const rangedFabState = getRangedFabState(state);
      const stateFabState = getStateFabState(state);
      const songFabState = getSongFabState(state);

      renderTileActionFabVisual(state);
      renderActionFabVisual(rangedFabVisualEl, showRanged ? rangedFabState.visual : null);
      renderActionFabVisual(stateFabVisualEl, showState ? playerVisual : null);
      const anyAdjacentActions = syncAdjacentActionButtons(state, showAdjacentActions);
      const yesFabMeta = getConfirmFabMeta({
        yesNoPromptActive,
        birthTextConfig,
        birthAhwActive,
        birthStatsActive,
        skillEditorActive,
      });
      const closeFabMeta = getCloseFabMeta({
        yesNoPromptActive,
        tileContextActive,
        birthTextActive,
        birthAhwActive,
        birthStatsActive,
        skillEditorActive,
        targetPreviewPromptActive,
        dismissableModalActive,
        genericOverlayActive,
      });

      actionFabsEl.hidden =
        !showYes && !showClose && !showInventory && !showRanged && !showState && !showSong && !showTileAction && !anyAdjacentActions;
      actionFabsEl.classList.toggle("action-fabs-overlay-dialog", showClose);
      yesFabEl.hidden = !showYes;
      yesFabEl.title = yesFabMeta.title;
      yesFabEl.setAttribute("aria-label", yesFabMeta.ariaLabel);
      inventoryFabEl.hidden = !showInventory;
      rangedFabEl.hidden = !showRanged;
      rangedFabEl.title = rangedFabState.title;
      rangedFabEl.setAttribute("aria-label", rangedFabState.ariaLabel);
      stateFabWrapEl.hidden = !showState;
      stateFabEl.title = stateFabState.title;
      stateFabEl.setAttribute("aria-label", stateFabState.ariaLabel);
      stateFabEl.classList.toggle("state-fab-active", showState && stateFabState.active);
      stateFabLabelEl.hidden = !showState || !stateFabState.label;
      stateFabLabelEl.textContent = showState ? stateFabState.label : "";
      songFabWrapEl.hidden = !showSong;
      songFabEl.title = songFabState.title;
      songFabEl.setAttribute("aria-label", songFabState.ariaLabel);
      songFabEl.classList.toggle("song-fab-active", showSong && songFabState.active);
      songFabLabelEl.hidden = !showSong || !songFabState.active || !songFabState.label;
      songFabLabelEl.textContent = showSong && songFabState.active ? songFabState.label : "";
      tileActionFabEl.hidden = !showTileAction;
      tileActionFabEl.title = `${tileActionLabel} (,)`;
      tileActionFabEl.setAttribute("aria-label", tileActionLabel);
      closeFabEl.hidden = !showClose;
      closeFabEl.title = closeFabMeta.title;
      closeFabEl.setAttribute("aria-label", closeFabMeta.ariaLabel);
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

    // Moves an active targeting prompt to one tapped map grid when the backend supports it.
    function tryMapTargetAtClientPoint(clientX, clientY) {
      if (!api || typeof api.targetMap !== "function") return false;

      const hit = clientPointToMapWorld(clientX, clientY);
      if (!hit) return false;
      if (!api.targetMap(hit.worldY, hit.worldX)) return false;

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
        if (isInteractiveTopPromptActive() || morePromptActive) {
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
        if (isInteractiveTopPromptActive() || morePromptActive) return;

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
        if (isInteractiveTopPromptActive() || morePromptActive) {
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
        if (isInteractiveTopPromptActive() || morePromptActive) return;

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

    // Binds shared text input and button handling for the birth name/history overlays.
    function bindOverlayBirthTextInput() {
      overlayModalEl.addEventListener("pointerdown", (ev) => {
        const config = getBirthTextScreenConfig();
        if (!config || !overlayModalEl.classList.contains(config.overlayClass)) return;
        ev.stopPropagation();
      });

      overlayModalEl.addEventListener("input", (ev) => {
        const config = getBirthTextScreenConfig();
        if (!config || !overlayModalEl.classList.contains(config.overlayClass)) return;

        const inputEl = ev.target.closest(config.inputSelector);
        if (!inputEl) return;

        commitBirthTextDraft(inputEl.value);
        syncBirthTextEditorIntoView(config.kind);
      });

      overlayModalEl.addEventListener("focusin", (ev) => {
        const config = getBirthTextScreenConfig();
        if (!config || !overlayModalEl.classList.contains(config.overlayClass)) return;
        if (!ev.target.closest(config.inputSelector)) return;

        syncBirthTextEditorIntoView(config.kind);
      });

      overlayModalEl.addEventListener("keydown", (ev) => {
        const config = getBirthTextScreenConfig();
        if (!config || !overlayModalEl.classList.contains(config.overlayClass)) return;

        const inputEl = ev.target.closest(config.inputSelector);
        if (!inputEl) return;

        if (ev.key === "Escape") {
          dispatchBirthTextScreenAction("cancel", config.kind);
          ev.preventDefault();
          ev.stopPropagation();
          return;
        }

        if (config.kind === "name" && ev.key === "Tab") {
          dispatchBirthTextScreenAction("reroll", config.kind);
          ev.preventDefault();
          ev.stopPropagation();
          return;
        }

        if (config.acceptsSubmitShortcut(ev)) {
          dispatchBirthTextScreenAction("accept", config.kind, inputEl.value);
          ev.preventDefault();
          ev.stopPropagation();
        }
      });

      overlayModalEl.addEventListener("click", (ev) => {
        const config = getBirthTextScreenConfig();
        if (!config || !overlayModalEl.classList.contains(config.overlayClass)) return;

        const actionEl = ev.target.closest("[data-birth-text-action]");
        if (!actionEl) return;

        const action = String(actionEl.dataset.birthTextAction || "");
        if (!dispatchBirthTextScreenAction(action, config.kind)) return;

        ev.preventDefault();
        ev.stopPropagation();
      });
    }

    // Binds numeric input and button handling for the birth age/height/weight overlay.
    function bindOverlayBirthAhwInput() {
      overlayModalEl.addEventListener("pointerdown", (ev) => {
        if (!overlayModalEl.classList.contains("overlay-birth-ahw")) return;
        ev.stopPropagation();
      });

      overlayModalEl.addEventListener("input", (ev) => {
        if (!overlayModalEl.classList.contains("overlay-birth-ahw")) return;

        const inputEl = ev.target.closest("[data-birth-ahw-field]");
        if (!inputEl) return;

        const key = String(inputEl.dataset.birthAhwField || "");
        if (!key) return;

        commitBirthAhwDraft({
          ...birthAhwDraft,
          [key]: inputEl.value,
        });
        syncBirthAhwEditorIntoView();
      });

      overlayModalEl.addEventListener("focusin", (ev) => {
        if (!overlayModalEl.classList.contains("overlay-birth-ahw")) return;
        if (!ev.target.closest("[data-birth-ahw-field]")) return;

        syncBirthAhwEditorIntoView();
      });

      overlayModalEl.addEventListener("keydown", (ev) => {
        if (!overlayModalEl.classList.contains("overlay-birth-ahw")) return;

        const inputEl = ev.target.closest("[data-birth-ahw-field]");
        if (!inputEl) return;

        if (ev.key === "Escape") {
          pushAscii(27);
          forceRedraw = true;
          ev.preventDefault();
          ev.stopPropagation();
          return;
        }

        if (ev.key === "Enter") {
          if (!submitActiveBirthDraft("ahw")) return;
          pushAscii(13);
          forceRedraw = true;
          ev.preventDefault();
          ev.stopPropagation();
        }
      });

      overlayModalEl.addEventListener("click", (ev) => {
        if (!overlayModalEl.classList.contains("overlay-birth-ahw")) return;
        if (!api) return;

        const actionEl = ev.target.closest("[data-birth-ahw-action]");
        if (!actionEl) return;

        const action = String(actionEl.dataset.birthAhwAction || "");

        if (action === "accept") {
          if (!submitActiveBirthDraft("ahw")) return;
          pushAscii(13);
          forceRedraw = true;
        } else if (action === "reroll") {
          birthAhwDirty = false;
          pushAscii(" ".charCodeAt(0));
          forceRedraw = true;
        } else if (action === "cancel") {
          pushAscii(27);
          forceRedraw = true;
        } else {
          return;
        }

        ev.preventDefault();
        ev.stopPropagation();
      });
    }

    // Binds click handling for the custom birth stat-allocation overlay.
    function bindOverlayBirthStatsInput() {
      overlayModalEl.addEventListener("pointerdown", (ev) => {
        if (!overlayModalEl.classList.contains("overlay-birth-stats")) return;
        ev.stopPropagation();
      });

      overlayModalEl.addEventListener("click", (ev) => {
        if (!overlayModalEl.classList.contains("overlay-birth-stats")) return;
        if (!api) return;

        const adjustEl = ev.target.closest("[data-birth-adjust]");
        if (adjustEl) {
          const index = Number(adjustEl.dataset.birthStatIndex);
          const delta = Number(adjustEl.dataset.birthAdjust);
          if (queueBirthStatAdjustment(index, delta)) {
            forceRedraw = true;
          }
          ev.preventDefault();
          ev.stopPropagation();
          return;
        }

        const selectEl = ev.target.closest("[data-birth-stat-index]");
        if (selectEl) {
          const index = Number(selectEl.dataset.birthStatIndex);
          if (queueBirthStatSelection(index)) {
            forceRedraw = true;
          }
          ev.preventDefault();
          ev.stopPropagation();
        }
      });
    }

    // Binds click handling for the editable skill-allocation overlay.
    function bindOverlayCharacterSkillInput() {
      overlayModalEl.addEventListener("pointerdown", (ev) => {
        if (!overlayModalEl.classList.contains("overlay-character-skills")) return;
        ev.stopPropagation();
      });

      overlayModalEl.addEventListener("click", (ev) => {
        if (!overlayModalEl.classList.contains("overlay-character-skills")) return;
        if (!api) return;

        const adjustEl = ev.target.closest("[data-skill-editor-adjust]");
        if (adjustEl) {
          const index = Number(adjustEl.dataset.skillEditorIndex);
          const delta = Number(adjustEl.dataset.skillEditorAdjust);
          if (queueCharacterSkillAdjustment(index, delta)) {
            forceRedraw = true;
          }
          ev.preventDefault();
          ev.stopPropagation();
          return;
        }

        const selectEl = ev.target.closest("[data-skill-editor-index]");
        if (selectEl) {
          const index = Number(selectEl.dataset.skillEditorIndex);
          if (queueCharacterSkillSelection(index)) {
            forceRedraw = true;
          }
          ev.preventDefault();
          ev.stopPropagation();
        }
      });
    }

    // Binds click handling for the custom semantic character-sheet overlay.
    function bindOverlayCharacterSheetInput() {
      overlayModalEl.addEventListener("pointerdown", (ev) => {
        if (!overlayModalEl.classList.contains("overlay-character-sheet")) return;
        ev.stopPropagation();
      });

      overlayModalEl.addEventListener("click", (ev) => {
        if (!overlayModalEl.classList.contains("overlay-character-sheet")) return;

        const actionEl = ev.target.closest("[data-character-action-key]");
        if (!actionEl) return;

        const key = Number(actionEl.dataset.characterActionKey);
        if (!Number.isInteger(key) || key <= 0) return;

        pushAscii(key);
        forceRedraw = true;
        ev.preventDefault();
        ev.stopPropagation();
      });
    }

    // Binds click handling for the custom tile-context action popup.
    function bindOverlayTileContextInput() {
      overlayModalEl.addEventListener("pointerdown", (ev) => {
        if (!overlayModalEl.classList.contains("overlay-tile-context")) return;
        ev.stopPropagation();
      });

      overlayModalEl.addEventListener("click", (ev) => {
        if (!overlayModalEl.classList.contains("overlay-tile-context")) return;
        if (!api || typeof api.executeTileContextAction !== "function") return;

        const actionEl = ev.target.closest("[data-tile-context-action]");
        if (!actionEl) return;

        const dir = Number(actionEl.dataset.tileContextDir);
        const actionId = Number(actionEl.dataset.tileContextAction);
        if (!Number.isInteger(dir) || !Number.isInteger(actionId) || actionId <= 0) {
          return;
        }

        if (!api.executeTileContextAction(dir, actionId)) {
          return;
        }

        hideTileContextOverlay({ redraw: false });
        setCompactSideOpen(false);
        hoveredMenuIndex = -1;
        forceRedraw = true;
        ev.preventDefault();
        ev.stopPropagation();
      });
    }

    // Binds tap-to-dismiss for both semantic modals and plain captured overlays.
    function bindOverlayModalInput() {
      overlayModalEl.addEventListener("click", (ev) => {
        if (
          overlayModalEl.classList.contains("overlay-menu") ||
          overlayModalEl.classList.contains("overlay-tile-context")
        ) {
          return;
        }
        if (tryDismissActiveOverlay()) {
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

        if (morePromptActive) {
          const keyCode = mapKeyEventToAscii(ev);
          advanceMorePrompt(keyCode);
          ev.preventDefault();
          return;
        }

        if (ev.key === "Enter" && isTargetPreviewTopPromptActive()) {
          pushAscii("t".charCodeAt(0));
          forceRedraw = true;
          ev.preventDefault();
          return;
        }

        if (isTileContextOverlayActive()) {
          if (ev.key === "Escape") {
            hideTileContextOverlay();
          }
          ev.preventDefault();
          return;
        }

        const keyCode = mapKeyEventToAscii(ev);

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
      const triggerShellToolbarCommand = (ev, keyCode) => {
        ev.preventDefault();
        ev.stopPropagation();

        const heap = getHeapU8();
        const state = heap ? readPlayerState(heap) : activePlayerState;
        if (!canUseShellToolbarScreenButton(state)) return;

        setCompactSideOpen(false);
        pushAscii(keyCode);
        forceRedraw = true;
      };

      sideToggleEl.addEventListener("pointerdown", (ev) => {
        ev.stopPropagation();
      });

      sideToggleEl.addEventListener("click", (ev) => {
        ev.preventDefault();
        ev.stopPropagation();
        if (isCompactLayout()) {
          setCompactSideOpen(!compactSideOpen);
          return;
        }
        setSideCollapsed(!sideCollapsed);
      });

      sideCollapseButtonEl.addEventListener("pointerdown", (ev) => {
        ev.stopPropagation();
      });

      sideCollapseButtonEl.addEventListener("click", (ev) => {
        ev.preventDefault();
        ev.stopPropagation();
        if (isCompactLayout()) {
          setCompactSideOpen(false);
          return;
        }
        setSideCollapsed(true);
      });

      for (const buttonEl of [hudCharacterButtonEl, sideCharacterButtonEl]) {
        if (!buttonEl) continue;

        buttonEl.addEventListener("pointerdown", (ev) => {
          ev.stopPropagation();
        });

        buttonEl.addEventListener("click", (ev) => {
          triggerShellToolbarCommand(ev, "@".charCodeAt(0));
        });
      }

      for (const [buttonEl, keyCode] of [
        [sideLogButtonEl, "P".charCodeAt(0) & 0x1f],
        [sideKnowledgeButtonEl, "~".charCodeAt(0)],
      ]) {
        buttonEl.addEventListener("pointerdown", (ev) => {
          ev.stopPropagation();
        });

        buttonEl.addEventListener("click", (ev) => {
          triggerShellToolbarCommand(ev, keyCode);
        });
      }

      sideBackdropEl.addEventListener("pointerdown", (ev) => {
        ev.preventDefault();
        ev.stopPropagation();
        setCompactSideOpen(false);
      });

      sideWrapEl.addEventListener("pointerdown", (ev) => {
        ev.stopPropagation();
      });
    }

    // Binds the floating action buttons to their current contextual actions.
    function bindFloatingActionInput() {
      const clearAlternateFabLongPress = (buttonEl) => {
        const timerId = alternateFabLongPressTimers.get(buttonEl);
        if (timerId) {
          globalThis.clearTimeout(timerId);
        }
        alternateFabLongPressTimers.delete(buttonEl);
      };

      const bindAlternateFabTrigger = (buttonEl, triggerAction) => {
        buttonEl.addEventListener("contextmenu", (ev) => {
          void triggerAction(ev);
        });

        buttonEl.addEventListener("pointerdown", (ev) => {
          if (ev.pointerType === "mouse" || ev.button !== 0) return;
          clearAlternateFabLongPress(buttonEl);
          const timerId = globalThis.setTimeout(() => {
            alternateFabLongPressTimers.delete(buttonEl);
            if (triggerAction(ev)) {
              alternateFabSuppressedClicks.add(buttonEl);
            }
          }, TILE_CONTEXT_LONG_PRESS_MSEC);
          alternateFabLongPressTimers.set(buttonEl, timerId);
        });

        for (const eventName of ["pointerup", "pointercancel", "pointerleave"]) {
          buttonEl.addEventListener(eventName, () => {
            clearAlternateFabLongPress(buttonEl);
          });
        }
      };

      const bindTileContextTrigger = (buttonEl, getDir) => {
        bindAlternateFabTrigger(buttonEl, (ev) => {
          const dir = Number(getDir(buttonEl));
          if (!Number.isInteger(dir) || dir < 1 || dir > 9) return false;
          ev.preventDefault();
          ev.stopPropagation();
          return openTileContextOverlay(dir);
        });
      };

      for (const buttonEl of adjacentActionFabEls) {
        bindTileContextTrigger(buttonEl, (el) => Number(el.dataset.adjDir));

        buttonEl.addEventListener("pointerdown", (ev) => {
          ev.stopPropagation();
        });

        buttonEl.addEventListener("click", (ev) => {
          ev.preventDefault();
          ev.stopPropagation();

          if (alternateFabSuppressedClicks.has(buttonEl)) {
            alternateFabSuppressedClicks.delete(buttonEl);
            return;
          }

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

      rangedFabEl.addEventListener("pointerdown", (ev) => {
        ev.stopPropagation();
      });

      stateFabEl.addEventListener("pointerdown", (ev) => {
        ev.stopPropagation();
      });

      bindAlternateFabTrigger(stateFabEl, (ev) => {
        const heap = getHeapU8();
        const state = heap ? readPlayerState(heap) : activePlayerState;
        if (!canUseShellToolbarScreenButton(state)) return false;

        ev.preventDefault();
        ev.stopPropagation();
        setCompactSideOpen(false);
        pushAscii("@".charCodeAt(0));
        hoveredMenuIndex = -1;
        forceRedraw = true;
        return true;
      });

      yesFabEl.addEventListener("pointerdown", (ev) => {
        ev.stopPropagation();
      });

      yesFabEl.addEventListener("click", (ev) => {
        ev.preventDefault();
        ev.stopPropagation();
        const yesNoPromptActive = isYesNoTopPromptActive();
        const targetPreviewPromptActive = isTargetPreviewTopPromptActive();
        const birthTextActive = isBirthTextScreenActive();
        const birthAhwActive = isBirthAhwScreenActive();
        const birthStatsActive = isBirthStatsScreenActive();
        const skillEditorActive = isCharacterSkillEditorActive();
        if (!yesNoPromptActive && !targetPreviewPromptActive && !birthTextActive && !birthAhwActive && !birthStatsActive && !skillEditorActive) return;

        setCompactSideOpen(false);
        if (birthTextActive || birthAhwActive) {
          if (!submitActiveBirthDraft()) return;
        }
        pushAscii(
          yesNoPromptActive
            ? "y".charCodeAt(0)
            : (birthTextActive || birthAhwActive || birthStatsActive || skillEditorActive)
              ? 13
              : "t".charCodeAt(0)
        );
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

      rangedFabEl.addEventListener("click", (ev) => {
        ev.preventDefault();
        ev.stopPropagation();
        const heap = getHeapU8();
        const state = heap ? readPlayerState(heap) : null;
        if (!canUseRangedFab(state)) return;

        setCompactSideOpen(false);
        if (api.openRangedTarget()) {
          hoveredMenuIndex = -1;
          forceRedraw = true;
        }
      });

      stateFabEl.addEventListener("click", (ev) => {
        ev.preventDefault();
        ev.stopPropagation();

        if (alternateFabSuppressedClicks.has(stateFabEl)) {
          alternateFabSuppressedClicks.delete(stateFabEl);
          return;
        }

        const heap = getHeapU8();
        const state = heap ? readPlayerState(heap) : null;
        if (!canUseStateFab(state)) return;

        setCompactSideOpen(false);
        if (api.toggleStealth()) {
          hoveredMenuIndex = -1;
          forceRedraw = true;
        }
      });

      songFabEl.addEventListener("pointerdown", (ev) => {
        ev.stopPropagation();
      });

      songFabEl.addEventListener("click", (ev) => {
        ev.preventDefault();
        ev.stopPropagation();
        const heap = getHeapU8();
        const state = heap ? readPlayerState(heap) : null;
        if (!canUseSongFab(state)) return;

        setCompactSideOpen(false);
        if (api.openSongMenu()) {
          hoveredMenuIndex = -1;
          forceRedraw = true;
        }
      });

      tileActionFabEl.addEventListener("pointerdown", (ev) => {
        ev.stopPropagation();
      });

      bindTileContextTrigger(tileActionFabEl, () => 5);

      tileActionFabEl.addEventListener("click", (ev) => {
        ev.preventDefault();
        ev.stopPropagation();

        if (alternateFabSuppressedClicks.has(tileActionFabEl)) {
          alternateFabSuppressedClicks.delete(tileActionFabEl);
          return;
        }

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

        if (tryDismissActiveOverlay()) {
          return;
        }

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

        if (eligible && !tryMapTargetAtClientPoint(ev.clientX, ev.clientY)) {
          tryMapTravelAtClientPoint(ev.clientX, ev.clientY);
        }
      };

      const commitTouchTapAt = (clientX, clientY) => {
        const eligible = touchTapEligible;
        touchTapPointerId = null;
        touchTapEligible = false;

        if (eligible && !tryMapTargetAtClientPoint(clientX, clientY)) {
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
        "-a",
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
          let autoResumeBaseName = "";

          ensurePersistDirs();
          if (err) {
            disablePersistFs("syncfs init failed; continuing with session storage only", err);
          } else {
            persistFsEnabled = true;
            autoResumeBaseName = readPersistedAutoResumeBaseName(FS);
            flushPersistFs(true);
          }

          if (
            autoResumeBaseName &&
            Array.isArray(Module.arguments) &&
            !Module.arguments.some((arg) => /^-u/i.test(String(arg || ""))) &&
            !Module.arguments.some((arg) => /^-n$/i.test(String(arg || "")))
          ) {
            Module.arguments.push(`-u${autoResumeBaseName}`);
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
          typeof Module._web_get_birth_state_ptr === "function" &&
          typeof Module._web_get_birth_state_len === "function" &&
          typeof Module._web_get_player_state_ptr === "function" &&
          typeof Module._web_get_player_state_len === "function" &&
          typeof Module._web_get_tile_context_state_ptr === "function" &&
          typeof Module._web_get_tile_context_state_len === "function" &&
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
          typeof Module._web_get_menu_snapshot_retained === "function" &&
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
          typeof Module._web_get_prompt_kind === "function" &&
          typeof Module._web_get_prompt_more_hint === "function" &&
          typeof Module._web_get_prompt_text_ptr === "function" &&
          typeof Module._web_get_prompt_text_len === "function" &&
          typeof Module._web_get_prompt_attrs_ptr === "function" &&
          typeof Module._web_get_prompt_attrs_len === "function" &&
          typeof Module._web_get_prompt_revision === "function" &&
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
          openRangedTarget:
            typeof Module._web_open_ranged_target === "function"
              ? Module._web_open_ranged_target
              : null,
          toggleStealth:
            typeof Module._web_toggle_stealth === "function"
              ? Module._web_toggle_stealth
              : null,
          openSongMenu:
            typeof Module._web_open_song_menu === "function"
              ? Module._web_open_song_menu
              : null,
          saveGameAutomatically:
            typeof Module._web_request_autosave === "function"
              ? Module._web_request_autosave
              : typeof Module._web_save_game_automatically === "function"
                ? Module._web_save_game_automatically
              : null,
          targetMap:
            typeof Module._web_target_map === "function"
              ? Module._web_target_map
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
          getBirthStatePtr: Module._web_get_birth_state_ptr,
          getBirthStateLen: Module._web_get_birth_state_len,
          getBirthStateRevision:
            typeof Module._web_get_birth_state_revision === "function"
              ? Module._web_get_birth_state_revision
              : null,
          getBirthSubmitTextPtr:
            typeof Module._web_get_birth_submit_text_ptr === "function"
              ? Module._web_get_birth_submit_text_ptr
              : null,
          submitBirthForm:
            typeof Module._web_submit_birth_form === "function"
              ? Module._web_submit_birth_form
              : null,
          getCharacterSheetStatePtr:
            typeof Module._web_get_character_sheet_state_ptr === "function"
              ? Module._web_get_character_sheet_state_ptr
              : null,
          getCharacterSheetStateLen:
            typeof Module._web_get_character_sheet_state_len === "function"
              ? Module._web_get_character_sheet_state_len
              : null,
          getCharacterSheetStateRevision:
            typeof Module._web_get_character_sheet_state_revision === "function"
              ? Module._web_get_character_sheet_state_revision
              : null,
          getPlayerStatePtr: Module._web_get_player_state_ptr,
          getPlayerStateLen: Module._web_get_player_state_len,
          getTileContextStatePtr: Module._web_get_tile_context_state_ptr,
          getTileContextStateLen: Module._web_get_tile_context_state_len,
          executeTileContextAction:
            typeof Module._web_execute_tile_context_action === "function"
              ? Module._web_execute_tile_context_action
              : null,
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
          getMenuSnapshotRetained: Module._web_get_menu_snapshot_retained,
          getMenuDetailsPtr: Module._web_get_menu_details_ptr,
          getMenuDetailsLen: Module._web_get_menu_details_len,
          getMenuDetailsAttrsPtr: Module._web_get_menu_details_attrs_ptr,
          getMenuDetailsAttrsLen: Module._web_get_menu_details_attrs_len,
          getMenuSummaryPtr: Module._web_get_menu_summary_ptr,
          getMenuSummaryLen: Module._web_get_menu_summary_len,
          getMenuSummaryAttrsPtr: Module._web_get_menu_summary_attrs_ptr,
          getMenuSummaryAttrsLen: Module._web_get_menu_summary_attrs_len,
          getMenuSummaryRows:
            typeof Module._web_get_menu_summary_rows === "function"
              ? Module._web_get_menu_summary_rows
              : null,
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
          getPromptKind: Module._web_get_prompt_kind,
          getPromptMoreHint: Module._web_get_prompt_more_hint,
          getPromptTextPtr: Module._web_get_prompt_text_ptr,
          getPromptTextLen: Module._web_get_prompt_text_len,
          getPromptAttrsPtr: Module._web_get_prompt_attrs_ptr,
          getPromptAttrsLen: Module._web_get_prompt_attrs_len,
          getPromptRevision: Module._web_get_prompt_revision,
          getSoundCount:
            typeof Module._web_get_sound_count === "function"
              ? Module._web_get_sound_count
              : null,
          getSoundNamePtr:
            typeof Module._web_get_sound_name_ptr === "function"
              ? Module._web_get_sound_name_ptr
              : null,
          getSoundNameLen:
            typeof Module._web_get_sound_name_len === "function"
              ? Module._web_get_sound_name_len
              : null,
          popSoundEvent:
            typeof Module._web_pop_sound_event === "function"
              ? Module._web_pop_sound_event
              : null,
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
        if (
          typeof api.getSoundCount === "function" &&
          typeof api.getSoundNamePtr === "function" &&
          typeof api.getSoundNameLen === "function"
        ) {
          soundPlayer = createWebSoundPlayer({
            module: Module,
            getSoundCount: api.getSoundCount,
            getSoundNamePtr: api.getSoundNamePtr,
            getSoundNameLen: api.getSoundNameLen,
            noiseSamplePath: FRONTEND_NOISE_SAMPLE_PATH,
          });
          soundPlayer.bindUnlockGestures(document);
        }

        setStatusText("Running (-mweb). Tap or click the map to travel. Pinch or wheel to zoom. The backpack button opens inventory.");
        bindInput();
        bindResponsiveShellInput();
        bindFloatingActionInput();
        bindMenuPointerInput();
        bindOverlayMenuInput();
        bindOverlayBirthTextInput();
        bindOverlayBirthAhwInput();
        bindOverlayBirthStatsInput();
        bindOverlayCharacterSkillInput();
        bindOverlayCharacterSheetInput();
        bindOverlayTileContextInput();
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
        if (heap) {
          readBirthState(heap);
          readCharacterSheetState(heap);
          overlayActive = updateOverlaySemantic(heap);
        }
        followPlayerInViewport(overlayActive, true);
      }
    });

})();
