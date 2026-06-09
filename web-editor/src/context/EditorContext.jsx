import { createContext, useContext, useReducer, useCallback } from "react";

const EditorContext = createContext(null);

const initialState = {
  script: `-- Write your KAG scene script here
function scene_start()
    KAG.clear_screen()
    KAG.show_image("bg", "assets/bg/classroom.png", 0, 0)
    KAG.play_bgm("assets/bgm/morning.ogg", 1.0)
    KAG.show_text("Welcome to my visual novel!")
    KAG.wait_click()
end

scene_start()
`,
  assets: [],
  logs: [],
  status: "disconnected",
  activePanel: "editor",
  timelineEvents: [],
  selectedEvent: null,
  stageZoom: 1,
  stageGrid: true,
  resolution: { w: 1280, h: 720 },
  aiMessages: [],
  aiProvider: "openai",
  aiSettings: { openaiKey: "", openaiModel: "gpt-4o", codexEndpoint: "", customUrl: "", customKey: "" },
  previewRunning: false,
};

function editorReducer(state, action) {
  switch (action.type) {
    case "SET_SCRIPT": return { ...state, script: action.payload };
    case "SET_ASSETS": return { ...state, assets: action.payload };
    case "ADD_LOG": return { ...state, logs: [...state.logs.slice(-199), { ...action.payload, time: new Date().toTimeString().slice(0, 8) }] };
    case "SET_STATUS": return { ...state, status: action.payload };
    case "SET_ACTIVE_PANEL": return { ...state, activePanel: action.payload };
    case "SET_TIMELINE": return { ...state, timelineEvents: action.payload };
    case "SET_SELECTED": return { ...state, selectedEvent: action.payload };
    case "SET_ZOOM": return { ...state, stageZoom: action.payload };
    case "TOGGLE_GRID": return { ...state, stageGrid: !state.stageGrid };
    case "SET_RESOLUTION": return { ...state, resolution: action.payload };
    case "ADD_AI_MESSAGE": return { ...state, aiMessages: [...state.aiMessages, action.payload] };
    case "SET_AI_PROVIDER": return { ...state, aiProvider: action.payload };
    case "SET_AI_SETTINGS": return { ...state, aiSettings: { ...state.aiSettings, ...action.payload } };
    case "SET_PREVIEW": return { ...state, previewRunning: action.payload };
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