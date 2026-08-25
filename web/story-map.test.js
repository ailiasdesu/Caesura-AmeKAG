// =============================================================================
//  Caesura (AmeKAG) — web/story-map.test.js
//  Unit tests for Web Story Map visual layout & SVG generation
// =============================================================================

import { describe, it, expect } from 'vitest';
import { parseStoryGraph, layoutNodes, renderStoryGraphToSvg } from './story-map.mjs';

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
    expect(svg).toContain('class="story-node"');
    expect(svg).toContain('data-id="*start"');
    expect(svg).toContain('data-line="1"');
    expect(svg).toContain('Good End (L50)');
    expect(svg).toContain('</svg>');
  });
});
