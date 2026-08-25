// =============================================================================
//  Caesura (AmeKAG) — web/story-map.mjs
//  Interactive Visual Story Map & Graph Renderer (Unified Semantic Layer)
// =============================================================================

/**
 * Parses JSON topology produced by `kag_semantic.lua flow --format json`
 * into a structured visual graph representation.
 * @param {Object} rawJson
 * @returns {Object} { scenes: Array<{ name, nodes, edges, diagnostics }> }
 */
export function parseStoryGraph(rawJson) {
  if (!rawJson || typeof rawJson !== 'object') {
    return { scenes: [] };
  }

  const scenes = [];
  for (const [sceneName, data] of Object.entries(rawJson)) {
    const nodes = [];
    const edges = [];

    if (data && data.nodes) {
      for (const [labelName, lbl] of Object.entries(data.nodes)) {
        nodes.push({
          id: labelName,
          title: lbl.title || labelName,
          line: lbl.line || 1,
          col: lbl.col || 1,
          isEntry: !!lbl.is_entry,
        });
      }
    }

    if (data && Array.isArray(data.edges)) {
      for (const e of data.edges) {
        edges.push({
          from: e.from,
          to: e.to,
          type: e.type || 'jump', // choice | jump | call
          text: e.text || '',
          line: e.line || 1,
        });
      }
    }

    scenes.push({
      name: sceneName,
      nodes,
      edges,
      diagnostics: Array.isArray(data.diagnostics) ? data.diagnostics : [],
    });
  }

  return { scenes };
}

/**
 * Computes simple 2D layout coordinates for scene nodes.
 * @param {Array} nodes
 * @returns {Map<string, { x: number, y: number, width: number, height: number }>}
 */
export function layoutNodes(nodes) {
  const layout = new Map();
  const nodeWidth = 180;
  const nodeHeight = 60;
  const strideY = 100;

  nodes.forEach((n, idx) => {
    layout.set(n.id, {
      x: 50,
      y: 50 + idx * strideY,
      width: nodeWidth,
      height: nodeHeight,
    });
  });

  return layout;
}

/**
 * Normalizes script file path for comparison (forward slashes, no leading ./).
 * @param {string} p
 * @returns {string}
 */
