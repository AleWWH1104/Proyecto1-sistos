// ── State ─────────────────────────────────────────────────────────────────
let myUsername   = '';
let activeChatDM = null; // null = general, string = username for DM
let users        = [];   // [{name, status}]

const STATUS_LABEL = ['ACTIVO', 'OCUPADO', 'INACTIVO'];
const STATUS_CLASS = ['active', 'busy', 'inactive'];

// ── DOM refs ──────────────────────────────────────────────────────────────
const loginScreen    = document.getElementById('login-screen');
const chatScreen     = document.getElementById('chat-screen');
const inpUsername    = document.getElementById('inp-username');
const inpIp          = document.getElementById('inp-ip');
const inpPort        = document.getElementById('inp-port');
const btnConnect     = document.getElementById('btn-connect');
const loginError     = document.getElementById('login-error');
const myUsernameLabel= document.getElementById('my-username-label');
const myStatusBadge  = document.getElementById('my-status-badge');
const myAvatar       = document.getElementById('my-avatar');
const usersList      = document.getElementById('users-list');
const searchUsers    = document.getElementById('search-users');
const messagesEl     = document.getElementById('messages');
const msgInput       = document.getElementById('msg-input');
const btnSend        = document.getElementById('btn-send');
const btnRefresh     = document.getElementById('btn-refresh');
const btnQuit        = document.getElementById('btn-quit');
const btnStatusMenu  = document.getElementById('btn-status-menu');
const statusMenu     = document.getElementById('status-menu');
const chatHeaderName = document.getElementById('chat-header-name');
const chatHeaderSub  = document.getElementById('chat-header-sub');
const chatHeaderAv   = document.getElementById('chat-header-avatar');
const btnInfo        = document.getElementById('btn-info');
const infoModal      = document.getElementById('info-modal');
const modalUsername  = document.getElementById('modal-username');
const modalIp        = document.getElementById('modal-ip');
const modalStatus    = document.getElementById('modal-status');
const modalClose     = document.getElementById('modal-close');

// ── Helpers ───────────────────────────────────────────────────────────────
function now() {
  return new Date().toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' });
}

function initial(name) {
  return (name || '?')[0].toUpperCase();
}

function addMessage({ cls, sender, text, time }) {
  const div = document.createElement('div');
  div.className = `msg ${cls}`;
  if (sender) div.innerHTML += `<div class="sender">${sender}</div>`;
  div.innerHTML += `<div>${text}</div><div class="time">${time || now()}</div>`;
  messagesEl.appendChild(div);
  messagesEl.scrollTop = messagesEl.scrollHeight;
}

function systemMsg(text) {
  addMessage({ cls: 'system', text });
}

// ── Login ─────────────────────────────────────────────────────────────────
btnConnect.addEventListener('click', async () => {
  const username = inpUsername.value.trim();
  const ip       = inpIp.value.trim();
  const port     = parseInt(inpPort.value.trim());

  if (!username || !ip || !port) {
    loginError.textContent = 'Completa todos los campos.';
    return;
  }

  btnConnect.disabled    = true;
  btnConnect.textContent = 'Conectando…';
  loginError.textContent = '';

  const result = await window.chat.connect({ username, ip, port });

  if (!result.ok) {
    loginError.textContent = result.message || 'Error al conectar.';
    btnConnect.disabled    = false;
    btnConnect.textContent = 'Conectar';
    return;
  }

  myUsername = username;
  myUsernameLabel.textContent = username;
  myAvatar.textContent        = initial(username);

  loginScreen.classList.remove('active');
  chatScreen.classList.add('active');

  systemMsg('Conectado al chat 🎉');
  window.chat.listUsers();
});

inpUsername.addEventListener('keydown', e => { if (e.key === 'Enter') btnConnect.click(); });
inpPort.addEventListener('keydown',     e => { if (e.key === 'Enter') btnConnect.click(); });

// ── Users list ────────────────────────────────────────────────────────────
function renderUsers(list) {
  users = list;
  const filter = searchUsers.value.toLowerCase();

  usersList.innerHTML = '';

  // General chat entry always first
  const general = document.createElement('div');
  general.className = `user-item general${activeChatDM === null ? ' selected' : ''}`;
  general.innerHTML = `
    <div class="avatar">G</div>
    <div>
      <div class="uname">Chat general</div>
      <div class="ustatus">todos</div>
    </div>`;
  general.addEventListener('click', () => selectChat(null));
  usersList.appendChild(general);

  list
    .filter(u => u.name !== myUsername)
    .filter(u => u.name.toLowerCase().includes(filter))
    .forEach(u => {
      const item = document.createElement('div');
      item.className = `user-item${activeChatDM === u.name ? ' selected' : ''}`;
      item.innerHTML = `
        <div class="avatar">${initial(u.name)}</div>
        <div style="flex:1;min-width:0">
          <div class="uname">${u.name}</div>
          <div class="ustatus">${STATUS_LABEL[u.status] || 'ACTIVO'}</div>
        </div>
        <div class="dot ${STATUS_CLASS[u.status] || 'active'}"></div>`;
      item.addEventListener('click', () => selectChat(u.name));
      usersList.appendChild(item);
    });
}

