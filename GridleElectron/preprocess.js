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
        document.getElementById("mainImage").src = paths[0];
    }
}

function resizeImg(img) {
    var maxSize = document.getElementById("imgHolder").clientWidth;
    if (maxSize > 400)
        maxSize = 400;

    var height = img.naturalHeight;
    var width = img.naturalWidth;

    if (height >= width) {
        img.height = maxSize;
        img.width = (maxSize/height) * width;
    } else {
        img.width = maxSize;
        img.height = (maxSize/width) * height;
    }
}

function preprocessImg(img) {
    console.log('pp')

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
