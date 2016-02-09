const remote = require('remote');
const BrowserWindow = remote.BrowserWindow;
const dialog = remote.require('dialog');
const execFile = require('child_process').execFile;
const $ = require('jQuery');
// const $.serializeJSON = require('jQuery.serializeJSON');

var OperatingMode = Object.freeze({SingleSimulation : {},
                                   CompareProb0 : {},
                                   CompareProb1 : {},
                                   CompareTwo : {}});

var state = {};
// state.defaultJSON = JSON.parse('default.json');
(function loadDefauts(){
    $.getJSON('default.json', function(data){
        state.defaults = data;
    });
    state.mode = OperatingMode.SingleSimulation;
})();

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
<input type="number" class="u-full-width" placeholder="' + state.defaults.MaxRelErr.toString() +
        '" id="maxRelErr" name="maxRelErr">\
</div>'
    + '<div class="six columns">\
<label for="maxIter">Max Iterations</label>\
<input type="number" class="u-full-width" placeholder="' + state.defaults.MaxIterations.toString() +
        '" id="maxIter" name="maxIter">\
</div></div>'
    // The above pair is 1 row
    +
        '<div class="row">\
<div class="six columns">\
<label for="ppm">Pixels Per Meter</label>\
<input type="number" class="u-full-width" placeholder="' + state.defaults.PixelsPerMeter.toString() +
        '" id="ppm" name="ppm">\
</div>'
    + '<div class="three columns">\
<label for="vzip">Vertical Zip</label>\
<input type="checkbox" id="vzip" name="vzip"></div>'
    + '<div class="three columns">\
<label for="hzip">Horizontal Zip</label>\
<input type="checkbox" id="hzip" name="hzip">\
</div></div>'

    if (state.mode === OperatingMode.CompareProb0
        || state.mode === OperatingMode.CompareProb1) {

        html += '<div class="row">\
<div class="six columns">\
<label for="innerRad">Inner Circle Radius</label>\
<input type="number" class="u-full-width" id="innerRad" name="innerRad">\
</div>'
        +
'<div class="six columns">\
<label for="outerRad">Outer Circle Radius</label>\
<input type="number" class="u-full-width" id="outerRad" name="outerRad">\
</div></div>'
    }
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
    var name = select.name.toString();
    var number = name.match('[^\\d]*(\\d*)')[1];
    if (select.value !== 'const') {
        document.getElementById('color' + number.toString()).setAttribute('disabled', 'disabled');
    } else {
        document.getElementById('color' + number.toString()).removeAttribute('disabled');
    }
}

function selectJSONToOption(str) {
    switch (str) {
    case 'Constant':
        return '<option value="const" selected>Constant</option>\
<option value="hlerp">Horizontal Lerp</option>\
<option value="vlerp">Vertical Lerp</option>'
    case 'HorizontalLerp':
        return '<option value="const">Constant</option>\
<option value="hlerp" selected>Horizontal Lerp</option>\
<option value="vlerp">Vertical Lerp</option>'
    case 'VerticalLerp':
        return '<option value="const">Constant</option>\
<option value="hlerp">Horizontal Lerp</option>\
<option value="vlerp" selected>Vertical Lerp</option>'
    }
}

function colorHTMLObject(col, i) {

    var inMap = false;
    var index = 0;
    var cmap = state.defaults.ColorMap;
    for (var j = 0; j < cmap.length; ++j) {
        if (JSON.stringify(cmap[j].Color) === JSON.stringify(col)) {
            inMap = true;
            index = j;
        }
    }

    if (inMap) {
        var name = 'color' + i.toString();
        var html = '<div class="six columns">\
<label for="' + name + '">Color '+ i.toString() +'\
<div class="color-box u-fill-width" style="background-color:'+ rgbToHex(col.r, col.g, col.b)+ '"></div>\
</label>';
        if (cmap[index].Type === 'Constant'){
            html +='<input type="number" name="color' + i.toString() + '" class="u-full-width" placeholder="Constant" value="' + cmap[index].Value + '" id="color' + i.toString() + '">';
        } else {
            html +='<input type="number" name="color' + i.toString() + '" class="u-full-width" placeholder="Constant" disabled="disabled" id="color' + i.toString() + '">';
        }
        html += '<select class="u-full-width" name="selectColor' + i.toString() + '" "id="selectColor' + i.toString() + '" onchange="checkConstant(this)">';
        html += selectJSONToOption(cmap[index].Type);
        html += '</select></div>';
        return html;

    } else {

        var name = 'color' + i.toString();
        var html = '<div class="six columns">\
<label for="' + name + '">Color '+ i.toString() +'\
<div class="color-box u-fill-width" style="background-color:'+ rgbToHex(col.r, col.g, col.b)+ '"></div>\
</label>\
<input type="number" name="color' + i.toString() + '" class="u-full-width" placeholder="Constant" id="color' + i.toString() + '">\
<select class="u-full-width" name="selectColor' + i.toString() + '" "id="selectColor' + i.toString() + '" onchange="checkConstant(this)">\
<option value="const">Constant</option>\
<option value="hlerp">Horizontal Lerp</option>\
<option value="vlerp">Vertical Lerp</option>\
</select>\
</div>'
        return html;
    }

}

