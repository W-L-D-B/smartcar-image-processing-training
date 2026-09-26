(function () {
  document.documentElement.classList.add("js");

  document.querySelectorAll(".lesson").forEach(function (lesson) {
    var methods = Array.from(lesson.querySelectorAll(".method"));
    methods.forEach(function (method, index) {
      method.dataset.level = String(index);
      method.hidden = index !== 0;
    });

    var switcher = document.createElement("div");
    switcher.className = "tier-switcher";
    switcher.setAttribute("role", "group");
    switcher.setAttribute("aria-label", "选择算法层级");
    methods.forEach(function (method, index) {
      var button = document.createElement("button");
      button.type = "button";
      button.textContent = method.querySelector(".level").textContent;
      button.setAttribute("aria-pressed", index === 0 ? "true" : "false");
      button.addEventListener("click", function () {
        methods.forEach(function (item, i) {
          item.hidden = i !== index;
          switcher.children[i].setAttribute("aria-pressed", i === index ? "true" : "false");
        });
      });
      switcher.appendChild(button);
    });
    lesson.querySelector(".method-grid").before(switcher);
  });

  var W = 160, H = 120;
  function syntheticFrame(mode, width, height) {
    var data = new Uint8ClampedArray(width * height);
    var centers = new Float32Array(height);
    for (var y = 0; y < height; y++) {
      var t = y / Math.max(1, height - 1);
      var center = mode === "curve" ? 78 + 48 * t * t : 79 + 11 * (t - 0.4);
      var half = 20 + 18 * t;
      centers[y] = center;
      var left = Math.max(4, Math.round(center - half));
      var right = Math.min(width - 5, Math.round(center + half));
      for (var x = 0; x < width; x++) {
        var value = x > left + 1 && x < right - 2 ? 145 : 218;
        if ((x >= left - 2 && x < left + 2) || (x >= right - 1 && x < right + 3)) value = x < center ? 38 : 42;
        if (mode === "shadow" && x < width * 0.525 && y >= height * .23 && y < height * .78) value = Math.round(value * .30);
        if (mode === "glare" && x >= width * .4125 && x < width * .644 && y >= height * .225 && y < height * .783) value = 250;
        if (mode === "missing" && x >= left - 3 && x < left + 4 && y >= height * .483) value = 180;
        if (mode === "noise" && ((x * 17 + y * 31) % 997 === 0 || (x * 29 + y * 11) % 1321 === 0)) value = (x + y) % 2 ? 0 : 255;
        data[y * width + x] = value;
      }
    }
    return { data: data, centers: centers };
  }

  function paint(canvas, data, width, height, binaryThreshold) {
    var scratch = document.createElement("canvas");
    scratch.width = width;
    scratch.height = height;
    var sctx = scratch.getContext("2d");
    var image = sctx.createImageData(width, height);
    for (var i = 0; i < data.length; i++) {
      var value = binaryThreshold === null ? data[i] : (data[i] <= binaryThreshold ? 255 : 0);
      var p = i * 4;
      image.data[p] = value;
      image.data[p + 1] = value;
      image.data[p + 2] = value;
      image.data[p + 3] = 255;
    }
    sctx.putImageData(image, 0, 0);
    var ctx = canvas.getContext("2d");
    ctx.imageSmoothingEnabled = false;
    ctx.clearRect(0, 0, canvas.width, canvas.height);
    ctx.drawImage(scratch, 0, 0, canvas.width, canvas.height);
  }

  var thresholdSlider = document.getElementById("threshold-slider");
  var sceneSelect = document.getElementById("scene-select");
  var grayCanvas = document.getElementById("gray-canvas");
  var binaryCanvas = document.getElementById("binary-canvas");
  var thresholdStatus = document.getElementById("threshold-status");
  function updateThreshold() {
    var t = Number(thresholdSlider.value);
    var scene = syntheticFrame(sceneSelect.value, W, H);
    paint(grayCanvas, scene.data, W, H, null);
    paint(binaryCanvas, scene.data, W, H, t);
    var count = 0;
    for (var i = 0; i < scene.data.length; i++) if (scene.data[i] <= t) count++;
    document.getElementById("threshold-value").value = String(t);
    thresholdStatus.textContent = "阈值 T = " + t + "；暗前景像素 " + count + " / " + (W * H) + "（" + (100 * count / (W * H)).toFixed(1) + "%）";
  }
  thresholdSlider.addEventListener("input", updateThreshold);
  sceneSelect.addEventListener("change", updateThreshold);
  updateThreshold();

  var fullW = 256, fullH = 160;
  var fullScene = syntheticFrame("normal", fullW, fullH);
  var fullCanvas = document.getElementById("full-canvas");
  var scaledCanvas = document.getElementById("scaled-canvas");
  var scaleSlider = document.getElementById("scale-slider");
  function updateScale() {
    var scale = Number(scaleSlider.value) / 100;
    var w = Math.max(1, Math.round(fullW * scale));
    var h = Math.max(1, Math.round(fullH * scale));
    var small = new Uint8ClampedArray(w * h);
    for (var y = 0; y < h; y++) {
      for (var x = 0; x < w; x++) {
        var sx = Math.min(fullW - 1, Math.floor((x + .5) * fullW / w));
        var sy = Math.min(fullH - 1, Math.floor((y + .5) * fullH / h));
        small[y * w + x] = fullScene.data[sy * fullW + sx];
      }
    }
    paint(fullCanvas, fullScene.data, fullW, fullH, null);
    paint(scaledCanvas, small, w, h, null);
    document.getElementById("scale-value").value = Math.round(scale * 100) + "%";
    document.getElementById("scaled-size").textContent = w + " × " + h;
    document.getElementById("gray-bytes").textContent = (w * h).toLocaleString("zh-CN") + " B";
    document.getElementById("packed-bytes").textContent = Math.ceil(w * h / 8).toLocaleString("zh-CN") + " B";
  }
  scaleSlider.addEventListener("input", updateScale);
  updateScale();
}());
