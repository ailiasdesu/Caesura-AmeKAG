import { createContext, useContext, useReducer } from "react";

const EditorContext = createContext(null);

const initialState = {
  // Editor
  activePanel: "scene",       // scene | code
  openFiles: [{ name: "scene_start.cae", content: "-- Caesura KAG Script\n-- Scene: Start\n\nKAG.layer_show(\"bg\", \"classroom.png\")\nKAG.text_show(\"Welcome to Caesura Engine\")\nKAG.system_wait(3.0)\n" }],
  activeFileIdx: 0,
  unsaved: false,

  // Engine connection
  status: "disconnected",     // connected | disconnected
  enginePid: null,

  // Stage
  resolution: { w: 1920, h: 1080 },
  stageZoom: 0.55,
  stageGrid: true,
  stageSafe: false,
  previewing: false,

  // Scenes
  sceneList: [
    { id: "start", name: "start.cae", active: true },
    { id: "prologue", name: "prologue.cae" },
    { id: "ch1", name: "chapter1.cae" },
  ],

  // Logs
  logs: [
    { level: "info", message: "Editor started" },
  ],

  // Package
  packageResult: null,
  packageProgress: 0,
};

function editorReducer(state, action) {
  switch (action.type) {
    case "SET_ACTIVE_PANEL":
      return { ...state, activePanel: action.payload };
    case "SET_ACTIVE_FILE":
      return { ...state, activeFileIdx: action.payload, unsaved: false };
    case "OPEN_FILE":
      return { ...state, openFiles: [...state.openFiles, action.payload], activeFileIdx: state.openFiles.length, unsaved: false };
    case "CLOSE_FILE": {
      const idx = action.payload;
      const files = state.openFiles.filter((_, i) => i !== idx);
      return { ...state, openFiles: files.length ? files : [{ name: "untitled.lua", content: "" }], activeFileIdx: Math.min(state.activeFileIdx, files.length - 1) };
    }
    case "UPDATE_FILE":
      return { ...state, openFiles: state.openFiles.map((f, i) => i === state.activeFileIdx ? { ...f, content: action.payload } : f), unsaved: true };
    case "MARK_SAVED":
      return { ...state, unsaved: false };

    // Stage
    case "SET_ZOOM":
      return { ...state, stageZoom: action.payload };
    case "SET_RESOLUTION":
      return { ...state, resolution: action.payload };
    case "TOGGLE_GRID":
      return { ...state, stageGrid: !state.stageGrid };
    case "TOGGLE_SAFE":
      return { ...state, stageSafe: !state.stageSafe };
    case "SET_PREVIEW":
      return { ...state, previewing: action.payload };

    // Scenes
    case "SET_ACTIVE_SCENE": {
      const scenes = state.sceneList.map((s, i) => ({ ...s, active: i === action.payload }));
      return { ...state, sceneList: scenes };
    }
    case "ADD_SCENE":
      return { ...state, sceneList: [...state.sceneList, { id: `scene_${Date.now()}`, name: action.payload.name, template: action.payload.template }] };
    case "REORDER_SCENES": {
      const { from, to } = action.payload;
      const scenes = [...state.sceneList];
      const [item] = scenes.splice(from, 1);
      scenes.splice(to, 0, item);
      return { ...state, sceneList: scenes };
    }

    // Engine
    case "SET_STATUS":
      return { ...state, status: action.payload };

    // Logs
    case "ADD_LOG":
      return { ...state, logs: [...state.logs, action.payload].slice(-200) };

    // Package
    case "SET_PACKAGE_RESULT":
      return { ...state, packageResult: action.payload };
    case "SET_PACKAGE_PROGRESS":
      return { ...state, packageProgress: action.payload };

    default:
      return state;
  }
}

export function EditorProvider({ children }) {
  const [state, dispatch] = useReducer(editorReducer, initialState);
  return (
    <EditorContext.Provider value={{ state, dispatch }}>
      {children}
    </EditorContext.Provider>
  );
}

export function useEditor() {
  const ctx = useContext(EditorContext);
  if (!ctx) throw new Error("useEditor must be used within EditorProvider");
  return ctx;
}