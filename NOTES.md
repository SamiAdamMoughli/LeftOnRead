Wraith/
├── .github/                # CI/CD workflows (e.g., GitHub Actions for auto-build)
│   └── workflows/
│       └── build.yml
├── build/                  # Compiled binaries and build artifacts (gitignored)
├── docs/                   # Detailed documentation
│   └── architecture.md     # The file you just created
├── include/                # Header files (.hpp) - The "Interfaces"
│   └── wraith/
│       ├── Common.hpp      # Shared types, NTAPI definitions, and constants
│       ├── PayloadManager.hpp
│       ├── FileGhoster.hpp
│       ├── SectionManager.hpp
│       ├── ProcessExecutor.hpp
│       └── ErrorHandler.hpp
├── payloads/               # Test PE files (e.g., a simple calc.exe or hello.exe)
│   └── test_payload.bin
├── src/                    # Implementation files (.cpp) - The "Logic"
│   ├── main.cpp            # Orchestrator (implements the "Ghosting Workflow")
│   ├── PayloadManager.cpp
│   ├── FileGhoster.cpp
│   ├── SectionManager.cpp
│   ├── ProcessExecutor.cpp
│   └── ErrorHandler.cpp
├── tests/                  # Unit tests for specific modules
│   └── test_pe_validation.cpp
├── .gitignore              # Exclude /build, .vs, .obj, etc.
├── CMakeLists.txt          # Build configuration (Cross-platform build system)
└── README.md               # Quick start guide and usage
