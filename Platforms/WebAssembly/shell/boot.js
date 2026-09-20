// Session id, boot overlay, project snapshot and the emscripten Module.

console.log('[shell] editor shell; build-tag=editor-4-modular');

// ---- session bootstrap (per browser tab) --------------------------
var sid = sessionStorage.getItem('o2sid');
if (!sid) {
    sid = Math.random().toString(36).slice(2) + Math.random().toString(36).slice(2);
    sessionStorage.setItem('o2sid', sid);
}
// The page may be served from the host root or under a path prefix
// (games.host/<slug>/editor/). Everything network-facing hangs off o2Base.
window.o2Base = location.pathname.replace(/\/[^\/]*$/, '');
document.cookie = 'o2sid=' + sid + '; Path=' + (window.o2Base || '/') + '; SameSite=Strict';
window.o2fsEndpoint = window.o2Base + '/api';

var statusEl = document.getElementById('status');
var statusText = document.getElementById('status-text');
var progressBar = document.querySelector('#progress > span');
function setStatus(text, frac) {
    if (!text) { statusEl.classList.add('hidden'); return; }
    statusEl.classList.remove('hidden');
    statusText.textContent = text;
    if (typeof frac === 'number')
        progressBar.style.width = (frac * 100).toFixed(1) + '%';
}


// ---- snapshot streaming ------------------------------------------
function loadSnapshot(done, fail) {
    fetch(o2Base + '/api/fs/snapshot').then(function (resp) {
        if (!resp.ok) throw new Error('snapshot HTTP ' + resp.status);
        var total = +resp.headers.get('X-Uncompressed-Length') || 0;
        var reader = resp.body.getReader();
        var chunks = [], loaded = 0;
        function pump() {
            return reader.read().then(function (r) {
                if (r.done) return;
                chunks.push(r.value);
                loaded += r.value.length;
                setStatus('Downloading project… ' + (loaded / 1048576).toFixed(1) + ' MB', total ? loaded / total : 0);
                return pump();
            });
        }
        return pump().then(function () {
            var buf = new Uint8Array(loaded), off = 0;
            for (var i = 0; i < chunks.length; i++) { buf.set(chunks[i], off); off += chunks[i].length; }
            return buf;
        });
    }).then(function (buf) {
        var magic = String.fromCharCode.apply(null, buf.subarray(0, 8));
        if (magic !== 'O2SNAP01') throw new Error('bad snapshot magic');
        var indexLen = new DataView(buf.buffer, buf.byteOffset + 8, 4).getUint32(0, true);
        var index = JSON.parse(new TextDecoder().decode(buf.subarray(12, 12 + indexLen)));
        var off = 12 + indexLen;

        setStatus('Unpacking project…', 1);
        var FS = Module.FS;
        var madeDirs = {};
        function mkdirs(dir) {
            if (madeDirs[dir]) return;
            FS.mkdirTree(dir);
            madeDirs[dir] = true;
        }
        for (var i = 0; i < index.files.length; i++) {
            var f = index.files[i];
            var full = '/project/' + f.p;
            mkdirs(full.substring(0, full.lastIndexOf('/')));
            FS.writeFile(full, buf.subarray(off, off + f.s));
            if (f.m) try { FS.utime(full, f.m, f.m); } catch (e) {}
            off += f.s;
        }
        mkdirs('/project/Bin/WebAssembly');
        console.log('[shell] snapshot unpacked:', index.files.length, 'files');
        done();
    }).catch(function (e) {
        console.error('[shell] snapshot failed', e);
        setStatus('Failed to load project: ' + e.message);
        fail(e);
    });
}

// ---- patches for the AI agent's screenshot/input tools ------------
// keep the WebGL back buffer readable so canvas screenshots work
(function () {
    var orig = HTMLCanvasElement.prototype.getContext;
    HTMLCanvasElement.prototype.getContext = function (type, attrs) {
        if (this.id === 'canvas' && (type === 'webgl2' || type === 'webgl'))
            attrs = Object.assign({}, attrs, { preserveDrawingBuffer: true });
        return orig.call(this, type, attrs);
    };
    // synthetic PointerEvents have no active pointer: capture calls throw
    var sc = Element.prototype.setPointerCapture;
    Element.prototype.setPointerCapture = function (id) { try { return sc.call(this, id); } catch (e) {} };
    var rc = Element.prototype.releasePointerCapture;
    Element.prototype.releasePointerCapture = function (id) { try { return rc.call(this, id); } catch (e) {} };
})();

// Ring buffer of the engine's own output, so the AI agent (and anyone debugging)
// can read what the editor printed without scraping the Log window off a screenshot
var engineLogLines = [];
function engineLog(kind, text) {
    engineLogLines.push((kind === 'err' ? 'ERR ' : '') + text);
    if (engineLogLines.length > 500) engineLogLines.splice(0, engineLogLines.length - 500);
}

// ---- crash watch --------------------------------------------------
// A wasm trap or abort kills the engine's main loop for good: restart the
// editor right away and show what happened, with the call stack resolved
// through the symbol map the build ships (--emit-symbol-map).
var crashHandled = false;