function normalizePath(p) {
  if (!p || typeof p !== 'string') return '';
  return p.replace(/\\/g, '/').replace(/^\.\//, '');
}

/**
 * Finds the active node in a scene graph corresponding to a given source line.
 * Supports both `findActiveNodeByLine(sceneGraph, file, line)` and `findActiveNodeByLine(sceneGraph, line)`.
 * @param {Object} sceneGraph - Scene graph object with `{ name, nodes }`
 * @param {string|number} fileOrLine - File path string or line number if file omitted
 * @param {number} [lineNum] - Source line number if file was provided
 * @returns {Object|null} Matching node or null
 */
export function findActiveNodeByLine(sceneGraph, fileOrLine, lineNum) {
  if (!sceneGraph || !Array.isArray(sceneGraph.nodes) || sceneGraph.nodes.length === 0) {
    return null;
  }

  let file = null;
  let line = 1;

  if (typeof fileOrLine === 'number') {
    line = fileOrLine;
  } else if (typeof fileOrLine === 'string') {
    file = fileOrLine;
    line = typeof lineNum === 'number' ? lineNum : 1;
  } else if (typeof lineNum === 'number') {
    line = lineNum;
  }

  // If a file is specified, check whether it matches this scene's name
  if (file && sceneGraph.name) {
    const normFile = normalizePath(file);
    const normScene = normalizePath(sceneGraph.name);
    if (normFile !== normScene && !normFile.endsWith('/' + normScene) && !normScene.endsWith('/' + normFile)) {
      return null;
    }
  }

  if (line <= 0) return null;

  // Sort nodes by line ascending
  const sortedNodes = [...sceneGraph.nodes].sort((a, b) => (a.line || 1) - (b.line || 1));

  let active = null;
  for (const n of sortedNodes) {
    if ((n.line || 1) <= line) {
      active = n;
    } else {
      break;
    }
  }

  // If line is before all nodes, fall back to first node if line >= 1
  if (!active && sortedNodes.length > 0 && line >= 1) {
    active = sortedNodes[0];
  }

  return active;
}

/**
 * Computes viewBox dimensions and offset to center the viewport on a specific node.
 * @param {Map|Array} layoutOrNodes - Layout Map or Node list
 * @param {string} targetNodeId - Target node ID (e.g. '*start')
 * @param {{ width: number, height: number }} [viewportSize={ width: 600, height: 400 }]
 * @param {number} [zoom=1]
 * @returns {{ minX: number, minY: number, width: number, height: number, viewBoxString: string }|null}
 */
export function computeViewBox(layoutOrNodes, targetNodeId, viewportSize = { width: 600, height: 400 }, zoom = 1) {
  let layout = null;
  if (layoutOrNodes instanceof Map) {
    layout = layoutOrNodes;
  } else if (Array.isArray(layoutOrNodes)) {
    layout = layoutNodes(layoutOrNodes);
  } else {
    return null;
  }

  const box = layout.get(targetNodeId);
  if (!box) return null;

  const viewWidth = Math.max(50, viewportSize.width / (zoom > 0 ? zoom : 1));
  const viewHeight = Math.max(50, viewportSize.height / (zoom > 0 ? zoom : 1));

  const centerX = box.x + box.width / 2;
  const centerY = box.y + box.height / 2;

  const minX = Math.round(centerX - viewWidth / 2);
  const minY = Math.round(centerY - viewHeight / 2);
  const width = Math.round(viewWidth);
  const height = Math.round(viewHeight);

  return {
    minX,
    minY,
    width,
    height,
    viewBoxString: `${minX} ${minY} ${width} ${height}`,
  };
}

/**
 * Updates an SVG element's viewBox to center on a target node.
 * @param {SVGElement|HTMLElement} svgElement
 * @param {string} nodeId
 * @param {{ width: number, height: number }} [viewportSize]
 * @param {number} [zoom=1]
 * @returns {boolean} Whether centering was successful
 */
export function centerStoryMapOnNode(svgElement, nodeId, viewportSize = { width: 600, height: 400 }, zoom = 1) {
  if (!svgElement) return false;
  const nodeEl = svgElement.querySelector(`[data-id="${nodeId}"]`);
  if (!nodeEl) return false;

  const rectEl = nodeEl.querySelector('rect') || nodeEl;
  const x = parseFloat(rectEl.getAttribute('x') || '0');
  const y = parseFloat(rectEl.getAttribute('y') || '0');
  const w = parseFloat(rectEl.getAttribute('width') || '180');
  const h = parseFloat(rectEl.getAttribute('height') || '60');

  const viewWidth = viewportSize.width / (zoom > 0 ? zoom : 1);
  const viewHeight = viewportSize.height / (zoom > 0 ? zoom : 1);
  const minX = Math.round(x + w / 2 - viewWidth / 2);
  const minY = Math.round(y + h / 2 - viewHeight / 2);
  const width = Math.round(viewWidth);
  const height = Math.round(viewHeight);

  svgElement.setAttribute('viewBox', `${minX} ${minY} ${width} ${height}`);
  return true;
}

/**
 * Updates DOM classes for the currently executing node in an rendered SVG element.
 * @param {SVGElement|HTMLElement} svgElement
 * @param {{ nodeId?: string, line?: number, isPaused?: boolean }} executionState
 */
export function setStoryMapExecutionNode(svgElement, { nodeId, line, isPaused = false } = {}) {
  if (!svgElement) return;

  // Clear previous executing state
  const prevExecuting = svgElement.querySelectorAll('.node-box-executing, .node-box-paused, [data-executing="true"]');
  prevExecuting.forEach((el) => {
    el.classList.remove('node-box-executing', 'node-box-paused');
    el.removeAttribute('data-executing');
    el.removeAttribute('data-paused');
  });

  let targetId = nodeId;
  if (!targetId && typeof line === 'number') {
    const nodes = Array.from(svgElement.querySelectorAll('.story-node'));
    let matchedNode = null;
    let maxLine = -1;
    for (const el of nodes) {
      const nodeLine = parseInt(el.getAttribute('data-line') || '1', 10);
      if (nodeLine <= line && nodeLine > maxLine) {
        maxLine = nodeLine;
        matchedNode = el;
      }
    }
    if (matchedNode) {
      targetId = matchedNode.getAttribute('data-id');
    }
  }

  if (targetId) {
    const nodeEl = svgElement.querySelector(`[data-id="${targetId}"]`);
    if (nodeEl) {
      nodeEl.setAttribute('data-executing', 'true');
      if (isPaused) nodeEl.setAttribute('data-paused', 'true');
      const rect = nodeEl.querySelector('rect');
      if (rect) {
        rect.classList.add('node-box-executing');
        if (isPaused) rect.classList.add('node-box-paused');
      }
    }
  }
}

/**
 * Updates DOM classes for the currently active/selected node in an rendered SVG element.
 * @param {SVGElement|HTMLElement} svgElement
 * @param {{ nodeId?: string, line?: number }} activeState
 */
export function setStoryMapActiveNode(svgElement, { nodeId, line } = {}) {
  if (!svgElement) return;

  // Clear previous active state
  const prevActive = svgElement.querySelectorAll('.node-box-active, [data-active="true"]');
  prevActive.forEach((el) => {
    el.classList.remove('node-box-active');
    el.removeAttribute('data-active');
  });

  let targetId = nodeId;
  if (!targetId && typeof line === 'number') {
    const nodes = Array.from(svgElement.querySelectorAll('.story-node'));
    let matchedNode = null;
    let maxLine = -1;
    for (const el of nodes) {
      const nodeLine = parseInt(el.getAttribute('data-line') || '1', 10);
      if (nodeLine <= line && nodeLine > maxLine) {
        maxLine = nodeLine;
        matchedNode = el;
      }
    }
    if (matchedNode) {
      targetId = matchedNode.getAttribute('data-id');
    }
  }

  if (targetId) {
    const nodeEl = svgElement.querySelector(`[data-id="${targetId}"]`);
    if (nodeEl) {
      nodeEl.setAttribute('data-active', 'true');
      const rect = nodeEl.querySelector('rect');
      if (rect) {
        rect.classList.add('node-box-active');
      }
    }
  }
}

/**
 * Attaches interactive click listener to the story map SVG / container.
 * Emits custom event `story-node-select` and calls `onSelect({ file, line, label })`.
 * @param {SVGElement|HTMLElement} rootElement
 * @param {Function} [onSelect] - Callback receiving `{ file, line, label }`
 * @returns {Function} Cleanup function to unbind listeners
 */
export function bindStoryMapEvents(rootElement, onSelect) {
  if (!rootElement || typeof rootElement.addEventListener !== 'function') {
    return () => {};
  }

  const handleClick = (e) => {
    const target = e.target;
    if (!target) return;

    // Find closest .story-node element
    const nodeEl = target.closest ? target.closest('.story-node') : null;
    if (!nodeEl) return;

    const label = nodeEl.getAttribute('data-id') || '';
    const line = parseInt(nodeEl.getAttribute('data-line') || '1', 10);
    const file = nodeEl.getAttribute('data-file') || rootElement.getAttribute('data-file') || '';

    const payload = { file, line, label };

    // Dispatch custom event
    if (typeof CustomEvent === 'function' && typeof rootElement.dispatchEvent === 'function') {
      const customEvent = new CustomEvent('story-node-select', {
        detail: payload,
        bubbles: true,
        composed: true,
      });
      rootElement.dispatchEvent(customEvent);
    }

    // Call callback if provided
    if (typeof onSelect === 'function') {
      onSelect(payload);
    }
  };

  rootElement.addEventListener('click', handleClick);

  return () => {
    rootElement.removeEventListener('click', handleClick);
  };
}

/**
 * Renders SVG string representing the story graph.
 * @param {Object} sceneGraph
 * @param {Object} [options={}] - Render options (activeNodeId, executingNodeId, executingLine, isPaused, viewBox)
 * @returns {string} SVG markup
 */
export function renderStoryGraphToSvg(sceneGraph, options = {}) {
  if (!sceneGraph || !Array.isArray(sceneGraph.nodes)) {
    return '<svg xmlns="http://www.w3.org/2000/svg" width="100" height="100"></svg>';
  }

  const {
    activeNodeId,
    activeLine,
    executingNodeId,
    executingLine,
    isPaused = false,
    viewBox: customViewBox,
  } = options;

  let effectiveActiveId = activeNodeId;
  if (!effectiveActiveId && typeof activeLine === 'number') {
    const activeNode = findActiveNodeByLine(sceneGraph, activeLine);
    if (activeNode) effectiveActiveId = activeNode.id;
  }

  let effectiveExecutingId = executingNodeId;
  if (!effectiveExecutingId && typeof executingLine === 'number') {
    const execNode = findActiveNodeByLine(sceneGraph, executingLine);
    if (execNode) effectiveExecutingId = execNode.id;
  }

  const layout = layoutNodes(sceneGraph.nodes);
  const svgHeight = Math.max(200, sceneGraph.nodes.length * 100 + 100);
  const viewBoxStr = customViewBox || `0 0 600 ${svgHeight}`;
  const sceneName = sceneGraph.name || '';

  const elements = [
    `<svg xmlns="http://www.w3.org/2000/svg" viewBox="${viewBoxStr}" class="caesura-story-map" data-file="${sceneName}">`,
    `<style>
      .node-box { fill: #1e1e2e; stroke: #89b4fa; stroke-width: 2; rx: 8; transition: stroke 0.2s, stroke-width 0.2s; }
      .node-box-entry { stroke: #a6e3a1; }
      .node-box-active { stroke: #cba6f7; stroke-width: 3; filter: drop-shadow(0 0 6px rgba(203, 166, 247, 0.6)); }
      .node-box-executing { stroke: #f9e2af; stroke-width: 3; filter: drop-shadow(0 0 8px rgba(249, 226, 175, 0.8)); }
      .node-box-paused { stroke: #f38ba8; stroke-dasharray: 4 2; }
      .node-text { fill: #cdd6f4; font-family: sans-serif; font-size: 13px; dominant-baseline: middle; pointer-events: none; }
      .edge-line { stroke: #6c7086; stroke-width: 1.5; stroke-dasharray: 4,4; }
      .edge-choice { stroke: #fab387; stroke-dasharray: none; }
      .story-node { cursor: pointer; }
      .story-node:hover .node-box { stroke: #b4befe; }
    </style>`,
  ];

  // Draw edges
  if (Array.isArray(sceneGraph.edges)) {
    for (const e of sceneGraph.edges) {
      const fromBox = layout.get(e.from);
      const toBox = layout.get(e.to);
      if (fromBox && toBox) {
        const x1 = fromBox.x + fromBox.width / 2;
        const y1 = fromBox.y + fromBox.height;
        const x2 = toBox.x + toBox.width / 2;
        const y2 = toBox.y;
        const isChoice = e.type === 'choice';
        // class list is assembled from present flags only: an absent flag must
        // not leave a stray separator behind (class="edge-line " breaks any
        // exact-match consumer, CSS selector debugging and DOM assertions).
        const edgeClasses = ['edge-line', isChoice ? 'edge-choice' : '']
          .filter(Boolean).join(' ');
        elements.push(
          `<path d="M ${x1} ${y1} C ${x1} ${(y1 + y2) / 2}, ${x2} ${(y1 + y2) / 2}, ${x2} ${y2}" class="${edgeClasses}" />`
        );
      }
    }
  }

  // Draw nodes
  for (const n of sceneGraph.nodes) {
    const box = layout.get(n.id);
    if (!box) continue;
    const isEntry = n.isEntry;
    const isActive = n.id === effectiveActiveId;
    const isExecuting = n.id === effectiveExecutingId;

    const rectClasses = [
      'node-box',
      isEntry ? 'node-box-entry' : '',
      isActive ? 'node-box-active' : '',
      isExecuting ? 'node-box-executing' : '',
      isExecuting && isPaused ? 'node-box-paused' : '',
    ].filter(Boolean).join(' ');

    // Same rule for the group: only present state classes/attributes are
    // emitted, joined by exactly one space. The old template interpolated
    // empty strings, producing class="story-node  " and '<g ... >' with
    // stray gaps — cosmetically invisible but it made every exact class
    // assertion (and any consumer comparing className) wrong.
    const nodeClasses = [
      'story-node',
      isActive ? 'node-active' : '',
      isExecuting ? 'node-executing' : '',
    ].filter(Boolean).join(' ');
    const nodeAttrs = [
      `class="${nodeClasses}"`,
      `data-id="${n.id}"`,
      `data-line="${n.line}"`,
      `data-file="${sceneName}"`,
      isActive ? 'data-active="true"' : '',
      isExecuting ? 'data-executing="true"' : '',
    ].filter(Boolean).join(' ');

    elements.push(
      `<g ${nodeAttrs}>`,
      `  <rect x="${box.x}" y="${box.y}" width="${box.width}" height="${box.height}" class="${rectClasses}" />`,
      `  <text x="${box.x + 12}" y="${box.y + box.height / 2}" class="node-text">${n.title} (L${n.line})</text>`,
      `</g>`
    );
  }

  elements.push('</svg>');
  return elements.join('\n');
}
