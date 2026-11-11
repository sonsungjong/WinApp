#pragma once
#include <string_view>

namespace Api
{
inline constexpr std::string_view ControlPage = R"HTML(<!doctype html>
<html lang="ko"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>NextVideoPlayer 제어</title><style>
*{box-sizing:border-box}body{font:16px 'Segoe UI',sans-serif;background:#161922;color:#edf1fa;max-width:760px;margin:32px auto;padding:0 20px}h1{font-size:25px}section{background:#222736;padding:20px;border-radius:14px;margin:18px 0}button,input,textarea{font:inherit;border-radius:7px;padding:10px;border:1px solid #4b556b;background:#12151e;color:#fff}button{cursor:pointer;margin:4px}button:hover{border-color:#3de0ed}input{width:125px;margin:4px}textarea{width:100%;height:100px;resize:vertical}label{display:inline-block;margin:8px 14px 8px 0}small,.muted{color:#aab5ca}#connection{color:#3de0ed}#file{overflow-wrap:anywhere}#error{color:#ffb1b1;white-space:pre-wrap}.row{display:flex;gap:5px;flex-wrap:wrap}input[type=checkbox]{width:auto}
</style><h1>NextVideoPlayer 원격 제어</h1><div id="connection">연결 중…</div>
<section><div id="file">플레이어에서 영상을 열어 주세요.</div><p class="muted" id="state">상태 대기 중</p>
<div class="row"><button data-play="playing">▶ 재생</button><button data-play="paused">Ⅱ 일시정지</button><button data-play="stopped">■ 정지</button></div>
<label>위치(초) <input id="position" type="number" min="0" step="0.1" value="0"></label><button id="seek">이동</button>
<div><label>배속 <input id="speed" type="number" min="0.2" max="16" step="0.1" value="1"></label><button id="setSpeed">적용</button></div>
<div><label>음량 <input id="volume" type="number" min="0" max="100" value="100"></label><button id="setVolume">적용</button><label><input id="muted" type="checkbox">음소거</label><label><input id="repeat" type="checkbox">반복</label></div>
</section><section><strong>파일 경로로 열기</strong><p><small>플레이어가 실행 중인 PC의 전체 경로를 한 줄에 하나씩 입력하세요. 현재 재생목록에 추가됩니다.</small></p><textarea id="files" placeholder="C:\Videos\sample.mp4"></textarea><button id="open">목록에 추가하고 재생</button></section>
<div id="error" role="status"></div><p class="muted"><small>WebSocket 주소 <span id="endpoint"></span><br>서버를 끄거나 포트를 바꿔도 플레이어는 계속 사용할 수 있습니다.</small></p>
<script>
const $ = id => document.getElementById(id);
let socket = null, retry = null, serial = 0;
const endpoint = 'ws://' + location.host + '/api/v1';
$('endpoint').textContent = endpoint;

function send(values) {
    if (!socket || socket.readyState !== WebSocket.OPEN) {
        $('error').textContent = '플레이어 연결을 확인해 주세요.';
        return;
    }
    socket.send(JSON.stringify({type: 'all', requestId: 'web-' + (++serial), ...values}));
    $('error').textContent = '';
}

function update(state) {
    $('file').textContent = state.file || '플레이어에서 영상을 열어 주세요.';
    $('state').textContent = (state.loading ? '파일 준비 중' : state.playback) + ' · ' +
        (state.positionMs / 1000).toFixed(1) + ' / ' + (state.durationMs / 1000).toFixed(1) +
        '초 · ' + Number(state.speed).toFixed(2) + '배속';
    for (const [id, value] of [['position', state.positionMs / 1000], ['speed', state.speed], ['volume', state.volume]]) {
        if (document.activeElement !== $(id)) {
            $(id).value = id === 'position' ? value.toFixed(1) : id === 'speed' ? value.toFixed(2) : value;
        }
    }
    $('muted').checked = state.muted;
    $('repeat').checked = state.repeat;
    if (state.mediaError) $('error').textContent = state.mediaError;
}

function connect() {
    socket = new WebSocket(endpoint);
    socket.onopen = () => {
        $('connection').textContent = '연결됨';
        send({});
    };
    socket.onmessage = event => {
        try {
            const message = JSON.parse(event.data);
            if (message.state) update(message.state);
            if (message.ok === false) $('error').textContent = message.error.message;
        } catch (_) {
            $('error').textContent = '응답을 읽을 수 없습니다.';
        }
    };
    socket.onclose = () => {
        $('connection').textContent = '연결 끊김 · 다시 연결 중…';
        clearTimeout(retry);
        retry = setTimeout(connect, 2000);
    };
    socket.onerror = () => { $('connection').textContent = '연결할 수 없습니다.'; };
}

document.querySelectorAll('[data-play]').forEach(button => {
    button.onclick = () => send({playback: button.dataset.play});
});
$('seek').onclick = () => send({positionMs: Number($('position').value) * 1000});
$('setSpeed').onclick = () => send({speed: Number($('speed').value)});
$('setVolume').onclick = () => send({volume: Number($('volume').value)});
$('muted').onchange = () => send({muted: $('muted').checked});
$('repeat').onchange = () => send({repeat: $('repeat').checked});
$('open').onclick = () => {
    const files = $('files').value.split(/\r?\n/).map(path => path.trim().replace(/^"|"$/g, '')).filter(Boolean);
    if (files.length) send({files, playback: 'playing', positionMs: 0});
};
connect();
</script></html>)HTML";
}
