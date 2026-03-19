const { contextBridge, ipcRenderer } = require('electron');

contextBridge.exposeInMainWorld('chat', {
  // renderer → main
  connect:      (opts)             => ipcRenderer.invoke('connect', opts),
  sendBroadcast:(message)          => ipcRenderer.send('send-broadcast', message),
  sendDM:       (to, message)      => ipcRenderer.send('send-dm', { to, message }),
  changeStatus: (status)           => ipcRenderer.send('change-status', status),
  listUsers:    ()                 => ipcRenderer.send('list-users'),
  getUserInfo:  (username)         => ipcRenderer.send('get-user-info', username),
  quit:         ()                 => ipcRenderer.send('quit'),

  // main → renderer (event listeners)
  onBroadcast:    (cb) => ipcRenderer.on('broadcast',      (_, d) => cb(d)),
  onDM:           (cb) => ipcRenderer.on('dm',             (_, d) => cb(d)),
  onAllUsers:     (cb) => ipcRenderer.on('all-users',      (_, d) => cb(d)),
  onUserInfo:     (cb) => ipcRenderer.on('user-info',      (_, d) => cb(d)),
  onServerResp:   (cb) => ipcRenderer.on('server-response',(_, d) => cb(d)),
});