'use strict'

const electron = require('electron');
const app = electron.app;
const BrowserWindow = electron.BrowserWindow;

var mainWindow = null;

app.on('window-all-closed', function()
       {
           if (process.platform != 'darwin')
           {
               app.quit();
           }
       });

app.on('ready', function()
       {
           mainWindow = new BrowserWindow({width: 960,
                                    height: 720,
                                    icon: './Skeleton-2.0.4/images/favicon.png',
                                    // 'title-bar-style': 'hidden'
                                    // frame: false
                                          });
           mainWindow.loadURL('file://' + __dirname + '/index.html');

           mainWindow.on('closed', function()
                         {
                             // Let GC deal with it
                             mainWindow = null;
                         });

       });
