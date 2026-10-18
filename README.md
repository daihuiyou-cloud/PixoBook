# PixoBook — AI 素材库 (AI Material Library)

A desktop application for managing, browsing, searching, and organizing AI-generated image assets. Built with **Qt 5** and **C++17**.

![PixoBook 主界面](docs/images/dark.png)

## Features

- **Smart import** — Scan directories for images; automatic duplicate detection via SHA-256 hashing
- **AI metadata parsing** — Automatically detects source tool and extracts generation parameters:
  - **Stable Diffusion** — Parses prompt, negative prompt, seed, steps, CFG scale, model, sampler from PNG `tEXt`/`zTXt` chunks
  - **Midjourney** — Extracts seed and model name from filename patterns
  - **DALL-E** — Extracts prompt from filename patterns
- **Database** — SQLite-backed storage with tags, favorites, and metadata querying
- **Full-text search** — FTS5 index over prompts and filenames. ASCII terms match word prefixes; CJK text is indexed per character so a substring such as "月光" matches inside a longer prompt. Falls back to `LIKE` when the SQLite build has no FTS5.
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
selective word, a miss, a CJK phrase, and a deep page. Compare the numbers
across commits; they are measurements of the same machine, not absolute
targets.

Reference run with the FTS5 index (development machine):

| assets | import | common | selective | miss | CJK | deep page |
|--------|--------|--------|-----------|------|-----|-----------|
| 10k    | 2.4 s  | 3.0 ms | 1.1 ms    | 0.1 ms | 0.8 ms | 5.0 ms |
| 50k    | 21.5 s | 9.9 ms | 19.4 ms   | 0.5 ms | 8.8 ms | 8.1 ms |

The import figure includes building the search index (roughly 2.3k assets/s at
50k). Before FTS5, the same machine measured 31 ms / 308 ms for the common
word and 53 ms / 501 ms for the deep page at 10k / 50k.

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
