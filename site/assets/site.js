(function () {
  document.documentElement.classList.add("js");

  document.querySelectorAll(".lesson").forEach(function (lesson) {
    var methods = Array.from(lesson.querySelectorAll(".method"));
    methods.forEach(function (method, index) { method.hidden = index !== 0; });
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
  function clamp(v) { return Math.max(0, Math.min(255, Math.round(v))); }

  function makeFrame(mode, width, height) {
    var gray = new Uint8ClampedArray(width * height);
    for (var y = 0; y < height; y++) {
      var t = y / Math.max(1, height - 1);
      var curve = mode === "curve" ? 22 * t * t : 7 * (t - .35);
      var center = width * .5 + curve;
      var half = width * (.10 + .115 * t);
      var left = center - half;
      var right = center + half;
      for (var x = 0; x < width; x++) {
        var outside = 35 + 27 * t + 15 * x / width + 4 * Math.sin(y * .31 + x * .08);
        var inside = 173 + 32 * t - 10 * x / width + 7 * Math.sin(x * .13 + y * .07);
        var edgeDist = Math.min(Math.abs(x-left), Math.abs(x-right));
        var value = (x > left && x < right) ? inside : outside;
        if (edgeDist < 3) value = outside + (inside-outside) * edgeDist / 3;
        if (mode === "shadow" && x < width * .58 && y > height*.20 && y < height*.75)
          value -= 55;
        if (mode === "glare" && x > width*.43 && x < width*.72 && y > height*.18 && y < height*.54)
          value = Math.max(value, 246);
        if (mode === "missing" && x > right-4 && x < right+5 && y > height*.46)
          value = 95 + 18*t;
        if (mode === "noise" && ((x*17+y*31)%997===0 || (x*29+y*11)%1321===0))
          value = ((x+y)%2) ? 0 : 255;
        gray[y*width+x] = clamp(value);
      }
    }
    return gray;
  }

  function paint(canvas, values, width, height) {
    var scratch = document.createElement("canvas");
    scratch.width = width;
    scratch.height = height;
    var ctx0 = scratch.getContext("2d");
    var frameData = ctx0.createImageData(width, height);
    for (var i=0;i<values.length;i++) {
      var v=values[i], k=i*4;
      frameData.data[k]=v; frameData.data[k+1]=v; frameData.data[k+2]=v; frameData.data[k+3]=255;
    }
    ctx0.putImageData(frameData,0,0);
    var ctx=canvas.getContext("2d");
    ctx.imageSmoothingEnabled=false;
    ctx.clearRect(0,0,canvas.width,canvas.height);
    ctx.drawImage(scratch,0,0,canvas.width,canvas.height);
  }

  function histOf(values) {
    var hist=new Uint32Array(256);
    for(var i=0;i<values.length;i++)hist[values[i]]++;
    return hist;
  }
  function otsuThreshold(hist,total) {
    var sumAll=0,w0=0,sum0=0,best=-1,bestT=0;
    for(var i=0;i<256;i++)sumAll+=i*hist[i];
    for(var t=0;t<255;t++){
      w0+=hist[t]; sum0+=t*hist[t];
      var w1=total-w0;
      if(!w0||!w1)continue;
      var m0=sum0/w0, m1=(sumAll-sum0)/w1, d=m0-m1;
      var score=w0*w1*d*d;
      if(score>best){best=score;bestT=t+1;}
    }
    return bestT;
  }
  function intermeans(values) {
    var min=255,max=0;
    for(var i=0;i<values.length;i++){if(values[i]<min)min=values[i];if(values[i]>max)max=values[i];}
    var t=Math.floor((min+max)/2);
    for(var n=0;n<24;n++){
      var s0=0,s1=0,c0=0,c1=0;
      for(var j=0;j<values.length;j++){if(values[j]<=t){s0+=values[j];c0++;}else{s1+=values[j];c1++;}}
      if(!c0||!c1)break;
      var next=Math.floor((s0/c0+s1/c1)/2);
      if(Math.abs(next-t)<=1){t=next;break;}
      t=next;
    }
    return t;
  }
  function buildIntegral(values,width,height) {
    var stride=width+1, integral=new Float64Array((width+1)*(height+1));
    for(var y=1;y<=height;y++){
      var row=0;
      for(var x=1;x<=width;x++){
        row+=values[(y-1)*width+x-1];
        integral[y*stride+x]=integral[(y-1)*stride+x]+row;
      }
    }
    return integral;
  }
  function boxSum(integral,stride,x0,y0,x1,y1) {
    return integral[y1*stride+x1]-integral[y0*stride+x1]-integral[y1*stride+x0]+integral[y0*stride+x0];
  }
  function thresholdFrame(values,mode,param,width,height) {
    var mask=new Uint8ClampedArray(values.length);
    var threshold=Number(param), rowT=null, integral=null, radius=5;
    if(mode==="otsu")threshold=otsuThreshold(histOf(values),values.length);
    if(mode==="row"){
      rowT=new Uint8Array(height);
      for(var y=0;y<height;y++)rowT[y]=intermeans(values.subarray(y*width,(y+1)*width));
    }
    if(mode==="local")integral=buildIntegral(values,width,height);
    for(var y=0;y<height;y++){
      for(var x=0;x<width;x++){
        var t=threshold;
        if(mode==="row")t=rowT[y];
        if(mode==="local"){
          var x0=Math.max(0,x-radius),x1=Math.min(width,x+radius+1);
          var y0=Math.max(0,y-radius),y1=Math.min(height,y+radius+1);
          var count=(x1-x0)*(y1-y0);
          t=boxSum(integral,width+1,x0,y0,x1,y1)/count+Number(param)/5;
        }
        mask[y*width+x]=values[y*width+x]>=t?255:0;
      }
    }
    return {mask:mask,threshold:threshold,rowT:rowT};
  }
  function drawHistogram(canvas,hist,marker) {
    var ctx=canvas.getContext("2d"),w=canvas.width,h=canvas.height;
    ctx.clearRect(0,0,w,h);ctx.fillStyle="#fffefa";ctx.fillRect(0,0,w,h);
    var max=1;for(var i=0;i<256;i++)if(hist[i]>max)max=hist[i];
    ctx.strokeStyle="#d9e1da";
    for(var g=1;g<4;g++){ctx.beginPath();ctx.moveTo(25,h*g/4);ctx.lineTo(w-10,h*g/4);ctx.stroke();}
    for(var b=0;b<256;b++){
      var bh=hist[b]/max*(h-38);
      ctx.fillStyle="#52696a";ctx.fillRect(26+b*(w-38)/256,h-22-bh,Math.max(1,(w-38)/256),bh);
    }
    if(marker>=0&&marker<256){var xx=26+marker*(w-38)/256;ctx.strokeStyle="#ed7a3b";ctx.lineWidth=3;ctx.beginPath();ctx.moveTo(xx,8);ctx.lineTo(xx,h-18);ctx.stroke();}
    ctx.fillStyle="#183235";ctx.font="13px sans-serif";ctx.fillText("0",25,h-4);ctx.fillText("255",w-38,h-4);
  }

  var sceneSelect=document.getElementById("scene-select");
  var thresholdMode=document.getElementById("threshold-mode");
  var thresholdSlider=document.getElementById("threshold-slider");
  var currentScene=makeFrame("normal",W,H),currentMask=null;
  function updateThreshold() {
    currentScene=makeFrame(sceneSelect.value,W,H);
    var mode=thresholdMode.value,param=Number(thresholdSlider.value);
    var result=thresholdFrame(currentScene,mode,param,W,H);
    currentMask=result.mask;
    paint(document.getElementById("gray-canvas"),currentScene,W,H);
    paint(document.getElementById("threshold-canvas"),currentMask,W,H);
    drawHistogram(document.getElementById("histogram-canvas"),histOf(currentScene),
      (mode==="fixed"||mode==="otsu")?result.threshold:-1);
    document.getElementById("threshold-value").value=String(mode==="fixed"||mode==="otsu"?result.threshold:param);
    document.getElementById("threshold-label").value=mode==="fixed"?"T":mode==="otsu"?"Otsu T":mode==="row"?"行阈值起点":"局部 C";
    thresholdSlider.disabled = mode==="otsu";
    var white=0;for(var i=0;i<currentMask.length;i++)if(currentMask[i])white++;
    document.getElementById("threshold-status").textContent=
      "模式："+thresholdMode.options[thresholdMode.selectedIndex].text+
      "；全局阈值="+(mode==="fixed"||mode==="otsu"?result.threshold:"逐行/逐点")+
      "；白前景 "+white+" / "+(W*H)+" ("+(100*white/(W*H)).toFixed(1)+"%)";
    updateScanning();
  }
  sceneSelect.addEventListener("change",function(){updateThreshold();updateSobel();});
  thresholdMode.addEventListener("change",updateThreshold);
  thresholdSlider.addEventListener("input",updateThreshold);

  function sobelMagnitude(pixels,width,height) {
    var mag=new Uint8ClampedArray(width*height);
    for(var y=1;y<height-1;y++)for(var x=1;x<width-1;x++){
      var p00=pixels[(y-1)*width+x-1],p01=pixels[(y-1)*width+x],p02=pixels[(y-1)*width+x+1];
      var p10=pixels[y*width+x-1],p12=pixels[y*width+x+1];
      var p20=pixels[(y+1)*width+x-1],p21=pixels[(y+1)*width+x],p22=pixels[(y+1)*width+x+1];
      var gx=-p00+p02-2*p10+2*p12-p20+p22;
      var gy=-p00-2*p01-p02+p20+2*p21+p22,i=y*width+x;
      mag[i]=clamp((Math.abs(gx)+Math.abs(gy))/8);
    }
    return mag;
  }
  function updateSobel() {
    var src=makeFrame(sceneSelect.value,W,H),magnitude=sobelMagnitude(src,W,H);
    var th=Number(document.getElementById("sobel-threshold").value);
    var out=new Uint8ClampedArray(W*H),count=0;
    for(var i=0;i<out.length;i++){out[i]=magnitude[i]>=th?255:0;if(out[i])count++;}
    paint(document.getElementById("sobel-input"),src,W,H);
    paint(document.getElementById("sobel-output"),out,W,H);
    document.getElementById("sobel-value").value=String(th);
    document.getElementById("sobel-status").textContent="阈值 T="+th+"；二值边缘像素："+count+" / "+(W*H);
  }
  document.getElementById("sobel-threshold").addEventListener("input",updateSobel);

  function edgePair(mask,y,start) {
    var seed=start;
    if(seed<1||seed>=W-1)seed=Math.floor(W/2);
    if(!mask[y*W+seed]){
      var found=false;
      for(var d=0;d<W&&!found;d++){
        var l=seed-d,r=seed+d;
        if(l>=0&&mask[y*W+l]){seed=l;found=true;}
        else if(r<W&&mask[y*W+r]){seed=r;found=true;}
      }
      if(!found)return {left:-1,right:-1,valid:false};
    }
    var left=-1,right=-1;
    for(var x=seed;x>0;x--)if(mask[y*W+x]&&!mask[y*W+x-1]){left=x;break;}
    for(var xx=seed;xx<W-1;xx++)if(mask[y*W+xx]&&!mask[y*W+xx+1]){right=xx;break;}
    return {left:left,right:right,valid:left>=0&&right>left};
  }
  function scanRows(mask) {
    var rows=new Array(H);
    for(var y=H-1;y>=0;y--)rows[y]=edgePair(mask,y,W/2);
    return {rows:rows,seedL:-1,seedR:-1,stop:H};
  }
  function scanInherit(mask) {
    var rows=new Array(H),seed=Math.floor(W/2);
    for(var y=H-1;y>=0;y--){var e=edgePair(mask,y,seed);rows[y]=e;if(e.valid)seed=Math.floor((e.left+e.right)/2);}
    return {rows:rows,seedL:-1,seedR:-1,stop:H};
  }
  function longestSeeds(mask) {
    var lengths=new Int16Array(W);
    for(var x=1;x<W-1;x++){var n=0;for(var y=H-1;y>=0&&mask[y*W+x];y--)n++;lengths[x]=n;}
    var sl=-1,sr=-1,bl=-1,br=-1;
    for(var x=1;x<W-1;x++)if(lengths[x]>bl){bl=lengths[x];sl=x;}
    for(var x=W-2;x>0;x--)if(lengths[x]>br){br=lengths[x];sr=x;}
    return {left:sl,right:sr,stop:Math.min(bl,br)};
  }
  function scanLongest(mask) {
    var s=longestSeeds(mask),rows=new Array(H),seed=Math.floor((s.left+s.right)/2);
    for(var y=0;y<H;y++)rows[y]=(s.stop>0&&y>=H-s.stop)?edgePair(mask,y,seed):{left:-1,right:-1,valid:false};
    return {rows:rows,seedL:s.left,seedR:s.right,stop:s.stop};
  }
  function updateScanning() {
    if(!currentScene)return;
    var mask=currentMask||thresholdFrame(currentScene,"fixed",128,W,H).mask;
    var mode=document.getElementById("scan-mode").value,y=Number(document.getElementById("scan-row").value);
    var result=mode==="longest"?scanLongest(mask):mode==="inherit"?scanInherit(mask):scanRows(mask);
    var e=result.rows[y];
    paint(document.getElementById("scan-mask"),mask,W,H);
    var maskCanvas=document.getElementById("scan-mask");
    var maskCtx=maskCanvas.getContext("2d");
    maskCtx.save();maskCtx.scale(maskCanvas.width/W,maskCanvas.height/H);
    maskCtx.strokeStyle="#E0A51C";maskCtx.lineWidth=1.5;maskCtx.setLineDash([3,2]);
    maskCtx.beginPath();maskCtx.moveTo(0,y+0.5);maskCtx.lineTo(W,y+0.5);maskCtx.stroke();
    maskCtx.restore();

    var canvas=document.getElementById("scan-result");
    paint(canvas,currentScene,W,H);
    var ctx=canvas.getContext("2d");
    ctx.save();ctx.scale(canvas.width/W,canvas.height/H);ctx.lineWidth=1.8;
    function traceRows(valueAt,color,dash){
      ctx.strokeStyle=color;ctx.setLineDash(dash);ctx.beginPath();
      var open=false;
      for(var yy=H-1;yy>=0;yy--){
        var row=result.rows[yy];
        if(row&&row.valid){
          var xx=valueAt(row);
          if(!open){ctx.moveTo(xx+0.5,yy+0.5);open=true;}
          else ctx.lineTo(xx+0.5,yy+0.5);
        }else open=false;
      }
      ctx.stroke();
    }
    traceRows(function(row){return row.left;},"#D64A3B",[]);
    traceRows(function(row){return row.right;},"#2678C8",[]);
    traceRows(function(row){return (row.left+row.right)/2;},"#168A58",[5,3]);
    if(mode==="longest"&&result.seedL>=0){
      ctx.strokeStyle="#7652A5";ctx.lineWidth=1.2;ctx.setLineDash([3,3]);ctx.beginPath();
      ctx.moveTo(result.seedL+0.5,H-1);ctx.lineTo(result.seedL+0.5,Math.max(0,H-result.stop));
      ctx.moveTo(result.seedR+0.5,H-1);ctx.lineTo(result.seedR+0.5,Math.max(0,H-result.stop));
      ctx.stroke();
    }
    ctx.strokeStyle="#E0A51C";ctx.lineWidth=1.5;ctx.setLineDash([3,2]);ctx.beginPath();
    ctx.moveTo(0,y+0.5);ctx.lineTo(W,y+0.5);ctx.stroke();
    if(e&&e.valid){
      var mid=(e.left+e.right)/2;
      ctx.setLineDash([]);
      ctx.fillStyle="#D64A3B";ctx.beginPath();ctx.arc(e.left+0.5,y+0.5,2.2,0,Math.PI*2);ctx.fill();
      ctx.fillStyle="#2678C8";ctx.beginPath();ctx.arc(e.right+0.5,y+0.5,2.2,0,Math.PI*2);ctx.fill();
      ctx.fillStyle="#7652A5";ctx.beginPath();ctx.arc(mid+0.5,y+0.5,2.6,0,Math.PI*2);ctx.fill();
    }
    ctx.restore();
    document.getElementById("scan-row-value").value=String(y);
    document.getElementById("scan-status").textContent=e&&e.valid
      ?"y="+y+"；L="+e.left+"；R="+e.right+"；中点="+Math.floor((e.left+e.right)/2)+"；各行中线与边界已用不同颜色连线"
       +(mode==="longest"?"；种子="+result.seedL+"/"+result.seedR+"，搜索截止 "+result.stop+" 行":"")
      :"y="+y+" 没有有效双边界；记 valid=0，不把画面边缘伪造为边线。";
  }
  document.getElementById("scan-mode").addEventListener("change",updateScanning);
  document.getElementById("scan-row").addEventListener("input",updateScanning);

  /* A boundary of a white road region, climbed from the near end to the far end. */
  var tracePath=[[2,7],[2,6],[3,5],[4,4],[4,3],[3,2],[2,1],[2,0]];
  var traceDirections=["N","NE","NE","N","NW","NW","N"];
  function drawNeighborSteps() {
    var canvas=document.getElementById("neighbor-canvas"),ctx=canvas.getContext("2d");
    var step=Number(document.getElementById("neighbor-steps").value),cell=38,cols=8,rows=8;
    var ox=(canvas.width-cols*cell)/2,oy=(canvas.height-rows*cell)/2;
    ctx.clearRect(0,0,canvas.width,canvas.height);ctx.fillStyle="#f0f1ed";ctx.fillRect(0,0,canvas.width,canvas.height);
    for(var y=0;y<rows;y++)for(var x=0;x<cols;x++){
      var index=tracePath.findIndex(function(p){return p[0]===x&&p[1]===y;});
      var roadLeft=[2,2,2,3,4,4,3,2][y];
      var onRoad=x>=roadLeft&&x<=Math.min(cols-1,roadLeft+3);
      ctx.fillStyle=onRoad?"#E7EFE9":"#303B38";
      if(index>=0)ctx.fillStyle="#BCCBC2";
      ctx.fillRect(ox+x*cell+1,oy+y*cell+1,cell-2,cell-2);
      ctx.strokeStyle="#b8c4bd";ctx.strokeRect(ox+x*cell+1,oy+y*cell+1,cell-2,cell-2);
      if(index>=0&&index<step){ctx.fillStyle="#173A32";ctx.font="14px sans-serif";ctx.textAlign="center";ctx.textBaseline="middle";ctx.fillText(String(index),ox+x*cell+cell/2,oy+y*cell+cell/2);}
    }
    var current=tracePath[Math.min(step,tracePath.length-1)];
    var center=function(p){return [ox+p[0]*cell+cell/2,oy+p[1]*cell+cell/2];};
    if(step>0){
      ctx.beginPath();
      var start=center(tracePath[0]);ctx.moveTo(start[0],start[1]);
      for(var i=1;i<=step;i++){var p=center(tracePath[i]);ctx.lineTo(p[0],p[1]);}
      ctx.strokeStyle="#D43F35";ctx.lineWidth=4;ctx.lineCap="round";ctx.lineJoin="round";ctx.setLineDash([]);ctx.stroke();
    }
    var cur=center(current);
    if(step<tracePath.length-1){
      var next=center(tracePath[step+1]);
      ctx.beginPath();ctx.moveTo(cur[0],cur[1]);ctx.lineTo(next[0],next[1]);
      ctx.strokeStyle="#B3261E";ctx.lineWidth=2.2;ctx.setLineDash([4,2]);ctx.stroke();ctx.setLineDash([]);
      var angle=Math.atan2(next[1]-cur[1],next[0]-cur[0]),head=8;
      ctx.beginPath();ctx.moveTo(next[0],next[1]);
      ctx.lineTo(next[0]-head*Math.cos(angle-.55),next[1]-head*Math.sin(angle-.55));
      ctx.lineTo(next[0]-head*Math.cos(angle+.55),next[1]-head*Math.sin(angle+.55));ctx.closePath();
      ctx.fillStyle="#B3261E";ctx.fill();
    }
    ctx.beginPath();ctx.arc(cur[0],cur[1],8,0,Math.PI*2);
    ctx.fillStyle="#D43F35";ctx.fill();ctx.strokeStyle="#fff";ctx.lineWidth=2;ctx.stroke();
    document.getElementById("neighbor-steps-value").value=String(step);
    var entry=step>0?traceDirections[Math.min(step-1,traceDirections.length-1)]:"种子";
    var nextDir=step<tracePath.length-1?traceDirections[step]:"到达演示终点";
    document.getElementById("neighbor-status").textContent="已爬="+step+" 步；当前位置 ("+current[0]+","+current[1]+")；进入="+entry+"；下一步="+nextDir+"。";
  }
  document.getElementById("neighbor-steps").addEventListener("input",drawNeighborSteps);

  function solveLinear(matrix) {
    var n=matrix.length;
    for(var col=0;col<n;col++){
      var pivot=col;
      for(var row=col+1;row<n;row++)if(Math.abs(matrix[row][col])>Math.abs(matrix[pivot][col]))pivot=row;
      if(Math.abs(matrix[pivot][col])<1e-10)return null;
      var tmp=matrix[col];matrix[col]=matrix[pivot];matrix[pivot]=tmp;
      var d=matrix[col][col];for(var j=col;j<=n;j++)matrix[col][j]/=d;
      for(var r=0;r<n;r++)if(r!==col){var f=matrix[r][col];for(var k=col;k<=n;k++)matrix[r][k]-=f*matrix[col][k];}
    }
    return matrix.map(function(row){return row[n];});
  }
  function homography(src,dst) {
    var a=[];
    for(var i=0;i<4;i++){
      var x=src[i][0],y=src[i][1],u=dst[i][0],v=dst[i][1];
      a.push([x,y,1,0,0,0,-u*x,-u*y,u]);
      a.push([0,0,0,x,y,1,-v*x,-v*y,v]);
    }
    var h=solveLinear(a);
    return h?[h[0],h[1],h[2],h[3],h[4],h[5],h[6],h[7],1]:null;
  }
  function inverse3(m) {
    var a=m[0],b=m[1],c=m[2],d=m[3],e=m[4],f=m[5],g=m[6],h=m[7],i=m[8];
    var det=a*(e*i-f*h)-b*(d*i-f*g)+c*(d*h-e*g);
    if(Math.abs(det)<1e-10)return null;
    return [(e*i-f*h)/det,(c*h-b*i)/det,(b*f-c*e)/det,
      (f*g-d*i)/det,(a*i-c*g)/det,(c*d-a*f)/det,
      (d*h-e*g)/det,(b*g-a*h)/det,(a*e-b*d)/det];
  }
  function updateIpm() {
    var widthPercent=Number(document.getElementById("ipm-top-width").value);
    var source=makeFrame("curve",W,H),half=W*widthPercent/200,cx=W/2;
    var src=[[cx-half,12],[cx+half,12],[W-2,H-2],[1,H-2]];
    var dst=[[W*.28,8],[W*.72,8],[W*.72,H-8],[W*.28,H-8]];
    var h=homography(src,dst),inv=h?inverse3(h):null,output=new Uint8ClampedArray(W*H);
    if(inv)for(var y=0;y<H;y++)for(var x=0;x<W;x++){
      var den=inv[6]*x+inv[7]*y+inv[8];
      if(Math.abs(den)<1e-9)continue;
      var sx=(inv[0]*x+inv[1]*y+inv[2])/den;
      var sy=(inv[3]*x+inv[4]*y+inv[5])/den;
      var ix=Math.round(sx),iy=Math.round(sy);
      if(ix>=0&&ix<W&&iy>=0&&iy<H)output[y*W+x]=source[iy*W+ix];
    }
    var inputCanvas=document.getElementById("ipm-input");
    paint(inputCanvas,source,W,H);paint(document.getElementById("ipm-output"),output,W,H);
    var ctx=inputCanvas.getContext("2d");
    ctx.save();ctx.scale(inputCanvas.width/W,inputCanvas.height/H);
    ctx.strokeStyle="#fff";ctx.lineWidth=1;ctx.setLineDash([2,2]);ctx.beginPath();
    ctx.moveTo(src[0][0],src[0][1]);ctx.lineTo(src[1][0],src[1][1]);ctx.lineTo(src[2][0],src[2][1]);ctx.lineTo(src[3][0],src[3][1]);ctx.closePath();ctx.stroke();
    ctx.setLineDash([]);src.forEach(function(p){ctx.fillStyle="#252f2e";ctx.fillRect(p[0]-1.5,p[1]-1.5,3,3);});ctx.restore();
    document.getElementById("ipm-width-value").value=widthPercent+"%";
    document.getElementById("ipm-status").textContent=inv
      ?"源四点："+src.map(function(p){return "("+p[0].toFixed(0)+","+p[1]+")";}).join(" ")
       +"；H[2][2]=1；目标像素通过 H⁻¹ 回原图取样。"
      :"四点退化，单应矩阵不可解。";
  }
  document.getElementById("ipm-top-width").addEventListener("input",updateIpm);

  function updateSampling() {
    var scale=Number(document.getElementById("scale-slider").value)/100,w0=256,h0=160;
    var source=makeFrame("normal",w0,h0),w=Math.round(w0*scale),h=Math.round(h0*scale);
    var small=new Uint8ClampedArray(w*h);
    for(var y=0;y<h;y++)for(var x=0;x<w;x++){
      var sx=Math.min(w0-1,Math.floor((x+.5)*w0/w));
      var sy=Math.min(h0-1,Math.floor((y+.5)*h0/h));
      small[y*w+x]=source[sy*w0+sx];
    }
    paint(document.getElementById("full-canvas"),source,w0,h0);
    paint(document.getElementById("scaled-canvas"),small,w,h);
    document.getElementById("scale-value").value=Math.round(scale*100)+"%";
    document.getElementById("scaled-size").textContent=w+" × "+h;
    document.getElementById("gray-bytes").textContent=(w*h).toLocaleString("zh-CN")+" B";
    document.getElementById("packed-bytes").textContent=Math.ceil(w*h/8).toLocaleString("zh-CN")+" B";
  }
  document.getElementById("scale-slider").addEventListener("input",updateSampling);

  updateThreshold();updateSobel();updateScanning();drawNeighborSteps();updateIpm();updateSampling();
}());
