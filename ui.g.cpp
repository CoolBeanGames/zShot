// generated from ZSL by zslc.py - do not edit.
#include "zui.h"
#include <string>
#include <unordered_map>

namespace {
constexpr const char* kZslDocument = R"ZSL(<!DOCTYPE html>
<html lang="en" data-zui-theme="holo">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>zUI</title>
<link rel="stylesheet" href="zui/css/zui.css">
<link rel="stylesheet" href="zui/css/themes/holo.css">
<link rel="stylesheet" href="zui/css/themes/clean.css">
</head>
<body>
<div class="zui-html"><div class="zui-link"></div><div class="zui-link"></div><div class="zui-script"></div><div class="zui-script"></div><div class="zui-body"><div class="zui-panel"><div class="zui-panel__body zui-panel__body--flush"><div class="zui-col zui-gap-2"><div class="zui-row zui-gap-2"><div class="zui-img"></div><div class="zui-col zui-gap-2"><span>zShot</span><span>Version 8 (Build 8)</span></div></div><div class="zui-row zui-gap-2"><button class="zui-btn" id="ok-btn" data-zui-id="ok">OK</button></div></div></div></div><div class="zui-script">zui.receive(&quot;ok&quot;, function() {
            zui.send(&quot;close-about&quot;, {});
        });</div></div></div>
<script>window.__zslState = {};</script>
<script src="zui/icons/sprite.js"></script>
<script src="zui/js/zui.js"></script>
<script>
(function(){
  var state = window.__zslState;
  function render(){
  }
  zui.receive('state', function(p){ Object.assign(state, p||{}); render(); });
  document.addEventListener('DOMContentLoaded', function(){ zui.wire(document); render(); });
})();
</script>
</body>
</html>
)ZSL";
}

// Call after constructing the host. Native handlers are optional: screens with
// no host-side behavior still compile and link without generated global stubs.
void build_ui(
    zui::Host& host,
    const std::unordered_map<std::string, zui::MessageHandler>& handlers) {

    host.load_document(kZslDocument);
}