function isWasmCrash(err, message) {
    if (typeof WebAssembly !== 'undefined' && err instanceof WebAssembly.RuntimeError) return true;
    var t = String(message || (err && err.message) || '');
    return /RuntimeError|memory access out of bounds|null function or function signature|unreachable|table index is out of bounds|Aborted\(/.test(t);
}

// wasm frames come as wasm-function[N]; the .symbols file maps N to the name
function symbolizeStack(stack) {
    if (!/wasm-function\[\d+\]/.test(stack)) return Promise.resolve(stack);
    return fetch('Editor.html.symbols').then(function (r) {
        if (!r.ok) throw new Error('no symbol map');
        return r.text();
    }).then(function (text) {
        var map = {};
        text.split('\n').forEach(function (line) {
            var i = line.indexOf(':');
            if (i > 0) map[line.slice(0, i)] = line.slice(i + 1);
        });
        return stack.replace(/wasm-function\[(\d+)\]/g, function (m, n) {
            return map[n] ? map[n] + ' [' + n + ']' : m;
        });
    }).catch(function () { return stack; });
}

function showCrashBanner(record, blocked) {
    var prev = document.querySelector('.crash-banner');
    if (prev) prev.remove();
    var d = document.createElement('div');
    d.className = 'crash-banner';
    var head = document.createElement('div');
    head.className = 'crash-head';
    head.textContent = blocked
        ? '⚠ The editor keeps crashing — auto-restart paused'
        : '⚠ The editor crashed and was restarted';
    d.appendChild(head);
    var msg = document.createElement('div');
    msg.className = 'crash-msg';
    msg.textContent = record.message;
    d.appendChild(msg);
    if (record.stack) {
        var pre = document.createElement('pre');
        pre.textContent = record.stack;
        d.appendChild(pre);
    }
    var row = document.createElement('div');
    row.className = 'crash-row';
    function btn(label, fn, quiet) {
        var b = document.createElement('button');
        b.className = 'tbtn' + (quiet ? ' quiet' : '');
        b.textContent = label;
        b.onclick = fn;
        row.appendChild(b);
        return b;
    }
    if (blocked) btn('Restart the editor', function () { location.reload(); });
    var copyBtn = btn('Copy', function () {
        navigator.clipboard.writeText(record.t + '  ' + record.message + '\n' + (record.stack || ''))
            .then(function () { copyBtn.textContent = 'Copied'; }, function () {});
    }, true);
    btn('Close', function () { d.remove(); }, true);
    d.appendChild(row);
    document.body.appendChild(d);
}

// What emscripten said on stderr last: a failed start names its real reason there
// ("wasm streaming compile failed: TypeError: Failed to fetch"), the abort itself does not.
var lastErrLines = [];

// The editor never started because its wasm did not arrive: a network matter (a dropped
// connection, a server restart under a deploy), not a crash of the editor. It is retried with a
// growing pause and never counted as a crash strike; after a few tries the page says what is wrong.
function handleDownloadFailure(message) {
    var tries = 0;
    try { tries = +sessionStorage.getItem('o2_dl_retries') || 0; } catch (e) {}
    var why = lastErrLines.filter(function (l) { return /streaming compile failed/.test(l); }).pop() || '';
    console.error('[shell] editor download failed (try ' + (tries + 1) + ')', message, why);
    try {
        fetch(o2Base + '/api/agent/log', { method: 'POST', headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ t: new Date().toISOString(), kind: 'download-failed', message: message, reason: why, tries: tries + 1, online: navigator.onLine }) }).catch(function () {});
    } catch (e) {}
    if (tries >= 4) {
        try { sessionStorage.removeItem('o2_dl_retries'); } catch (e) {}
        setStatus('Could not download the editor — check the connection and reload');
        showCrashBanner({ t: new Date().toISOString(), message: 'The editor could not be downloaded (' + (why || message) + '). This is a network problem, not a crash: reload the page when the connection is back.', stack: '' }, true);
        return;
    }
    try { sessionStorage.setItem('o2_dl_retries', String(tries + 1)); } catch (e) {}
    var wait = [1500, 3000, 6000, 10000][tries];
    setStatus('The connection dropped while downloading the editor — retrying…');
    var go = function () { location.reload(); };
    // offline: wait for the network to come back rather than burn the tries
    if (navigator.onLine === false) window.addEventListener('online', function () { setTimeout(go, 800); }, { once: true });
    else setTimeout(go, wait);
}

