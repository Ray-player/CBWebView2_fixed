// 设计时的基准分辨率（宽屏比例 16:9）
const designWidth = 1920;
const designHeight = 1080;
let currentScale = 1;
//默认能够交互的元素类名数组
const elements = getChildClassNamesByParent("main");
function main() {
    /*
    UE消息传递方法：window.chrome.webview.postMessage
    */
    setTimeout(() => {
        sendElementPositions(elements);
    }, 100);

    // 初始缩放
    //updateScale();
    // 窗口大小改变时重新缩放
    //window.addEventListener('resize', sendElementPositions(elements));
}
main();

/*已弃用：更新缩放比
function updateScale() {
    const windowWidth = window.innerWidth;
    const windowHeight = window.innerHeight;

    // 计算宽高比
    const designRatio = designWidth / designHeight;
    const windowRatio = windowWidth / windowHeight;

    if (windowRatio > designRatio) {
        // 窗口比设计更宽，按高度缩放
        currentScale = windowHeight / designHeight;
    } else {
        // 窗口比设计更高，按宽度缩放
        currentScale = windowWidth / designWidth;
    }

    document.body.style.transform = `scale(${currentScale})`;
    document.body.style.width = `${designWidth}px`;
    document.body.style.height = `${designHeight}px`;

    // 缩放后发送元素位置信息

    // 延迟0.1秒后发送元素位置信息
    setTimeout(() => {
        sendElementPositions(elements);
    }, 100);
}
*/
/**
 * 根据父容器类名获取其直接子元素的类名数组
 * @param {string} parentClassName - 父容器类名（如 'main'）
 * @returns {string[]} 子元素的类名数组
 */
function getChildClassNamesByParent(parentClassName) {
    const parentElement = document.querySelector(`.${parentClassName}`);
    if (!parentElement) {
        console.error(`未找到类名为 ${parentClassName} 的元素`);
        return [];
    }

    const children = parentElement.children;
    const childClassNames = [];

    for (let child of children) {
        if (child.classList.length > 0) {
            // 获取第一个类名作为标识（可按需扩展）
            childClassNames.push(child.classList[0]);
        } else {
            // 如果没有类名，则跳过或使用 tag 名替代（可选）
            //childClassNames.push(child.tagName.toLowerCase());
        }
    }
    //console.log("子类名"+childClassNames)
    return childClassNames;
}
// 获取元素缩放后的位置和尺寸
function getScaledElementPositions(classNames) {
    const elements = classNames.map(className => document.querySelector(`.${className}`));

    return elements.map(el => {
        if (!el) return null; // 处理未找到元素的情况
        const rect = el.getBoundingClientRect();
        return {
            x: rect.left,
            y: rect.top,
            width: rect.width,
            high: rect.height
        };
    }).filter(Boolean); // 过滤掉 null 值
}

// 发送元素位置信息(关键函数，决定UE上前端页面的点击范围)
function sendElementPositions(classNames) {
    console.log("分辨率w"+window.innerWidth);
    console.log("分辨率h"+window.innerHeight);
    //console.log("子类数组：" + elements);
    const positions = getScaledElementPositions(classNames);
    console.log("位置信息:"+positions);
    window.chrome.webview.postMessage({
        type: "webPosition",
        position: positions,
        resolutionW: window.innerWidth,
        resolutionH: window.innerHeight
    });
}

// UECall函数，测试用
function UECall(messageData) {
    console.log("UECall:", messageData);
    ShowMessageInBox(messageData);
}

//其他功能函数
function buttonCall() {
    //getChildClassNamesByParent("main");
    const inputElement = document.getElementById("textInput");
    if (inputElement) {
        const inputText = inputElement.value;
        console.log("输入的文本是:", inputText);
        // 发送到 UE
        window.chrome.webview.postMessage({ type: "inputText", text: inputText });
    } else {
        console.error("未找到输入框");
    }
}
function ShowMessageInBox(messageData) {
    const rulerLeft = document.querySelector(".ruler-left");
    if (rulerLeft) {
        // 将 messageData 显示在 .ruler-left 元素内
        rulerLeft.textContent = `接收到的消息: ${JSON.stringify(messageData)}`;
    } else {
        console.error("未找到元素");
    }
}