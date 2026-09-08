## 07-09-2026:

- [x] AET-Toolchain
  - [x] Workbench
    - [x] Basic controls - Pass 1
      - [x] Word delete
      - [x] Paste
    - [x] Plug the assembler (dumb version, no lsp emulation)
      - [x] Assembling
      - [x] Code highlighting

## Open

- [ ] World UI elements
  - [ ] Take the 2d Renderer and apply a 3d projection? I think this should be enough. It needs its own path. Idk how to name it. While I'm at it, I need to redesign the 2d renderer to accept arbitrary textures on every render pass. I hate it but the most portable solution might be texture_array
  - [ ] Indication on halt/term machines
  - [ ] Indication on faulting machines
- [ ] AET-Toolchain
  - [ ] Workbench
    - [ ] Basic controls - Pass 2
      - [ ] Move up and down
      - [ ] Word move (start and end)
      - [ ] Line move (start and end)
    - [ ] Basic controls - Pass 3
      - [ ] Basic active selection with shift+left/right
      - [ ] Move cursor to click
    - [ ] Basic controls - Pass 4
      - [ ] Expanded active selection to work with all move motions
    - [ ] Toolchain integration
      - [ ] Assembler diagnosis
