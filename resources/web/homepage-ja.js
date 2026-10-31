// Reviewed copy corrections for the Japanese homepage inside FlashStudio.
// Never injected into an identity provider or the user's external browser.
(() => {
    if (location.protocol !== 'https:' || location.port && location.port !== '443' ||
        !['desktop.voxelshare.com', 'desktop.voxelshare.cn'].includes(location.hostname)) return;
    if (window.__flashstudioJapaneseCopy) return;
    window.__flashstudioJapaneseCopy = true;
    const replacements = new Map([
        ['読んだ後に同意して受領', '以下の内容を確認し、同意します：'],
        ['《サービス利用規約》', '利用規約'],
        ['《プライバシー方針》', 'プライバシーポリシー'],
    ]);
    const correct = root => {
        if (root.nodeType === Node.TEXT_NODE) {
            if (root.parentElement?.closest('script,style,textarea,input,[contenteditable]')) return;
            const before = root.nodeValue;
            const replacement = replacements.get(before.trim());
            if (replacement) root.nodeValue = before.replace(before.trim(), replacement);
            return;
        }
        if (root.nodeType !== Node.ELEMENT_NODE) return;
        const walker = document.createTreeWalker(root, NodeFilter.SHOW_TEXT);
        for (let node = walker.nextNode(); node; node = walker.nextNode()) correct(node);
    };
    const start = () => {
        correct(document.body);
        new MutationObserver(records => {
            for (const record of records) {
                if (record.type === 'characterData') correct(record.target);
                else for (const node of record.addedNodes) correct(node);
            }
        }).observe(document.body, {subtree: true, childList: true, characterData: true});
    };
    if (document.body) start();
    else document.addEventListener('DOMContentLoaded', start, {once: true});
})();
