const { app, BrowserWindow, ipcMain } = require('electron');
const net = require('net');
const path = require('path');
const protobuf = require('protobufjs');

const PROTO_DIR = path.join(__dirname, '..', 'protos');

const MSG = {
  REGISTER:        1,
  GENERAL:         2,
  DM:              3,
  CHANGE_STATUS:   4,
  LIST_USERS:      5,
  GET_USER_INFO:   6,
  QUIT:            7,
  SERVER_RESPONSE: 10,
  ALL_USERS:       11,
  FOR_DM:          12,
  BROADCAST:       13,
  USER_INFO_RESP:  14,
};

let mainWindow = null;
let socket     = null;
let myUsername = '';
let myIp       = '';
let protos     = {};
let recvBuf    = Buffer.alloc(0);

async function loadProtos() {
  const root = new protobuf.Root();
  root.resolvePath = (_, filename) => {
    if (filename === 'common.proto')
      return path.join(PROTO_DIR, 'common.proto');
    return filename;
  };
  await root.load([
    path.join(PROTO_DIR, 'common.proto'),
    path.join(PROTO_DIR, 'cliente-side', 'register.proto'),
    path.join(PROTO_DIR, 'cliente-side', 'message_general.proto'),
    path.join(PROTO_DIR, 'cliente-side', 'message_dm.proto'),
    path.join(PROTO_DIR, 'cliente-side', 'change_status.proto'),
    path.join(PROTO_DIR, 'cliente-side', 'list_users.proto'),
    path.join(PROTO_DIR, 'cliente-side', 'get_user_info.proto'),
    path.join(PROTO_DIR, 'cliente-side', 'quit.proto'),
    path.join(PROTO_DIR, 'server-side', 'server_response.proto'),
    path.join(PROTO_DIR, 'server-side', 'all_users.proto'),
    path.join(PROTO_DIR, 'server-side', 'for_dm.proto'),
    path.join(PROTO_DIR, 'server-side', 'broadcast_messages.proto'),
    path.join(PROTO_DIR, 'server-side', 'get_user_info_response.proto'),
  ]);
  protos = {
    Register:            root.lookupType('chat.Register'),
    MessageGeneral:      root.lookupType('chat.MessageGeneral'),
    MessageDM:           root.lookupType('chat.MessageDM'),
    ChangeStatus:        root.lookupType('chat.ChangeStatus'),
    ListUsers:           root.lookupType('chat.ListUsers'),
    GetUserInfo:         root.lookupType('chat.GetUserInfo'),
    Quit:                root.lookupType('chat.Quit'),
    ServerResponse:      root.lookupType('chat.ServerResponse'),
    AllUsers:            root.lookupType('chat.AllUsers'),
    ForDm:               root.lookupType('chat.ForDm'),
    BroadcastDelivery:   root.lookupType('chat.BroadcastDelivery'),
    GetUserInfoResponse: root.lookupType('chat.GetUserInfoResponse'),
  };
}

function sendFrame(type, protoType, fields) {
  const bytes  = protoType.encode(protoType.create(fields)).finish();
  const header = Buffer.alloc(5);
  header.writeUInt8(type, 0);
  header.writeUInt32BE(bytes.length, 1);
  socket.write(Buffer.concat([header, bytes]));
}

function handleServerMessage(type, payload) {
  switch (type) {
    case MSG.SERVER_RESPONSE: {
      const msg = protos.ServerResponse.decode(payload).toJSON();
      mainWindow.webContents.send('server-response', {
        ok:      msg.isSuccessful || false,
        code:    msg.statusCode   || 0,
        message: msg.message      || '',
      });
      break;
    }
    case MSG.BROADCAST: {
      const msg = protos.BroadcastDelivery.decode(payload).toJSON();
      mainWindow.webContents.send('broadcast', {
        from:    msg.usernameOrigin || '',
        message: msg.message        || '',
      });
      break;
    }
    case MSG.FOR_DM: {
      const msg = protos.ForDm.decode(payload).toJSON();
      mainWindow.webContents.send('dm', {
        from:    msg.usernameDes || '',
        message: msg.message     || '',
      });
      break;
    }
    case MSG.ALL_USERS: {
      const msg   = protos.AllUsers.decode(payload).toJSON();
      const users = (msg.usernames || []).map((name, i) => ({
        name,
        status: (msg.status || [])[i] || 0,
      }));
      mainWindow.webContents.send('all-users', users);
      break;
    }
    case MSG.USER_INFO_RESP: {
      const msg = protos.GetUserInfoResponse.decode(payload).toJSON();
      mainWindow.webContents.send('user-info', {
        username: msg.username  || '',
        ip:       msg.ipAddress || '',
        status:   msg.status    || 0,
      });
      break;
    }
    default:
      console.warn('[main] Tipo desconocido:', type);
  }
}

