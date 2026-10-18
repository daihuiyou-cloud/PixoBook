# PixoBook — AI 素材库 (AI Material Library)

A desktop application for managing, browsing, searching, and organizing AI-generated image assets. Built with **Qt 5** and **C++17**.

![PixoBook 主界面](docs/images/dark.png)

## Features

- **Smart import** — Scan directories for images; automatic duplicate detection via SHA-256 hashing
- **AI metadata parsing** — Automatically detects source tool and extracts generation parameters:
  - **Stable Diffusion** — Parses prompt, negative prompt, seed, steps, CFG scale, model, sampler from PNG `tEXt`/`zTXt` chunks
  - **Midjourney** — Extracts seed and model name from filename patterns
  - **DALL-E** — Extracts prompt from filename patterns
- **Database** — SQLite-backed storage with tags, favorites, prompt/filename search, and metadata querying
- **Image gallery** — Grid-based thumbnail view with lazy loading, keyboard navigation, multi-select, and drag-and-drop import
- **Lightbox viewer** — Full-screen image viewer with zoom/pan and navigation
- **Detail panel** — Slide-out panel showing full image preview, metadata, and tag management
- **File watching** — Real-time monitoring of watched folders. New files are imported; a disappeared file is marked **missing** instead of being deleted, so tags and favorites survive an unplugged drive, a stale network share, or a temporarily unavailable folder. Rescanning the folder restores the row.
- **Annotation export** — Writes tags, favorites and parsed metadata as JSON (`pixobook-annotations.json`); the database can be rebuilt from a rescan, but annotations cannot.
- **Custom dark theme** — Fully custom-painted UI with VS Code Codicon icon font
- **Command palette** — `Ctrl+Shift+P` quick action menu
- **Tag system** — Color-coded tags with filtering

## Build

### Prerequisites

- **Qt 5** (Widgets, Sql, Concurrent, Test)
- **CMake** 3.16+
- **C++17** compiler (MSVC recommended on Windows)

### Build steps

```bash
mkdir build
cd build
cmake ..
cmake --build .
```

### Run tests

```bash
cd build
ctest
```

### Benchmark

```bash
./build/RelWithDebInfo/pixobook_bench --assets 10000 --runs 30
```

The benchmark builds a synthetic library in a temporary database and reports
batched-import time plus search latency (min/p50/p95) for a common word, a
selective word, a miss, and a deep page. Compare the numbers across commits;
they are measurements of the same machine, not absolute targets. Reference
run on the development machine (10k assets): import 0.65 s, search p50 31 ms,
deep page p50 53 ms. At 50k assets the `LIKE '%word%'` scan reaches p50 308 ms,
which is why FTS5 is the next architectural step.

## Project structure

```
PixoBook/
├── CMakeLists.txt          # Build configuration
├── resources/              # Qt resources (fonts, QRC)
├── src/
│   ├── main.cpp            # Application entry point
│   ├── core/               # Interfaces and utilities
│   ├── database/           # SQLite database layer
│   ├── models/             # Data structures (Asset, Metadata, Tag)
│   ├── parsers/            # AI metadata parsers (SD/MJ/DALL-E)
│   ├── services/           # Business logic (scanner, cache, watcher, controller)
│   └── ui/                 # Qt widgets and custom-painted UI
├── tests/                  # Qt Test unit tests
└── docs/                   # Development documentation
```

## License

Proprietary — all rights reserved.