function SubmitButtonHTML() {
    var html = '<div class="row" align="right">\
<input type="button" value="Simulate" onclick="runForm();"></div>';
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

function outputSimDataForm(json) {
    var form = document.getElementById('colorData');
    state.numColors = json.ColorMap.length;
    var header = headerToHTML(json);
    var cmap = colorDataToHTML(json);
    var simButton = SubmitButtonHTML();
    form.innerHTML = '<form id="mainForm">' + header + cmap + simButton + '</form>';
}

function setOperatingMode() {
    switch ($('#simMode').val()) {
    case "singleSim": {
        state.mode = OperatingMode.SingleSimulation;
    } break;
    case "compareProb0": {
        state.mode = OperatingMode.CompareProb0;
    } break;
    case "compareProb1": {
        state.mode = OperatingMode.CompareProb1;
    } break;
    case "compare2": {
        state.mode = OperatingMode.CompareTwo;
    } break;
    };
}


function preprocessImg(img) {
    // TODO(Chris): Need to handle bindir properly
    var path = img.src.match('//([\\s\\S]*)$');
    if (path != null) {
        path = path[1];
    }

    setOperatingMode();

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
                                       outputSimDataForm(json);
                                       state.jsonIn = json;
                                   } else {
                                    dialog.showErrorBox(
                                        "Unable to parse json.",
                                        json.toString());
                                   }
                               }
                           });
}

function jsonDefaultOr(json, field, value, func) {
    if (value === '') {
        json[field] = state.defaults[field];
    } else {
        json[field] = func(value);
    }
    return json;
}

function runForm() {
    var form = $('#mainForm').serializeArray();
    var data = JSON.stringify(form);
    console.log(form);
    // console.log(data);
    var json = state.jsonIn;

    var cmap = [];

    for (var i in form) {
        // console.log(form[i].name);
        // console.log(form[i].value);
        switch (form[i].name) {

        case 'maxRelErr': {
            json = jsonDefaultOr(json, 'MaxRelErr', form[i].value, parseFloat)
        } continue;

        case 'maxIter': {
            json = jsonDefaultOr(json, 'MaxIterations', form[i].value, parseFloat)
        } continue;

        case 'ppm': {
            json = jsonDefaultOr(json, 'PixelsPerMeter', form[i].value, parseFloat)
        } continue;

        case 'vzip': {
            json.VerticalZip = true;
        } continue;

        case 'hzip': {
            json.HorizontalZip = true;
        } continue;

        case 'innerRad': {
            if (form[i].value === '') {
                alert('Please set the inner radius in pixels');
                return;
            } else {
                json.AnalyticInnerRadius = parseFloat(form[i].value);
            }
        } continue;

        case 'outerRad': {
            if (form[i].value === '') {
                alert('Please set the outer radius in pixels');
                return;
            } else {
                json.AnalyticOuterRadius = parseFloat(form[i].value);
            }
        } continue;
        }

        if (form[i].name.match(/color/i)) {
            var str = form[i].name.match(/([^\d]*)/);
            var num = parseInt(form[i].name.match(/[^\d]*([\d]*)/)[1]);
            switch (str[0]) {
            case 'color': {
                if (cmap[num] == null) {
                    cmap[num] = {};
                }
                var val = parseFloat(form[i].value);
                if (isNaN(val)) {
                    alert('Please provide a Constant value for ' + $('label[for="' + form[i].name.toString() + '"]')[0].innerText);
                    return;
                } else {
                    cmap[num].Value = val;
                }
            } continue;

            case 'selectColor': {
                if (cmap[num] == null) {
                    cmap[num] = {};
                }
                cmap[num].Type = form[i].value;
                cmap[num].Color = state.defaults.ColorMap[num].Color;

            } continue;

            default:
                console.log('Unknown thingy:');
                console.log(str);
            }
        }
    }

    json.ColorMap = cmap;
    console.log(JSON.stringify(json));

}
