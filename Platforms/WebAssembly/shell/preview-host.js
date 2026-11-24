// The preview pane: the game client of this session, running in its own frame.
//
// The page has two faces over one working copy — the editor with the agent, and
// the game with the agent — and the bar switches between them mid-session. Both
// keep running: the client is loaded once and goes on playing behind the editor,
// so switching back is instant and the game keeps its state. It picks up new
// assets the way it always does — rebuild_assets, then restart.
//
// Everything the agent does in preview mode goes through the same tools as in the
// editor; ai.js just aims them at this frame's Module and canvas (see o2Preview).

var o2Preview = (function () {
    var root = document.getElementById('preview');
    var stage = document.getElementById('preview-stage');
    var screen = document.getElementById('preview-screen');
    var hint = document.getElementById('preview-hint');
    var sizeLabel = document.getElementById('pv-size');
    var restartBtn = document.getElementById('pv-restart');

    var frame = null;          // the client; it outlives a switch back to the editor
    var crashed = false;       // a dead client is remounted on the next switch
    var mode = 'editor';
    var ready = false;
    var listeners = [];

    // Portrait sizes; landscape swaps them. "Fit" follows the pane, which is what
    // you want while working, the fixed ones are for checking a real device — and
    // each is drawn in its own body, so the shape you are testing is on screen.
    // w×h: the CSS viewport the game gets; px: the real screen; inch: its diagonal;
    // look, pad ([sides, top and bottom] in portrait), r (the screen's corners): the
    // body; safe: [top, bottom] of the safe area a game on the whole screen gets in
    // portrait (safeAreaOf). The portal (o2portal frontend ui/devices.ts) and the o2 editor's Game window (GameDevices.cpp) keep the same list.
    function D(id, name, kind, look, w, h, px, inch, pad, r, safe) { return { id: id, name: name, kind: kind, look: look, w: w, h: h, px: px, inch: inch, pad: pad, r: r, safe: safe || [0, 0] }; }
    var DEVICES = [
        { id: 'fit', name: 'Fit to pane', kind: 'fit', look: 'fit' },
        { group: 'Phones' },
        D('iphone-se', 'iPhone SE', 'phone', 'home', 375, 667, [750, 1334], 4.7, [16, 72], 0, [0, 0]),
        D('iphone-16', 'iPhone 16', 'phone', 'island', 393, 852, [1179, 2556], 6.1, [12, 12], 55, [59, 34]),
        D('iphone-16-plus', 'iPhone 16 Plus', 'phone', 'island', 430, 932, [1290, 2796], 6.7, [12, 12], 55, [59, 34]),
        D('iphone-16-pro', 'iPhone 16 Pro', 'phone', 'island', 402, 874, [1206, 2622], 6.3, [10, 10], 62, [62, 34]),
        D('iphone-16-pro-max', 'iPhone 16 Pro Max', 'phone', 'island', 440, 956, [1320, 2868], 6.9, [10, 10], 62, [62, 34]),
        D('pixel-8', 'Pixel 8', 'phone', 'punch', 412, 915, [1080, 2400], 6.2, [10, 11], 42, [32, 0]),
        D('pixel-8-pro', 'Pixel 8 Pro', 'phone', 'punch', 448, 998, [1344, 2992], 6.7, [9, 10], 40, [32, 0]),
        D('galaxy-s24', 'Galaxy S24', 'phone', 'punch', 360, 780, [1080, 2340], 6.2, [8, 9], 36, [28, 0]),
        D('galaxy-s24-ultra', 'Galaxy S24 Ultra', 'phone', 'punch', 384, 824, [1440, 3120], 6.8, [8, 8], 14, [28, 0]),
        { group: 'Tablets' },
        D('ipad-mini', 'iPad mini', 'tablet', 'tablet', 744, 1133, [1488, 2266], 8.3, [22, 22], 22, [24, 20]),
        D('ipad', 'iPad', 'tablet', 'tablet', 820, 1180, [1640, 2360], 10.9, [24, 24], 22, [24, 20]),
        D('ipad-pro-11', 'iPad Pro 11″', 'tablet', 'tablet', 834, 1210, [1668, 2420], 11, [20, 20], 22, [24, 20]),
        D('ipad-pro-13', 'iPad Pro 13″', 'tablet', 'tablet', 1032, 1376, [2064, 2752], 13, [22, 22], 22, [24, 20]),
        D('galaxy-tab-s9', 'Galaxy Tab S9', 'tablet', 'tablet', 800, 1280, [1600, 2560], 11, [18, 18], 14, [0, 0]),
        { group: 'Desktop' },
        D('steam-deck', 'Steam Deck', 'desktop', 'console', 1280, 800, [1280, 800], 7, [36, 190], 4),
        D('720p', 'Window 720p', 'desktop', 'window', 1280, 720, [1280, 720], 0, [0, 0], 0),
        D('1080p', 'Window 1080p', 'desktop', 'window', 1920, 1080, [1920, 1080], 0, [0, 0], 0),
    ];
    DEVICES.forEach(function (d) { if (d.id) d.label = d.w ? d.name + ' — ' + d.w + '×' + d.h : d.name; });
    // what the menu says of a device: its real screen and its size
    function specOf(d) { return d.w ? d.px[0] + '×' + d.px[1] + (d.inch ? ' · ' + d.inch + '″' : '') : ''; }

    // The body around the screen, in the device's CSS pixels: padding (top, right,
    // bottom, left), the frame, the outer corner, how far the side buttons stick out
    function bodyOf(d, landscape) {
        if (d.look === 'fit') return { pt: 0, pr: 0, pb: 0, pl: 0, border: 0, outer: 4, reach: 0 };
        if (d.look === 'window') return { pt: 30, pr: 0, pb: 0, pl: 0, border: 1, outer: 10, reach: 0 };
        if (d.look === 'console') return { pt: 40, pr: 190, pb: 40, pl: 190, border: 0, outer: 80, reach: 0 };
        var flip = landscape && d.kind !== 'desktop';
        var x = flip ? d.pad[1] : d.pad[0], y = flip ? d.pad[0] : d.pad[1];
        var border = d.look === 'tablet' ? 3 : 4;
        return { pt: y, pr: x, pb: y, pl: x, border: border, outer: d.look === 'home' ? 60 : d.r + Math.min(x, y) + border, reach: 4 };
    }
    // The safe area of a game on the device's whole screen, in its CSS pixels. An
    // iPhone SE and the Android phones hide their bars in a full-screen game, an iPad
    // keeps them either way; a phone on its side has turned counterclockwise (the
    // island or the camera at the left): an iPhone keeps both sides clear and a
    // shorter home bar, an Android phone only the camera's side. (devices.ts: safeAreaOf)
    var NO_SAFE = { top: 0, right: 0, bottom: 0, left: 0 };
    function safeAreaOf(d, landscape) {
        var top = d.safe ? d.safe[0] : 0, bottom = d.safe ? d.safe[1] : 0;
        if (!top && !bottom) return NO_SAFE;
        if (!landscape || d.kind === 'desktop' || d.look === 'tablet') return { top: top, right: 0, bottom: bottom, left: 0 };
        return d.look === 'island' ? { top: 0, right: top, bottom: 21, left: top } : { top: 0, right: 0, bottom: 0, left: top };
    }
    // the side buttons: edge, where along it and how long; a body on its side has
    // turned counterclockwise (the left edge is at the bottom, the top at the left)
    var BUTTONS = {
        island: [['l', .18, .045], ['l', .27, .08], ['l', .37, .08], ['r', .29, .12], ['r', .6, .07]],
        punch: [['r', .26, .08], ['r', .38, .15]],
        home: [['l', .15, .04], ['l', .23, .07], ['l', .32, .07], ['r', .23, .09]],
        tablet: [['t', .82, .06], ['r', .08, .08], ['r', .18, .08]],
    };
    function buttonsOf(d, landscape) {
        return (BUTTONS[d.look] || []).map(function (b) {
            if (!landscape || d.kind === 'desktop') return b;
            if (b[0] === 'l') return ['b', b[1], b[2]];
            if (b[0] === 'r') return ['t', b[1], b[2]];
            return [b[0] === 't' ? 'l' : 'r', 1 - b[1] - b[2], b[2]];
        });
    }

    // Silhouettes for the picker: the menu shows what each preset is, not just
    // its numbers
    var SHAPES = {
        fit: '<svg viewBox="0 0 24 24"><rect x="2.5" y="4.5" width="19" height="15" rx="2"/><path d="M7 9h4M7 12h7"/></svg>',
        desktop: '<svg viewBox="0 0 24 24"><rect x="2.5" y="4.5" width="19" height="12" rx="1.5"/><path d="M9 19.5h6M12 16.5v3"/></svg>',
        phone: '<svg viewBox="0 0 24 24"><rect x="7.5" y="2.5" width="9" height="19" rx="2"/><path d="M10.6 4.6h2.8"/></svg>',
        tablet: '<svg viewBox="0 0 24 24"><rect x="5.5" y="2.5" width="13" height="19" rx="2"/><circle cx="12" cy="19" r=".9"/></svg>',
    };
    // the bar's own icons (16px, stroked like the rest of the bar)
    function svg(d) { return '<svg class="icon" viewBox="0 0 16 16"><path d="' + d + '" fill="none" stroke="currentColor" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round"/></svg>'; }
    var ICON = {
        portrait: svg('M5.5 1.8h5a1.2 1.2 0 011.2 1.2v10a1.2 1.2 0 01-1.2 1.2h-5A1.2 1.2 0 014.3 13V3a1.2 1.2 0 011.2-1.2zM7.2 12.3h1.6'),
        landscape: svg('M1.8 5.5a1.2 1.2 0 011.2-1.2h10a1.2 1.2 0 011.2 1.2v5a1.2 1.2 0 01-1.2 1.2H3a1.2 1.2 0 01-1.2-1.2zM12.3 7.2v1.6'),
        restart: svg('M13 8a5 5 0 11-1.5-3.5M13 2.5v3h-3'),
    };
    // A game is a phone game until told otherwise: the pane opens as an upright
    // iPhone, and remembers what the visitor picked instead.
    function stored(key) { try { return localStorage.getItem(key); } catch (e) { return null; } }
    function remember(key, value) { try { localStorage.setItem(key, value); } catch (e) {} }
    var OLD_IDS = { 'iphone-15': 'iphone-16', 'iphone-15-pro-max': 'iphone-16-plus' };
    function byId(id) { id = OLD_IDS[id] || id; for (var i = 0; i < DEVICES.length; i++) if (DEVICES[i].id === id) return DEVICES[i]; return null; }
    // The portal names the project's own device and orientation (?device=&orient=: its Settings → Play): the pane
    // opens on them every time, and a pick here lasts as long as the page. Alone, the page remembers the last pick.
    function param(k) { var m = location.search.match(new RegExp('[?&]' + k + '=([\\w-]+)')); return m ? m[1] : ''; }
    var given = !!byId(param('device'));
    var device = byId(param('device')) || byId(stored('o2pv_device')) || byId('iphone-16');
    var orientation = (given ? param('orient') : stored('o2pv_orient')) === 'landscape' ? 'landscape' : 'portrait';  // meaningful for the fixed sizes only

    // On a phone there is nothing to emulate: the pane is the device. One rule for
    // the whole shell — a narrow page, or a touch screen lying on its side — and
    // the stylesheet follows it through body.handheld, so it is written down once.
    var MOBILE_QUERY = '(max-width: 820px), (pointer: coarse) and (max-height: 600px)';
    var mobileMq = window.matchMedia ? window.matchMedia(MOBILE_QUERY) : null;
    var mobile = !!(mobileMq && mobileMq.matches);
    var immersive = false;     // full screen: the game and nothing else
    var solo = false;          // the portal shows this page for its agent alone (a tab that is neither face)

    function emit(kind) { listeners.forEach(function (fn) { try { fn(kind); } catch (e) {} }); }

    // The screen gets a body around it (built here so a UI change needs no wasm
    // relink), and the hint gets a progress bar for the client's own loading.
    var device_el = document.createElement('div');
    device_el.id = 'preview-device';
    screen.parentNode.insertBefore(device_el, screen);
    device_el.appendChild(screen);
    // the body's details (the island, the camera, the home bar, the buttons) are made for each device in layout()
    var bezel = document.createElement('div');
    bezel.className = 'pv-bezel';
    device_el.appendChild(bezel);
    var DETAILS = {
        island: ['pv-island', 'pv-hbar'], punch: ['pv-cam'], tablet: ['pv-cam'], home: ['pv-speaker', 'pv-cam', 'pv-home'],
        window: ['pv-dots'], console: ['pv-stick l', 'pv-stick r', 'pv-tpad l', 'pv-tpad r', 'pv-dpad', 'pv-abxy'],
    };
    var builtFor = '';
    function dressBody(d, landscape) {
        var key = d.id + ':' + landscape;
        if (builtFor === key) return;
        builtFor = key;
        var html = (DETAILS[d.look] || []).map(function (c) { return '<i class="' + c + '"></i>'; }).join('');
        buttonsOf(d, landscape).forEach(function (b) {
            var along = b[0] === 'l' || b[0] === 'r' ? 'top:' + (b[1] * 100) + '%;height:' + (b[2] * 100) + '%' : 'left:' + (b[1] * 100) + '%;width:' + (b[2] * 100) + '%';
            html += '<i class="pv-btn ' + b[0] + '" style="' + along + '"></i>';
        });
        bezel.innerHTML = html;
    }

    var progressWrap = document.createElement('div');
    progressWrap.className = 'pv-progress';
    progressWrap.innerHTML = '<span></span>';
    var progressBar = progressWrap.firstChild;
    hint.parentNode.insertBefore(progressWrap, hint.nextSibling);

    function setHint(text, frac) {
        if (!text) {
            hint.style.display = 'none';
            progressWrap.style.display = 'none';
            return;
        }
        hint.style.display = '';
        hint.textContent = text;
        progressWrap.style.display = typeof frac === 'number' ? '' : 'none';
        if (typeof frac === 'number')
            progressBar.style.width = (Math.max(0, Math.min(1, frac)) * 100).toFixed(1) + '%';
    }

    // ---- geometry ----------------------------------------------------
    // The screen keeps its device size in CSS pixels (so the agent's canvas
    // coordinates are the device's) and is scaled down only to fit the pane.
    function layout() {
        if (mobile) {
            // this device: every pixel of the pane, at the pixel ratio it really has
            screen.style.width = '100%';
            screen.style.height = '100%';
            screen.style.borderRadius = '';
            device_el.className = 'kind-native';
            device_el.removeAttribute('style');
            bezel.innerHTML = ''; builtFor = '';
            sizeLabel.textContent = screen.clientWidth + '×' + screen.clientHeight;
            tellSafeArea();
            return;
        }
        var pane = stage.getBoundingClientRect();
        // full screen keeps the emulated device, with air around its body, and gives
        // "Fit" the whole window
        var pad = !immersive ? 40 : device.id === 'fit' ? 0 : 64;
        var availW = Math.max(120, pane.width - pad), availH = Math.max(120, pane.height - pad);
        var w, h, scale = 1;
        var land = orientation === 'landscape' && device.kind !== 'desktop';
        var b = bodyOf(device, land);
        if (device.id === 'fit') {
            w = Math.round(availW); h = Math.round(availH);
        } else {
            w = land ? device.h : device.w;
            h = land ? device.w : device.h;
            // the whole body has to fit: the screen, the bezels, the frame and the buttons
            var ow = w + b.pl + b.pr + 2 * (b.border + b.reach), oh = h + b.pt + b.pb + 2 * (b.border + b.reach);
            scale = Math.min(1, availW / ow, availH / oh);
        }
        screen.style.width = w + 'px';
        screen.style.height = h + 'px';
        screen.style.borderRadius = device.look === 'window' ? '0 0 9px 9px' : (device.r || 0) + 'px';
        device_el.className = 'kind-' + device.kind + ' look-' + device.look + ' ' + (land ? 'is-landscape' : 'is-portrait');
        var st = device_el.style;
        st.padding = b.pt + 'px ' + b.pr + 'px ' + b.pb + 'px ' + b.pl + 'px';
        st.borderWidth = b.border + 'px';
        st.borderRadius = b.outer + 'px';
        [['--pt', b.pt], ['--pr', b.pr], ['--pb', b.pb], ['--pl', b.pl], ['--bd', b.border], ['--reach', b.reach]]
            .forEach(function (v) { st.setProperty(v[0], v[1] + 'px'); });
        dressBody(device, land);
        device_el.style.transform = scale < 1 ? 'scale(' + scale + ')' : '';
        sizeLabel.textContent = w + '×' + h + (scale < 1 ? '  ·  ' + Math.round(scale * 100) + '%' : '');
        tellSafeArea();
    }

    // ---- the game's safe area ----------------------------------------
    // The client reads window.o2SafeArea = {top, right, bottom, left} (CSS pixels of
    // its page) every frame and keeps its interface inside. A frame cannot see the
    // screen: an emulated device's come from its table (the screen is the device's
    // size, the body is only scaled); otherwise it is what of the screen's notch and
    // home bar reaches into the frame - this page's own env(), or what the portal
    // measured around this page (--o2-safe-*, the embed block below).
    var safeProbe = null, sentSafe = '';
    function measuredSafe() {
        if (!safeProbe) {
            safeProbe = document.createElement('div');
            safeProbe.style.cssText = 'position:fixed;left:0;top:0;width:0;height:0;visibility:hidden;pointer-events:none;padding:' +
                ['top', 'right', 'bottom', 'left'].map(function (s) {
                    return 'max(env(safe-area-inset-' + s + ', 0px), var(--o2-safe-' + s + ', 0px))';
                }).join(' ');
            document.body.appendChild(safeProbe);
        }
        var cs = getComputedStyle(safeProbe), r = frame.getBoundingClientRect();
        function side(v, gap) { return Math.max(0, Math.round((parseFloat(v) || 0) - gap)); }
        return { top: side(cs.paddingTop, r.top), right: side(cs.paddingRight, document.documentElement.clientWidth - r.right),
                 bottom: side(cs.paddingBottom, window.innerHeight - r.bottom), left: side(cs.paddingLeft, r.left) };
    }
    // (a frame of a face nobody looks at has no box to measure: it is told when its face is back - setMode lays it out)
    function tellSafeArea(force) {
        if (!frame || mode !== 'preview') return;
        var land = orientation === 'landscape' && device.kind !== 'desktop';
        var s = !mobile && device.id !== 'fit' ? safeAreaOf(device, land) : measuredSafe();
        var key = JSON.stringify(s);
        if (!force && key === sentSafe) return;
        sentSafe = key;
        var win = frame.contentWindow;
        if (!win) return;
        try { win.o2SafeArea = { top: s.top, right: s.right, bottom: s.bottom, left: s.left }; } catch (e) {}
        try { win.postMessage({ o2SafeArea: s }, location.origin); } catch (e) {}
    }

    window.addEventListener('resize', function () { if (mode === 'preview') layout(); });
    // the pane also changes when the agent panel is resized or folded away
    if (window.ResizeObserver)
        new ResizeObserver(function () { if (mode === 'preview') layout(); }).observe(stage);

    // (the agent alone, on a phone, is the same sheet over nothing: body.agent-solo)
    function applyHandheld() {
        document.body.classList.toggle('handheld', mobile && (mode === 'preview' || solo));
        document.body.classList.toggle('agent-solo', mobile && solo);
    }
    // Frames go to the face somebody looks at (boot.js: o2Power). `onScreen` is the portal's word: it keeps
    // this page while other tabs of the project are open, and shows it for its agent alone.
    var onScreen = true;
    function applyPower() {
        if (window.o2Power) o2Power.show({ editor: onScreen && mode === 'editor', preview: onScreen && mode === 'preview' });
    }
    function setOnScreen(on) {
        on = !!on;
        if (on === onScreen) return;
        onScreen = on;
        applyPower();
        emit('screen');
    }
    applyPower();
    function setSolo(on) {
        on = !!on;
        if (on === solo) return;
        solo = on;
        applyHandheld();
        emit('solo');
    }
    function onMobileChange() {
        if (mobile === mobileMq.matches) return;
        mobile = mobileMq.matches;
        applyHandheld();
        emit('mobile');
        if (mode === 'preview') layout();
    }
    if (mobileMq) {
        if (mobileMq.addEventListener) mobileMq.addEventListener('change', onMobileChange);
        else if (mobileMq.addListener) mobileMq.addListener(onMobileChange);
    }

    // ---- the client --------------------------------------------------
    function mount() {
        if (frame) return;
        ready = false;
        crashed = false;
        setHint('Loading the game client…', 0);
        frame = document.createElement('iframe');
        frame.id = 'preview-frame';
        frame.setAttribute('title', 'Game preview');
        frame.src = 'GamePreview.html';
        frame.addEventListener('load', watchFrame);
        screen.appendChild(frame);
    }

    function unmount() {
        if (!frame) return;
        frame.remove();
        frame = null;
        ready = false;
        emit('unmounted');
    }

    window.addEventListener('message', function (e) {
        if (e.origin !== location.origin || !e.data || !e.data.o2preview) return;
        if (e.data.o2preview === 'ready') {
            ready = true;
            setHint(null);
            emit('ready');
        } else if (e.data.o2preview === 'progress') {
            if (!ready) setHint(e.data.text || 'Loading the game client…',
                                typeof e.data.frac === 'number' ? e.data.frac : undefined);
        } else if (e.data.o2preview === 'crash') {
            ready = false;
            crashed = true;
            setHint('The game crashed: ' + e.data.message);
            emit('crash');
        }
    });

    // Restart with what is on disk now: the client rebuilds nothing by itself, so
    // whatever was already built (by the agent's rebuild_assets, or by the editor
    // before the switch) is what the game starts on.
    function restart() {
        if (!frame) { mount(); return Promise.resolve({ reloaded: true }); }
        var win = frame.contentWindow;
        if (ready && win && win.Module && typeof win.Module._o2_web_restart === 'function') {
            win.Module._o2_web_restart();
            return Promise.resolve({ restarted: true });
        }
        // not up yet (or the client crashed): a fresh frame is the restart
        unmount();
        mount();
        return Promise.resolve({ reloaded: true });
    }

    // A new client on the files as the server has them now. A restart keeps the client's copy of them, and that has
    // nothing of what the editor next door built after the client had loaded (boot.js: the rebuild after a page load).
    function reload() {
        if (!frame) return;
        unmount();
        mount();
    }

    restartBtn.onclick = function () { restart(); };
    restartBtn.className = 'pv-btn-bar';
    restartBtn.innerHTML = ICON.restart + 'Restart';

    // ---- full screen -------------------------------------------------
    // The game and nothing else: the bar and the agent go away here, the page
    // this one is embedded in hides its own header when told (see the embed block
    // below). The Fullscreen API is asked as well, from the tap itself — but an
    // iPhone has none for elements, so the look never depends on it.
    var ICON_FULL = '<svg class="icon" viewBox="0 0 16 16"><path d="M2.5 6V2.5H6M10 2.5h3.5V6M13.5 10v3.5H10M6 13.5H2.5V10" fill="none" stroke="currentColor" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round"/></svg>';
    var ICON_UNFULL = '<svg class="icon" viewBox="0 0 16 16"><path d="M6 2.5V6H2.5M13.5 6H10V2.5M10 13.5V10h3.5M2.5 10H6v3.5" fill="none" stroke="currentColor" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round"/></svg>';
    var fsEntered = false;
    function fsElement() { return document.fullscreenElement || document.webkitFullscreenElement || null; }

    var fullBtn = document.createElement('button');
    fullBtn.id = 'pv-full';
    fullBtn.className = 'pv-btn-bar';
    fullBtn.title = 'Full screen: the game and nothing else';
    fullBtn.innerHTML = ICON_FULL + 'Full screen';
    fullBtn.onclick = function () { setImmersive(true); };
    restartBtn.parentNode.insertBefore(fullBtn, restartBtn.nextSibling);

    // the way back: a button in the corner that fades out of the game's way and
    // comes back when something happens near it
    var exitBtn = document.createElement('button');
    exitBtn.id = 'pv-exit';
    exitBtn.title = 'Exit full screen';
    exitBtn.innerHTML = ICON_UNFULL;
    root.appendChild(exitBtn);
    var dimTimer = null, exitArmed = false;
    function wakeExit() {
        exitBtn.classList.remove('dim');
        clearTimeout(dimTimer);
        dimTimer = setTimeout(function () { exitBtn.classList.add('dim'); }, 3000);
    }
    // a tap on the faded button only brings it back: it must not end a game by accident
    exitBtn.addEventListener('pointerdown', function (e) {
        exitArmed = !exitBtn.classList.contains('dim');
        e.stopPropagation();
        wakeExit();
    });
    exitBtn.onclick = function () { if (exitArmed) setImmersive(false); };
    // a mouse has no such accidents: under the pointer it is simply there
    exitBtn.addEventListener('pointermove', function (e) { if (e.pointerType === 'mouse') wakeExit(); });

    var EXIT_REACH = 150;
    function nearExit(x, y) {
        if (!immersive) return;
        var r = exitBtn.getBoundingClientRect();
        var dx = x - (r.left + r.width / 2), dy = y - (r.top + r.height / 2);
        if (dx * dx + dy * dy < EXIT_REACH * EXIT_REACH) wakeExit();
    }
    function onPointer(e) { if (!exitBtn.contains(e.target)) nearExit(e.clientX, e.clientY); }
    // the game is a frame of its own and keeps its events: listen inside it too
    function onFramePointer(e) {
        if (!immersive || !frame) return;
        var r = frame.getBoundingClientRect();
        var k = frame.clientWidth ? r.width / frame.clientWidth : 1;
        nearExit(r.left + e.clientX * k, r.top + e.clientY * k);
    }
    function onEscape(e) { if (e.key === 'Escape' && immersive) setImmersive(false); }
    document.addEventListener('pointerdown', onPointer, true);
    document.addEventListener('pointermove', onPointer, true);
    document.addEventListener('keydown', onEscape);
    function watchFrame() {
        tellSafeArea(true);
        try {
            var doc = frame.contentDocument;
            doc.addEventListener('pointerdown', onFramePointer, true);
            doc.addEventListener('pointermove', onFramePointer, true);
            doc.addEventListener('keydown', onEscape);
        } catch (e) {}
    }

    function setImmersive(on) {
        on = !!on && mode === 'preview';
        if (on === immersive) return;
        immersive = on;
        document.body.classList.toggle('immersive', on);
        var el = document.documentElement;
        if (on) {
            var request = el.requestFullscreen || el.webkitRequestFullscreen;
            if (request) {
                try {
                    var asked = request.call(el, { navigationUI: 'hide' });
                    if (asked && asked.catch) asked.catch(function () {});
                } catch (e) {}
            }
            wakeExit();
        } else if (fsElement()) {
            var leave = document.exitFullscreen || document.webkitExitFullscreen;
            try {
                var left = leave && leave.call(document);
                if (left && left.catch) left.catch(function () {});
            } catch (e) {}
        }
        layout();
        emit('immersive');
        window.dispatchEvent(new Event('resize'));
    }
    // left by the browser's own way out (Esc, the back gesture): follow it
    function onFsChange() {
        if (fsElement()) { fsEntered = true; return; }
        if (!fsEntered) return;
        fsEntered = false;
        setImmersive(false);
    }
    document.addEventListener('fullscreenchange', onFsChange);
    document.addEventListener('webkitfullscreenchange', onFsChange);

    // ---- controls ----------------------------------------------------
    var setOrientation = function () {};
    function buildControls() {
        var deviceSlot = document.getElementById('pv-device-slot');
        var dd = document.createElement('span');
        dd.className = 'pv-dd';
        var btn = document.createElement('button');
        btn.className = 'pv-btn-bar';
        btn.innerHTML = '<svg class="icon" viewBox="0 0 16 16"><use href="#i-device"/></svg>' +
                        '<span class="pv-dd-label"></span>' +
                        '<svg class="icon chev" viewBox="0 0 16 16"><use href="#i-chev"/></svg>';
        var menu = document.createElement('div');
        menu.className = 'pv-menu';
        function shortName(d) { return d.id === 'fit' ? 'Fit' : d.name; }
        function markCurrent() {
            menu.querySelectorAll('button').forEach(function (x) { x.classList.toggle('on', x.dataset.id === device.id); });
            btn.querySelector('.pv-dd-label').textContent = shortName(device);
        }
        DEVICES.forEach(function (d) {
            if (d.group) {
                var head = document.createElement('div');
                head.className = 'pv-group';
                head.textContent = d.group;
                menu.appendChild(head);
                return;
            }
            var item = document.createElement('button');
            item.dataset.id = d.id;
            item.innerHTML = '<span class="pv-shape">' + (SHAPES[d.kind] || '') + '</span>' +
                             '<span class="pv-name">' + d.name + '</span>' +
                             (d.w ? '<span class="pv-dim">' + specOf(d) + '</span>' : '');
            item.title = d.w ? d.name + ': ' + d.px[0] + '×' + d.px[1] + ' px' + (d.inch ? ', ' + d.inch + '″' : '') + ' · the game gets ' + d.w + '×' + d.h : d.name;
            item.onclick = function () {
                device = d;
                if (!given) remember('o2pv_device', d.id);
                menu.classList.remove('open');
                // a monitor lies on its side; nobody wants a 720×1280 "desktop"
                setOrientation(d.kind === 'desktop' ? 'landscape' : orientation);
                markCurrent();
                layout();
            };
            menu.appendChild(item);
        });
        btn.onclick = function (e) { e.stopPropagation(); menu.classList.toggle('open'); };
        document.addEventListener('click', function () { menu.classList.remove('open'); });
        dd.appendChild(btn);
        dd.appendChild(menu);
        deviceSlot.appendChild(dd);
        markCurrent();
        markDevice = markCurrent;

        var orientSlot = document.getElementById('pv-orient');
        var seg = document.createElement('span');
        seg.className = 'pv-seg';
        [['portrait', 'Portrait'], ['landscape', 'Landscape']].forEach(function (o) {
            var b = document.createElement('button');
            b.className = 'pv-btn-bar';
            b.innerHTML = ICON[o[0]] + o[1];
            b.dataset.orient = o[0];
            b.onclick = function () { setOrientation(o[0]); layout(); };
            seg.appendChild(b);
        });
        setOrientation = function (next) {
            orientation = next;
            if (!given) remember('o2pv_orient', next);
            // a window has no portrait: the pair is not offered for one
            seg.style.display = device.kind === 'desktop' || device.kind === 'fit' ? 'none' : '';
            seg.querySelectorAll('button').forEach(function (x) {
                x.classList.toggle('on', x.dataset.orient === orientation);
            });
        };
        setOrientation(orientation);
        orientSlot.appendChild(seg);
    }
    buildControls();

    // the portal's setting changed while the page is up (Settings → Play)
    var markDevice = function () {};
    function setDevice(id, orient) {
        var d = byId(id);
        if (!d) return;
        device = d;
        given = true;
        setOrientation(orient === 'landscape' || d.kind === 'desktop' ? 'landscape' : 'portrait');
        markDevice();
        layout();
    }

    // ---- mode --------------------------------------------------------
    function setMode(next) {
        if (next !== 'preview' && next !== 'editor') return;
        if (next === mode) return;
        if (next !== 'preview') setImmersive(false);
        mode = next;
        document.body.classList.toggle('mode-preview', mode === 'preview');
        applyHandheld();
        applyPower();
        if (mode === 'preview') {
            if (frame && !crashed) {
                // it kept running while the editor was in front: just show it
                layout();
            } else {
                // the editor may still owe the server the tail of a build — worth
                // a moment, never worth making the switch feel broken
                if (crashed) unmount();
                var drained = window.__o2DrainMirror ? window.__o2DrainMirror() : Promise.resolve();
                var bounded = Promise.race([drained, new Promise(function (r) { setTimeout(r, 1500); })]);
                bounded.then(function () {
                    if (mode !== 'preview') return;
                    mount();
                    layout();
                });
                layout();
            }
        }
        // leaving does not unload the client: it stays as it is behind the editor,
        // the way the editor stays behind it — loaded, and without frames (o2Power)
        var sw = document.getElementById('modeswitch');
        sw.classList.toggle('at-preview', mode === 'preview');
        sw.querySelectorAll('button').forEach(function (b) {
            b.classList.toggle('on', b.dataset.mode === mode);
        });
        emit('mode');
        // the editor canvas was hidden while away: let it size itself again
        window.dispatchEvent(new Event('resize'));
    }

    return {
        setMode: setMode,
        mode: function () { return mode; },
        isActive: function () { return mode === 'preview'; },
        isReady: function () { return mode === 'preview' && ready && !!frame; },
        // the client, whichever face is in front: it keeps running in the
        // background, so files changed meanwhile still have to reach its copy
        liveWindow: function () { return ready && frame ? frame.contentWindow : null; },
        frameWindow: function () { return frame && frame.contentWindow; },
        canvas: function () {
            try { return frame.contentDocument.getElementById('canvas'); } catch (e) { return null; }
        },
        restart: restart,
        reload: reload,
        setDevice: setDevice,
        device: function () {
            var w = screen.clientWidth, h = screen.clientHeight;
            if (mobile) return { id: 'native', label: 'This device', orientation: w > h ? 'landscape' : 'portrait', width: w, height: h };
            return { id: device.id, label: device.label, orientation: orientation, width: w, height: h };
        },
        isMobile: function () { return mobile; },
        // the portal measured the screen around this page again (--o2-safe-*)
        tellSafeArea: function () { tellSafeArea(); },
        isImmersive: function () { return immersive; },
        setImmersive: setImmersive,
        isSolo: function () { return solo; },
        setSolo: setSolo,
        isOnScreen: function () { return onScreen; },
        setOnScreen: setOnScreen,
        onChange: function (fn) { listeners.push(fn); },
    };
})();

