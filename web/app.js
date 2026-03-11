(() => {
    /* ==========================================================================
     * Setup And Shared Helpers
     * Reads globally provided helper utilities and caches core DOM references.
     * ========================================================================== */

    const {
      clamp,
      renderColoredText,
      renderSideText,
      setHtmlIfChanged,
    } = globalThis.WebHelpers;

    const statusEl = document.getElementById("status");
    const mapWrapEl = document.querySelector(".map-wrap");
    const mapCanvas = document.getElementById("map");
    const overlayModalEl = document.getElementById("overlay-modal");
    const sideEl = document.getElementById("side");
    const logEl = document.getElementById("log");
    const WEB_BUILD_TAG = "20260312-3";
    const PERSIST_APEX_DIR = "/persist/apex";
    const PERSIST_SAVE_DIR = "/persist/save-web";
    const PERSIST_USER_DIR = "/persist/user";
    const PERSIST_DATA_DIR = "/persist/data";

    /* ==========================================================================
     * Rendering Constants
     * Defines fixed values used by map drawing, cell formats, and UX behavior.
     * ========================================================================== */

    const ctx = mapCanvas.getContext("2d", { alpha: false });

    const WEB_CELL_EMPTY = 0;
    const WEB_CELL_TEXT = 1;
    const WEB_CELL_PICT = 2;
    const MAP_TILE_SCALE = 2;
    const MAP_DEFAULT_ZOOM = 3;
    const MAP_MIN_ZOOM = 0.5;
    const MAP_MAX_ZOOM = 6;
    const MAP_FOLLOW_EDGE_RATIO = 0.30;

    const WEB_FLAG_ALERT = 0x01;
    const WEB_FLAG_GLOW = 0x02;
    const WEB_FLAG_FG_PICT = 0x04;
    const WEB_FLAG_BG_PICT = 0x08;

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
    let mapZoom = MAP_DEFAULT_ZOOM;
    let lastPlayerMapX = -1;
    let lastPlayerMapY = -1;

    const iconMeta = {
      alertAttr: 0,
      alertChar: 0,
      glowAttr: 0,
      glowChar: 0,
    };
    const utf8Decoder = new TextDecoder("utf-8");

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

    // Returns a byte slice from wasm memory for per-character color attributes.
    function readBytes(heap, ptr, len) {
      if (!ptr || !len || len <= 0) return null;
      const end = ptr + len;
      if (ptr < 0 || end > heap.length) return null;
      return heap.subarray(ptr, end);
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
     * Camera And FX Utilities
     * Manages viewport fitting, focus-follow behavior, and transient FX timing.
     * ========================================================================== */

    // Restricts user zoom to the supported range.
    function clampMapZoom(zoom) {
      return Math.min(MAP_MAX_ZOOM, Math.max(MAP_MIN_ZOOM, zoom));
    }

    // Updates zoom level and recomputes the canvas CSS size.
    function setMapZoom(nextZoom) {
      mapZoom = clampMapZoom(nextZoom);
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
    function drawMap(mapPtr, fxPtr, fxCols, fxRows, fxBlocked, heap) {
      const drawCols = Math.max(0, mapCols);
      const drawRows = Math.max(0, mapRows);
      const cellCount = drawCols * drawRows;
      const currentCells = new Array(cellCount);
      const facingRight = new Uint8Array(cellCount);
      const actorEntries = [];
      const currentActorPositions = new Set();
      const mapWorldX = api ? api.getMapWorldX() : 0;
      const mapWorldY = api ? api.getMapWorldY() : 0;
      const playerMapX = api ? api.getPlayerMapX() : -1;
      const playerMapY = api ? api.getPlayerMapY() : -1;
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
        }
      }

      prevPlayerWorldX = playerWorldX;

      const nextActorFacing = new Map();
      for (const actor of actorEntries) {
        nextActorFacing.set(actor.posKey, facingRight[actor.idx] ? 1 : 0);
      }
      prevActorFacing = nextActorFacing;
      prevActorPositions = new Set(nextActorFacing.keys());

      fitMapToViewport();
      return { drawCols, drawRows };
    }

    /* ==========================================================================
     * Semantic Panel Rendering
     * Renders overlays, side panel, and log from semantic text buffers.
     * ========================================================================== */

    // Updates modal overlay text from semantic wasm buffers.
    function updateOverlaySemantic(heap) {
      if (!api) return false;

      const mode = api.getOverlayMode();
      if (!mode) {
        overlayModalEl.style.display = "none";
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
        overlayModalEl.style.display = "none";
        setHtmlIfChanged(overlayModalEl, "");
        return false;
      }

      overlayModalEl.style.display = "block";
      setHtmlIfChanged(overlayModalEl, renderColoredText(text, attrs, 1));
      return true;
    }

    // Updates side and log panels using semantic text and color attributes.
    function updateSemanticPanels(heap) {
      if (!api) return;
      const sidePtr = api.getSideTextPtr();
      const sideLen = api.getSideTextLen();
      const logPtr = api.getLogTextPtr();
      const logLen = api.getLogTextLen();
      const logAttrPtr = api.getLogAttrsPtr();
      const logAttrLen = api.getLogAttrsLen();

      const sideText = readUtf8(heap, sidePtr, sideLen).trimEnd();
      const logText = readUtf8(heap, logPtr, logLen).trimEnd();
      const logAttrs = readBytes(heap, logAttrPtr, logAttrLen);

      const sideHtml = sideText ? renderSideText(sideText) : "<span class=\"term-c2\">(side panel empty)</span>";
      const logHtml = logText ? renderColoredText(logText, logAttrs, 1) : "<span class=\"term-c2\">(message panel empty)</span>";

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
      if (!forceRedraw && frameId === lastFrameId && !mapFxActive) return;
      forceRedraw = false;
      lastFrameId = frameId;

      const cols = api.getCols();
      const rows = api.getRows();
      const heap = getHeapU8();
      if (!heap) return;

      if (cols <= 0 || rows <= 0) return;

      refreshLayoutMetadata(cols, rows);
      refreshIconMetadata();

      const overlayActive = updateOverlaySemantic(heap);
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
      drawMap(mapPtr, fxPtr, fxCols, fxRows, blockFx, heap);
      updateSemanticPanels(heap);
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

      document.addEventListener("keydown", (ev) => {
        if (!api) return;

        if (Object.hasOwn(keyMap, ev.key)) {
          pushAscii(keyMap[ev.key]);
          ev.preventDefault();
          return;
        }

        if (Object.hasOwn(codeMap, ev.code)) {
          pushAscii(codeMap[ev.code]);
          ev.preventDefault();
          return;
        }

        if (ev.key.length === 1 && !ev.ctrlKey && !ev.metaKey) {
          pushAscii(ev.key.charCodeAt(0));
          ev.preventDefault();
        }
      });
    }

    // Binds mouse/touch interactions for map zooming and drag-to-pan.
    function bindMapZoomInput() {
      mapWrapEl.addEventListener("wheel", (ev) => {
        if (!mapCanvas.width || !mapCanvas.height) return;
        ev.preventDefault();
        const factor = Math.exp((-ev.deltaY) * 0.0015);
        setMapZoom(mapZoom * factor);
      }, { passive: false });

      const touchPoints = new Map();
      let pinchBaseDistance = 0;
      let pinchBaseZoom = mapZoom;
      let dragPointerId = null;
      let dragLastX = 0;
      let dragLastY = 0;

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
            startDrag(ev.pointerId, ev.clientX, ev.clientY);
          } else if (touchPoints.size === 2) {
            stopDrag();
            updatePinchBase();
          }
          ev.preventDefault();
        } else if (ev.pointerType === "mouse") {
          if (ev.button !== 0) return;
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
              setMapZoom(pinchBaseZoom * (distance / pinchBaseDistance));
            }
            ev.preventDefault();
            return;
          }

          if (touchPoints.size === 1 && dragPointerId === ev.pointerId) {
            updateDrag(ev.clientX, ev.clientY);
            ev.preventDefault();
          }
          return;
        }

        if (ev.pointerType === "mouse" && dragPointerId === ev.pointerId) {
          updateDrag(ev.clientX, ev.clientY);
          ev.preventDefault();
        }
      }, { passive: false });

      const clearPointer = (ev) => {
        stopDrag(ev.pointerId);
        if (touchPoints.has(ev.pointerId)) {
          touchPoints.delete(ev.pointerId);
          resetPinch();
          resumeSingleTouchDrag();
        }
      };

      mapWrapEl.addEventListener("pointerup", clearPointer);
      mapWrapEl.addEventListener("pointercancel", clearPointer);
      mapWrapEl.addEventListener("lostpointercapture", clearPointer);
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
    tileImage.src = `./lib/16x16_microchasm.png?v=${WEB_BUILD_TAG}`;

    window.Module = {
      locateFile: (path) => `./lib/${path}?v=${WEB_BUILD_TAG}`,
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

        if (!FS || !IDBFS) {
          console.error("Emscripten FS runtime is not available");
          return;
        }

        const ensureDir = (path) => {
          try { FS.mkdir(path); } catch (_) {}
        };

        ensureDir("/persist");
        FS.mount(IDBFS, {}, "/persist");

        Module.addRunDependency("syncfs");
        FS.syncfs(true, (err) => {
          if (err) {
            console.error("syncfs init failed:", err);
          } else {
            ensureDir(PERSIST_APEX_DIR);
            ensureDir(PERSIST_SAVE_DIR);
            ensureDir(PERSIST_USER_DIR);
            ensureDir(PERSIST_DATA_DIR);
            FS.syncfs(false, (flushErr) => {
              if (flushErr) console.error("syncfs flush failed:", flushErr);
            });
          }
          Module.removeRunDependency("syncfs");
        });
      }],
      print: (text) => console.log(text),
      printErr: (text) => console.error(text),
      setStatus: (text) => { statusEl.textContent = text; },
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
          typeof Module._web_get_map_world_x === "function" &&
          typeof Module._web_get_map_world_y === "function" &&
          typeof Module._web_get_map_cells_ptr === "function" &&
          typeof Module._web_get_player_map_x === "function" &&
          typeof Module._web_get_player_map_y === "function" &&
          typeof Module._web_get_icon_alert_attr === "function" &&
          typeof Module._web_get_icon_alert_char === "function" &&
          typeof Module._web_get_icon_glow_attr === "function" &&
          typeof Module._web_get_icon_glow_char === "function" &&
          typeof Module._web_get_frame_id === "function" &&
          typeof Module._web_push_key === "function" &&
          typeof Module._web_get_side_text_ptr === "function" &&
          typeof Module._web_get_side_text_len === "function" &&
          typeof Module._web_get_log_text_ptr === "function" &&
          typeof Module._web_get_log_text_len === "function" &&
          typeof Module._web_get_log_attrs_ptr === "function" &&
          typeof Module._web_get_log_attrs_len === "function" &&
          typeof Module._web_get_overlay_mode === "function" &&
          typeof Module._web_get_overlay_col === "function" &&
          typeof Module._web_get_overlay_row === "function" &&
          typeof Module._web_get_overlay_cols === "function" &&
          typeof Module._web_get_overlay_rows === "function" &&
          typeof Module._web_get_overlay_text_ptr === "function" &&
          typeof Module._web_get_overlay_text_len === "function" &&
          typeof Module._web_get_overlay_attrs_ptr === "function" &&
          typeof Module._web_get_overlay_attrs_len === "function" &&
          typeof Module._web_get_fx_cells_ptr === "function" &&
          typeof Module._web_get_fx_cells_cols === "function" &&
          typeof Module._web_get_fx_cells_rows === "function" &&
          typeof Module._web_get_fx_cells_seq === "function" &&
          typeof Module._web_get_fx_delay_seq === "function" &&
          typeof Module._web_get_fx_delay_msec === "function";

        if (!hasAPI) {
          statusEl.textContent = "WASM started, but web_* API missing";
          return;
        }

        api = {
          getCols: Module._web_get_cols,
          getRows: Module._web_get_rows,
          getCellsPtr: Module._web_get_cells_ptr,
          getCellStride: Module._web_get_cell_stride,
          getTileWid: Module._web_get_tile_wid,
          getTileHgt: Module._web_get_tile_hgt,
          getMapCols: Module._web_get_map_cols,
          getMapRows: Module._web_get_map_rows,
          getMapWorldX: Module._web_get_map_world_x,
          getMapWorldY: Module._web_get_map_world_y,
          getMapCellsPtr: Module._web_get_map_cells_ptr,
          getPlayerMapX: Module._web_get_player_map_x,
          getPlayerMapY: Module._web_get_player_map_y,
          getAlertAttr: Module._web_get_icon_alert_attr,
          getAlertChar: Module._web_get_icon_alert_char,
          getGlowAttr: Module._web_get_icon_glow_attr,
          getGlowChar: Module._web_get_icon_glow_char,
          getFrameId: Module._web_get_frame_id,
          pushKey: Module._web_push_key,
          getSideTextPtr: Module._web_get_side_text_ptr,
          getSideTextLen: Module._web_get_side_text_len,
          getLogTextPtr: Module._web_get_log_text_ptr,
          getLogTextLen: Module._web_get_log_text_len,
          getLogAttrsPtr: Module._web_get_log_attrs_ptr,
          getLogAttrsLen: Module._web_get_log_attrs_len,
          getOverlayMode: Module._web_get_overlay_mode,
          getOverlayCol: Module._web_get_overlay_col,
          getOverlayRow: Module._web_get_overlay_row,
          getOverlayCols: Module._web_get_overlay_cols,
          getOverlayRows: Module._web_get_overlay_rows,
          getOverlayTextPtr: Module._web_get_overlay_text_ptr,
          getOverlayTextLen: Module._web_get_overlay_text_len,
          getOverlayAttrsPtr: Module._web_get_overlay_attrs_ptr,
          getOverlayAttrsLen: Module._web_get_overlay_attrs_len,
          getFxCellsPtr: Module._web_get_fx_cells_ptr,
          getFxCellsCols: Module._web_get_fx_cells_cols,
          getFxCellsRows: Module._web_get_fx_cells_rows,
          getFxCellsSeq: Module._web_get_fx_cells_seq,
          getFxDelaySeq: Module._web_get_fx_delay_seq,
          getFxDelayMsec: Module._web_get_fx_delay_msec,
        };

        cellStride = api.getCellStride();
        tileSrcW = api.getTileWid();
        tileSrcH = api.getTileHgt();
        tileW = tileSrcW * MAP_TILE_SCALE;
        tileH = tileSrcH * MAP_TILE_SCALE;

        statusEl.textContent = "Running (-mweb). Click page and type to play. Wheel/pinch map to zoom.";
        bindInput();
        bindMapZoomInput();
        fitMapToViewport();

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
      fitMapToViewport();
      if (api) {
        const heap = getHeapU8();
        let overlayActive = false;
        if (heap) overlayActive = updateOverlaySemantic(heap);
        followPlayerInViewport(overlayActive, true);
      }
    });

})();
