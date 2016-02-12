'use strict'

const electron = require('electron');
const app = electron.app;
const BrowserWindow = electron.BrowserWindow;

var mainWindow = null;
global.state = {inner: null, graphs: null};

app.on('window-all-closed', function()
       {
            app.quit();
       });

app.on('ready', function()
       {
           mainWindow = new BrowserWindow({width: 960,
                                           height: 720,
                                           icon: './Skeleton-2.0.4/images/favicon.png',
                                           minWidth: 800,
                                           minHeight: 600
                                          });
           mainWindow.loadURL('file://' + __dirname + '/index.html');

           mainWindow.on('closed', function()
                         {
                             // Let GC deal with it
                             mainWindow = null;
                         });

       });
