const remote = require('remote');
const BrowserWindow = remote.BrowserWindow;
const dialog = remote.require('dialog');
const execFile = require('child_process').execFile;
const mustache = require('./node_modules/mustache/mustache.min.js');
const ndjson = require('./node_modules/ndjson/index.js');
const $ = require('./node_modules/jquery/dist/jquery.min.js');
const fs = require('fs');
const EOL = require('os').EOL;

var graphs = remote.getGlobal('state').graphs;

(function () {
    window.onload = function () {
        var sel = document.getElementById('selectGraph');
        sel.innerHTML = mustache.to_html($('#selectTemplate').html(), {graphs});
        updateGraph(document.getElementById('simType'));
    }
}
)();

function updateGraph(select) {
    var gph = document.getElementById('graphHolder');
    gph.innerHTML = mustache.to_html($('#graphTemplate').html(), {value : select.value});
}
