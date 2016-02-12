const remote = require('remote');
const BrowserWindow = remote.BrowserWindow;
const dialog = remote.require('dialog');
const execFile = require('child_process').execFile;
const mustache = require('./node_modules/mustache/mustache.min.js');
const ndjson = require('./node_modules/ndjson/index.js');
// preload.js
// require('module').globalPaths.push(__dirname + '/node_modules');
const $ = require('./node_modules/jquery/dist/jquery.min.js');
const fs = require('fs');
const EOL = require('os').EOL;
// const $.serializeJSON = require('jQuery.serializeJSON');

var OperatingMode = Object.freeze({SingleSimulation : {},
                                   CompareProb0 : {},
                                   CompareProb1 : {},
                                   CompareTwo : {}});

var state = {
    child : null
};

(function loadDefauts(){
    $.getJSON('default.json', function(data){
        state.defaults = data;
    });
    state.mode = OperatingMode.SingleSimulation;
    window.onbeforeunload = function (event) {
        if (state.child != null) {
            state.child.kill();
            alert("Killing child on exit...");
        }
    }

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
    var html = mustache.to_html($('#headerTemplate').html(), json);

    if (state.mode === OperatingMode.CompareProb0
        || state.mode === OperatingMode.CompareProb1) {

        html += $('#header0Or1Template').html();
    }
    return html;
}

// http://stackoverflow.com/a/5624139/3847013 for the next two functions
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
        return $('#selectConstant').html();
    case 'HorizontalLerp':
        return $('#selectHLerp').html();
    case 'VerticalLerp':
        return $('#selectVLerp').html();
    }
}

function disableBox(str) {
    switch (str) {
    case 'Constant':
        return '';
    default:
        return 'disabled="disabled"'
    }
}

function colorHTMLObject(col, i) {

    var inMap = false;
    var index = 0;
    var cmap = state.defaults.ColorMap;
    for (var j = 0; j < cmap.length; ++j) {
        // Order matters here so be careful to match the default.json file when using this
        if (JSON.stringify(cmap[j].Color) === JSON.stringify(col)) {
            inMap = true;
            index = j;
        }
    }

    var data;
    if (inMap) {
        data = cmap[index];
        data.i = i;
        data.selector = selectJSONToOption(cmap[index].Type);
        data.disabled = disableBox(cmap[index].Type);
        data.colorString = rgbToHex(col.r, col.g, col.b);
    } else {
        data = {
            i : i,
            selector : selectJSONToOption('Constant'),
            colorString : rgbToHex(col.r, col.g, col.b)
        };
    }
    var html = mustache.to_html($('#colorTemplate').html(), data);
    return html;
}


function colorDataToHTML(json) {
    var html = '';
    for (i = 0; i < json.ColorMap.length; i += 2) {
        var col = json.ColorMap[i];
        var colObj = {};
        colObj.colorObj1 = colorHTMLObject(col, i);

        if (i+1 < json.ColorMap.length) {
            col = json.ColorMap[i+1];
            colObj.colorObj2 = colorHTMLObject(col, i+1);
        }

        html += mustache.to_html($('#colorRowTemplate').html(), colObj);
    }
    var colorData = mustache.to_html($('#colorDataTemplate').html(), { colorList : html});
    return colorData;
}

function outputSimDataForm(json) {
    var form = document.getElementById('colorData');
    var html = mustache.to_html($('#formTemplate').html(), { header : headerToHTML(json),
                                                             colors : colorDataToHTML(json),
                                                             button : $('#submitButtonTemplate').html()});
    form.innerHTML = html;
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
    // console.log(data);
    var json = Object.assign({}, state.jsonIn);

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
                cmap[num].Color = json.ColorMap[num];

            } continue;

            default:
                console.log('Unknown thingy:');
                console.log(str);
            }
        }
    }

    json.ColorMap = cmap;
    const jsonStr = JSON.stringify(json, null, 4);

    var modeString;

    switch (state.mode) {
    case OperatingMode.SingleSimulation: {
        modeString = '-i';
    } break;

    case OperatingMode.CompareProb0: {
        modeString = '-0';
    } break;

    case OperatingMode.CompareProb1: {
        modeString = '-1';
    } break;

    case OperatingMode.CompareTwo: {
        modeString = '-c';
    } break;
    }

    state.jsonExec = jsonStr;
    remote.getGlobal('state').inner = state;

    runSimulation(modeString, jsonStr);
}

// To Marc, with <3
function killChild() {
    state.child.kill();
}

function viewOutput() {

}

function runSimulation(modeString, json) {
    document.getElementById('simulationData').innerHTML = $('#simulationDataTemplate').html();

    if (state.child == null)
    {

        const child = execFile('bin/gridle', ['-j', '-g', modeString], { cwd: ".."},
                               (error, stdout, stderr) => {
                                   if (error !== null) {
                                       alert(stderr);
                                   } else {
                                       viewOutput();
                                   }
                               });

        child.stdout.pipe(ndjson.parse())
            .on('data', (data) => {
                var out = document.getElementById('gridleOutput');
                var currentText = out.innerHTML;
                var str = $('<div>').text(data.message).html();
                currentText += EOL + str;
                currentText = currentText.match(/\s*([\s\S]*)/)[1];
                out.innerHTML = currentText;
            })

        state.child = child;
        child.stdin.write(json);
        child.stdin.end();
        child.on('close', (code) => {
            state.child = null;
            console.log(`Done with ${code}`);
            if (code == 0) {
                viewOutput();
            }
        })
    } else {
        // $('#gridleOutput').text('Process already running: please kill current simulation first');
        alert('Process already running: please kill current simulation first');
    }

}
