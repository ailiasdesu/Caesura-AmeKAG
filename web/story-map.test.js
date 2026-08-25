// @vitest-environment jsdom
// =============================================================================
//  Caesura (AmeKAG) — web/story-map.test.js
//  Unit + DOM tests for the Web Story Map: parsing, layout, SVG generation,
//  bi-directional sync, line lookup and execution highlight.
//
//  Sprint 1 / t9 — consolidation + environment fix:
//    * web/test/story-map.test.mjs was merged in here. Two test files for one
//      module drifted apart (this file asserted class="story-node" while the
//      other only ever checked substrings), and the repo convention is flat
//      web/*.test.js. Every assertion from both files is preserved below; the
//      duplicated basic-render coverage is kept as two separate cases because
//      they use different fixtures (a minimal graph and the M3 sample graph).
//    * The merged file declares '@vitest-environment jsdom': the M3 cases use
//      document/MouseEvent, and running them under the default node
//      environment failed with "document is not defined". The pure-function
//      cases are environment-agnostic, so one jsdom file covers both.
// =============================================================================

import { describe, it, expect, vi, beforeEach } from 'vitest';
import {
  parseStoryGraph,
  layoutNodes,
  renderStoryGraphToSvg,
  findActiveNodeByLine,
  computeViewBox,
  centerStoryMapOnNode,
  setStoryMapExecutionNode,
  setStoryMapActiveNode,
  bindStoryMapEvents,
} from './story-map.mjs';

describe('Web Story Map (Unified Semantic Layer)', () => {
  it('parses empty or invalid input gracefully', () => {
    expect(parseStoryGraph(null)).toEqual({ scenes: [] });
    expect(parseStoryGraph({})).toEqual({ scenes: [] });
  });

  it('parses structured scene graph from JSON topology', () => {
    const mockJson = {
      'story.ks': {
        nodes: {
          '*start': { title: 'Opening', line: 10, is_entry: true },
          '*choice': { title: 'First Branch', line: 25 },
        },
        edges: [
          { from: '*start', to: '*choice', type: 'choice', text: 'Go forth' },
        ],
        diagnostics: [],
      },
    };

    const graph = parseStoryGraph(mockJson);
    expect(graph.scenes.length).toBe(1);
    expect(graph.scenes[0].name).toBe('story.ks');
    expect(graph.scenes[0].nodes.length).toBe(2);
    expect(graph.scenes[0].edges.length).toBe(1);
    expect(graph.scenes[0].edges[0].text).toBe('Go forth');
  });

  it('computes 2D layout boxes for nodes', () => {
    const nodes = [
      { id: '*node1', title: 'Node 1', line: 1 },
      { id: '*node2', title: 'Node 2', line: 20 },
    ];
    const layout = layoutNodes(nodes);
    expect(layout.has('*node1')).toBe(true);
    expect(layout.has('*node2')).toBe(true);
    expect(layout.get('*node2').y).toBeGreaterThan(layout.get('*node1').y);
  });

  it('renders SVG markup containing node rects and labels', () => {
    const sceneGraph = {
      name: 'test.ks',
      nodes: [
        { id: '*start', title: 'Start', line: 1, isEntry: true },
        { id: '*ending', title: 'Good End', line: 50 },
      ],
      edges: [],
    };

    const svg = renderStoryGraphToSvg(sceneGraph);
    expect(svg).toContain('<svg');
    // Exact class match: an unhighlighted node carries the single class
    // 'story-node' with no stray separator (t9 fixed the emitter, the
    // assertion was NOT relaxed).
    expect(svg).toContain('class="story-node"');
    expect(svg).toContain('data-id="*start"');
    expect(svg).toContain('data-line="1"');
    expect(svg).toContain('Good End (L50)');
    expect(svg).toContain('</svg>');
  });

  it('emits class lists and tags without stray separators (t9)', () => {
    // Regression guard for the class-assembly fix: absent state flags must not
    // leave empty slots behind, in either the <g> class list or the edge class.
    const sceneGraph = {
      name: 'test.ks',
      nodes: [
        { id: '*a', title: 'A', line: 1, isEntry: true },
        { id: '*b', title: 'B', line: 9 },
      ],
      edges: [{ from: '*a', to: '*b', type: 'jump' }],
    };
    const plain = renderStoryGraphToSvg(sceneGraph);
    expect(plain).toContain('class="story-node"');
    expect(plain).toContain('class="edge-line"');
    expect(plain).not.toContain('class="story-node "');
    expect(plain).not.toContain('class="edge-line "');
    expect(plain).not.toMatch(/<g\s{2,}/);
    expect(plain).not.toMatch(/\s+>/);

    // A choice edge and highlighted nodes still join with exactly one space.
    const highlighted = renderStoryGraphToSvg(sceneGraph, {
      activeNodeId: '*a',
      executingNodeId: '*b',
      edges: sceneGraph.edges,
    });
    expect(highlighted).toContain('class="story-node node-active"');
    expect(highlighted).toContain('class="story-node node-executing"');
    expect(highlighted).not.toMatch(/class="[^"]*\s{2,}[^"]*"/);
  });
});

