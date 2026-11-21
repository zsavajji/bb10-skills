#!/usr/bin/env python3
"""Make the BB10 NDK's Qt 4.8 plugins.qmltypes readable by Qt 6 qmllint, in place.

Qt 4.8 numbers revisions 0, 2, 3, 4; Qt 6 wants them as versions (major << 8 | minor).
Every bb.* module is 1.x, so revision N becomes 1.N. qmldir files get a module line.
Run it once on a fresh copy: revisions are rewritten in place.
"""
import pathlib
import re
import sys

root = pathlib.Path(sys.argv[1])
for qmldir in root.rglob("qmldir"):
    module = ".".join(re.sub(r"\.1$", "", p) for p in qmldir.parent.relative_to(root).parts)
    text = qmldir.read_text()
    if not text.startswith("module "):
        qmldir.write_text(f"module {module}\n" + text)

for types in root.rglob("plugins.qmltypes"):
    text = types.read_text()

    def revisions(m):
        versions = re.findall(r'"[^"]* (\d+)\.(\d+)"', m.group(1))
        encoded = ", ".join(str(int(a) << 8 | int(b)) for a, b in versions)
        return m.group(1) + f"exportMetaObjectRevisions: [{encoded}]"

    text = re.sub(r"(exports: \[[^\]]*\]\s*)exportMetaObjectRevisions: \[[^\]]*\]", revisions, text)
    text = re.sub(r"\brevision: (\d+)", lambda m: f"revision: {256 + int(m.group(1))}", text)
    # Qt 4.8 type names Qt 6 knows by others.
    text = text.replace('type: "string"', 'type: "QString"')
    text = text.replace('type: "QDeclarativeComponent"', 'type: "QQmlComponent"')
    # QML's WebView is QmlWebView; its C++ base is exported under the same name too.
    text = re.sub(r'(name: "bb::cascades::WebView"\s*prototype: "[^"]*"\s*)exports: \[[^\]]*\]\s*'
                  r'exportMetaObjectRevisions: \[[^\]]*\]', r"\1", text)
    types.write_text(text)