ipcMain.handle('connect', async (_, { username, ip, port }) => {
  return new Promise((resolve) => {
    myUsername = username;
    recvBuf    = Buffer.alloc(0);
    let resolved = false;

    socket = new net.Socket();

    socket.on('data', (chunk) => {
      recvBuf = Buffer.concat([recvBuf, chunk]);
      while (recvBuf.length >= 5) {
        const type   = recvBuf.readUInt8(0);
        const length = recvBuf.readUInt32BE(1);
        if (recvBuf.length < 5 + length) break;
        const payload = recvBuf.slice(5, 5 + length);
        recvBuf       = recvBuf.slice(5 + length);
        if (!resolved) {
          if (type === MSG.SERVER_RESPONSE) {
            const msg = protos.ServerResponse.decode(payload).toJSON();
            resolved  = true;
            if (msg.isSuccessful) {
              resolve({ ok: true, message: msg.message });
            } else {
              socket.destroy();
              resolve({ ok: false, message: msg.message });
            }
          }
        } else {
          handleServerMessage(type, payload);
        }
      }
    });

    socket.on('error', (err) => {
      if (!resolved) { resolved = true; resolve({ ok: false, message: err.message }); }
    });

    socket.on('close', () => {
      if (!resolved) { resolved = true; resolve({ ok: false, message: 'Conexión cerrada inesperadamente.' }); }
    });

    socket.connect(port, ip, () => {
      myIp = socket.localAddress || '127.0.0.1';
      sendFrame(MSG.REGISTER, protos.Register, { username: myUsername, ip: myIp });
    });
  });
});

ipcMain.on('send-broadcast', (_, message) => {
  sendFrame(MSG.GENERAL, protos.MessageGeneral, {
    message, status: 0, usernameOrigin: myUsername, ip: myIp,
  });
});

ipcMain.on('send-dm', (_, { to, message }) => {
  sendFrame(MSG.DM, protos.MessageDM, {
    message, status: 0, usernameDes: to, ip: myIp,
  });
});

ipcMain.on('change-status', (_, status) => {
  sendFrame(MSG.CHANGE_STATUS, protos.ChangeStatus, { status, username: myUsername, ip: myIp });
});

ipcMain.on('list-users', () => {
  sendFrame(MSG.LIST_USERS, protos.ListUsers, { username: myUsername, ip: myIp });
});

ipcMain.on('get-user-info', (_, username) => {
  sendFrame(MSG.GET_USER_INFO, protos.GetUserInfo, {
    usernameDes: username, username: myUsername, ip: myIp,
  });
});

ipcMain.on('quit', () => {
  if (socket) {
    try { sendFrame(MSG.QUIT, protos.Quit, { quit: true, ip: myIp }); } catch (_) {}
    socket.destroy();
  }
  app.quit();
});

async function createWindow() {
  await loadProtos();
  mainWindow = new BrowserWindow({
    width: 420, height: 720, minWidth: 360, minHeight: 500,
    titleBarStyle: 'hiddenInset',
    webPreferences: {
      preload: path.join(__dirname, 'preload.js'),
      contextIsolation: true,
      nodeIntegration: false,
    },
  });
  mainWindow.loadFile(path.join(__dirname, 'renderer', 'index.html'));
}

app.whenReady().then(createWindow);
app.on('window-all-closed', () => app.quit());