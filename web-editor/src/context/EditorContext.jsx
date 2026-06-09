import { createContext, useContext, useReducer } from "react";

const EditorContext = createContext(null);

const initialState = {
  /* Script & Editor */
  script: `-- Caesura KAG Script
function scene_start()
    KAG.clear_screen()
    KAG.show_image("bg", "assets/bg/classroom.png", 0, 0)
    KAG.play_bgm("assets/bgm/morning.ogg", 1.0)
    KAG.show_text("Welcome to my visual novel!")
    KAG.wait_click()
end

scene_start()
`,
  openFiles: [{ name: "main.lua", content: "" }],
  activeFileIdx: 0,
  unsaved: false,

  /* Assets */
  assets: [],

  /* Scene list */
  scenes: [
    { id: "scene_start", name: "Scene Start", line: 1 },
  ],
  activeScene: "scene_start",

  /* Stage */
  stageZoom: 1,
  stageGrid: true,
  stageSafe: true,
  resolution: { w: 1280, h: 720 },
  previewing: false,

  /* Properties (selected object) */
  selectedObject: null,  // { type: "sprite"|"live2d"|"minigame", ... }

  /* Logs */
  logs: [],
  logFilter: "all",
  logSearch: "",

  /* Engine connection */
  status: "disconnected",  // connected | disconnected | error

  /* AI */
  aiMessages: [],
  aiProvider: "openai",
  aiGenerating: false,
  aiSettings: {
    openaiKey: "",
    openaiModel: "gpt-4o",
    codexEndpoint: "",
    customUrl: "",
    customKey: "",
  },

  /* Live2D */
  live2dEnabled: false,
  live2dModels: [],
  activeLive2DModel: null,

  /* MiniGame */
  miniGameObjects: [
    { id: 1, type: "Cube", name: "Cube_1", pos: [0, 0, 0], scale: [1, 1, 1], roughness: 0.5, metallic: 0, specular: 0.5, collision: true, gravity: true },
  ],
  miniGameLights: [
    { id: 1, type: "ambient", color: "#404050", intensity: 0.3 },
    { id: 2, type: "directional", direction: [0, -1, 0], color: "#ffffff", intensity: 0.8 },
  ],

  /* Settings */
  activePanel: "scene",       // scene | code | ai | live2d | minigame
  bottomTab: "code",          // code | ai | logs | settings
  settingsTab: "ai",          // ai | editor | engine | package | about
  showSaveManager: false,
  packagePlatform: 'win',
  packaging: false,
  packageResult: null,
};

