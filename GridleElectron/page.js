const remote = require('remote');
const BrowserWindow = remote.BrowserWindow;
const dialog = remote.require('dialog');
const execFile = require('child_process').execFile;

function OpenFileDialog() {
    // var fileBrowser = new BrowserWindow({height: 400, width: 640});
    const paths = dialog.showOpenDialog({ properties: ['openFile'],
                                          filters: [
                                              { name: "PNG Images", extensions: ['png'] },
                                              { name: 'All files', extensions: ['*'] }
                                          ]
                                        });
    // fileBrowser = null;
    console.log(paths);
    if (paths) {
        document.getElementById("MainImage").src = paths[0];
    }
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

function ReadText() {
    // var str = document.getElementById("Editor").value;
    // document.getElementById("PrintHere").innerHTML = str;
    OpenFileDialog();
}