(function () {
    document.querySelectorAll('#modeswitch button').forEach(function (b) {
        b.onclick = function () { o2Preview.setMode(b.dataset.mode); };
        b.classList.toggle('on', b.dataset.mode === 'editor');
    });
})();

// ---- embedded in the o2 portal ---------------------------------------
// The portal shows this page in a frame under its own header: which face is in
// front and whether the agent panel is out is decided up there (the Editor and
// Play tabs, the Agent button), and what used to be buttons of this bar — the
// changed files, the asset browser, zip export — lives in the project's Git tab.
// Full screen goes the other way: asked for here ({o2shell:'immersive'}), and the
// page takes its header off; it can end it too ({o2portal:'immersive', on:false}).
// {o2portal:'viewport'} brings what a frame cannot see — safe areas, the keyboard.
// {o2portal:'solo'}: the frame is up for the agent alone, on a tab that is neither face.
// {o2portal:'shown'}: whether a face of this page is on the screen at all (it is kept while other tabs are open).
(function () {
    if (window.parent === window || !/[?&]embed=1/.test(location.search)) return;
    document.body.classList.add('embed');

    // A tab of the portal (?title=Play): the game face's bar is that tab's header, as the portal's other tabs have it -
    // the title, the controls as the page's own buttons, in the portal's fonts
    var titled = location.search.match(/[?&]title=([^&]+)/);
    if (titled) {
        var fonts = document.createElement('link');
        fonts.rel = 'stylesheet';
        fonts.href = 'https://fonts.googleapis.com/css2?family=Geologica:wght@600&family=Onest:wght@400;500;600&display=swap';
        document.head.appendChild(fonts);
        var heading = document.createElement('h2');
        heading.className = 'pv-title';
        heading.textContent = decodeURIComponent(titled[1]);
        var bar = document.getElementById('topbar');
        bar.insertBefore(heading, bar.firstChild);
        document.body.classList.add('portal-tab');
    }

    // git moved the tree since the built assets were made: build once after boot
    var m = location.search.match(/[?&]rebuild=([\w-]+)/);
    var revKey = 'o2_rebuilt_rev:' + o2Base;
    if (m && sessionStorage.getItem(revKey) !== m[1]) {
        sessionStorage.setItem(revKey, m[1]);
        if (m[1] !== '0') sessionStorage.setItem('o2_rebuild_after_load', '1');
    }

    // on a phone the agent is a row that is always there: "shown" is its sheet (ai.js)
    function agentShown() {
        return window.__o2AgentShown ? window.__o2AgentShown() : !document.body.classList.contains('ai-hidden');
    }
    // dock: how wide the agent's panel is - outside the editor tabs the portal shows this frame as a drawer of
    // exactly that width (the agent on every tab of a project), so dragging the panel's edge resizes the drawer
    function dockWidth() {
        var v = parseInt(getComputedStyle(document.body).getPropertyValue('--ai-dock'), 10);
        return v > 0 ? v : 420;
    }
    function report() {
        parent.postMessage({ o2shell: 'state', mode: o2Preview.mode(), agent: agentShown(), dock: dockWidth(),
                             immersive: o2Preview.isImmersive(), solo: o2Preview.isSolo(), shown: o2Preview.isOnScreen() }, location.origin);
    }
    function px(v) { return (Math.max(0, Math.round(+v || 0))) + 'px'; }

    window.addEventListener('message', function (e) {
        if (e.origin !== location.origin || !e.data || !e.data.o2portal) return;
        var d = e.data;
        if (d.o2portal === 'mode') {
            o2Preview.setMode(d.mode);
        } else if (d.o2portal === 'agent') {
            var hidden = !agentShown();
            if (window.__o2ToggleAgent && (d.open === undefined || d.open === hidden)) window.__o2ToggleAgent();
        } else if (d.o2portal === 'sync') {
            // files the portal's own browser saved or removed under Assets/
            if (window.__o2SyncFiles) window.__o2SyncFiles(d.changed || [], d.deleted || []);
        } else if (d.o2portal === 'device') {
            o2Preview.setDevice(d.device, d.orient);
        } else if (d.o2portal === 'solo') {
            o2Preview.setSolo(!!d.on);
        } else if (d.o2portal === 'shown') {
            o2Preview.setOnScreen(!!d.on);
        } else if (d.o2portal === 'immersive') {
            // the page left the Play tab under a game in full screen
            o2Preview.setImmersive(!!d.on);
        } else if (d.o2portal === 'viewport') {
            // what only the top page can see: the notch and the home bar around this
            // frame, and how much of it the on-screen keyboard covers. Not answered
            // with a state report, or the two pages would talk forever.
            var inset = d.insets || {}, st = document.documentElement.style;
            st.setProperty('--o2-safe-top', px(inset.top));
            st.setProperty('--o2-safe-right', px(inset.right));
            st.setProperty('--o2-safe-bottom', px(inset.bottom));
            st.setProperty('--o2-safe-left', px(inset.left));
            st.setProperty('--o2-kb', px(d.keyboard));
            document.documentElement.classList.toggle('kb-open', d.keyboard > 0);
            o2Preview.tellSafeArea();
            return;
        } else if (d.o2portal === 'reload') {
            if (d.rebuild) sessionStorage.setItem('o2_rebuild_after_load', '1');
            (window.__o2DrainMirror ? window.__o2DrainMirror() : Promise.resolve())
                .then(function () { location.reload(); });
            return;
        }
        report();
    });

    o2Preview.onChange(function (kind) {
        if (kind === 'immersive') parent.postMessage({ o2shell: 'immersive', on: o2Preview.isImmersive() }, location.origin);
        report();
    });
    // class: the panel shown or hidden, the face; style: --ai-dock while its edge is dragged
    new MutationObserver(report).observe(document.body, { attributes: true, attributeFilter: ['class', 'style'] });
    report();
})();
