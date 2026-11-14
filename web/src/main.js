/**
 * MiniShell WebAssembly UI Bridge
 * Connects the web terminal interface to the C WebAssembly engine.
 */

(function () {
  'use strict';

  // DOM Elements
  const terminalScreen = document.getElementById('terminal-screen');
  const terminalHistory = document.getElementById('terminal-history');
  const terminalInput = document.getElementById('terminal-input');
  const promptPrefix = document.getElementById('prompt-prefix');
  const currentDirDisplay = document.getElementById('current-dir-display');
  const btnClear = document.getElementById('btn-clear');
  const btnReset = document.getElementById('btn-reset');
  const exampleButtons = document.querySelectorAll('.cmd-pill');

  // State
  let history = [];
  let historyIndex = -1;
  let wasmModule = null;
  let wasmLoaded = false;
  let currentCwd = '/home/user';

  // C API bindings
  let c_wasm_shell_init = null;
  let c_wasm_shell_reset = null;
  let c_wasm_shell_execute = null;
  let c_wasm_shell_get_cwd = null;
  let c_wasm_shell_get_prompt = null;

  function appendOutput(content, className = 'cmd-stdout') {
    if (!content && className !== 'cmd-line') return;
    const div = document.createElement('div');
    div.className = `history-item ${className}`;
    div.textContent = content;
    terminalHistory.appendChild(div);
    scrollToBottom();
  }

  function scrollToBottom() {
    terminalScreen.scrollTop = terminalScreen.scrollHeight;
  }

  function updatePrompt(cwd) {
    if (cwd) {
      currentCwd = cwd;
    }
    currentDirDisplay.textContent = currentCwd;
    promptPrefix.textContent = `minishell:${currentCwd}$ `;
  }

  function clearTerminal() {
    terminalHistory.innerHTML = '';
    scrollToBottom();
  }

  function resetVFS() {
    if (wasmLoaded && c_wasm_shell_reset) {
      c_wasm_shell_reset();
      updatePrompt('/home/user');
      appendOutput('[System] Virtual filesystem restored to default sample files.', 'cmd-system');
    } else {
      appendOutput('[System] Virtual filesystem reset.', 'cmd-system');
    }
    terminalInput.focus();
  }

  // Execute a command string through WASM engine
  function executeCommand(commandLine) {
    const trimmed = commandLine.trim();
    if (!trimmed) {
      appendOutput(`minishell:${currentCwd}$ `, 'cmd-line');
      return;
    }

    // Echo command line
    appendOutput(`minishell:${currentCwd}$ ${commandLine}`, 'cmd-line');

    // Add to UI history
    history.push(commandLine);
    historyIndex = history.length;

    // Handle 'clear' command client-side immediately
    if (trimmed === 'clear') {
      clearTerminal();
      return;
    }

    if (!wasmLoaded) {
      appendOutput('minishell: WebAssembly engine is initializing or not available. Please wait...', 'cmd-stderr');
      return;
    }

    try {
      // Call C wasm_shell_execute
      const resultJsonStr = c_wasm_shell_execute(trimmed);
      const result = JSON.parse(resultJsonStr);

      if (result.stdout) {
        // Strip trailing newline if present to prevent double spacing
        let out = result.stdout;
        if (out.endsWith('\n')) out = out.slice(0, -1);
        appendOutput(out, 'cmd-stdout');
      }

      if (result.stderr) {
        let err = result.stderr;
        if (err.endsWith('\n')) err = err.slice(0, -1);
        appendOutput(err, 'cmd-stderr');
      }

      if (result.cwd) {
        updatePrompt(result.cwd);
      }
    } catch (err) {
      appendOutput(`minishell: internal error: ${err.message}`, 'cmd-stderr');
    }

    scrollToBottom();
  }

  // Load WebAssembly Module
  async function initWasm() {
    appendOutput('Initializing MiniShell WebAssembly engine...', 'cmd-system');

    // Resolve path for GitHub Pages subpaths
    const baseWasmPath = window.location.pathname.endsWith('/')
      ? window.location.pathname
      : window.location.pathname.substring(0, window.location.pathname.lastIndexOf('/') + 1);

    const candidates = [
      'public/wasm/minishell.js',
      'wasm/minishell.js',
      `${baseWasmPath}public/wasm/minishell.js`,
      `${baseWasmPath}wasm/minishell.js`
    ];

    let loadedScript = false;
    for (const url of candidates) {
      try {
        await loadScript(url);
        loadedScript = true;
        break;
      } catch (e) {
        // try next candidate
      }
    }

    if (loadedScript && typeof createMiniShellModule === 'function') {
      try {
        const mod = await createMiniShellModule();
        wasmModule = mod;

        // Bind C functions using cwrap
        c_wasm_shell_init = mod.cwrap('wasm_shell_init', null, []);
        c_wasm_shell_reset = mod.cwrap('wasm_shell_reset', null, []);
        c_wasm_shell_execute = mod.cwrap('wasm_shell_execute', 'string', ['string']);
        c_wasm_shell_get_cwd = mod.cwrap('wasm_shell_get_cwd', 'string', []);
        c_wasm_shell_get_prompt = mod.cwrap('wasm_shell_get_prompt', 'string', []);

        c_wasm_shell_init();
        wasmLoaded = true;

        const cwd = c_wasm_shell_get_cwd();
        updatePrompt(cwd);

        appendOutput('WebAssembly engine active. Virtual in-memory filesystem mounted at /home/user.', 'cmd-system');
        appendOutput('Type "help" for a list of supported virtual commands.\n', 'cmd-system');
      } catch (err) {
        console.error('Failed to instantiate WebAssembly module:', err);
        setupFallbackMode();
      }
    } else {
      setupFallbackMode();
    }
  }

  function setupFallbackMode() {
    appendOutput('[Notice] Pre-compiled WebAssembly binary not found locally.', 'cmd-system');
    appendOutput('To compile the C source into WebAssembly:', 'cmd-system');
    appendOutput('  make wasm   (or deploy via GitHub Actions to Pages)', 'cmd-system');
    appendOutput('The native POSIX shell can be built locally with:', 'cmd-system');
    appendOutput('  make && ./bin/minishell\n', 'cmd-system');
  }

  function loadScript(src) {
    return new Promise((resolve, reject) => {
      const script = document.createElement('script');
      script.src = src;
      script.onload = () => resolve();
      script.onerror = () => reject(new Error(`Failed to load ${src}`));
      document.head.appendChild(script);
    });
  }

  // Keyboard and Event Listeners
  terminalInput.addEventListener('keydown', (e) => {
    if (e.key === 'Enter') {
      e.preventDefault();
      const cmd = terminalInput.value;
      terminalInput.value = '';
      executeCommand(cmd);
    } else if (e.key === 'ArrowUp') {
      e.preventDefault();
      if (history.length > 0 && historyIndex > 0) {
        historyIndex--;
        terminalInput.value = history[historyIndex];
      }
    } else if (e.key === 'ArrowDown') {
      e.preventDefault();
      if (historyIndex < history.length - 1) {
        historyIndex++;
        terminalInput.value = history[historyIndex];
      } else {
        historyIndex = history.length;
        terminalInput.value = '';
      }
    } else if (e.ctrlKey && e.key === 'l') {
      e.preventDefault();
      clearTerminal();
    } else if (e.ctrlKey && e.key === 'c') {
      e.preventDefault();
      appendOutput(`minishell:${currentCwd}$ ${terminalInput.value}^C`, 'cmd-line');
      terminalInput.value = '';
      scrollToBottom();
    }
  });

  // Clicking terminal focuses input
  terminalScreen.addEventListener('click', () => {
    terminalInput.focus();
  });

  // Buttons
  btnClear.addEventListener('click', clearTerminal);
  btnReset.addEventListener('click', resetVFS);

  // Quick Command Pills
  exampleButtons.forEach((btn) => {
    btn.addEventListener('click', () => {
      const cmd = btn.getAttribute('data-cmd');
      if (cmd) {
        executeCommand(cmd);
        terminalInput.focus();
      }
    });
  });

  // Initialize
  initWasm();
})();

