Module['Burogu_Preprocess'] = function(text, threshold = 10) {
    if (!text) return "";

    text = text.replace(/([，。！？；：」』）》])(?![ \u200B])/g, "$1 ");
    text = text.replace(/([\u4e00-\u9fa5])([a-zA-Z0-9])/g, "$1 $2");
    text = text.replace(/([a-zA-Z0-9])([\u4e00-\u9fa5])/g, "$1 $2");

    const words = text.split(' ');
    const processedWords = words.map(word => {
        if (word.length <= threshold) return word;

        let result = "";
        let currentChunkLength = 0;

        for (const char of word) {
            result += char;
            const isFullWidth = char.charCodeAt(0) > 255;
            currentChunkLength += isFullWidth ? 1 : 0.5;

            if (currentChunkLength >= threshold) {
                result += " ";
                currentChunkLength = 0;
            }
        }
        return result;
    });

    return processedWords.join(' ');
};

Module['Burogu_SafeAllocateUTF8'] = function(str) {
    if (!str) return 0;

    const encoder = new TextEncoder();
    const bytes = encoder.encode(str);
    const len = bytes.length + 1;

    const ptr = Module._malloc(len);

    if (typeof stringToUTF8 !== 'undefined') {
        stringToUTF8(str, ptr, len);
    } else {
        Module.HEAPU8.set(bytes, ptr);
        Module.HEAPU8[ptr + len - 1] = 0;
    }

    return ptr;
};

// mount the glyph range file to the virtual FS before the main module runs
Module['Burogu_PreloadGlyphRange'] = function() {
    return fetch('markdown/glyph_range.txt')
            .then(res => res.arrayBuffer())
            .then(buffer => {
                try {
                    FS.mkdir('/markdown');
                } catch (e) {}
                FS.writeFile('/markdown/glyph_range.txt', new Uint8Array(buffer));
                console.log("Glyph range preloaded to FS");
            });
};

Module['onRuntimeInitialized'] = function() {
    console.log("Wasm runtime ready, starting preloads...");

    Module.Burogu_PreloadGlyphRange()
            .then(
                    () => {
                        console.log("Glyph range preloaded.");
                        callMain();
                    })
            .catch(err => {
                console.error("Failed to preload glyph range:", err);
            });
};