function editorReducer(state, action) {
  switch (action.type) {
    /* Script */
    case "SET_SCRIPT": {
      const files = [...state.openFiles];
      files[state.activeFileIdx] = { ...files[state.activeFileIdx], content: action.payload };
      return { ...state, script: action.payload, openFiles: files, unsaved: true };
    }
    case "OPEN_FILE": {
      const exists = state.openFiles.findIndex(f => f.name === action.payload.name);
      if (exists >= 0) return { ...state, activeFileIdx: exists };
      return {
        ...state,
        openFiles: [...state.openFiles, action.payload],
        activeFileIdx: state.openFiles.length,
        script: action.payload.content,
      };
    }
    case "CLOSE_FILE": {
      if (state.openFiles.length <= 1) return state;
      const files = state.openFiles.filter((_, i) => i !== action.payload);
      const newIdx = Math.min(state.activeFileIdx, files.length - 1);
      return { ...state, openFiles: files, activeFileIdx: newIdx, script: files[newIdx].content };
    }
    case "SET_ACTIVE_FILE": return { ...state, activeFileIdx: action.payload, script: state.openFiles[action.payload].content };
    case "MARK_SAVED": return { ...state, unsaved: false };

    /* Assets */
    case "SET_ASSETS": return { ...state, assets: action.payload };

    /* Scenes */
    case "SET_SCENES": return { ...state, scenes: action.payload };
    case "ADD_SCENE": return { ...state, scenes: [...state.scenes, action.payload] };
    case "REMOVE_SCENE": return { ...state, scenes: state.scenes.filter(s => s.id !== action.payload) };
    case "SET_ACTIVE_SCENE": return { ...state, activeScene: action.payload };
    case "REORDER_SCENES": return { ...state, scenes: action.payload };

    /* Stage */
    case "SET_ZOOM": return { ...state, stageZoom: action.payload };
    case "TOGGLE_GRID": return { ...state, stageGrid: !state.stageGrid };
    case "TOGGLE_SAFE": return { ...state, stageSafe: !state.stageSafe };
    case "SET_RESOLUTION": return { ...state, resolution: action.payload };
    case "SET_PREVIEW": return { ...state, previewing: action.payload };

    /* Selection */
    case "SELECT_OBJECT": return { ...state, selectedObject: action.payload };

    /* Logs */
    case "ADD_LOG": return {
      ...state,
      logs: [...state.logs.slice(-299), { ...action.payload, time: new Date().toTimeString().slice(0, 8) }],
    };
    case "SET_LOG_FILTER": return { ...state, logFilter: action.payload };
    case "SET_LOG_SEARCH": return { ...state, logSearch: action.payload };
    case "CLEAR_LOGS": return { ...state, logs: [] };

    /* Engine */
    case "SET_STATUS": return { ...state, status: action.payload };

    /* AI */
    case "ADD_AI_MESSAGE": return { ...state, aiMessages: [...state.aiMessages, action.payload] };
    case "SET_AI_PROVIDER": return { ...state, aiProvider: action.payload };
    case "SET_AI_SETTINGS": return { ...state, aiSettings: { ...state.aiSettings, ...action.payload } };
    case "SET_AI_GENERATING": return { ...state, aiGenerating: action.payload };
    case "CLEAR_AI_MESSAGES": return { ...state, aiMessages: [] };

    /* Live2D */
    case "SET_LIVE2D_ENABLED": return { ...state, live2dEnabled: action.payload };
    case "SET_LIVE2D_MODELS": return { ...state, live2dModels: action.payload };
    case "SET_ACTIVE_LIVE2D_MODEL": return { ...state, activeLive2DModel: action.payload };

    /* MiniGame */
    case "ADD_MINIGAME_OBJECT": return { ...state, miniGameObjects: [...state.miniGameObjects, action.payload] };
    case "REMOVE_MINIGAME_OBJECT": return { ...state, miniGameObjects: state.miniGameObjects.filter(o => o.id !== action.payload) };
    case "UPDATE_MINIGAME_OBJECT": return {
      ...state,
      miniGameObjects: state.miniGameObjects.map(o => o.id === action.payload.id ? { ...o, ...action.payload } : o),
    };
    case "ADD_MINIGAME_LIGHT": return { ...state, miniGameLights: [...state.miniGameLights, action.payload] };
    case "REMOVE_MINIGAME_LIGHT": return { ...state, miniGameLights: state.miniGameLights.filter(l => l.id !== action.payload) };
    case "UPDATE_MINIGAME_LIGHT": return {
      ...state,
      miniGameLights: state.miniGameLights.map(l => l.id === action.payload.id ? { ...l, ...action.payload } : l),
    };

    /* Navigation */
    case "SET_ACTIVE_PANEL": return { ...state, activePanel: action.payload };
    case "SET_BOTTOM_TAB": return { ...state, bottomTab: action.payload };
    case "SET_SETTINGS_TAB": return { ...state, settingsTab: action.payload };
    case "SET_SHOW_SAVE_MANAGER": return { ...state, showSaveManager: action.payload };

    case 'SET_PACKAGE_PLATFORM': return { ...state, packagePlatform: action.payload };
    case 'SET_PACKAGING': return { ...state, packaging: action.payload };
    case 'SET_PACKAGE_RESULT': return { ...state, packageResult: action.payload };
    default: return state;
  }
}

export function EditorProvider({ children }) {
  const [state, dispatch] = useReducer(editorReducer, initialState);
  return <EditorContext.Provider value={{ state, dispatch }}>{children}</EditorContext.Provider>;
}

export function useEditor() {
  const ctx = useContext(EditorContext);
  if (!ctx) throw new Error("useEditor must be used within EditorProvider");
  return ctx;
}

export { EditorContext };
