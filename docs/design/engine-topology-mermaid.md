flowchart LR
    subgraph root["Composition Root"]
        main["main.cpp"]
        engine["entry/Engine"]
    end
    subgraph di["Dependency Injection"]
        registry["BackendRegistry (26 I*)"]
    end
    subgraph core["Core Runtime"]
        platform["platform (SDL3)"]
        input["input (Input Router)"]
        script["script (Lua+KAG)"]
        render["render (bgfx 22cpp)"]
        audio["audio (SoLoud)"]
        resource["resource (Asset Pipeline)"]
    end
    subgraph data["Data Layer"]
        archive["archive (CARC+AES)"]
        storage["storage (Save/Load)"]
    end
    subgraph opt["Optional Subsystems"]
        live2d["live2d (PNG/Cubism)"]
        minigame["minigame (3D)"]
        steam["steam (Achievements)"]
    end
    subgraph tools["Tools"]
        debug["debug (Log/HotReload)"]
        job["job (Multithread)"]
        rpc["rpc (HTTP Editor)"]
    end

    main --> engine
    engine -->|"Register All Backends"| registry

    registry -.->|"I*"| platform
    registry -.->|"I*"| input
    registry -.->|"I*"| script
    registry -.->|"I*"| render
    registry -.->|"I*"| audio
    registry -.->|"I*"| resource
    registry -.->|"I*"| archive
    registry -.->|"I*"| storage
    registry -.->|"I*"| live2d
    registry -.->|"I*"| minigame
    registry -.->|"I*"| steam
    registry -.->|"I*"| debug
    registry -.->|"I*"| job
    registry -.->|"I*"| rpc

    platform -->|"SDL_Event"| input
    input -->|"resume coroutine"| script
    script -->|"KAG Commands"| render
    script -->|"BGM/SE"| audio
    script -->|"Save/Load"| storage
    render -->|"Texture Query"| resource
    resource -->|"Read Archive"| archive
    storage -->|"Encrypt"| archive
    job -->|"Async Decode"| resource
    debug -->|"Hot Reload"| script
    rpc -->|"Execute Script"| script

    style root fill:#C2E5FF,stroke:#3DADFF
    style di fill:#FFECBD,stroke:#FFC943
    style core fill:#CDF4D3,stroke:#66D575
    style data fill:#DCCCFF,stroke:#874FFF
    style opt fill:#FFCDC2,stroke:#FF7556
    style tools fill:#D9D9D9,stroke:#B3B3B3