searchUsers.addEventListener('input', () => renderUsers(users));

btnRefresh.addEventListener('click', () => window.chat.listUsers());

window.chat.onAllUsers(list => renderUsers(list));

// ── Select chat (general or DM) ───────────────────────────────────────────
function selectChat(username) {
  activeChatDM = username;
  renderUsers(users);

  if (username === null) {
    chatHeaderName.textContent = 'Chat general';
    chatHeaderSub.textContent  = 'todos los usuarios';
    chatHeaderAv.textContent   = 'G';
    btnInfo.style.display      = 'none';
  } else {
    chatHeaderName.textContent = username;
    chatHeaderSub.textContent  = 'mensaje directo';
    chatHeaderAv.textContent   = initial(username);
    btnInfo.style.display      = 'block';
  }

  messagesEl.innerHTML = '';
  systemMsg(username ? `DM con ${username}` : 'Chat general');
  msgInput.focus();
}

// ── Send message ──────────────────────────────────────────────────────────
function sendMessage() {
  const text = msgInput.value.trim();
  if (!text) return;
  msgInput.value = '';

  if (activeChatDM === null) {
    window.chat.sendBroadcast(text);
    addMessage({ cls: 'outgoing', sender: 'Tú', text });
  } else {
    window.chat.sendDM(activeChatDM, text);
    addMessage({ cls: 'dm-out', sender: 'Tú', text });
  }
}

btnSend.addEventListener('click', sendMessage);
msgInput.addEventListener('keydown', e => { if (e.key === 'Enter') sendMessage(); });

// ── Receive messages ──────────────────────────────────────────────────────
window.chat.onBroadcast(({ from, message }) => {
  if (activeChatDM === null) {
    addMessage({ cls: 'incoming', sender: from, text: message });
  } else {
    // badge / notification could go here
  }
});

window.chat.onDM(({ from, message }) => {
  if (activeChatDM === from) {
    addMessage({ cls: 'dm-in', sender: from, text: message });
  } else {
    systemMsg(`💬 DM de ${from} (abre su chat para leerlo)`);
  }
});

window.chat.onServerResp(({ ok, message }) => {
  if (message && message !== 'Mensaje enviado.') {
    systemMsg(ok ? `✓ ${message}` : `✗ ${message}`);
  }
  // Re-fetch users after status change
  window.chat.listUsers();
});

// ── Status menu ───────────────────────────────────────────────────────────
btnStatusMenu.addEventListener('click', e => {
  e.stopPropagation();
  statusMenu.classList.toggle('hidden');
});

document.addEventListener('click', () => statusMenu.classList.add('hidden'));

statusMenu.querySelectorAll('button').forEach(btn => {
  btn.addEventListener('click', () => {
    const s = parseInt(btn.dataset.status);
    window.chat.changeStatus(s);
    myStatusBadge.textContent = STATUS_LABEL[s];
    myStatusBadge.className   = `badge ${STATUS_CLASS[s]}`;
    statusMenu.classList.add('hidden');
  });
});

// Update own badge when server notifies inactivity
window.chat.onServerResp(({ ok, message }) => {
  if (ok && message && message.includes('INACTIVO')) {
    myStatusBadge.textContent = 'INACTIVO';
    myStatusBadge.className   = 'badge inactive';
  }
  if (ok && message && message.includes('ACTIVO')) {
    myStatusBadge.textContent = 'ACTIVO';
    myStatusBadge.className   = 'badge active';
  }
});

// ── User info modal ───────────────────────────────────────────────────────
btnInfo.addEventListener('click', () => {
  if (activeChatDM) window.chat.getUserInfo(activeChatDM);
});

window.chat.onUserInfo(({ username, ip, status }) => {
  modalUsername.textContent = username;
  modalIp.textContent       = ip;
  modalStatus.textContent   = STATUS_LABEL[status] || 'ACTIVO';
  infoModal.classList.remove('hidden');
});

modalClose.addEventListener('click', () => infoModal.classList.add('hidden'));

// ── Quit ──────────────────────────────────────────────────────────────────
btnQuit.addEventListener('click', () => window.chat.quit());