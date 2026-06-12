# Editor Setup

This page covers editor configuration for working with Lucis (`.lc`) files, including how to install the VS Code extension with syntax highlighting and the built-in Language Server.

## Extension Source

The editor extensions live in the `editors/` directory at the root of the project. Currently there is support for:

- **VS Code and forks** (Cursor, VSCodium, etc.) — `editors/vscode-lucis/`

The extension provides:

- Full syntax highlighting via TextMate grammar (`syntaxes/lucis.tmLanguage.json`)
- Language configuration (bracket matching, auto-closing, comment toggling)
- LSP client that connects to the Lucis compiler's built-in language server

## VS Code

### Prerequisites

- **Node.js** (for building the extension)
- **vsce** — the VS Code Extension CLI:

```bash
npm install -g @vscode/vsce
```

- **Lucis compiler** built and available in your `PATH` (the LSP runs via `lucis lsp`)

### Installing the Extension

#### Option 1: Install the pre-built `.vsix`

If a `.vsix` file is already present in `editors/vscode-lucis/`:

```bash
code --install-extension editors/vscode-lucis/lucis-0.4.0.vsix
```

Replace the version number with the actual file name.

#### Option 2: Build and install from source

1. Navigate to the extension directory:

```bash
cd editors/vscode-lucis
```

2. Install dependencies:

```bash
npm install
```

3. Package the extension into a `.vsix`:

```bash
vsce package
```

This produces a file like `lucis-0.4.0.vsix` in the current directory.

4. Install the packaged extension:

```bash
code --install-extension lucis-0.4.0.vsix
```

5. Reload VS Code. All `.lc` files will now have syntax highlighting and LSP features.

### Language Server

The extension automatically starts the Lucis Language Server when you open a `.lc` file. The server runs via `lucis lsp` over stdio and provides:

- **Diagnostics** — error highlighting as you type
- **Hover** — type information and doc-comments on hover
- **Completion** — keywords, types, stdlib functions, struct fields, extend methods, and doc-tag suggestions inside `/** */` blocks
- **Signature Help** — parameter info when typing function calls
- **Semantic Tokens** — enhanced highlighting for variables, functions, types, parameters, etc.

The `lucis` binary must be in your `PATH` for the LSP to work. If the server crashes or becomes unresponsive, use the command palette (`Ctrl+Shift+P`) and run **Lucis: Restart Language Server**.

### Extension Structure

```
editors/vscode-lucis/
├── extension.js                  # LSP client + activation logic
├── language-configuration.json   # Brackets, comments, auto-closing
├── package.json                  # Extension manifest
└── syntaxes/
    └── lucis.tmLanguage.json       # TextMate grammar for syntax highlighting
```

## Other Editors (VS Code Forks)

The extension is compatible with any VS Code fork that supports `.vsix` installation:

- **Cursor** — `cursor --install-extension lucis-0.4.0.vsix`
- **VSCodium** — `codium --install-extension lucis-0.4.0.vsix`

The LSP will work identically as long as `lucis` is in your `PATH`.

### Vim / Neovim

There is no dedicated plugin yet. You can set up basic filetype detection:

```vim
" In ~/.config/nvim/filetype.vim or ~/.vim/filetype.vim
au BufRead,BufNewFile *.lc set filetype=c
```

This gives C-like syntax highlighting as a temporary measure.

### Emacs

Add to your Emacs configuration:

```elisp
(add-to-list 'auto-mode-alist '("\\.lc\\'" . c-mode))
```

### ANTLR4 Extension (For Compiler Development)

If you are working on the Lucis compiler itself, the **ANTLR4 grammar syntax support** extension is useful for editing the grammar files (`.g4`):

- **Name:** ANTLR4 grammar syntax support
- **Id:** `mike-lischke.vscode-antlr4`
- **Marketplace:** [mike-lischke.vscode-antlr4](https://marketplace.visualstudio.com/items?itemName=mike-lischke.vscode-antlr4)

Install it from the VS Code marketplace or via command line:

```bash
code --install-extension mike-lischke.vscode-antlr4
```

The project includes a `.vscode/settings.json` configured so that whenever you save a `.g4` file, the extension automatically compiles the grammar and regenerates the C++ parser/lexer files into the correct output directory (`src/generated/`).

The grammar files are:

- `grammar/LucisLexer.g4` — Lexer rules (tokens, keywords, operators)
- `grammar/LucisParser.g4` — Parser rules (syntax structure)

> **Important:** Never run `antlr4` manually or edit files in `src/generated/` directly. Always let the extension handle generation via save.

## See Also

- [Installation](installation.md) — Build the compiler from source
- [Hello World](hello-world.md) — Write and run your first program
- [CLI Usage](cli-usage.md) — Compiler flags and options
- [Doc-Comments](../language/doc-comments.md) — Documenting code with `/** */`
