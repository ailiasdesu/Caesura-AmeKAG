flowchart LR
    subgraph root["组合根"]
        main["main.cpp"]
        engine["entry/Engine"]
    end
    subgraph di["依赖注入"]
        registry["BackendRegistry (26 I*)"]
    end
    subgraph core["核心运行时"]
        platform["platform (SDL3)"]
        input["input (输入路由)"]
        script["script (Lua+KAG)"]
        render["render (bgfx 22cpp)"]
        audio["audio (SoLoud)"]
        resource["resource (资产管线)"]
    end
    subgraph data["数据层"]
        archive["archive (CARC+AES)"]
        storage["storage (存档)"]
    end
    subgraph opt["可选子系统"]
        live2d["live2d (PNG/Cubism)"]
        minigame["minigame (3D)"]
        steam["steam (成就)"]
    end
    subgraph tools["工具"]
        debug["debug (日志/热重载)"]
        job["job (多线程)"]
        rpc["rpc (HTTP editor)"]
    end

    main --> engine
    engine -->|"注册所有后端"| registry

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
    input -->|"resume协程"| script
    script -->|"KAG命令"| render
    script -->|"BGM/SE"| audio
    script -->|"存读档"| storage
    render -->|"纹理查询"| resource
    resource -->|"读取归档"| archive
    storage -->|"加密"| archive
    job -->|"异步解码"| resource
    debug -->|"热重载"| script
    rpc -->|"执行脚本"| script

    style root fill:#C2E5FF,stroke:#3DADFF
    style di fill:#FFECBD,stroke:#FFC943
    style core fill:#CDF4D3,stroke:#66D575
    style data fill:#DCCCFF,stroke:#874FFF
    style opt fill:#FFCDC2,stroke:#FF7556
    style tools fill:#D9D9D9,stroke:#B3B3B3