describe('Web Story Map Bi-directional Sync & Inspection (M3)', () => {
  const sampleGraph = {
    name: 'assets/script/main.ks',
    nodes: [
      { id: '*start', title: 'Opening Scene', line: 1, isEntry: true },
      { id: '*choice_1', title: 'Decision Point A', line: 20, isEntry: false },
      { id: '*ending_good', title: 'True Ending', line: 55, isEntry: false },
    ],
    edges: [
      { from: '*start', to: '*choice_1', type: 'jump', line: 15 },
      { from: '*choice_1', to: '*ending_good', type: 'choice', text: 'Accept', line: 30 },
    ],
  };

  describe('findActiveNodeByLine', () => {
    it('returns null for empty or null graph', () => {
      expect(findActiveNodeByLine(null, 10)).toBeNull();
      expect(findActiveNodeByLine({ nodes: [] }, 10)).toBeNull();
    });

    it('returns the active node given line without file parameter', () => {
      expect(findActiveNodeByLine(sampleGraph, 1)?.id).toBe('*start');
      expect(findActiveNodeByLine(sampleGraph, 15)?.id).toBe('*start');
      expect(findActiveNodeByLine(sampleGraph, 20)?.id).toBe('*choice_1');
      expect(findActiveNodeByLine(sampleGraph, 40)?.id).toBe('*choice_1');
      expect(findActiveNodeByLine(sampleGraph, 55)?.id).toBe('*ending_good');
      expect(findActiveNodeByLine(sampleGraph, 100)?.id).toBe('*ending_good');
    });

    it('matches file path correctly with normalization (slashes and prefix)', () => {
      expect(findActiveNodeByLine(sampleGraph, 'assets/script/main.ks', 25)?.id).toBe('*choice_1');
      expect(findActiveNodeByLine(sampleGraph, 'assets\\script\\main.ks', 25)?.id).toBe('*choice_1');
      expect(findActiveNodeByLine(sampleGraph, './assets/script/main.ks', 25)?.id).toBe('*choice_1');
      expect(findActiveNodeByLine(sampleGraph, 'main.ks', 25)?.id).toBe('*choice_1');
    });

    it('returns null when file does not match the scene', () => {
      expect(findActiveNodeByLine(sampleGraph, 'other_scene.ks', 25)).toBeNull();
    });

    it('handles boundary conditions: line 0 or negative', () => {
      expect(findActiveNodeByLine(sampleGraph, 0)).toBeNull();
      expect(findActiveNodeByLine(sampleGraph, -5)).toBeNull();
    });
  });

  describe('renderStoryGraphToSvg with Highlights', () => {
    it('renders basic svg with nodes and edges', () => {
      const svg = renderStoryGraphToSvg(sampleGraph);
      expect(svg).toContain('<svg');
      expect(svg).toContain('data-file="assets/script/main.ks"');
      expect(svg).toContain('data-id="*start"');
      expect(svg).toContain('Opening Scene (L1)');
      expect(svg).toContain('True Ending (L55)');
    });

    it('applies active highlight styles and attributes when activeNodeId or activeLine is provided', () => {
      const svg1 = renderStoryGraphToSvg(sampleGraph, { activeNodeId: '*choice_1' });
      expect(svg1).toContain('node-box-active');
      expect(svg1).toContain('data-active="true"');

      const svg2 = renderStoryGraphToSvg(sampleGraph, { activeLine: 25 });
      expect(svg2).toContain('node-box-active');
      expect(svg2).toContain('data-id="*choice_1" data-line="20" data-file="assets/script/main.ks" data-active="true"');
    });

    it('applies execution highlight styles when executingNodeId or executingLine is provided', () => {
      const svg1 = renderStoryGraphToSvg(sampleGraph, { executingNodeId: '*ending_good' });
      expect(svg1).toContain('node-box-executing');
      expect(svg1).toContain('data-executing="true"');

      const svg2 = renderStoryGraphToSvg(sampleGraph, { executingLine: 10, isPaused: true });
      expect(svg2).toContain('node-box-executing');
      expect(svg2).toContain('node-box-paused');
      expect(svg2).toContain('data-executing="true"');
    });
  });

  describe('Viewport Centering Math (computeViewBox & centerStoryMapOnNode)', () => {
    it('computes correct viewBox centered on target node', () => {
      const layout = layoutNodes(sampleGraph.nodes);
      const vb = computeViewBox(layout, '*start', { width: 600, height: 400 }, 1);
      expect(vb).not.toBeNull();
      expect(vb.width).toBe(600);
      expect(vb.height).toBe(400);
      // Start node box is at (50, 50, 180, 60), center is (140, 80)
      // minX = 140 - 300 = -160, minY = 80 - 200 = -120
      expect(vb.minX).toBe(-160);
      expect(vb.minY).toBe(-120);
      expect(vb.viewBoxString).toBe('-160 -120 600 400');
    });

    it('updates SVG element viewBox attribute in DOM', () => {
      const div = document.createElement('div');
      div.innerHTML = renderStoryGraphToSvg(sampleGraph);
      const svg = div.querySelector('svg');
      expect(svg).toBeTruthy();

      const ok = centerStoryMapOnNode(svg, '*choice_1', { width: 800, height: 600 });
      expect(ok).toBe(true);
      expect(svg.getAttribute('viewBox')).toBeTruthy();
      expect(svg.getAttribute('viewBox')).not.toBe('0 0 600 400');
    });
  });

  describe('Interactive Event Dispatching (bindStoryMapEvents)', () => {
    it('dispatches custom event and calls callback with { file, line, label } on node click', () => {
      const div = document.createElement('div');
      div.innerHTML = renderStoryGraphToSvg(sampleGraph);
      const svg = div.querySelector('svg');
      const onSelect = vi.fn();
      const eventListener = vi.fn();

      svg.addEventListener('story-node-select', eventListener);
      const unbind = bindStoryMapEvents(svg, onSelect);

      const targetNode = svg.querySelector('[data-id="*choice_1"] rect');
      expect(targetNode).toBeTruthy();

      targetNode.dispatchEvent(new MouseEvent('click', { bubbles: true }));

      expect(onSelect).toHaveBeenCalledTimes(1);
      expect(onSelect).toHaveBeenCalledWith({
        file: 'assets/script/main.ks',
        line: 20,
        label: '*choice_1',
      });

      expect(eventListener).toHaveBeenCalledTimes(1);
      expect(eventListener.mock.calls[0][0].detail).toEqual({
        file: 'assets/script/main.ks',
        line: 20,
        label: '*choice_1',
      });

      // Cleanup
      unbind();
      targetNode.dispatchEvent(new MouseEvent('click', { bubbles: true }));
      expect(onSelect).toHaveBeenCalledTimes(1);
    });
  });

  describe('Dynamic DOM State Updates', () => {
    let container;
    let svg;

    beforeEach(() => {
      container = document.createElement('div');
      container.innerHTML = renderStoryGraphToSvg(sampleGraph);
      svg = container.querySelector('svg');
    });

    it('updates active node class in DOM via setStoryMapActiveNode', () => {
      setStoryMapActiveNode(svg, { nodeId: '*choice_1' });
      const nodeEl = svg.querySelector('[data-id="*choice_1"]');
      const rectEl = nodeEl.querySelector('rect');
      expect(rectEl.classList.contains('node-box-active')).toBe(true);

      // Switch active node by line
      setStoryMapActiveNode(svg, { line: 55 });
      const goodEndNode = svg.querySelector('[data-id="*ending_good"] rect');
      expect(goodEndNode.classList.contains('node-box-active')).toBe(true);
      expect(rectEl.classList.contains('node-box-active')).toBe(false);
    });

    it('updates executing node class in DOM via setStoryMapExecutionNode', () => {
      setStoryMapExecutionNode(svg, { nodeId: '*start', isPaused: true });
      const startRect = svg.querySelector('[data-id="*start"] rect');
      expect(startRect.classList.contains('node-box-executing')).toBe(true);
      expect(startRect.classList.contains('node-box-paused')).toBe(true);

      // Step to line 22 (choice_1) without pause
      setStoryMapExecutionNode(svg, { line: 22, isPaused: false });
      const choiceRect = svg.querySelector('[data-id="*choice_1"] rect');
      expect(choiceRect.classList.contains('node-box-executing')).toBe(true);
      expect(choiceRect.classList.contains('node-box-paused')).toBe(false);
      expect(startRect.classList.contains('node-box-executing')).toBe(false);
    });
  });
});
