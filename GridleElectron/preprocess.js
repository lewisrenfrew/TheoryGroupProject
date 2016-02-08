const remote = require('remote');
const BrowserWindow = remote.BrowserWindow;
const dialog = remote.require('dialog');
const execFile = require('child_process').execFile;

function loadImage() {
    const paths = dialog.showOpenDialog({ properties: ['openFile'],
                                          filters: [
                                              { name: 'PNG Images', extensions: ['png'] },
                                              { name: 'All files', extensions: ['*'] }
                                          ]
                                        });
    if (paths) {
        document.getElementById("mainImg").src = paths[0];
    }
}

function resizeImg(img) {
    var maxSize = document.getElementById("imgHolder").clientWidth;
    if (maxSize > 400)
        maxSize = 400;

    var height = img.naturalHeight;
    var width = img.naturalWidth;

    if (height > 400 || width > 400) {
        // No error if not present
        img.classList.remove('pixels');
    } else {
        // This won't duplicate it
        img.classList.add('pixels');
    }

    if (height >= width) {
        img.height = maxSize;
        img.width = (maxSize/height) * width;
    } else {
        img.width = maxSize;
        img.height = (maxSize/width) * height;
    }
}

function headerToHTML(json) {
    var html =
'<h6 class="sec-title">Default Parameters</h6>\
        <div class="row">\
<div class="six columns">\
<label for="maxRelErr">Max Relative Error</label>\
<input type="number" class="u-full-width" placeholder="' + json.MaxRelErr.toString() +
        '" id="maxRelErr">\
</div>'
    + '<div class="six columns">\
<label for="maxIter">Max Iterations</label>\
<input type="number" class="u-full-width" placeholder="' + json.MaxIterations.toString() +
        '" id="maxIter">\
</div></div>'
    // The above pair is 1 row
    +
        '<div class="row">\
<div class="six columns">\
<label for="ppm">Pixels Per Meter</label>\
<input type="number" class="u-full-width" placeholder="' + json.PixelsPerMeter.toString() +
        '" id="ppm">\
</div>'
    + '<div class="three columns">\
<label for="vzip">Vertical Zip</label>\
<input type="checkbox" id="vzip"></div>'
    + '<div class="three columns">\
<label for="hzip">Horizontal Zip</label>\
<input type="checkbox" id="hzip">\
</div></div>'
    // The above group is 1 row
    return html;
}


// http://stackoverflow.com/a/5624139/3847013 for the next to functions
function componentToHex(c) {
    var hex = c.toString(16);
    return hex.length == 1 ? "0" + hex : hex;
}

function rgbToHex(r, g, b) {
    return "#" + componentToHex(r) + componentToHex(g) + componentToHex(b);
}

function checkConstant(select) {
    var name = select.id.toString();
    var number = name.match('[^\\d]*(\\d*)')[1];
    if (select.value !== 'const') {
        document.getElementById('color' + number.toString()).setAttribute('disabled', 'disabled');
    } else {
        document.getElementById('color' + number.toString()).removeAttribute('disabled');
    }
}

function colorHTMLObject(col, i) {
    var html = '<div class="six columns">\
<label for="color' + i.toString() + '">Color '+ i.toString() +'\
<div class="color-box u-fill-width" style="background-color:'+ rgbToHex(col.r, col.g, col.b)+ '"></div>\
</label>\
<input type="number" class="u-full-width" placeholder="Constant" id="color' + i.toString() + '">\
<select class="u-full-width" id="selectColor' + i.toString() + '" onchange="checkConstant(this)">\
<option value="const">Constant</option>\
<option value="hlerp">Horizontal Lerp</option>\
<option value="vlerp">Vertical Lerp</option>\
</select>\
</div>'
    return html;
}

function SubmitButtonHTML() {
    var html = '<div class="row" align="right">\
<input type="button" value="Simulate"></div>';
    return html;

}

function colorDataToHTML(json) {
    var html = '<h6 class="sec-title">Color Data</h6>'
    for (i = 0; i < json.ColorMap.length; i += 2) {
        html +='<div class="row">'
        var col = json.ColorMap[i];
        html += colorHTMLObject(col, i);

        if (i+1 < json.ColorMap.length) {
            col = json.ColorMap[i+1];
            html += colorHTMLObject(col, i+1);
        }
        html += '</div>'
    }
    return html;
}

function outputSimData(json) {
    var form = document.getElementById('colorData');
    var header = headerToHTML(json);
    var cmap = colorDataToHTML(json);
    var simButton = SubmitButtonHTML();
    form.innerHTML = header + cmap + simButton;

}

function preprocessImg(img) {
    // TODO(Chris): Need to handle bindir properly
    var path = img.src.match('//([\\s\\S]*)$');
    if (path != null) {
        path = path[1];
    }

    const child = execFile('bin/gridle', ['-E', path], { cwd: ".."},
                           (error, stdout, stderr) => {
                               var json;
                               if (stdout !== '') {
                                   json = stdout.match('\\*\\*JSON_START\\*\\*([\\s\\S]*)\\*\\*EOF\\*\\*');
                               }
                               if (json == null) {
                                   dialog.showErrorBox(
                                       "Error preprocessing image.",
                                       "Gridle returned: " + ((stdout !== '') ? stdout : '')
                                           + ((stderr !== '') ? stderr : ''))
                               } else {
                                   json = JSON.parse(json[1]);
                                   if (json != null) {
                                       outputSimData(json);
                                   } else {
                                    dialog.showErrorBox(
                                        "Unable to parse json.",
                                        json.toString());
                                   }
                               }
                           });
}

function RunPrinter() {
    const child = execFile('make', ['plot'], { cwd: ".." },
                           (error, stdout, stderr) => {
                               // var str = document.getElementById("PrintHere").innerHTML;
                               var str = '<pre><code>stdout: ' + stdout;
                               if (stderr !== '') {
                                   str += ' stderr: ' + stderr;
                               }
                               if (error !== null) {
                                   str += ' error: ' + error;
                               }
                               str += '</code></pre>';
                               // console.log(str);
                               document.getElementById("PrintHere").innerHTML = str;
                           });
    child.on('close', (code) => {
        console.log(`Done with ${code}`);
    })
}
