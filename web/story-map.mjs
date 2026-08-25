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
 * Renders SVG string representing the story graph.
 * @param {Object} sceneGraph
 * @returns {string} SVG markup
 */
export function renderStoryGraphToSvg(sceneGraph) {
  if (!sceneGraph || !Array.isArray(sceneGraph.nodes)) {
    return '<svg xmlns="http://www.w3.org/2000/svg" width="100" height="100"></svg>';
  }

  const layout = layoutNodes(sceneGraph.nodes);
  const svgHeight = Math.max(200, sceneGraph.nodes.length * 100 + 100);
  const elements = [
    `<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 600 ${svgHeight}" class="caesura-story-map">`,
    `<style>
      .node-box { fill: #1e1e2e; stroke: #89b4fa; stroke-width: 2; rx: 8; }
      .node-box-entry { stroke: #a6e3a1; }
      .node-text { fill: #cdd6f4; font-family: sans-serif; font-size: 13px; dominant-baseline: middle; }
      .edge-line { stroke: #6c7086; stroke-width: 1.5; stroke-dasharray: 4,4; }
      .edge-choice { stroke: #fab387; stroke-dasharray: none; }
    </style>`,
  ];

  // Draw nodes
  for (const n of sceneGraph.nodes) {
    const box = layout.get(n.id);
    if (!box) continue;
    const isEntry = n.isEntry;
    elements.push(
      `<g class="story-node" data-id="${n.id}" data-line="${n.line}">`,
      `  <rect x="${box.x}" y="${box.y}" width="${box.width}" height="${box.height}" class="node-box ${isEntry ? 'node-box-entry' : ''}" />`,
      `  <text x="${box.x + 12}" y="${box.y + box.height / 2}" class="node-text">${n.title} (L${n.line})</text>`,
      `</g>`
    );
  }

  elements.push('</svg>');
  return elements.join('\n');
}
