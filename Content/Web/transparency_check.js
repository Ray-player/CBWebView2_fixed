// 注入到页面后，按“真实可交互内容”而不是“任意可见容器”判断是否拦截鼠标。
(function() {
    var PASS_THROUGH_CLASS = 'transparent-pass-through';
    var FORCE_INTERACTIVE_CLASS = 'transparent-force-interactive';
    var PASS_THROUGH_ATTR = 'data-transparent-pass-through';
    var FORCE_INTERACTIVE_ATTR = 'data-transparent-interactive';
    var AUTO_CUSTOM_HIT_TEST_ATTR = 'data-cbwebview2-auto-hit-test';
    var PAGE_ROOT_HIT_TEST_ATTR = 'data-cbwebview2-page-root-hit-test';
    var DEBUG_OVERLAY_ID = 'cbwebview2-transparency-debug';
    var DEBUG_OVERLAY_DEFAULT_ENABLED = false;
    var INTERACTIVE_ROLES = {
        button: true,
        link: true,
        checkbox: true,
        radio: true,
        switch: true,
        tab: true,
        textbox: true,
        searchbox: true,
        slider: true,
        spinbutton: true,
        combobox: true,
        listbox: true,
        option: true,
        menuitem: true,
        menuitemcheckbox: true,
        menuitemradio: true,
        treeitem: true,
        gridcell: true
    };
    var INTERACTIVE_CURSORS = {
        pointer: true,
        text: true,
        move: true,
        grab: true,
        grabbing: true,
        crosshair: true,
        'col-resize': true,
        'row-resize': true,
        'ew-resize': true,
        'ns-resize': true,
        'nesw-resize': true,
        'nwse-resize': true
    };

    var lastClickable = null;
    var lastPointerEvent = null;
    var rafHandle = 0;
    var installObserver = null;
    var currentDebugInfo = null;
    var debugOverlayEnabled = !!(window.__cbwebview2TransparencyDebugEnabled === true || DEBUG_OVERLAY_DEFAULT_ENABLED);

    function isDebugResult(result) {
        return !!(result && typeof result === 'object' && typeof result.clickable === 'boolean');
    }

    function getCurrentRoute() {
        return window.location.hash || window.location.pathname || window.location.href || '';
    }

    function describeElement(el) {
        if (!el || el.nodeType !== 1) {
            return '(none)';
        }

        var label = el.tagName.toLowerCase();
        if (el.id) {
            label += '#' + el.id;
        }

        if (el.classList && el.classList.length) {
            label += '.' + Array.prototype.slice.call(el.classList, 0, 3).join('.');
        }

        return label;
    }

    function getElementDebugDetails(el, style) {
        var details = {};
        if (!el || el.nodeType !== 1) {
            return details;
        }

        if (!style) {
            style = window.getComputedStyle(el);
        }

        var rect = el.getBoundingClientRect();
        details.role = (el.getAttribute('role') || '');
        details.tabIndex = el.hasAttribute('tabindex') ? String(el.tabIndex) : '';
        details.cursor = style && style.cursor ? style.cursor : '';
        details.pointerEvents = style && style.pointerEvents ? style.pointerEvents : '';
        details.background = style && style.backgroundColor ? style.backgroundColor : '';
        details.size = Math.round(rect.width) + 'x' + Math.round(rect.height);

        if (el.tagName === 'A' && el.getAttribute('href')) {
            details.href = el.getAttribute('href');
        }

        return details;
    }

    function buildDebugResult(clickable, reason, el, details) {
        var result = {
            clickable: !!clickable,
            reason: reason || 'unknown',
            element: describeElement(el),
            route: getCurrentRoute()
        };

        if (details) {
            for (var key in details) {
                if (Object.prototype.hasOwnProperty.call(details, key)) {
                    result[key] = details[key];
                }
            }
        }

        return result;
    }

    function enrichDebugResult(result, x, y, elementsCount, fallbackElement) {
        if (!isDebugResult(result)) {
            return buildDebugResult(!!result, 'custom-hit-test', fallbackElement, {
                point: { x: Math.round(x), y: Math.round(y) },
                elementsCount: elementsCount
            });
        }

        if (!result.element || result.element === '(none)') {
            result.element = describeElement(fallbackElement);
        }

        if (!result.route) {
            result.route = getCurrentRoute();
        }

        if (!result.point) {
            result.point = { x: Math.round(x), y: Math.round(y) };
        }

        if (result.elementsCount == null) {
            result.elementsCount = elementsCount;
        }

        return result;
    }

    function ensureDebugOverlay() {
        if (!document.body) {
            return null;
        }

        var overlay = document.getElementById(DEBUG_OVERLAY_ID);
        if (overlay) {
            return overlay;
        }

        overlay = document.createElement('div');
        overlay.id = DEBUG_OVERLAY_ID;
        overlay.style.position = 'fixed';
        overlay.style.top = '8px';
        overlay.style.right = '8px';
        overlay.style.zIndex = '2147483647';
        overlay.style.maxWidth = '420px';
        overlay.style.padding = '10px 12px';
        overlay.style.borderRadius = '8px';
        overlay.style.fontFamily = 'Consolas, Menlo, Monaco, monospace';
        overlay.style.fontSize = '12px';
        overlay.style.lineHeight = '1.45';
        overlay.style.whiteSpace = 'pre-wrap';
        overlay.style.wordBreak = 'break-word';
        overlay.style.pointerEvents = 'none';
        overlay.style.color = '#ffffff';
        overlay.style.boxShadow = '0 8px 24px rgba(0, 0, 0, 0.35)';
        document.body.appendChild(overlay);
        return overlay;
    }

    function setDebugOverlayEnabled(enabled) {
        debugOverlayEnabled = !!enabled;
        window.__cbwebview2TransparencyDebugEnabled = debugOverlayEnabled;

        var overlay = document.getElementById(DEBUG_OVERLAY_ID);
        if (overlay) {
            overlay.style.display = debugOverlayEnabled ? 'block' : 'none';
        }

        if (debugOverlayEnabled) {
            updateDebugOverlay(currentDebugInfo || buildDebugResult(false, 'debug-overlay-enabled', null, {
                route: getCurrentRoute()
            }));
        }
    }

    function formatDebugInfo(info) {
        var lines = [
            'CBWebView2 HitTest',
            'state: ' + (info.clickable ? 'BLOCK' : 'PASS'),
            'reason: ' + (info.reason || 'unknown'),
            'element: ' + (info.element || '(none)')
        ];

        if (info.point) {
            lines.push('point: ' + info.point.x + ', ' + info.point.y);
        }

        if (info.elementsCount != null) {
            lines.push('stack: ' + info.elementsCount);
        }

        if (info.route) {
            lines.push('route: ' + info.route);
        }

        if (info.role) {
            lines.push('role: ' + info.role);
        }

        if (info.tabIndex) {
            lines.push('tabindex: ' + info.tabIndex);
        }

        if (info.cursor) {
            lines.push('cursor: ' + info.cursor);
        }

        if (info.pointerEvents) {
            lines.push('pe: ' + info.pointerEvents);
        }

        if (info.background) {
            lines.push('bg: ' + info.background);
        }

        if (info.size) {
            lines.push('size: ' + info.size);
        }

        if (info.href) {
            lines.push('href: ' + info.href);
        }

        return lines.join('\n');
    }

    function updateDebugOverlay(info) {
        currentDebugInfo = info || currentDebugInfo;
        if (!currentDebugInfo || !debugOverlayEnabled) {
            var hiddenOverlay = document.getElementById(DEBUG_OVERLAY_ID);
            if (hiddenOverlay) {
                hiddenOverlay.style.display = 'none';
            }
            return;
        }

        var overlay = ensureDebugOverlay();
        if (!overlay) {
            return;
        }

        overlay.style.display = 'block';
        overlay.style.background = currentDebugInfo.clickable ? 'rgba(176, 46, 46, 0.90)' : 'rgba(34, 128, 74, 0.90)';
        overlay.textContent = formatDebugInfo(currentDebugInfo);
    }

    function postClickableState(isClickable, debugInfo) {
        if (debugInfo) {
            updateDebugOverlay(debugInfo);
        }

        if (lastClickable === isClickable) {
            return;
        }

        lastClickable = isClickable;
        if (window.chrome && window.chrome.webview) {
            window.chrome.webview.postMessage('IsClickable:' + (isClickable ? '1' : '0'));
        }
    }

    function getBooleanAttribute(el, attrName) {
        var value = el.getAttribute(attrName);
        return value === '' || value === 'true' || value === '1';
    }

    function hasClass(el, className) {
        return !!(el.classList && el.classList.contains(className));
    }

    function isPassThroughMarked(el) {
        return hasClass(el, PASS_THROUGH_CLASS) || getBooleanAttribute(el, PASS_THROUGH_ATTR);
    }

    function isForceInteractiveMarked(el) {
        return hasClass(el, FORCE_INTERACTIVE_CLASS) || getBooleanAttribute(el, FORCE_INTERACTIVE_ATTR);
    }

    function isElementVisible(el, style) {
        if (!style) {
            style = window.getComputedStyle(el);
        }

        if (style.display === 'none' || style.visibility === 'hidden' || style.visibility === 'collapse') {
            return false;
        }

        if (style.pointerEvents === 'none') {
            return false;
        }

        if (parseFloat(style.opacity || '1') <= 0) {
            return false;
        }

        var rect = el.getBoundingClientRect();
        return rect.width > 0 && rect.height > 0;
    }

    function hasInlineInteraction(el) {
        return typeof el.onclick === 'function' ||
            typeof el.onmousedown === 'function' ||
            typeof el.onmouseup === 'function' ||
            typeof el.onpointerdown === 'function' ||
            typeof el.onpointerup === 'function';
    }

    function hasInteractiveRole(el) {
        var role = (el.getAttribute('role') || '').toLowerCase();
        return !!INTERACTIVE_ROLES[role];
    }

    function hasInteractiveTabIndex(el) {
        if (!el.hasAttribute('tabindex')) {
            return false;
        }

        return !isNaN(el.tabIndex) && el.tabIndex >= 0;
    }

    function hasInteractiveCursor(style) {
        return !!INTERACTIVE_CURSORS[style.cursor];
    }

    function isShellContainer(el) {
        if (!el || el.nodeType !== 1) {
            return false;
        }

        return el === document.body ||
            el === document.documentElement ||
            el.id === 'root' ||
            el.hasAttribute(PAGE_ROOT_HIT_TEST_ATTR);
    }

    function getColorAlpha(colorValue) {
        if (!colorValue || colorValue === 'transparent') {
            return 0;
        }

        var rgbaMatch = colorValue.match(/^rgba\(([^)]+)\)$/i);
        if (rgbaMatch) {
            var rgbaParts = rgbaMatch[1].split(',');
            if (rgbaParts.length >= 4) {
                return parseFloat(rgbaParts[3]);
            }
            return 1;
        }

        if (/^rgb\(/i.test(colorValue)) {
            return 1;
        }

        return 0;
    }

    function hasOpaqueVisualBackground(el, style) {
        if (!el || el.nodeType !== 1) {
            return false;
        }

        if (!style) {
            style = window.getComputedStyle(el);
        }

        if (isCityOverviewShellContainer(el)) {
            return false;
        }

        if (style.backgroundImage && style.backgroundImage !== 'none') {
            return true;
        }

        return getColorAlpha(style.backgroundColor) > 0.05;
    }

    function isCompactPointerInteractiveElement(el, style) {
        if (!hasInteractiveCursor(style)) {
            return false;
        }

        if (isShellContainer(el) || hasCanvasDescendant(el) || hasEChartsHostMarker(el) || isGenericFullScreenContainer(el)) {
            return false;
        }

        var rect = el.getBoundingClientRect();
        return rect.width <= 320 && rect.height <= 160 && rect.width * rect.height <= 24000;
    }

    function isIntrinsicInteractiveElement(el, style) {
        var tagName = el.tagName;

        if (tagName === 'A' && el.hasAttribute('href')) {
            return true;
        }

        if (tagName === 'BUTTON' || tagName === 'SUMMARY' || tagName === 'OPTION') {
            return true;
        }

        if (tagName === 'INPUT' || tagName === 'TEXTAREA' || tagName === 'SELECT') {
            return !el.disabled;
        }

        if (el.isContentEditable) {
            return true;
        }

        if (isForceInteractiveMarked(el)) {
            return true;
        }

        return false;
    }

    function isSignalInteractiveElement(el) {
        if (isShellContainer(el)) {
            return false;
        }

        if (!hasInteractiveSignalExcludingCursor(el)) {
            return false;
        }

        if (hasCanvasDescendant(el) || hasEChartsHostMarker(el) || isGenericFullScreenContainer(el)) {
            return false;
        }

        var rect = el.getBoundingClientRect();
        return rect.width <= 420 && rect.height <= 220 && rect.width * rect.height <= 50000;
    }

    function isSemanticNativeInteractiveElement(el) {
        var tagName = el.tagName;

        if (tagName === 'A' && el.hasAttribute('href')) {
            return true;
        }

        if (tagName === 'BUTTON' || tagName === 'SUMMARY' || tagName === 'OPTION') {
            return true;
        }

        if (tagName === 'INPUT' || tagName === 'TEXTAREA' || tagName === 'SELECT') {
            return !el.disabled;
        }

        return !!el.isContentEditable;
    }

    function isGenericFullScreenContainer(el) {
        var genericTags = {
            DIV: true,
            SECTION: true,
            MAIN: true,
            ARTICLE: true,
            ASIDE: true,
            HEADER: true,
            FOOTER: true,
            NAV: true
        };

        if (!genericTags[el.tagName]) {
            return false;
        }

        var rect = el.getBoundingClientRect();
        var viewportWidth = Math.max(window.innerWidth || 0, document.documentElement.clientWidth || 0);
        var viewportHeight = Math.max(window.innerHeight || 0, document.documentElement.clientHeight || 0);
        var viewportArea = viewportWidth * viewportHeight;
        var area = rect.width * rect.height;

        if (viewportArea <= 0) {
            return false;
        }

        return area >= viewportArea * 0.85;
    }

    function findCustomHitTest(el) {
        var node = el;
        while (node && node.nodeType === 1) {
            if (typeof node.customHitTest === 'function') {
                return node;
            }
            node = node.parentElement;
        }

        return null;
    }

    function runCustomHitTest(el, x, y) {
        var target = findCustomHitTest(el);
        if (!target) {
            return null;
        }

        try {
            var result = target.customHitTest(x, y);
            if (isDebugResult(result)) {
                return result;
            }

            return !!result;
        } catch (error) {
            return null;
        }
    }

    function findEChartsInstanceElement(el) {
        if (!window.echarts || typeof window.echarts.getInstanceByDom !== 'function') {
            return null;
        }

        var node = el;
        while (node && node.nodeType === 1) {
            try {
                if (window.echarts.getInstanceByDom(node)) {
                    return node;
                }
            } catch (error) {
                return null;
            }

            node = node.parentElement;
        }

        return null;
    }

    function runEChartsHitTest(el, x, y) {
        var chartElement = findEChartsInstanceElement(el);
        if (!chartElement) {
            return null;
        }

        try {
            var instance = window.echarts.getInstanceByDom(chartElement);
            if (!instance || typeof instance.getZr !== 'function') {
                return null;
            }

            var zr = instance.getZr();
            if (!zr || !zr.handler || typeof zr.handler.findHover !== 'function') {
                return null;
            }

            var rect = chartElement.getBoundingClientRect();
            var hover = zr.handler.findHover(x - rect.left, y - rect.top);
            return !!(hover && (hover.target || hover.topTarget));
        } catch (error) {
            return null;
        }
    }

    function hasCanvasDescendant(el) {
        return !!(el.querySelector && el.querySelector('canvas'));
    }

    function hasEChartsHostMarker(el) {
        return !!(el.getAttribute && el.getAttribute('_echarts_instance_'));
    }

    function isPointInsideRect(rect, x, y) {
        return x >= rect.left && x <= rect.right && y >= rect.top && y <= rect.bottom;
    }

    function isCityOverviewRoute() {
        var href = window.location.href || '';
        return href.indexOf('#/cityoverview/') >= 0 || href.indexOf('/cityoverview/') >= 0;
    }

    function hasMeaningfulInteractiveDescendantAt(container, x, y) {
        var elements = document.elementsFromPoint(x, y);
        for (var i = 0; i < elements.length; i++) {
            var el = elements[i];
            if (!el || el === container || el.nodeType !== 1) {
                continue;
            }

            if (!container.contains(el) || isPassThroughMarked(el)) {
                continue;
            }

            var style = window.getComputedStyle(el);
            if (!isElementVisible(el, style)) {
                continue;
            }

            if (isForceInteractiveMarked(el) || isIntrinsicInteractiveElement(el, style)) {
                return true;
            }
        }

        return false;
    }

    function runCanvasDescendantHitTest(container, x, y) {
        if (!container.querySelectorAll) {
            return null;
        }

        var canvases = container.querySelectorAll('canvas');
        var checkedCanvas = false;

        for (var i = 0; i < canvases.length; i++) {
            var canvas = canvases[i];
            var rect = canvas.getBoundingClientRect();
            if (!isPointInsideRect(rect, x, y)) {
                continue;
            }

            checkedCanvas = true;
            var pixelHit = runCanvasPixelHitTest(canvas, x, y);
            if (pixelHit === true) {
                return true;
            }
        }

        return checkedCanvas ? false : null;
    }

    function isLikelyCanvasHost(el) {
        if (!el || el.nodeType !== 1 || typeof el.customHitTest === 'function') {
            return false;
        }

        if (el.hasAttribute(AUTO_CUSTOM_HIT_TEST_ATTR) || isPassThroughMarked(el) || isForceInteractiveMarked(el)) {
            return false;
        }

        if (!hasCanvasDescendant(el)) {
            return false;
        }

        if (el.tagName !== 'DIV' && el.tagName !== 'SECTION' && el.tagName !== 'ARTICLE' && el.tagName !== 'MAIN') {
            return false;
        }

        var rect = el.getBoundingClientRect();
        return rect.width >= 40 && rect.height >= 40;
    }

    function isLargeCityOverviewContainer(el) {
        var rect = el.getBoundingClientRect();
        var viewportWidth = Math.max(window.innerWidth || 0, document.documentElement.clientWidth || 0);
        var viewportHeight = Math.max(window.innerHeight || 0, document.documentElement.clientHeight || 0);
        var viewportArea = viewportWidth * viewportHeight;
        if (viewportArea <= 0) {
            return false;
        }

        var area = rect.width * rect.height;
        return area >= viewportArea * 0.30 || rect.width >= viewportWidth * 0.85 || rect.height >= viewportHeight * 0.85;
    }

    function isCityOverviewShellContainer(el) {
        if (!el || el.nodeType !== 1) {
            return false;
        }

        if (el === document.body || el === document.documentElement) {
            return true;
        }

        if (el.id === 'root' || el.hasAttribute(PAGE_ROOT_HIT_TEST_ATTR)) {
            return true;
        }

        return false;
    }

    function isSmallCityOverviewInteractiveRegion(el) {
        var rect = el.getBoundingClientRect();
        return rect.width <= 320 && rect.height <= 160 && rect.width * rect.height <= 24000;
    }

    function hasInteractiveSignalExcludingCursor(el) {
        return isForceInteractiveMarked(el) || hasInlineInteraction(el) || hasInteractiveRole(el) || hasInteractiveTabIndex(el);
    }

    function isLikelyDelegatedInteractiveElement(el, style) {
        if (isCityOverviewShellContainer(el)) {
            return false;
        }

        if (hasInteractiveSignalExcludingCursor(el)) {
            if (hasCanvasDescendant(el) || hasEChartsHostMarker(el) || isLargeCityOverviewContainer(el)) {
                return false;
            }

            return isSmallCityOverviewInteractiveRegion(el);
        }

        if (!hasInteractiveCursor(style)) {
            return false;
        }

        if (hasCanvasDescendant(el) || hasEChartsHostMarker(el) || isLargeCityOverviewContainer(el)) {
            return false;
        }

        return isSmallCityOverviewInteractiveRegion(el);
    }

    function evaluateCityOverviewInteractiveContent(x, y) {
        var elements = document.elementsFromPoint(x, y);

        for (var i = 0; i < elements.length; i++) {
            var el = elements[i];
            if (!el || el.nodeType !== 1 || el.tagName === 'HTML' || el.tagName === 'BODY') {
                continue;
            }

            if (isPassThroughMarked(el)) {
                continue;
            }

            var style = window.getComputedStyle(el);
            if (!isElementVisible(el, style)) {
                continue;
            }

            if (isSemanticNativeInteractiveElement(el)) {
                return buildDebugResult(true, 'cityoverview-intrinsic-interactive', el, Object.assign({
                    point: { x: Math.round(x), y: Math.round(y) },
                    elementsCount: elements.length
                }, getElementDebugDetails(el, style)));
            }

            if (isLikelyDelegatedInteractiveElement(el, style)) {
                return buildDebugResult(true, 'cityoverview-delegated-interactive', el, Object.assign({
                    point: { x: Math.round(x), y: Math.round(y) },
                    elementsCount: elements.length
                }, getElementDebugDetails(el, style)));
            }

            if (hasOpaqueVisualBackground(el, style)) {
                return buildDebugResult(true, 'cityoverview-visual-background', el, Object.assign({
                    point: { x: Math.round(x), y: Math.round(y) },
                    elementsCount: elements.length
                }, getElementDebugDetails(el, style)));
            }

            if (el.tagName === 'CANVAS') {
                var directCanvasHit = runCanvasPixelHitTest(el, x, y);
                if (directCanvasHit === true) {
                    return buildDebugResult(true, 'cityoverview-canvas-pixel', el, Object.assign({
                        point: { x: Math.round(x), y: Math.round(y) },
                        elementsCount: elements.length
                    }, getElementDebugDetails(el, style)));
                }
                continue;
            }

            if (hasEChartsHostMarker(el) || hasCanvasDescendant(el)) {
                var canvasHit = runCanvasDescendantHitTest(el, x, y);
                if (canvasHit === true) {
                    return buildDebugResult(true, hasEChartsHostMarker(el) ? 'cityoverview-echarts-host' : 'cityoverview-canvas-host', el, Object.assign({
                        point: { x: Math.round(x), y: Math.round(y) },
                        elementsCount: elements.length
                    }, getElementDebugDetails(el, style)));
                }
                continue;
            }
        }

        return buildDebugResult(false, 'cityoverview-pass-through', elements[0], Object.assign({
            point: { x: Math.round(x), y: Math.round(y) },
            elementsCount: elements.length
        }, getElementDebugDetails(elements[0])));
    }

    function checkCityOverviewInteractiveContent(x, y) {
        return evaluateCityOverviewInteractiveContent(x, y).clickable;
    }

    function attachPageRootCustomHitTest(el) {
        if (!el || el.nodeType !== 1 || el.hasAttribute(PAGE_ROOT_HIT_TEST_ATTR)) {
            return;
        }

        el.customHitTest = function(x, y) {
            return evaluateCityOverviewInteractiveContent(x, y);
        };

        el.setAttribute(PAGE_ROOT_HIT_TEST_ATTR, '1');
    }

    function attachAutoCustomHitTest(el) {
        if (!isLikelyCanvasHost(el)) {
            return;
        }

        el.customHitTest = function(x, y) {
            var rect = el.getBoundingClientRect();
            if (!isPointInsideRect(rect, x, y)) {
                return buildDebugResult(false, 'auto-host-outside', el, {
                    point: { x: Math.round(x), y: Math.round(y) }
                });
            }

            if (hasMeaningfulInteractiveDescendantAt(el, x, y)) {
                return buildDebugResult(true, 'auto-host-descendant', el, {
                    point: { x: Math.round(x), y: Math.round(y) }
                });
            }

            var canvasHit = runCanvasDescendantHitTest(el, x, y);
            if (canvasHit !== null) {
                return buildDebugResult(canvasHit, canvasHit ? 'auto-host-canvas-hit' : 'auto-host-canvas-pass-through', el, {
                    point: { x: Math.round(x), y: Math.round(y) }
                });
            }

            return buildDebugResult(false, 'auto-host-pass-through', el, {
                point: { x: Math.round(x), y: Math.round(y) }
            });
        };

        el.setAttribute(AUTO_CUSTOM_HIT_TEST_ATTR, '1');
    }

    function installCityOverviewCustomHitTests(root) {
        if (!isCityOverviewRoute()) {
            return;
        }

        var scope = root && root.nodeType === 1 ? root : document.body;
        if (!scope) {
            return;
        }

        attachPageRootCustomHitTest(document.body);
        var pageRoot = document.getElementById('root');
        if (pageRoot) {
            attachPageRootCustomHitTest(pageRoot);
        }

        if (isLikelyCanvasHost(scope)) {
            attachAutoCustomHitTest(scope);
        }

        if (!scope.querySelectorAll) {
            return;
        }

        var candidates = scope.querySelectorAll('div,section,article,main');
        for (var i = 0; i < candidates.length; i++) {
            attachAutoCustomHitTest(candidates[i]);
        }
    }

    function ensureCityOverviewObserver() {
        if (!isCityOverviewRoute() || installObserver || !window.MutationObserver || !document.body) {
            return;
        }

        installObserver = new MutationObserver(function(mutations) {
            for (var i = 0; i < mutations.length; i++) {
                var mutation = mutations[i];
                for (var j = 0; j < mutation.addedNodes.length; j++) {
                    var node = mutation.addedNodes[j];
                    if (node && node.nodeType === 1) {
                        installCityOverviewCustomHitTests(node);
                    }
                }
            }
        });

        installObserver.observe(document.body, {
            childList: true,
            subtree: true
        });
    }

    function installPageSpecificAdapters() {
        installCityOverviewCustomHitTests(document.body);
        ensureCityOverviewObserver();
    }

    function runCanvasPixelHitTest(el, x, y) {
        if (el.tagName !== 'CANVAS') {
            return null;
        }

        var rect = el.getBoundingClientRect();
        if (rect.width <= 0 || rect.height <= 0 || el.width <= 0 || el.height <= 0) {
            return false;
        }

        var context = null;
        try {
            context = el.getContext('2d', { willReadFrequently: true }) || el.getContext('2d');
        } catch (error) {
            return null;
        }

        if (!context || typeof context.getImageData !== 'function') {
            return null;
        }

        var canvasX = Math.round((x - rect.left) * (el.width / rect.width));
        var canvasY = Math.round((y - rect.top) * (el.height / rect.height));
        if (!isFinite(canvasX) || !isFinite(canvasY)) {
            return false;
        }

        canvasX = Math.max(0, Math.min(el.width - 1, canvasX));
        canvasY = Math.max(0, Math.min(el.height - 1, canvasY));

        var startX = Math.max(0, canvasX - 1);
        var startY = Math.max(0, canvasY - 1);
        var sampleWidth = Math.min(3, el.width - startX);
        var sampleHeight = Math.min(3, el.height - startY);

        try {
            var imageData = context.getImageData(startX, startY, sampleWidth, sampleHeight).data;
            for (var i = 3; i < imageData.length; i += 4) {
                if (imageData[i] > 8) {
                    return true;
                }
            }

            return false;
        } catch (error) {
            return null;
        }
    }

    function shouldTreatAsPassThroughContainer(el, style) {
        if (isForceInteractiveMarked(el) || isIntrinsicInteractiveElement(el, style)) {
            return false;
        }

        if (hasCanvasDescendant(el)) {
            return true;
        }

        return isGenericFullScreenContainer(el);
    }

    function isCanvasLikeElement(el) {
        return el.tagName === 'CANVAS' || el.tagName === 'SVG' || el.tagName === 'PATH';
    }

    function evaluatePoint(x, y) {
        var elements = document.elementsFromPoint(x, y);

        for (var i = 0; i < elements.length; i++) {
            var el = elements[i];
            if (!el || el.nodeType !== 1) {
                continue;
            }

            if (el.tagName === 'HTML' || el.tagName === 'BODY') {
                continue;
            }

            if (isPassThroughMarked(el)) {
                continue;
            }

            var style = window.getComputedStyle(el);
            if (!isElementVisible(el, style)) {
                continue;
            }

            var customHit = runCustomHitTest(el, x, y);
            if (isDebugResult(customHit)) {
                var normalizedCustomHit = enrichDebugResult(customHit, x, y, elements.length, el);
                if (normalizedCustomHit.clickable) {
                    return normalizedCustomHit;
                }
                continue;
            }
            if (customHit === true) {
                return buildDebugResult(true, 'custom-hit-test', el, {
                    point: { x: Math.round(x), y: Math.round(y) },
                    elementsCount: elements.length
                });
            }
            if (customHit === false) {
                continue;
            }

            var echartsHit = runEChartsHitTest(el, x, y);
            if (echartsHit === true) {
                return buildDebugResult(true, 'echarts-hit', el, {
                    point: { x: Math.round(x), y: Math.round(y) },
                    elementsCount: elements.length
                });
            }
            if (echartsHit === false) {
                continue;
            }

            var canvasPixelHit = runCanvasPixelHitTest(el, x, y);
            if (canvasPixelHit === true) {
                return buildDebugResult(true, 'canvas-pixel-hit', el, {
                    point: { x: Math.round(x), y: Math.round(y) },
                    elementsCount: elements.length
                });
            }
            if (canvasPixelHit === false) {
                continue;
            }

            if (hasCanvasDescendant(el) && !isForceInteractiveMarked(el)) {
                // 对承载图表 canvas 的宿主容器，不再因为矩形覆盖或 cursor 样式就拦截鼠标。
                // 是否可点击优先依赖 canvas 像素命中、customHitTest、ECharts 命中或显式 interactive 标记。
                if (!hasInlineInteraction(el) && !hasInteractiveRole(el) && !hasInteractiveTabIndex(el)) {
                    continue;
                }
            }

            if (isForceInteractiveMarked(el) || isSemanticNativeInteractiveElement(el)) {
                return buildDebugResult(true, 'intrinsic-interactive', el, Object.assign({
                    point: { x: Math.round(x), y: Math.round(y) },
                    elementsCount: elements.length
                }, getElementDebugDetails(el, style)));
            }

            if (isSignalInteractiveElement(el)) {
                return buildDebugResult(true, 'signal-interactive', el, Object.assign({
                    point: { x: Math.round(x), y: Math.round(y) },
                    elementsCount: elements.length
                }, getElementDebugDetails(el, style)));
            }

            if (isCompactPointerInteractiveElement(el, style)) {
                return buildDebugResult(true, 'cursor-interactive', el, {
                    point: { x: Math.round(x), y: Math.round(y) },
                    elementsCount: elements.length,
                    cursor: style.cursor || ''
                });
            }

            if (hasOpaqueVisualBackground(el, style)) {
                return buildDebugResult(true, 'visual-background', el, Object.assign({
                    point: { x: Math.round(x), y: Math.round(y) },
                    elementsCount: elements.length
                }, getElementDebugDetails(el, style)));
            }

            if (isCanvasLikeElement(el)) {
                // 对 canvas / svg 这类大面积绘制层，不再仅凭矩形区域就判定为可点击。
                // 优先依赖 customHitTest、ECharts 图元命中，或显式 interactive 标记。
                continue;
            }

            if (shouldTreatAsPassThroughContainer(el, style)) {
                continue;
            }
        }

        return buildDebugResult(false, 'pass-through', elements[0], {
            point: { x: Math.round(x), y: Math.round(y) },
            elementsCount: elements.length
        });
    }

    function checkElementAt(x, y) {
        return evaluatePoint(x, y).clickable;
    }

    function flushPointerState() {
        rafHandle = 0;
        if (!lastPointerEvent) {
            return;
        }

        var result = evaluatePoint(lastPointerEvent.clientX, lastPointerEvent.clientY);
        postClickableState(result.clickable, result);
    }

    function schedulePointerCheck(event) {
        lastPointerEvent = event;
        if (rafHandle) {
            return;
        }

        rafHandle = window.requestAnimationFrame(flushPointerState);
    }

    function resetClickableState() {
        lastPointerEvent = null;
        if (rafHandle) {
            window.cancelAnimationFrame(rafHandle);
            rafHandle = 0;
        }

        postClickableState(false, buildDebugResult(false, 'pointer-reset', null, {
            route: getCurrentRoute()
        }));
    }

    document.addEventListener('mousemove', schedulePointerCheck, { passive: true });
    document.addEventListener('pointermove', schedulePointerCheck, { passive: true });

    document.addEventListener('mouseout', function(e) {
        if (!e.relatedTarget) {
            resetClickableState();
        }
    });

    document.addEventListener('mouseleave', resetClickableState, { passive: true });
    window.addEventListener('blur', resetClickableState, { passive: true });

    if (document.readyState === 'loading') {
        document.addEventListener('DOMContentLoaded', installPageSpecificAdapters, { once: true });
    } else {
        installPageSpecificAdapters();
    }

    window.addEventListener('load', installPageSpecificAdapters, { passive: true });
    updateDebugOverlay(buildDebugResult(false, 'debug-overlay-ready', null, {
        route: getCurrentRoute()
    }));

    // 预留给业务页面调试或主动触发检测。
    window.__cbwebview2TransparencyCheck = {
        checkElementAt: checkElementAt,
        evaluatePoint: evaluatePoint,
        getDebugInfo: function() {
            return currentDebugInfo;
        },
        installPageAdapters: installPageSpecificAdapters,
        reset: resetClickableState,
        isDebugEnabled: function() {
            return debugOverlayEnabled;
        },
        renderDebug: function() {
            updateDebugOverlay(currentDebugInfo || buildDebugResult(false, 'debug-overlay-ready', null, {
                route: getCurrentRoute()
            }));
        },
        setDebugEnabled: function(enabled) {
            setDebugOverlayEnabled(enabled);
        },
        forceCheck: function(x, y) {
            var result = evaluatePoint(x, y);
            postClickableState(result.clickable, result);
        }
    };
})();