function handleCrash(message, stack) {
    if (crashHandled) return;
    crashHandled = true;
    message = String(message || 'wasm crash');
    if (/fetching of the wasm failed/.test(message)) { handleDownloadFailure(message); return; }
    console.error('[shell.crash]', message, stack);
    setStatus('The editor crashed — restarting…');
    symbolizeStack(String(stack || '')).then(function (sym) {
        var record = { t: new Date().toISOString(), message: message, stack: sym.slice(0, 8000) };
        try {
            fetch(o2Base + '/api/agent/log', {
                method: 'POST', headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ t: record.t, kind: 'crash', message: message, stack: record.stack.slice(0, 4000) }),
            }).catch(function () {});
        } catch (e) {}
        var times = [];
        try { times = JSON.parse(sessionStorage.getItem('o2_crash_times') || '[]'); } catch (e) {}
        var now = Date.now();
        times = times.filter(function (t) { return now - t < 120000; });
        times.push(now);
        try {
            sessionStorage.setItem('o2_crash_times', JSON.stringify(times));
            sessionStorage.setItem('o2_crash', JSON.stringify(record));
        } catch (e) {}
        // a crash loop would reload forever; three strikes and the page waits for a human
        if (times.length >= 3) {
            try { sessionStorage.removeItem('o2_crash'); } catch (e) {}
            showCrashBanner(record, true);
            return;
        }
        // pending MEMFS -> server mirror writes should land before the world resets
        var drained = window.__o2DrainMirror ? window.__o2DrainMirror() : Promise.resolve();
        Promise.race([drained, new Promise(function (r) { setTimeout(r, 2500); })])
            .then(function () { location.reload(); });
    });
}

window.addEventListener('error', function (e) {
    if (isWasmCrash(e.error, e.message)) {
        e.preventDefault();
        handleCrash(e.message || String(e.error), e.error && e.error.stack);
    }
});
window.addEventListener('unhandledrejection', function (e) {
    if (isWasmCrash(e.reason)) {
        e.preventDefault();
        handleCrash(String((e.reason && e.reason.message) || e.reason), e.reason && e.reason.stack);
    }
});

// the previous life of this tab ended in a crash: say so, with the stack
try {
    var prevCrash = sessionStorage.getItem('o2_crash');
    if (prevCrash) {
        sessionStorage.removeItem('o2_crash');
        showCrashBanner(JSON.parse(prevCrash), false);
    }
} catch (e) {}

var Module = {
    canvas: (function () {
        var c = document.getElementById('canvas');
        c.addEventListener('webglcontextlost', function (e) {
            console.error('[shell] webglcontextlost');
            setStatus('WebGL context lost — reload the page');
            e.preventDefault();
        }, false);
        return c;
    })(),
    print: function (text) { console.log('[wasm.out]', text); engineLog('out', text); },
    printErr: function (text) { console.error('[wasm.err]', text); lastErrLines.push(String(text)); if (lastErrLines.length > 8) lastErrLines.shift(); engineLog('err', text); },
    setStatus: function (text) {
        if (text) console.log('[shell.setStatus]', text);
        var m = text && text.match(/([^\(]+)\((\d+(\.\d+)?)\/(\d+)\)/);
        if (m) setStatus(m[1].trim(), parseFloat(m[2]) / parseFloat(m[4]));
        else if (text) setStatus(text);
        else setStatus(null);
    },
    preRun: [function () {
        Module.addRunDependency('project-snapshot');
        loadSnapshot(
            function () { Module.removeRunDependency('project-snapshot'); },
            function () { /* startup halts with the error shown */ });
    }],
    onRuntimeInitialized: function () {
        console.log('[shell.onRuntimeInitialized]');
        try { sessionStorage.removeItem('o2_dl_retries'); } catch (e) {}
        // Assets restored from a zip are only sources: build them before the user
        // wonders why the editor still shows the old project
        // A working copy that has never been built (a project just imported from
        // its repository) has no game data at all: build it, then start over once,
        // because the editor has already tried to open its scene on nothing.
        var neverBuilt = false;
        try { neverBuilt = !Module.FS.analyzePath('/project/BuiltAssets/WebAssembly/Data.json').exists; } catch (e) {}
        if (sessionStorage.getItem('o2_rebuild_after_load') || neverBuilt) {
            sessionStorage.removeItem('o2_rebuild_after_load');
            setStatus(neverBuilt ? 'Building the project\'s assets for the first time…' : 'Building the restored assets…');
            setTimeout(function () {
                var ok = true;
                try { Module._o2_web_rebuild_assets(); }
                catch (e) { ok = false; console.error('[shell] rebuild after load failed', e); }
                if (neverBuilt && ok && !sessionStorage.getItem('o2_first_build:' + o2Base)) {
                    sessionStorage.setItem('o2_first_build:' + o2Base, '1');   // once: a build that leaves no Data.json must not loop
                    setStatus('Opening the project…');
                    (window.__o2DrainMirror ? window.__o2DrainMirror() : Promise.resolve())
                        .then(function () { return fetch(o2Base + '/api/session/built', { method: 'POST' }).catch(function () {}); })
                        .then(function () { location.reload(); });
                    return;
                }
                setStatus(null);
            }, 1500);
        }
    },
    onAbort: function (reason) { handleCrash('abort: ' + reason, new Error().stack); },
    onExit: function (code) { console.log('[shell.onExit]', code); }
};
setStatus('Downloading editor…');
window.onerror = function (e) { console.error('[shell]', e); };
